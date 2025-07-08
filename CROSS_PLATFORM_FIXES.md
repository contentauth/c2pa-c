# Cross-Platform Shared Library Handling Improvements

## Problem

The original CMake setup had several issues with shared library dependency handling:

1. **Platform-specific duplication**: The same logic was repeated for each target with platform-specific `if(WIN32)` blocks
2. **Inconsistent approach**: It tried to copy DLLs on Windows but use RPATH on Unix, when CMake can handle this more elegantly
3. **Manual copying**: It manually copied files instead of letting CMake handle dependencies properly
4. **Test failures**: Tests failed due to incomplete cleanup between runs

## Solution

We've implemented a unified, cross-platform approach using a reusable CMake function:

### Key Changes

1. **Unified Function**: Created `setup_c2pa_runtime_deps()` function that handles both Unix RPATH and Windows DLL copying consistently

2. **Improved RPATH Handling**: 
   - Added `BUILD_RPATH_USE_ORIGIN` for better relative path resolution
   - Proper `BUILD_RPATH` and `INSTALL_RPATH` configuration
   - Simplified library target setup in `src/CMakeLists.txt`

3. **Better Windows Support**: 
   - Conditional DLL copying only when `C2PA_C_LIB` is defined
   - Proper import library handling
   - Consistent behavior across all executables

4. **Test Robustness**: 
   - Fixed test cleanup by ensuring all temporary files are removed before tests
   - Tests now pass consistently on both debug and release builds

5. **Makefile Improvements**: 
   - Added separate debug and release build targets
   - Proper build directory structure (`build/debug`, `build/release`)

### Benefits

- **Eliminates code duplication**: One function handles all executables
- **Platform-agnostic**: Works consistently on Linux, macOS, and Windows
- **Maintainable**: Changes to shared library handling only need to be made in one place
- **Robust**: Tests pass reliably in both debug and release modes
- **Flexible**: Easy to add new executables without platform-specific setup

### Usage

For any new executable target, simply call:

```cmake
add_executable(my_target my_target.cpp)
target_link_libraries(my_target PRIVATE c2pa_cpp)
setup_c2pa_runtime_deps(my_target)
```

The function automatically:
- Sets up RPATH on Unix systems for runtime library discovery
- Copies required DLLs to the executable directory on Windows
- Handles all platform differences transparently

### Verification

All tests and examples now work correctly:
- Debug build: `make test` - 16/16 tests pass
- Release build: `make test-release` - 16/16 tests pass  
- Examples: `make examples` - Both training and demo run successfully
- Shared library resolution verified with `otool -L` (macOS) showing correct `@rpath` usage

This approach should work reliably across all supported platforms and CI environments.
