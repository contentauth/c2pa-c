cmake_minimum_required(VERSION 3.16)

# GoogleTest via FetchContent
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

# Function to setup runtime dependencies for any target
function(setup_c2pa_runtime_deps target_name)
    # Copy shared library to executable directory on all platforms
    if(DEFINED C2PA_C_LIB)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${C2PA_C_LIB}"
            $<TARGET_FILE_DIR:${target_name}>
            COMMAND ${CMAKE_COMMAND} -E echo "Copied ${C2PA_C_LIB} to $<TARGET_FILE_DIR:${target_name}>"
            COMMENT "Copying c2pa_c library to ${target_name} directory"
        )
        
        # On Windows, add debugging commands to check DLL dependencies
        if(WIN32)
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E echo "=== Windows DLL Debug Info for ${target_name} ==="
                COMMAND dir $<TARGET_FILE_DIR:${target_name}> || echo "Failed to list directory"
                COMMAND where dumpbin >nul 2>&1 && dumpbin /dependents $<TARGET_FILE:${target_name}> || echo "dumpbin not available"
                COMMENT "Windows DLL debugging for ${target_name}"
            )
        else()
            add_custom_command(TARGET ${target_name} POST_BUILD
                COMMAND ls -la $<TARGET_FILE_DIR:${target_name}> || echo "Failed to list directory"
            )
        endif()
    endif()
    
    # On Linux, set RPATH to look in the executable's directory
    if(UNIX AND NOT APPLE)
        set_target_properties(${target_name} PROPERTIES
            BUILD_RPATH "$ORIGIN"
            INSTALL_RPATH "$ORIGIN"
            BUILD_WITH_INSTALL_RPATH TRUE
            SKIP_BUILD_RPATH FALSE
            # Clear any automatic RPATH from linked libraries
            LINK_WHAT_YOU_USE FALSE
        )
        # Force override any inherited RPATH
        set_property(TARGET ${target_name} PROPERTY BUILD_RPATH "$ORIGIN")
        set_property(TARGET ${target_name} PROPERTY INSTALL_RPATH "$ORIGIN")
    endif()
    
    # On macOS, override the RPATH to remove hardcoded paths
    if(APPLE)
        set_target_properties(${target_name} PROPERTIES
            BUILD_RPATH "@executable_path"
            INSTALL_RPATH "@executable_path"
            BUILD_WITH_INSTALL_RPATH TRUE
            SKIP_BUILD_RPATH FALSE
        )
        # Force override any inherited RPATH
        set_property(TARGET ${target_name} PROPERTY BUILD_RPATH "@executable_path")
        set_property(TARGET ${target_name} PROPERTY INSTALL_RPATH "@executable_path")
    endif()
endfunction()

# C++ tests
file(GLOB CPP_TESTS "*.test.cpp")
add_executable(c2pa_c_tests ${CPP_TESTS})
target_link_libraries(c2pa_c_tests PRIVATE c2pa_cpp nlohmann_json::nlohmann_json gtest_main)
setup_c2pa_runtime_deps(c2pa_c_tests)

include(GoogleTest)

# Add debug commands to check what's happening on Linux
if(UNIX AND NOT APPLE)
    add_test(NAME debug_rpath_info 
        COMMAND bash -c "echo '=== RPATH Debug Info ===' && echo 'Working dir:' && pwd && echo 'Library files in tests dir:' && ls -la ${CMAKE_BINARY_DIR}/tests/ && echo 'RPATH of c2pa_c_tests:' && readelf -d ${CMAKE_BINARY_DIR}/tests/c2pa_c_tests | grep -E '(RPATH|RUNPATH)' && echo 'RPATH of ctest:' && readelf -d ${CMAKE_BINARY_DIR}/tests/ctest | grep -E '(RPATH|RUNPATH)' && echo 'ldd output for c2pa_c_tests:' && ldd ${CMAKE_BINARY_DIR}/tests/c2pa_c_tests"
    )
    
    add_test(NAME debug_library_location
        COMMAND bash -c "echo '=== Library Location Debug ===' && echo 'Current dir:' && pwd && echo 'Contents of build/debug:' && ls -la ${CMAKE_BINARY_DIR}/ && echo 'Contents of _deps:' && ls -la ${CMAKE_BINARY_DIR}/_deps/ && echo 'Contents of prebuilt lib:' && ls -la ${CMAKE_BINARY_DIR}/_deps/c2pa_prebuilt-src/lib/ && echo 'Does the relative path exist from tests dir?' && ls -la ${CMAKE_BINARY_DIR}/tests/../_deps/c2pa_prebuilt-src/lib/"
    )
endif()

# For better reliability, especially on Linux, manually add tests instead of auto-discovery
# This avoids the working directory and library path issues during discovery
add_test(NAME cpp_tests COMMAND c2pa_c_tests)
set_tests_properties(cpp_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/tests:$ENV{LD_LIBRARY_PATH}"
)

# On Windows, add a debug test to try running the executable directly
if(WIN32)
    add_test(NAME cpp_tests_debug_windows 
        COMMAND ${CMAKE_COMMAND} -E echo "Attempting to run c2pa_c_tests.exe directly..."
    )
    add_test(NAME cpp_tests_check_deps_windows
        COMMAND cmd /c "cd ${CMAKE_BINARY_DIR}/tests && dir && echo. && echo Trying to run executable... && c2pa_c_tests.exe --help"
    )
    set_tests_properties(cpp_tests_check_deps_windows PROPERTIES
        WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )
endif()

# C tests
add_executable(ctest test.c)
target_link_libraries(ctest PRIVATE "${C2PA_C_LIB}")
target_include_directories(ctest PRIVATE "${C2PA_PREBUILT_INCLUDE_DIR}")
setup_c2pa_runtime_deps(ctest)

# Register tests with CTest
add_test(NAME c_test COMMAND ctest)
set_tests_properties(c_test PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/tests:$ENV{LD_LIBRARY_PATH}"
)

