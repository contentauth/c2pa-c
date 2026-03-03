cmake_minimum_required(VERSION 3.27)
project(Examples)

# Install and make available nlohmann_json
include(FetchContent)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

# Function to setup runtime dependencies for any target
function(setup_c2pa_runtime_deps target_name)
    # Copy shared library to executable directory on all platforms
    if(DEFINED C2PA_C_LIB)
        add_custom_command(TARGET ${target_name} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${C2PA_C_LIB}"
            $<TARGET_FILE_DIR:${target_name}>
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
        )
        # Force override any inherited RPATH
        set_property(TARGET ${target_name} PROPERTY BUILD_RPATH "@executable_path")
        set_property(TARGET ${target_name} PROPERTY INSTALL_RPATH "@executable_path")
    endif()
endfunction()

# Example executables
add_executable(training training.cpp)
target_link_libraries(training PRIVATE nlohmann_json::nlohmann_json c2pa_cpp)
if(NOT MSVC)
    target_compile_options(training PRIVATE -Wno-deprecated-declarations)
endif()
if(MSVC)
    target_compile_options(training PRIVATE /wd4996)
endif()
setup_c2pa_runtime_deps(training)

add_executable(demo demo.cpp)
target_link_libraries(demo PRIVATE nlohmann_json::nlohmann_json c2pa_cpp)
if(NOT MSVC)
    target_compile_options(demo PRIVATE -Wno-deprecated-declarations)
endif()
if(MSVC)
    target_compile_options(demo PRIVATE /wd4996)
endif()
setup_c2pa_runtime_deps(demo)
