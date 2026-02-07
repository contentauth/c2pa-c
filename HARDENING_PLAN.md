# C2PA-C Hardening Plan — Master Document

## Status: Phase 1 Complete, Phase 2 Planned

## Completed (Phase 1)

All security fixes (S1-S6) and performance optimizations (P1-P3) have been implemented and verified.

### Security Fixes Applied

| ID | Severity | Fix | Files Changed |
|----|----------|-----|---------------|
| S1 | HIGH | `system()` return value checked; error propagation | `unit_test.h`, `cmd_signer.cpp` |
| S2 | MEDIUM | `findValueByKey()` hardened: null checks, bounds checking, `size_t` for lengths, `memcpy` instead of `strncpy` | `unit_test.h` |
| S3 | MEDIUM | `fread()`/`ftell()` return values validated; abort on short read | `unit_test.h` |
| S4 | MEDIUM | `mkstemp()` replaces hardcoded temp file paths; files cleaned up after use | `unit_test.h`, `cmd_signer.cpp` |
| S5 | LOW-MED | Debug-checked `checked_stream_cast<>()` replaces raw `reinterpret_cast` | `c2pa.cpp` |
| S6 | MEDIUM | `malloc()` return checked (fixed alongside S2) | `unit_test.h` |

### Performance Optimizations Applied

| ID | Impact | Fix | Measured Improvement |
|----|--------|-----|---------------------|
| P1 | HIGH | `SignerFunc` accepts `const uint8_t*, size_t` instead of `std::vector` copy | **2.8x** (1KB), **65.9x** (100KB), **602x** (1MB) |
| P2 | MEDIUM | `vector::reserve(count)` before loop in `c_mime_types_to_vector()` | **2.34x** faster |
| P3 | LOW | Cached `path_to_string()` result in `open_file_binary()` | Eliminated redundant conversion |

### Tests & Infrastructure Added

- `tests/security.test.cpp` — 15 security regression tests (GoogleTest)
- `tests/perf-checks.test.cpp` — 4 performance timing benchmarks (separate executable)
- `make perf-test` — new Makefile target for release-build perf benchmarks

---

## Planned (Phase 2) — Sub-Plans

Each sub-plan is a self-contained task that can be actioned independently.

| Sub-Plan | File | Scope | Priority |
|----------|------|-------|----------|
| 01 | [plans/01-security-edge-cases.md](plans/01-security-edge-cases.md) | Security edge case tests for `findValueByKey` and `signer_callback` | High |
| 02 | [plans/02-error-handling.md](plans/02-error-handling.md) | Error handling tests for Builder and Reader invalid inputs | High |
| 03 | [plans/03-reader-resource-access.md](plans/03-reader-resource-access.md) | Reader `get_resource()` API tests (currently **zero coverage**) | High |
| 04 | [plans/04-context-settings-archive-mime.md](plans/04-context-settings-archive-mime.md) | Context/Settings API, archive round-trip, MIME type tests | Medium |
| 05 | [plans/05-coverage-reporting.md](plans/05-coverage-reporting.md) | `make test-coverage` with gcov/lcov HTML report generation | Medium |

### Execution Order

Recommended: `05 → 01 → 03 → 02 → 04`

- Start with **05 (coverage)** to establish baseline metrics before adding tests
- Then **01 (security edge cases)** to complete security hardening verification
- Then **03 (reader resources)** to fill the biggest functional coverage gap
- Then **02 (error handling)** for robustness
- Finally **04 (context/settings)** for API completeness
