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
target_link_libraries(c2pa_c_tests
  PRIVATE
    c2pa_cpp
    nlohmann_json::nlohmann_json
    gtest_main
)
add_custom_command(TARGET c2pa_c_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${C2PA_C_LIB}" $<TARGET_FILE_DIR:c2pa_c_tests>
)

# C tests
add_executable(ctest test.c)
target_link_libraries(ctest PRIVATE c2pa_c)
add_custom_command(TARGET ctest POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${C2PA_C_LIB}" $<TARGET_FILE_DIR:ctest>
)

# Register tests with CTest
#add_test(NAME cpp_tests COMMAND c2pa_c_tests)
add_test(NAME c_test COMMAND ctest)

# Set the environment variable so tests find the shared library
#set_tests_properties(cpp_tests c_test EXPORT_PROPERTIES
include(GoogleTest)
gtest_discover_tests(c2pa_c_tests
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:c2pa_c>:$ENV{LD_LIBRARY_PATH}"
)

# Set the environment variable for the C test as well
set_tests_properties(c_test PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    ENVIRONMENT "LD_LIBRARY_PATH=$<TARGET_FILE_DIR:c2pa_c>:$ENV{LD_LIBRARY_PATH}"
)
