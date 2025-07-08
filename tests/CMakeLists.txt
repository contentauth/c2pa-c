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
    # Set RPATH for Unix systems to find shared libraries
    if(UNIX)
        set_target_properties(${target_name} PROPERTIES
            BUILD_RPATH "${CMAKE_BINARY_DIR}/_deps/c2pa_prebuilt-src/lib"
            INSTALL_RPATH "${CMAKE_BINARY_DIR}/_deps/c2pa_prebuilt-src/lib"
            BUILD_RPATH_USE_ORIGIN TRUE
        )
    endif()
    
    # Copy shared libraries to executable directory on Windows
    if(WIN32 AND DEFINED C2PA_C_LIB)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${C2PA_C_LIB}"
            $<TARGET_FILE_DIR:${target_name}>
            COMMENT "Copying c2pa_c library to ${target_name} directory"
        )
    endif()
endfunction()

# C++ tests
file(GLOB CPP_TESTS "*.test.cpp")
add_executable(c2pa_c_tests ${CPP_TESTS})
target_link_libraries(c2pa_c_tests PRIVATE c2pa_cpp nlohmann_json::nlohmann_json gtest_main)
setup_c2pa_runtime_deps(c2pa_c_tests)

include(GoogleTest)

# Set environment for test discovery on Linux
if(UNIX AND NOT APPLE)
    set(ENV{LD_LIBRARY_PATH} "${CMAKE_BINARY_DIR}/_deps/c2pa_prebuilt-src/lib:$ENV{LD_LIBRARY_PATH}")
endif()

gtest_discover_tests(c2pa_c_tests
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    PROPERTIES ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/_deps/c2pa_prebuilt-src/lib:$ENV{LD_LIBRARY_PATH}"
    DISCOVERY_TIMEOUT 10
    DISCOVERY_MODE PRE_TEST
)

# C tests
add_executable(ctest test.c)
target_link_libraries(ctest PRIVATE c2pa_c)
setup_c2pa_runtime_deps(ctest)

# Register tests with CTest
add_test(NAME c_test COMMAND ctest)
set_tests_properties(c_test PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/_deps/c2pa_prebuilt-src/lib:$ENV{LD_LIBRARY_PATH}"
)

