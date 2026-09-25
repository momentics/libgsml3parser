# FFI Bindings — Python / Go / Rust

First-party bindings over the stable C ABI of **libgsml3parser**
(`include/gsml3parser/gsml3parser_c.h`, `GSML3_ABI_VERSION == 1`). They expose
the full GSM L3 parser (all 12 PD domains), the A-bis RSL interface, the LAPDm
entity, and the BTS stack layer (subscriber registry / session, procedure
orchestrator, response builders) with one shared set of semantics: RAII-style
ownership, a borrowed-session rule, the queue callback model, synchronous
thread-local error copying, and zero-allocation buffer patterns on the hot
path. All three run against the **shared** build of the C core and carry no
third-party runtime dependencies; their demos and tests reproduce the same
behavioral vectors as `tests/test_c_api.cpp`.

## Overview

| Language | Location | Runtime deps | v1 surface | Demo |
|----------|----------|--------------|------------|------|
| Python | `python/` — package `gsml3parser` | stdlib only (`ctypes`); `pytest` is test-only | the whole C ABI: all 238 functions registered with explicit `argtypes`/`restype`; completeness against the header is pinned by `tests/test_api_surface.py` | `examples/bts_simulation.py` |
| Go | `go/` — module `github.com/momentics/libgsml3parser/bindings/go` | stdlib only (cgo; needs a C compiler) | core/config/message, A-bis RSL, LAPDm, registry/session, orchestrator/response functions plus the typed L3 builders `BuildCMServiceRequest` and `BuildSetup` — 128 of the header's functions | `cmd/gsmexample/main.go` |
| Rust | `rust/` — workspace `gsml3parser-sys` (handwritten `extern "C"` + `#[repr(C)]` mirrors, no bindgen/codegen) + safe `gsml3parser` crate | none | the same 128-function v1 surface; the sys crate's extern block is checked for completeness at compile time (`gsml3parser-sys/tests/surface.rs`) | `gsml3parser/examples/bts_simulation.rs` |

Every demo runs the unified "MO call over SDCCH" scenario: stack
initialization (registry + borrowed session + orchestrator + a LAPDm entity
whose callbacks are registered in the C core), one simulated MS-side UI frame
(`CMServiceRequest`), the automatic `CMServiceAccept` response built and
transmitted back through the same stack, a CC `Setup` → `CallProceeding`
second leg, the chain phase (`CALL_SETUP_MO`), and the T3101 (3 s) chain-timer
expiry. Exit code 0 on success is the contract the unified gate relies on.

## Prerequisites

Toolchains: **Python ≥ 3.10** (stdlib `ctypes`; no packages are installed —
in-repository runs add `python/` to `sys.path`), **Go ≥ 1.21 plus a C
toolchain for cgo** — on Windows the Go toolchain needs a GCC-compatible
compiler, so the unified gate probes candidates with a real `go build` smoke
test: a native MinGW `gcc` first (already on PATH, or the stock MSYS2 install)
and, as a fallback, a `clang` + lld pair routed through the small pass-through
wrapper in `bindings/go/ccshim`; on Linux it is plain gcc/clang. And **Rust
stable ≥ 1.75** (the gate additionally uses the clippy component).

The bindings wrap a **shared library build of the core**; they never compile
C++ themselves. From the repository root:

```powershell
cmake -S . -B build_bindings -DBUILD_SHARED_LIBS=ON -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=OFF
cmake --build build_bindings --config Release --target gsml3parser
New-Item -ItemType Directory -Force build_bindings\bin | Out-Null
Copy-Item build_bindings\Release\gsml3parser.dll, build_bindings\Release\gsml3parser.lib `
    -Destination build_bindings\bin        # Unix: copy libgsml3parser.so* (with symlinks)
```

`build_bindings/bin/` is the single flat artifact directory every binding
resolves. Library search order per consumer:

1. `GSML3PARSER_LIBRARY` — Python, explicit path to the shared library;
2. `GSML3PARSER_LIB_DIR` — explicit directory (honored by the Rust `build.rs`
   and as a Python fallback);
3. `<repo>/build_bindings/bin/` — created above;
4. core CMake build directories / `cmake --install` layout, then the platform
   search path (`PATH` on Windows, `LD_LIBRARY_PATH` on Unix).

The unified gate performs steps 1–4 automatically and also puts
`build_bindings/bin/` on the process path so cgo's import library and the
Windows DLL resolve identically.

## Quickstart

### Python

```python
import gsml3parser as g                       # loads build_bindings/bin/gsml3parser.* (ABI-checked)

msg = g.Message.from_hex("60 0D 00")          # parse: RR Channel Release
print(msg.name, msg.size(), msg.hex())        # ChannelRelease 3 600d00
msg.close()

with g.GsmL3Stack(tmsi=0x87654321, auto_response=True) as stack:
    l3 = g.build_cm_service_request(1, g.ID_TMSI, 0x87654321, None)   # MO call, C typed builder
    txs = stack.send_frame(g.lapdm_mini.ui(0, False, l3))             # MS-side UI in -> auto-response out
    dec = g.lapdm_frame_decode(txs[0])                                # zero-copy view inside txs[0]
    resp = g.Message.from_bytes(dec.info)
    try:
        print(stack.last_step.token, resp.name)   # 10 CMServiceAccept
    finally:
        resp.close()
```

### Go

```go
import (
	"fmt"

	gsml3parser "github.com/momentics/libgsml3parser/bindings/go"
)

func main() { // full working demo with asserts: go run ./cmd/gsmexample
	msg, err := gsml3parser.ParseHex("60 0D 00", nil)      // parse: RR Channel Release
	if err != nil { panic(err) }
	fmt.Println(msg.Name(), msg.Size())                     // ChannelRelease (3, nil)
	defer msg.Close()

	stack, err := gsml3parser.NewGsmL3Stack(gsml3parser.StackOptions{
		TMSI: 0x87654321, SAPI: 0, Profile: 0, AutoResponse: true,
	})
	if err != nil { panic(err) }
	l3, _ := gsml3parser.BuildCMServiceRequest(1 /* MO call */, gsml3parser.IDTMSI, 0x87654321, "")
	txs, err := stack.SendFrame(gsml3parser.UIFrame(0, false, l3))  // MS-side UI in -> auto-response out
	if err != nil { panic(err) }
	dec, _ := gsml3parser.DecodeFrame(txs[0])                        // zero-copy view inside txs[0]
	resp, err := gsml3parser.Parse(dec.Payload, nil)
	if err != nil { panic(err) }

	size, _ := resp.Size()
	step, _ := stack.LastStep()
	fmt.Println(step.Token, resp.Name(), size)   // 10 CMServiceAccept 4
	resp.Close()
	stack.Close()                                        // Close is idempotent
}
```

### Rust

```rust
use gsml3parser::lapdm::{decode_frame, mini};
use gsml3parser::{build_cm_service_request, GsmL3Stack, Message};
// full working demo with asserts: cargo run -p gsml3parser --example bts_simulation

fn run() -> Result<(), gsml3parser::GsmL3Error> {
	let msg = Message::parse_hex("60 0D 00", None)?;      // parse: RR Channel Release
	println!("{}", msg.name()?);                           // ChannelRelease

	let mut stack = GsmL3Stack::new(0x8765_4321, 0 /* plain registry */,
	                                0 /* SAPI 0 */, 0 /* profile */, true)?;
	let l3 = build_cm_service_request(1, gsml3parser::sys::GSML3_ID_TMSI, 0x8765_4321, None)?;
	let txs = stack.send_frame(&mini::ui(0, false, &l3)?)?; // MS-side UI in -> auto-response out
	let dec = decode_frame(&txs[0])?;                       // zero-copy view inside txs[0]
	let resp = Message::parse(dec.payload.expect("UI response carries L3"), None)?;

	let token = stack.last_step().map(|s| s.token).unwrap_or(0);
	println!("{} {}", token, resp.name()?);                  // 10 CMServiceAccept

	stack.close();                                          // idempotent take-pattern teardown
	Ok(())
}
```

With auto-response off (Python's keyword default is on, so pass
`auto_response=False`; Go sets `AutoResponse: false`; Rust passes `false` as its
mandatory final parameter) each `send_frame`/`SendFrame` leaves the L3 queue
unprocessed. Drain it and orchestrate manually — Python `drain_l3_events()`,
`feed_l3()`, `build_response()`; Go `DrainEvents()`, `FeedL3()`,
`BuildResponse()`; Rust `drain_l3_events()`, `feed_l3()`, `build_response()` —
then transmit through the entity (`send_ui`).

## Ownership & threading

- **Owned handles** — every C handle a constructor creates is released exactly
  once, by the language's RAII: Python `close()` / context manager / guarded
  `__del__`; Go idempotent `Close()` (`atomic` flag, `CompareAndSwap`); Rust
  `Drop` with `NonNull` fields and the take-pattern (a second `close()` or a
  post-`Drop` access is a no-op, not undefined behavior). Stack teardown always
  releases **entity → orchestrator → registry**. After close, every method of
  the closed object raises a typed error **without any FFI call** — there is no
  "closed" state on the raw C side, so this invariant is enforced by the
  wrappers (and proven per language by the test seams: `CALL_COUNTS` in Python,
  the test-only raw spies in Go, the unit tests around `close_once` in Rust).
- **Borrowed sessions** — a `Session` is never freed by any binding: the
  registry owns it (`gsml3_registry_free` releases every session). Session
  accessors check that their owner (registry/stack) is still open before
  touching the pointer; the Rust binding additionally encodes the borrow in
  `Session<'r>`.
- **One thread per handle** — owned C handles are single-thread by contract.
  Rust's `GsmL3Stack` implements `Send` (ownership can move to another thread)
  and deliberately does NOT implement `Sync`: sharing `&GsmL3Stack` would allow
  concurrent calls on the same entity, exactly what the ABI forbids. The Python
  and Go stacks follow the same one-stack-one-thread rule (their internal
  locks serialize receive/drain/respond, they are not sharing mechanisms).
  Only a sharded registry (`shard_count` 4/8/16/32) makes registry-mediated
  calls thread-safe on the C side; direct session access is unsynchronized in
  every flavor.

### Callback rules (queue model)

LAPDm entity callbacks fire **synchronously inside** the C call and receive
spans valid only during that callback — "transmit or copy, never retain". All
three bindings therefore let a callback do exactly one kind of work: a
zero-copy read of the span (ctypes `string_at`, Go `unsafe.Slice`, Rust
`slice::from_raw_parts`) plus an append to a lock-guarded queue — no FFI call
and no blocking, so the entity FSM is never mutated from within its own
callback. After the C call returns, the stack drains the queues and does the
real processing: parse L3, feed the orchestrator, build the pending response
into an exactly-sized buffer (`required_size` → `build_response`) and send it
as a new UI frame — whose transmit event is itself captured by the callback
queue.

Context restoration across the `void* user` boundary is language-native:

| Language | `user` token | Reclamation |
|----------|--------------|-------------|
| Python | a ctypes `_CallbackContext(alive=1)` structure; its address is stable and both `CFUNCTYPE` bridges plus the context are hard-anchored in `GsmL3Stack._c_cb_keepalive`, so the garbage collector can never collect a callback C still references (proved by `test_callbacks_survive_gc`) | the anchor dict is cleared on `close()` after `entity_free` |
| Go | the address of the entity's `cgo.Handle` **field** — one machine word with no Go pointers, so retaining it in the C core complies with cgo's pointer rules; the exported bridge reads it synchronously inside the C→Go call and resolves the queue owner | `handle.Delete()` runs in `Close()`, strictly after `gsml3_lapdm_entity_free` |
| Rust | `Box::into_raw(Box::new(Arc::new(TrampolineState)))`; static trampolines dereference, `Arc::clone`, read the span into an owned copy, and push under a mutex | `Box::from_raw` happens exactly once — in the entity's close/drop path, strictly after `gsml3_lapdm_entity_free` |

## Testing

The single unified gate builds (or reuses) the shared core and runs every
binding from the repository root:

```powershell
pwsh scripts/verify_bindings.ps1             # invariants + core lib + python + go + rust, fail-fast
pwsh scripts/verify_bindings.ps1 -Only go    # a single language
pwsh scripts/verify_bindings.ps1 -SkipLib    # reuse an existing build_bindings/bin
```

Per-language equivalents:

```powershell
python -m pytest bindings/python/tests -q
go vet ./... ; go test -count=1 -race ./...   # from bindings/go (CGO_ENABLED=1, lib dir on PATH)
cargo --manifest-path bindings/rust/Cargo.toml clippy --workspace -- -D warnings
cargo --manifest-path bindings/rust/Cargo.toml test --workspace
```

and the demos: `python bindings/python/examples/bts_simulation.py`,
`go run ./cmd/gsmexample` (from `bindings/go`), and
`cargo run -p gsml3parser --example bts_simulation`. GitHub Actions runs the
same unified script on `ubuntu-latest` and `windows-latest`
(`.github/workflows/bindings.yml`); the core-only gate remains
`scripts/verify.ps1` and is untouched by this directory. The gate starts with
repo invariants: the root `VERSION` file (single source of truth for the
product version, see §64 of `doc/API.md`) must be well-formed, CMake must read
from it, and every Cargo manifest literal must match it — drift fails the run.

## Extending the surface

The C ABI is the contract; extending a binding is additive and never touches
the core:

1. **Rust** — declare the new function in `gsml3parser-sys/src/lib.rs` (exact
   C types; LAI arguments are `int` in the `gsml3_build_*` builders but string
   pointers in the `gsml3_response_build_*` ones), extend
   `_use_all_128`-style coverage in `gsml3parser-sys/tests/surface.rs` so a
   missing or mistyped declaration fails to compile, then wrap it in the safe
   crate following the existing error/copy idioms.
2. **Go** — cgo already sees every header symbol; add the wrapper method
   (validate before FFI → call → `lastError()` on failure) and a test. The
   128-function count in this README/Overview is documentation only — there is
   no codegen to regenerate.
3. **Python** — the table in `gsml3parser/_library.py` already covers the whole
   header (the surface test fails on drift either way); adding a method to one
   of the wrappers is the only work left.

Bump the v1 counts here and in `doc/API.md §64` only when the exposed set
actually grows. The product version itself never needs touching: change the
root `VERSION` file and the manifests/gate keep it honest.
