# Refactoring Plan: Settings & Context Handling

## Context

The `Settings`, `Context`, `ContextBuilder`, and stream wrapper classes in `c2pa.hpp`/`c2pa.cpp` share substantial boilerplate for RAII pointer management, move semantics, and construction patterns. This plan identifies internal refactoring opportunities that preserve the public API while improving consistency, reducing duplication, and tightening encapsulation.

---

## Refactoring 1: Normalize Move Semantics to `std::exchange`

**Problem:** Six classes implement the same move-constructor/move-assignment/destructor pattern, but inconsistently — some use `std::exchange`, others manually null pointers; some null-check before `c2pa_free()`, others don't (both correct, but inconsistent).

**Change:** Standardize all to:
```cpp
// Move ctor: use std::exchange
Class(Class&& other) noexcept : ptr(std::exchange(other.ptr, nullptr)) {}
// Move assign: no null check (c2pa_free handles nullptr)
Class& operator=(Class&& other) noexcept {
    if (this != &other) { c2pa_free(ptr); ptr = std::exchange(other.ptr, nullptr); }
    return *this;
}
// Destructor: no null check
~Class() noexcept { c2pa_free(ptr); }
```

**Affected classes:** Context, ContextBuilder (in `c2pa.cpp`), Reader, Signer, Builder (inline in `c2pa.hpp`)

**Risk:** Low — behavior identical since `c2pa_free(nullptr)` is safe.

---

## Refactoring 2: Delegate `Context(const Settings&)` to ContextBuilder

**Problem:** `Context::Context(const Settings&)` at `c2pa.cpp:381-398` manually creates a `C2paContextBuilder*`, sets settings, and builds — duplicating what `ContextBuilder` already does with identical error handling.

**Change:** Replace with delegation:
```cpp
Context::Context(const Settings& settings)
    : Context(ContextBuilder().with_settings(settings).create_context()) {}
```

This mirrors the existing `Context(const std::string& json)` which already delegates through `Settings`.

**Files:** `c2pa.cpp` only (lines 381-398)
**Risk:** Low — ContextBuilder implements identical logic.


## Files to Modify

| File | Refactorings |
|------|-------------|
| [c2pa.hpp](include/c2pa.hpp) | 1, 3, 4, 5 |
| [c2pa.cpp](src/c2pa.cpp) | 1, 2, 3, 4, 5 |

## Implementation Order

1. **Refactoring 1** — Normalize `std::exchange` (smallest, zero risk, establishes consistency)
2. **Refactoring 2** — Context(Settings&) delegation (small, self-contained)

Each step should be a separate commit. Run the full test suite after each.

## Verification

```bash
cd build && cmake --build . && ctest --output-on-failure
```

Tests to watch:
- `context.test.cpp` — covers Settings/Context/ContextBuilder move semantics, validation, signing
- `reader.test.cpp` — covers Reader construction paths
- `builder.test.cpp` — covers Builder construction and signing
