# gsml3parser — Python binding

`ctypes` binding over the stable C ABI of **libgsml3parser**
(`include/gsml3parser/gsml3parser_c.h`, `GSML3_ABI_VERSION == 1`). Standard
library only — no third-party runtime dependencies. All 238 functions of the
C ABI (S1–S9: core/config/message, A-bis RSL, LAPDm, BTS registry/session,
orchestrator/response builders, typed getters/builders) are registered with
explicit `argtypes`/`restype`; completeness against the header is pinned by
`tests/test_api_surface.py`.

## Layout

- `gsml3parser/` — the package:
  - `stack.py` — high-level API (RAII wrappers, value objects, builders, the
    queue-model BTS stack),
  - `_library.py` — shared-library loading, ABI check, mirror structs and the
    full prototype table,
  - `_errors.py` — the C error model (codes 1:1, typed exception hierarchy,
    synchronous last-error copy),
  - `_lapdm.py` — MS/peer-side LAPDm mini-codec for simulation and tests.
- `tests/` — pytest suite (API surface, core wrappers, stack simulation).
- `examples/bts_simulation.py` — end-to-end MO-call demo; the exit code is the
  contract (`0` = all steps asserted).
- `build_backend.py` / `pyproject.toml` — PEP 517 packaging: the build reads
  the product version from the repository-root `VERSION` file and stamps it
  into the package metadata.

## Building and installing

The binding wraps a **prebuilt shared library** of the C core; it does not
compile C++. From the repository root:

```powershell
cmake -S . -B build_bindings -DBUILD_SHARED_LIBS=ON -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=OFF
cmake --build build_bindings --config Release --target gsml3parser
# flatten the artifacts (gsml3parser.dll/.lib on Windows, libgsml3parser.so* elsewhere):
#   <root>\build_bindings\bin
```

The loader searches, in order:

1. `GSML3PARSER_LIBRARY` — explicit path to the shared library;
2. `GSML3PARSER_LIB_DIR` — explicit directory containing it;
3. `<repo>/build_bindings/bin/` (the flat artifacts dir);
4. `<repo>/build/{Release,Debug}/` (Windows) or `<repo>/build/` (Unix);
5. the platform library search path.

After loading, the import fails fast unless the binary reports
`gsml3_abi_version() == 1`.

Install from this directory (`pip install .` — the version is stamped from the
root `VERSION` file at build time), or simply add it to `sys.path` for
in-repository use.

Run the suite and the demo from the repository root:

```
python -m pytest bindings/python/tests -q
python bindings/python/examples/bts_simulation.py
```

## Quickstart

```python
import gsml3parser as g                      # loads the shared C core, ABI-checked

msg = g.Message.from_hex("60 0D 00")         # RR Channel Release
print(msg.name, msg.pd, msg.size(), msg.write().hex())

with g.GsmL3Stack(tmsi=0x87654321) as stack:   # registry + session + orchestrator + LAPDm entity
    l3 = g.build_cm_service_request(1, g.ID_TMSI, 0x87654321, None)
    txs = stack.send_frame(g.lapdm_mini.ui(0, False, l3))   # auto-response: parse -> feed -> build -> send
    dec = g.lapdm_frame_decode(txs[0])                     # UI frame back from the BTS side
    assert dec.command == 1 and dec.info_offset is not None
```

`GsmL3Stack` drives the chain automatically (queue model, below); with
`auto_response=False` the L3 queue stays unprocessed after each `send_frame`,
for manual orchestration via `drain_l3_events()` / `feed_l3()` /
`build_response()` / `send_ui()`. Components (`stack.registry`,
`stack.session`, `stack.orchestrator`, `stack.lapdm_entity`) are reachable as
read-only accessors for advanced use (further sessions, link control); their
lifecycle belongs to the stack.

## Error model

The C ABI reports failures as a code + thread-local message; the next
successful call clears it. The binding therefore copies the pair
synchronously at every failing call site and raises a typed exception
(`gsml3parser._errors`): `GsmL3Error(code, message)` with subclasses per error
class (`InvalidArgument`, `TruncatedInput`, `ProtocolDomainError`,
`UnsupportedOperation`, `GsmL3NoMemory`, `BufferTooSmall`, `InternalError`),
plus `ClosedGsmL3ObjectError` for pure binding-level misuse (access after
close). Wrapper validation rejects `None`/empty/short inputs BEFORE the FFI
boundary; the C-documented NULL-safe entry points pass `None` through at the
raw level. Mutating calls whose C signature has no error return value (void /
count-only) are polled for the thread-local error immediately after the call.

## Ownership and callbacks

- **Owned handles** (`Config`, `Message`, `RslFrame`, `Registry`,
  `Orchestrator`, `LapdmEntity`): each wrapper releases exactly one C handle,
  idempotently — `close()` / context manager / guarded finalizer. After close
  every method raises without any FFI.
- **Borrowed** (`Session`): owned by its `Registry`, never freed by the
  binding; accessors refuse to run once the owning registry is closed.
- **Callback model (queue model)**: C entity callbacks fire synchronously
  inside `entity.receive()/send_*()` with spans valid only during the call.
  The stack's bridges therefore do only memory-safe work — copy the span and
  append to an internal queue; parsing, orchestration, response building and
  transmission all happen in Python after the C call returned, so the entity
  FSM is never mutated from inside its own callback. The two `CFUNCTYPE`
  closures are hard-anchored against the garbage collector for as long as the
  C core may invoke them.
- **Threading**: owned handles are single-thread (C ABI contract); a registry
  with `shard_count > 0` makes its mediated calls thread-safe in C, while
  direct `Session` access is unsynchronized by design. One stack belongs to one
  thread; the internal lock guards the receive/drain/orchestrate sequence, it
  is not a sharing mechanism.

## Versioning

The repository-root `VERSION` file (one semver line) is the single source of
truth: CMake bakes it into the shared core at build time (`gsml3_version()`),
the PEP 517 backend stamps it into package metadata, and
`gsml3parser.__version__` / `g.version()` read the value back from the loaded
core at import. A test pins `version() == VERSION file == __version__`.
