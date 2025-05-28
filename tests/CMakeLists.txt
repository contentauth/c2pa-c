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

include(GoogleTest)
gtest_discover_tests(c2pa_c_tests
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PROPERTIES ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:c2pa_c_tests>:$ENV{LD_LIBRARY_PATH}"
)
set_target_properties(c2pa_c_tests PROPERTIES
    BUILD_RPATH "$ORIGIN"
)

# C tests
add_executable(ctest test.c)
target_link_libraries(ctest PRIVATE c2pa_c)

# Register tests with CTest
add_test(NAME c_test COMMAND ctest)

set_tests_properties(c_test PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:c_test>:$ENV{LD_LIBRARY_PATH}"
)

set_target_properties(c2pa_c_tests PROPERTIES
    BUILD_RPATH "$ORIGIN"
)

get_target_property(TEST_DIR c2pa_c_tests RUNTIME_OUTPUT_DIRECTORY)
message(STATUS "Test exe dir: ${TEST_DIR}")
message(STATUS "C2PA_C_LIB: ${C2PA_C_LIB}")
message(STATUS "CMAKE_BINARY_DIR: ${CMAKE_BINARY_DIR}")

# copy the C library to the test directory
add_custom_target(copy_c2pa_c_lib ALL
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${C2PA_C_LIB}" $<TARGET_FILE_DIR:c2pa_c_tests>
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${C2PA_C_LIB}" $<TARGET_FILE_DIR:ctest>
)
add_dependencies(c2pa_c_tests copy_c2pa_c_lib)
add_dependencies(ctest copy_c2pa_c_lib)
