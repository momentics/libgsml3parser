# gsml3parser — Rust binding

Safe Rust wrapper over the stable C ABI of **libgsml3parser**
(`include/gsml3parser/gsml3parser_c.h`, `GSML3_ABI_VERSION == 1`). Zero runtime
dependencies (`std` only) and no bindgen/code generation: the raw layer is
handwritten. The workspace holds two crates — `gsml3parser-sys` (raw
`extern "C"` declarations + `#[repr(C)]` mirrors of the C structs and callback
types) and `gsml3parser` (the safe API). The v1 surface is C-ABI sections
S1–S7 — core/config/message, A-bis RSL, LAPDm, BTS registry/session,
orchestrator/response builders — plus the typed L3 builders
`gsml3_build_cm_service_request` and `gsml3_build_setup`: **128 of the
header's 236 functions**; the curated S8 typed getters stay in the Python
binding. Completeness is checked at compile time:
`gsml3parser-sys/tests/surface.rs` names every one of the 128 declarations, so
a missing or mistyped entry fails the build.

## Layout

- `gsml3parser-sys/` — the raw FFI layer (no dependencies):
  - `src/lib.rs` — the 128 `extern "C"` functions and the `#[repr(C)]` struct
    mirrors with exact C types,
  - `build.rs` — resolves the prebuilt shared library and emits the link
    directives (plain path search, zero build dependencies),
  - `tests/surface.rs` — the compile-time check of the full v1 surface.
- `gsml3parser/` — the safe crate:
  - `src/error.rs` — `GsmL3Error { code, op, msg }` + `ErrorKind` (1:1 with
    `gsml3_error`),
  - `src/message.rs` — `Message` / `Config` / `RslFrame`: parse/parse_hex,
    name/PD/MTI/TI, size/write_to/hex/dump, and the typed builders
    `build_cm_service_request` / `build_setup`,
  - `src/lapdm.rs` — `LapdmEntity` FSM, zero-copy `decode_frame` (`FrameInfo`),
    the static C trampolines, and the `mini` MS-side LAPDm codec for
    simulation,
  - `src/registry.rs` — `Registry` (plain + sharded 4/8/16/32) + `Session<'r>`,
    with the borrow encoded in the lifetime,
  - `src/stack.rs` — `GsmL3Stack`: registry + borrowed session + orchestrator
    + a bridged LAPDm entity with auto-response (the unified Python/Go/Rust
    stack).
- `gsml3parser/examples/bts_simulation.rs` — end-to-end MO-call demo; the exit
  code is the contract (`0` = all steps asserted).
- `gsml3parser/tests/integration.rs` — cross-crate integration suite.

## Building and linking

The binding wraps a **prebuilt shared library** of the C core; it never
compiles C++. `build.rs` searches, in order (first hit wins):

1. `$GSML3PARSER_LIB_DIR` — explicit override (CI, custom installs);
2. `<repo>/build_bindings/bin/` — the flat artifacts dir created by the unified gate;
3. `<repo>/build/Release/`, then `<repo>/build/` — core CMake build directories;
4. `<repo>/install_shared/lib/` — the `cmake --install` layout.

If none is found it fails with the exact cmake commands to run. From the
repository root:

```powershell
cmake -S . -B build_bindings -DBUILD_SHARED_LIBS=ON -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=OFF
cmake --build build_bindings --config Release --target gsml3parser
# flatten the artifacts (gsml3parser.dll + .lib, libgsml3parser.so*) into <root>\build_bindings\bin — see bindings/README.md "Prerequisites"
```

Run clippy, tests and the demo from the repository root:

```
cargo --manifest-path bindings/rust/Cargo.toml clippy --workspace -- -D warnings
cargo --manifest-path bindings/rust/Cargo.toml test --workspace
cargo --manifest-path bindings/rust/Cargo.toml run -p gsml3parser --example bts_simulation
```

## Quickstart

```rust
use gsml3parser::lapdm::{decode_frame, mini};
use gsml3parser::{build_cm_service_request, GsmL3Stack, Message};
// full working demo with asserts: cargo run -p gsml3parser --example bts_simulation

let msg = Message::parse_hex("06 0D 00", None)?;        // parse: RR Channel Release
println!("{}", msg.name()?);                             // ChannelRelease

let mut stack = GsmL3Stack::new(0x8765_4321, 0 /* plain registry */, 0 /* SAPI 0 */,
                                0 /* profile */, true)?; // auto_response: required final parameter
let l3 = build_cm_service_request(1 /* MO call */, gsml3parser::sys::GSML3_ID_TMSI, 0x8765_4321, None)?;
let txs = stack.send_frame(&mini::ui(0, false, &l3)?)?;  // MS-side UI in -> auto-response out
let dec = decode_frame(&txs[0])?;                        // zero-copy view inside txs[0]
let resp = Message::parse(dec.payload.expect("UI response carries L3"), None)?;

let token = stack.last_step().map(|s| s.token).unwrap_or(0);
println!("{} {}", token, resp.name()?);                  // 10 CMServiceAccept
stack.close();                                           // idempotent take-pattern teardown
```

`GsmL3Stack` drives the chain automatically (queue model, below); with
`auto_response = false` each `send_frame` leaves the L3 queue unprocessed for
manual orchestration via `drain_l3_events()` / `feed_l3()` /
`build_response()` / `send_ui()`. Components (`stack.registry()`,
`stack.session()`, `stack.orchestrator()`, `stack.entity()`) are reachable as
read-only accessors for advanced use (further sessions, link control); their
lifecycle belongs to the stack.

## Error model

The C ABI reports failures as a code + thread-local message; the next
successful call clears it. The binding therefore copies both synchronously at
every failing call site into `GsmL3Error { code, op, msg }` — rendered as
`gsml3: <op>: <msg> (<KIND>)` — where `code` is the raw `gsml3_error` value
and `kind()` maps it onto the typed `ErrorKind` (`Ok` … `Internal`). Wrapper
validation rejects out-of-domain input (zero TMSI, empty/oversize IMSI, frame
shorter than 2 bytes, shard count / SAPI / profile ranges) BEFORE the FFI
boundary with code `INVALID_ARG` (the NULL policy); C-documented NULL-safe
entry points pass nulls through and report their sentinels.

## Ownership and callbacks

- **Owned handles** (`Message`, `Config`, `RslFrame`, `LapdmEntity`, `Registry`,
  `GsmL3Stack`): each implements `Drop` with a take-pattern (nulled fields), so
  an explicit `close()` plus the later `Drop` is a no-op second pass. Stack
  teardown releases in the ABI order **entity → orchestrator → registry**.
  After any close, every method returns a closed-class error WITHOUT touching
  the freed handle.
- **Borrowed** (`Session<'r>`): owned by its `Registry`, never freed directly —
  the borrow is encoded in the lifetime, and `gsml3_registry_free` releases
  every session; accessors refuse to run once their owner is closed.
- **Callback model (queue model)**: C entity callbacks fire synchronously
  inside the C call with spans valid only during it. The static trampolines do
  memory-only work — dereference the `void* user` token (a
  `Box::into_raw(Box::new(Arc::new(TrampolineState)))`), clone the `Arc`, read
  the C span into an owned copy via `slice::from_raw_parts`, and push under a
  `Mutex`; they NEVER call any `gsml3_*` function, so the entity FSM is never
  mutated from within its own callback. The stack drains the queues only after
  the C call returns: parse → feed → `required_size` → exact-size
  `build_response` → `send_ui`. The token is reclaimed exactly once with
  `Box::from_raw`, strictly AFTER `gsml3_lapdm_entity_free`
  (`LapdmEntity::close_once`); late callbacks after close are dropped via the
  alive flag.
- **Threading**: `GsmL3Stack` is `Send` (the whole stack may move to another
  thread) and deliberately NOT `Sync` — sharing a `&GsmL3Stack` would allow
  unsynchronized concurrent calls on one entity, exactly what the C ABI
  forbids. The other wrappers hold raw pointers and are used on a single
  thread. A registry with `shard_count` 4/8/16/32 makes its registry-mediated
  calls thread-safe in C (per-shard locks); direct session access is
  unsynchronized by design.

## Versioning

Cargo cannot read a version from a file, so both manifests carry the product
version as a literal that MUST equal the repository-root `VERSION` file — the
single source of truth for releases; the unified gate's manifest version check
fails on drift. `version()` reads the product version back from the loaded
core at runtime; `abi_version()` pins the C ABI revision (1) against
`sys::ABI_VERSION`.
