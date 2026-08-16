---
name: Bug Report
about: Report incorrect parsing, builder output, or runtime failure
title: ""
labels: ["bug"]
assignees: []
---

## Summary

Short description of the bug (1–2 sentences).

## Steps to Reproduce

1. Build configuration: e.g. `cmake .. -DBUILD_TESTS=ON`, compiler and version
2. Minimal code or hex input that triggers the issue
3. Command to run the failing case

## Expected Behavior

What should happen according to GSM 04.08 / TS 24.008 (spec section if known).

## Actual Behavior

What actually happens (crash, wrong parse result, assertion failure, etc.). Include output or test failure text.

## Environment

- **OS:** (e.g. Ubuntu 24.04, Windows 11)
- **Compiler:** (e.g. GCC 13.2, Clang 18, MSVC 19.40)
- **CMake version:** (e.g. 3.28)
- **libgsml3parser commit or tag:**

## Additional Context

Attach pcap files, hex dumps, or failing test output if relevant.
