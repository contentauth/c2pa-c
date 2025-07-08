cmake_minimum_required(VERSION 3.27)
project(Examples)

# Install and make available nlohmann_json
include(FetchContent)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

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

# Example executables
add_executable(training training.cpp)
target_link_libraries(training PRIVATE nlohmann_json::nlohmann_json c2pa_cpp)
setup_c2pa_runtime_deps(training)

add_executable(demo demo.cpp)
target_link_libraries(demo PRIVATE nlohmann_json::nlohmann_json c2pa_cpp)
setup_c2pa_runtime_deps(demo)