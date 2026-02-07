# Sub-Plan 01: Security Edge Case Tests

**Parent:** [HARDENING_PLAN.md](../HARDENING_PLAN.md)
**Priority:** High
**Target file:** `tests/security.test.cpp` (append to existing)
**Dependencies:** None (Phase 1 already complete)

## Goal

Add edge case tests that verify the hardened `findValueByKey()` and `signer_callback()` functions handle adversarial/unusual inputs correctly.

## Tests to Add

### findValueByKey Edge Cases

| # | Test Name | Input | Expected |
|---|-----------|-------|----------|
| 1 | `NestedJsonKeys` | `{"a": {"key": "inner"}}` | Returns `"inner"` or `{"key"` depending on behavior; document which |
| 2 | `EscapedQuotesInValue` | `{"key": "val\"ue"}` | Returns `val\"ue` or `val` (naive parser limitation); document |
| 3 | `UnicodeCharactersInValue` | `{"key": "caf\u00e9"}` | Returns the value without crash |
| 4 | `DuplicateKeys` | `{"key": "first", "key": "second"}` | Returns first match (document behavior) |
| 5 | `WhitespaceOnlyValueAfterColon` | `{"key":   }` | Returns empty string or NULL; no crash |
| 6 | `NullByteInJson` | JSON with `\0` embedded | Handles gracefully (strlen stops at null) |
| 7 | `VeryLongKeyName` | Key of 10000+ characters | Returns NULL or correct value; no crash |

### signer_callback Edge Cases

| # | Test Name | Input | Expected |
|---|-----------|-------|----------|
| 8 | `SignatureBufferTooSmall` | `sig_max_len = 1` | Returns -1 (signature won't fit) |
| 9 | `ValidDataNullSignatureBuffer` | `signature = NULL` | Returns -1 |
| 10 | `LargeDataPayload` | 1MB data buffer | Succeeds (if openssl available) or fails gracefully |
| 11 | `ContextStringValidation` | Context = "testing context" vs "wrong" | Prints warning for wrong context |

## Implementation Notes

- Tests 8-11 require `build/` directory to exist (for mkstemp temp files)
- Tests 10-11 require openssl to be installed
- Use `GTEST_SKIP()` if openssl is not available on the test machine
- All tests should be `TEST()` not `TEST_F()` (no shared fixture needed)

## Verification

```bash
make test
# Security tests should show 26+ passing (15 existing + 11 new)
```
