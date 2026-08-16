# Description

Describe the changes and their motivation. Reference any related issue number.

## Type of Change

- [ ] Bug fix (correct parsing, builder output, or runtime behavior)
- [ ] New feature (message type, procedure, API addition)
- [ ] Spec compliance update (GSM 04.08, TS 24.008, GSM 04.06, etc.)
- [ ] Performance improvement (zero-alloc path, cache behavior)
- [ ] Documentation or example update
- [ ] Refactoring (no behavioral change)

## Checklist

- [ ] All tests pass (`ctest --output-on-failure`)
- [ ] New tests added for new code paths
- [ ] No heap allocation introduced on the hot parse/build path
- [ ] Public API headers in `include/` are self-contained
- [ ] `doc/` updated if public API changed
- [ ] Coding conventions followed (see [CONTRIBUTING.md](CONTRIBUTING.md))

## Spec References

List relevant GSM/3GPP spec sections covered by this PR (e.g. GSM 04.08 §9.1.25).
