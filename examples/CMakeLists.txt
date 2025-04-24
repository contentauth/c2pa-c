# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.27)

# install json
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

FetchContent_Declare(
    c2pa_prebuilt
    URL https://github.com/contentauth/c2pa-c/releases/download/v0.0.1-alpha/c2pa-c-macos-v0.0.1-alpha.zip
)

FetchContent_MakeAvailable(c2pa_prebuilt)

# Add the include directory for c2pa.h
include_directories(${c2pa_prebuilt_SOURCE_DIR}/include)

# Link the prebuilt library
link_directories(${c2pa_prebuilt_SOURCE_DIR}/lib)

# Set the project name
project(Examples)

# Include the directory containing the main project headers
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/tests)

# Add example1 executable
add_executable(training training.cpp)
target_link_libraries(training nlohmann_json::nlohmann_json)
target_link_libraries(training c2pa_cpp)

add_executable(demo demo.cpp)
target_link_libraries(demo nlohmann_json::nlohmann_json)
target_link_libraries(demo c2pa_cpp)