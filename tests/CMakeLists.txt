project(unit_tests)

# CMake Dependencies
include(FetchContent)

# Download GoogleTest testing framework.
FetchContent_Declare(
        googletest
        # Specify the commit you depend on and update it regularly.
        URL https://github.com/google/googletest/archive/5376968f6948923e2411081fd9372e71a59d8e77.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Download json
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

# Find OpenSSL
find_package(OpenSSL 3.2 REQUIRED)

add_library(test_signer test_signer.cpp)
# Ensure OpenSSL headers are available
target_include_directories(test_signer PUBLIC ${OPENSSL_INCLUDE_DIR})

# Add the Rust library
set(RUST_LIB ${CMAKE_SOURCE_DIR}/target/release/libc2pa_c.dylib)

# Create the unit test target.
file(GLOB unit_test_files CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/*.test.cpp" "${CMAKE_SOURCE_DIR}/tests/test_signer.cpp")
add_executable(unit_tests ${unit_test_files})
target_include_directories(unit_tests PUBLIC "${CMAKE_SOURCE_DIR}/include/")

target_link_libraries(unit_tests OpenSSL::SSL OpenSSL::Crypto)
target_link_libraries(unit_tests nlohmann_json::nlohmann_json)
target_link_libraries(unit_tests ${RUST_C_LIB})
target_link_libraries(unit_tests c2pa_cpp test_signer)
target_link_libraries(unit_tests gtest_main)

# Ensure OpenSSL headers are available for unit tests
target_include_directories(unit_tests PUBLIC ${OPENSSL_INCLUDE_DIR})

# Enable testing
enable_testing()

# Add test cases
add_test(NAME unit_tests COMMAND unit_tests)