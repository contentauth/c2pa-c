# Sub-Plan 05: Coverage Reporting

**Parent:** [HARDENING_PLAN.md](../HARDENING_PLAN.md)
**Priority:** Medium
**Target files:** `CMakeLists.txt`, `tests/CMakeLists.txt`, `Makefile`
**Dependencies:** None (should be done first to establish baseline)

## Goal

Add `make test-coverage` that builds with gcov instrumentation, runs all tests, and generates an lcov HTML report in `build/coverage/`.

## Implementation

### 1. CMake: Add Coverage Build Type

In the top-level `CMakeLists.txt`, add a coverage option:

```cmake
option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)

if(ENABLE_COVERAGE)
    if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
        add_compile_options(--coverage -fprofile-arcs -ftest-coverage)
        add_link_options(--coverage)
    endif()
endif()
```

### 2. Makefile: Add `test-coverage` target

```makefile
test-coverage: clean
    cmake -S . -B build/coverage -G "Ninja" -DCMAKE_BUILD_TYPE=Debug -DENABLE_COVERAGE=ON
    cmake --build build/coverage
    cd build/coverage && ctest --output-on-failure
    @echo "Generating coverage report..."
    lcov --capture --directory build/coverage --output-file build/coverage/coverage.info --ignore-errors mismatch
    lcov --remove build/coverage/coverage.info '/usr/*' '*/googletest/*' '*/json-src/*' '*/c2pa_prebuilt-src/*' --output-file build/coverage/coverage_filtered.info --ignore-errors unused
    genhtml build/coverage/coverage_filtered.info --output-directory build/coverage/html
    @echo ""
    @echo "=== Coverage Summary ==="
    lcov --summary build/coverage/coverage_filtered.info 2>&1
    @echo ""
    @echo "HTML report: build/coverage/html/index.html"
```

Add `test-coverage` to `.PHONY`.

### 3. Dependencies

- `lcov` must be installed (`brew install lcov` on macOS, `apt install lcov` on Linux)
- `genhtml` comes with lcov
- gcov is built into GCC/Clang

### 4. .gitignore

Add `build/coverage/` to `.gitignore` if not already covered by `build/`.

## Expected Output

```
=== Coverage Summary ===
  lines......: 72.3% (450 of 622 lines)
  functions..: 68.1% (124 of 182 functions)
  branches...: 52.4% (210 of 401 branches)

HTML report: build/coverage/html/index.html
```

The HTML report shows per-file line coverage with highlighted source code.

## Verification

```bash
make test-coverage
open build/coverage/html/index.html  # macOS
# Or: xdg-open build/coverage/html/index.html  # Linux
```

## Notes

- Coverage is only meaningful for `src/c2pa.cpp` and `include/c2pa.hpp` (our C++ wrapper code)
- The prebuilt Rust library (`libc2pa_c.dylib`) is excluded from coverage
- GoogleTest and nlohmann/json are excluded from coverage
- Baseline coverage should be measured before and after adding Phase 2 tests
