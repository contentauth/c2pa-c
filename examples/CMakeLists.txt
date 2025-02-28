# Set the minimum required version of CMake
cmake_minimum_required(VERSION 3.27)

# Find OpenSSL
find_package(OpenSSL 3.2 REQUIRED)

# install json
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)


# Set the project name
project(Examples)

# Include the directory containing the main project headers
include_directories(${CMAKE_SOURCE_DIR}/include)
include_directories(${CMAKE_SOURCE_DIR}/tests)

# Create function to reduce duplication
function(add_example name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} 
        nlohmann_json::nlohmann_json
        OpenSSL::SSL 
        OpenSSL::Crypto
        c2pa_cpp 
        test_signer
    )
endfunction()

add_example(training)
add_example(demo) 