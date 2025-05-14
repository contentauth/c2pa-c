# project(unit_tests)

# # CMake Dependencies
# include(FetchContent)
# FetchContent_Declare(
#   googletest
#   GIT_REPOSITORY https://github.com/google/googletest.git
#   GIT_TAG v1.16.0
#   OVERRIDE_FIND_PACKAGE)

# # For Windows: Prevent overriding the parent project's compiler/linker settings
# set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
# FetchContent_MakeAvailable(googletest)

# # Download json
# FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
# FetchContent_MakeAvailable(json)

# # Add the c2pa library
# set(RUST_LIB ${CMAKE_SOURCE_DIR}/target/release/libc2pa_c.dylib)

# # Create the unit test target.
# file(GLOB unit_test_files CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/*.test.cpp")
# add_executable(unit_tests ${unit_test_files})
# target_include_directories(unit_tests PUBLIC "${CMAKE_SOURCE_DIR}/include/")

# target_link_libraries(unit_tests nlohmann_json::nlohmann_json)
# target_link_libraries(unit_tests ${RUST_C_LIB})
# target_link_libraries(unit_tests c2pa_cpp)
# target_link_libraries(unit_tests gtest_main)

# # Enable testing
# enable_testing()

# # Add test cases
# add_test(NAME unit_tests COMMAND unit_tests)