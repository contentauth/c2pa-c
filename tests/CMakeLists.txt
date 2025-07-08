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
            COMMAND ls -la $<TARGET_FILE_DIR:${target_name}> || echo "Failed to list directory"
            COMMENT "Copying c2pa_c library to ${target_name} directory"
        )
    endif()
    
    # On Linux, set RPATH to look in the executable's directory
    if(UNIX AND NOT APPLE)
        set_target_properties(${target_name} PROPERTIES
            BUILD_RPATH "$ORIGIN"
            INSTALL_RPATH "$ORIGIN"
            BUILD_WITH_INSTALL_RPATH TRUE
        )
    endif()
endfunction()

# C++ tests
file(GLOB CPP_TESTS "*.test.cpp")
add_executable(c2pa_c_tests ${CPP_TESTS})
target_link_libraries(c2pa_c_tests PRIVATE c2pa_cpp nlohmann_json::nlohmann_json gtest_main)
setup_c2pa_runtime_deps(c2pa_c_tests)

include(GoogleTest)

# For better reliability, especially on Linux, manually add tests instead of auto-discovery
# This avoids the working directory and library path issues during discovery
add_test(NAME cpp_tests COMMAND c2pa_c_tests)
set_tests_properties(cpp_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/tests:$ENV{LD_LIBRARY_PATH}"
)

# C tests
add_executable(ctest test.c)
target_link_libraries(ctest PRIVATE c2pa_c)
setup_c2pa_runtime_deps(ctest)

# Register tests with CTest
add_test(NAME c_test COMMAND ctest)
set_tests_properties(c_test PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/tests:$ENV{LD_LIBRARY_PATH}"
)

