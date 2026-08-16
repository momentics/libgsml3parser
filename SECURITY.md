# Security Policy

## Supported Versions

| Version | Supported          |
|---------|--------------------|
| 0.14.x  | Yes                |
| < 0.14  | No                 |

## Reporting a Vulnerability

libgsml3parser processes untrusted binary data from the GSM Um interface. Incorrect handling of malformed L3 messages, LAPDm frames, or RSL packets can lead to buffer overflows, integer overflows, or denial-of-service conditions in downstream BTS software.

**If you discover a security vulnerability, please do NOT open a public GitHub issue.** Instead, report it privately by emailing **momentics@gmail.com** with the following information:

- Description of the vulnerability
- Affected component (parser, builder, LAPDm entity, stream processor, RSL parser, procedure runner)
- Minimal hex input or C++ code that triggers the issue
- Potential impact (memory corruption, DoS, information leak)

### What to Expect

- **Acknowledgment** within 48 hours of receiving your report
- **Assessment** and confirmation of the vulnerability within 5 business days
- **Fix development** and release of a patched version as soon as possible
- **Credit** in the release notes, if desired

### Scope

The following are considered in-scope security concerns:

- Buffer overflows or out-of-bounds reads in `BitReader`, `L3Framer`, `parseL3()`, or any message parser
- Integer overflow or underflow in length validation or bit-position arithmetic
- Missing bounds checks in LAPDm entity state machine or I-frame reassembly
- Unsafe memory access in `ResponseBuilder`, `RSLParser`, or procedure runners
- Denial-of-service via crafted input that triggers infinite loops or excessive allocation

The following are out of scope:

- Truncated input that returns a valid `ParseError::TruncatedInput` (expected behavior)
- Messages from unsupported PD domains that return `ParseError::InvalidPD` (expected behavior)
- Lack of support for a GSM feature listed in the Roadmap (track as a Feature Request instead)

### Security Design Principles

The library is designed with security in mind:

- **Zero heap allocation on the hot path** — reduces attack surface from allocator-based exploits
- **Bounds-checked `BitReader`** — every read validates remaining bits before access
- **Length-prefixed parsing** — declared TLV lengths are validated against available input
- **Immutable `ParserConfig`** — no race conditions on shared configuration
- **`Expected<T>` error type** — errors carry bit-position context for debugging malformed captures
