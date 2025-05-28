cmake_minimum_required(VERSION 3.16)

# GoogleTest via FetchContent
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

# C++ tests
file(GLOB CPP_TESTS "*.test.cpp")
add_executable(c2pa_c_tests ${CPP_TESTS})
target_link_libraries(c2pa_c_tests PRIVATE c2pa_cpp nlohmann_json::nlohmann_json gtest_main)

# C tests
add_executable(ctest test.c)
target_link_libraries(ctest PRIVATE c2pa_c)

# Register tests with CTest
add_test(NAME c_test COMMAND ctest)

include(GoogleTest)
gtest_discover_tests(c2pa_c_tests
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PROPERTIES ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}:$ENV{LD_LIBRARY_PATH}"
)

set_tests_properties(c_test PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}:$ENV{LD_LIBRARY_PATH}"
)

# Copy the shared library to the build root for test discovery/execution
add_custom_target(copy_c2pa_c_lib ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${C2PA_C_LIB}" "${CMAKE_BINARY_DIR}"
    BYPRODUCTS "${CMAKE_BINARY_DIR}/$(notdir ${C2PA_C_LIB})"
)
add_dependencies(c2pa_c_tests copy_c2pa_c_lib)
add_dependencies(ctest copy_c2pa_c_lib)
