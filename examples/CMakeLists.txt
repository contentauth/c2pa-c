cmake_minimum_required(VERSION 3.27)
project(Examples)

# Install and make available nlohmann_json
include(FetchContent)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

# Example executables
add_executable(training training.cpp)
target_link_libraries(training PRIVATE nlohmann_json::nlohmann_json c2pa_cpp)

add_executable(demo demo.cpp)
target_link_libraries(demo PRIVATE nlohmann_json::nlohmann_json c2pa_cpp)

add_executable(ingredient_folder ingredient_folder.cpp)
target_link_libraries(ingredient_folder PRIVATE nlohmann_json::nlohmann_json c2pa_cpp)
