# Contributing to libgsml3parser

By participating in this project you agree to abide by the [Code of Conduct](CODE_OF_CONDUCT.md). Security vulnerabilities should be reported via [SECURITY.md](SECURITY.md).

## Table of Contents

- [Quick Start](#quick-start)
- [Development Workflow](#development-workflow)
- [Coding Standards](#coding-standards)
- [Testing](#testing)
- [Pull Request Process](#pull-request-process)

## Quick Start

### Build Requirements

| Requirement | Minimum Version |
|-------------|-----------------|
| C++ compiler | GCC 11+, Clang 10+, MSVC 2022 17.3+ |
| CMake | 3.20 |
| Standard | C++20 |

### Building Locally

```bash
mkdir build && cd build
cmake .. -DBUILD_TESTS=ON -DBUILD_EXAMPLES=ON
cmake --build . --config Release --parallel
```

## Development Workflow

1. Fork the repository on GitHub.
2. Create a feature branch from `main`.
3. Make your changes following the coding standards below.
4. Run the test suite (see [Testing](#testing)).
5. Commit with clear, descriptive messages.
6. Push and open a Pull Request against `main`.

## Coding Standards

- **C++20 only** — no pre-C++20 fallbacks; use `std::span`, `std::expected`-style results, `std::variant`, etc.
- **Zero external dependencies** — the library must compile with the C++ standard library alone.
- **No heap allocation on the hot path** — parse functions should operate on stack-allocated types and spans.
- **Naming**
  - Files: `snake_case.cpp` / `snake_case.h`
  - Types: `PascalCase`
  - Functions and variables: `camelCase`
  - Constants and macros: `UPPER_SNAKE_CASE`
- **Headers** — use include guards (`#pragma once`). Guard against self-inclusion.
- **No RTTI or `dynamic_cast`** — use `std::variant` + `tryGet<T>()` for type dispatch.
- **Comments** — keep them minimal. Prefer self-documenting code. Add comments only where the _why_ is non-obvious.

## Testing

All new features and bug fixes must include tests. The project uses Google Test 1.14.0.

```bash
# Build with tests
cmake .. -DBUILD_TESTS=ON
cmake --build . --config Release --parallel

# Run tests
ctest --output-on-failure
```

Test files live in `tests/` and follow the naming convention `test_<module>.cpp`. Place golden test vectors (raw hex data with expected parse results) alongside their test file.

## Pull Request Process

1. **Title** — Use a short, imperative summary (e.g. "Add SMS Layer 3 message builders").
2. **Body** — Describe what changed, why, and any relevant context or spec references (GSM 04.08, TS 24.008, etc.).
3. **Tests** — Every PR must pass the full CI build (GCC, Clang, MSVC). Include new tests for new code.
4. **Documentation** — Update `doc/` if the change affects public API or message coverage.
5. **Breaking changes** — Clearly mark any API-breaking changes in the PR description.

The maintainer will review and merge once CI passes and the changes are approved.
