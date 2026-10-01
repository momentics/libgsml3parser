# gsml3parser — Go binding

cgo binding over the stable C ABI of **libgsml3parser**
(`include/gsml3parser/gsml3parser_c.h`, `GSML3_ABI_VERSION == 1`). Standard
library only at runtime; cgo needs a C toolchain to build. The v1 surface is
C-ABI sections S1–S7 — core/config/message, A-bis RSL, LAPDm, BTS
registry/session, orchestrator/response builders — plus the typed L3 builders
`gsml3_build_cm_service_request` and `gsml3_build_setup`: **128 of the
header's 236 functions**. The curated S8 typed getters are deliberately left
out and can be added without any ABI change. A package `init()` fails loudly
if the linked library reports another ABI revision.

## Layout

- Package root (module `github.com/momentics/libgsml3parser/bindings/go`):
  - `cgo.go` — core observers (`Version`, `ABIVersion`, `LastError`,
    `LastErrorCode`, `Free`), the ABI guard and the enum-mirror constants,
  - `message.go` — the `Message` handle: `Parse` / `ParseHex`, name/PD/MTI/TI,
    size/write, hex/dump with copy-then-free,
  - `lapdm.go` — `LapdmEntity` FSM + the exported `//export` callback bridges
    and their queues, zero-copy `DecodeFrame`,
  - `lapdmmini.go` — MS/peer-side LAPDm mini-codec (`UIFrame`) for simulation
    and tests,
  - `registry.go` — `Registry` (plain + sharded 4/8/16/32) + BORROWED `Session`
    (create by TMSI/IMSI, timers, transactions, channel assignment),
  - `orchestrator.go` — the `Orchestrator` chain API (`Feed`, `BuildResponse`,
    `RequiredSize`, `TakeRetransmit`, `ChainPhase`) and the typed builders
    `BuildCMServiceRequest` / `BuildSetup`,
  - `stack.go` — `GsmL3Stack`: registry + borrowed session + orchestrator + a
    bridged LAPDm entity with auto-response (the unified Python/Go/Rust stack),
  - `errors.go` — `Code` (1:1 with `gsml3_error`) and the typed `*Error`.
- `cmd/gsmexample/main.go` — end-to-end MO-call demo; the exit code is the
  contract (`0` = all steps asserted).
- Tests: `cgo_test.go` (core wrappers, NULL-safety), `stack_test.go` (queue
  model, closed-object FFI seams, `-race` concurrency), `nulltest_export.go`
  (test-only spies proving zero FFI after close).
- `ccshim/` — small pass-through wrapper routing a `clang` + lld pair as
  cgo's C compiler on Windows when no MinGW gcc is available.

## Building and linking

The binding wraps a **prebuilt shared library** of the C core; it does not
compile C++. From the repository root:

```powershell
cmake -S . -B build_bindings -DBUILD_SHARED_LIBS=ON -DBUILD_TESTS=OFF -DBUILD_EXAMPLES=OFF
cmake --build build_bindings --config Release --target gsml3parser
# flatten the artifacts (gsml3parser.dll + .lib, libgsml3parser.so*) into <root>\build_bindings\bin — see bindings/README.md "Prerequisites"
```

The `#cgo` directives link against `<root>/build_bindings/bin`; at RUN TIME the
dynamic loader finds the library via `PATH` (Windows) or `LD_LIBRARY_PATH`
(Linux) — the unified gate (`scripts/verify_bindings.ps1`) puts that directory
on both. cgo requires a C toolchain: a native/MSYS2 MinGW gcc on Windows (or
the `ccshim` clang+lld fallback), plain gcc/clang on Linux.

Run vet, tests and the demo from `bindings/go`:

```
go vet ./...
go test -count=1 -race ./...
go run ./cmd/gsmexample
```

## Quickstart

```go
import (
	"fmt"

	gsml3parser "github.com/momentics/libgsml3parser/bindings/go"
)

func main() { // full working demo with asserts: go run ./cmd/gsmexample
	msg, err := gsml3parser.ParseHex("06 0D 00", nil)      // parse: RR Channel Release
	if err != nil { panic(err) }
	fmt.Println(msg.Name(), msg.Size())                     // ChannelRelease (3, nil)
	defer msg.Close()

	stack, err := gsml3parser.NewGsmL3Stack(gsml3parser.StackOptions{
		TMSI: 0x87654321, SAPI: 0, Profile: 0, AutoResponse: true,
	})
	if err != nil { panic(err) }
	l3, _ := gsml3parser.BuildCMServiceRequest(1 /* MO call */, gsml3parser.IDTMSI, 0x87654321, "")
	txs, err := stack.SendFrame(gsml3parser.UIFrame(0, false, l3)) // MS-side UI in -> auto-response out
	if err != nil { panic(err) }
	dec, _ := gsml3parser.DecodeFrame(txs[0])                       // zero-copy view inside txs[0]
	resp, err := gsml3parser.Parse(dec.Payload, nil)
	if err != nil { panic(err) }

	step, _ := stack.LastStep()
	fmt.Println(step.Token, resp.Name())                        // 10 CMServiceAccept
	resp.Close()
	stack.Close()                                               // Close is idempotent
}
```

`GsmL3Stack` drives the chain automatically (queue model, below); with
`AutoResponse: false` each `SendFrame` leaves the L3 queue unprocessed for
manual orchestration via `DrainEvents()` / `FeedL3()` / `BuildResponse()` /
`SendUI()`. Components (`stack.Registry()`, `stack.Session()`,
`stack.Orchestrator()`, `stack.Entity()`) are reachable as read-only accessors
for advanced use (further sessions, link control); their lifecycle belongs to
the stack.

## Error model

The C ABI reports failures as a code + thread-local message; the next
successful call clears it. The binding therefore copies both synchronously at
every failing call site into a typed `*Error{Op, Code, Msg}`, rendered as
`gsml3: <op>: <message> (<CODE>)`, where `Code` mirrors `gsml3_error` 1:1
(`CodeOK` … `CodeDuplicate`). Wrapper validation rejects out-of-domain input
(SAPI 0..15, profile 0..2, shard count 0/4/8/16/32, frame length, identities)
BEFORE the FFI boundary (the NULL policy); C-documented NULL-safe entry points
pass nils through. Void-returning C operations are polled for the thread-local
error immediately after the call; `LastError()` / `LastErrorCode()` are
read-only observers that never clear the pending error.

## Ownership and callbacks

- **Owned handles** (`Message`, `Config`, `RslFrame`, `LapdmEntity`, `Registry`,
  `Orchestrator`, `GsmL3Stack`): `Close()` is idempotent (atomic take) and
  nils the raw pointer — the take pattern — so no method can ever touch a
  freed handle. Stack teardown releases in the ABI order **entity →
  orchestrator → registry**. After close, every method fails with a typed
  error and performs ZERO FFI calls (proven per seam by the test-only raw
  spies).
- **Borrowed** (`Session`): owned by its `Registry`, never closed directly —
  `gsml3_registry_free` releases every session; session methods refuse to run
  once their registry is closed.
- **Callback model (queue model)**: C entity callbacks fire synchronously
  inside the C call with spans valid only during it. The `//export` bridges do
  memory-safe work only: a zero-copy read of the C span via `unsafe.Slice`
  plus exactly one owned copy appended under the queue lock — no cgo call and
  no blocking, so the entity FSM is never mutated from within its own
  callback. The bridge `user` token is the address of the entity's
  `cgo.Handle` field (one machine word with no Go pointers, compliant with
  cgo's pointer rules) and is reclaimed by `handle.Delete()` strictly after
  `gsml3_lapdm_entity_free`.
- **Threading**: owned handles are single-thread per the C ABI — one stack per
  goroutine; the internal mutex serializes the receive/drain/orchestrate path
  as hygiene, it is NOT a sharing mechanism. A registry with `shard_count`
  4/8/16/32 makes its registry-mediated calls thread-safe in C (per-shard
  locks); direct `Session` access is unsynchronized by design (one thread per
  session, caller-synchronized).

## Versioning

The Go module deliberately carries NO product version (`go.mod`): the
repository-root `VERSION` file is the single source of truth for releases, and
there is no value in the module that could drift out of sync with it.
`Version()` reads the product version back from the loaded core at runtime;
`ABIVersion()` pins the C ABI revision (1) — a mismatch panics at package init
before any FFI call.
