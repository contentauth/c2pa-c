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

# C++ test registration
# For better reliability, especially on Linux, manually add tests instead of auto-discovery
# This avoids the working directory and library path issues during discovery
if(WIN32)
    # On Windows, just run the test executable directly - CTest handles it properly
    add_test(NAME cpp_tests COMMAND c2pa_c_tests.exe)
else()
    add_test(NAME cpp_tests COMMAND c2pa_c_tests)
endif()

set_tests_properties(cpp_tests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
)

# Set platform-specific environment
if(UNIX AND NOT WIN32)
    set_tests_properties(cpp_tests PROPERTIES
        ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/tests:$ENV{LD_LIBRARY_PATH}"
    )
endif()

# C tests
add_executable(ctest test.c)
if(WIN32)
    # On Windows, link to the import library (.lib file)
    target_link_libraries(ctest PRIVATE "${C2PA_PREBUILT_INCLUDE_DIR}/../lib/c2pa_c.dll.lib")
    
    # Add MSVC-specific compile options for C code to handle prebuilt library issues
    if(MSVC)
        # The prebuilt library has empty structs which are invalid in C but valid in C++
        # Compile as C++ to work around this prebuilt library limitation
        set_target_properties(ctest PROPERTIES 
            COMPILE_OPTIONS "/TP"  # Force compile as C++
            CXX_STANDARD 20       # Use C++20 for designated initializers
            CXX_STANDARD_REQUIRED ON
        )
        target_compile_options(ctest PRIVATE
            /wd4996  # Disable deprecated function warnings (strerror, strncpy, etc.)
            /wd4244  # Disable type conversion warnings  
            /wd4267  # Disable size_t conversion warnings
        )
    endif()
else()
    # On Unix systems, link directly to the shared library
    target_link_libraries(ctest PRIVATE "${C2PA_C_LIB}")
endif()
target_include_directories(ctest PRIVATE "${C2PA_PREBUILT_INCLUDE_DIR}")
setup_c2pa_runtime_deps(ctest)

# Register tests with CTest
add_test(NAME c_test COMMAND ctest)
set_tests_properties(c_test PROPERTIES
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    ENVIRONMENT "LD_LIBRARY_PATH=${CMAKE_BINARY_DIR}/tests:$ENV{LD_LIBRARY_PATH}"
)

