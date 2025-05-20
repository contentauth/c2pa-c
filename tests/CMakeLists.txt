cmake_minimum_required(VERSION 3.16)

# GoogleTest via FetchContent
include(FetchContent)
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)
FetchContent_MakeAvailable(googletest)

enable_testing()

# C++ tests
file(GLOB CPP_TESTS "*.test.cpp")
add_executable(c2pa_c_tests ${CPP_TESTS})
target_link_libraries(c2pa_c_tests
  PRIVATE
    c2pa_cpp
    nlohmann_json::nlohmann_json
    gtest_main
)
# C tests
add_executable(ctest test.c)
target_link_libraries(ctest PRIVATE c2pa_c)

# Register tests with CTest
add_test(NAME cpp_tests COMMAND c2pa_c_tests)
add_test(NAME c_test COMMAND ctest)

# Set the environment variable so tests find the shared library
set_tests_properties(cpp_tests c_test PROPERTIES
    ENVIRONMENT "LD_LIBRARY_PATH=${c2pa_prebuilt_SOURCE_DIR}/lib:$ENV{LD_LIBRARY_PATH}"
)

