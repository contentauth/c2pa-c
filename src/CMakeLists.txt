cmake_minimum_required(VERSION 3.10)
project(c2pa_cpp)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Include directories
set(INCLUDES ${CMAKE_SOURCE_DIR}/include)
include_directories(${INCLUDES})

# Source files
set(SOURCES c2pa.cpp)

# Create the static library
add_library(c2pa_cpp STATIC ${SOURCES})

# Add the Rust library
set(RUST_LIB ${CMAKE_SOURCE_DIR}/target/release/libc2pa_c.dylib)

target_link_libraries(c2pa_cpp ${RUST_LIB})

target_include_directories(c2pa_cpp PUBLIC ${INCLUDES})

# Define the installation rules
install(TARGETS c2pa_cpp
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)

install(FILES ${INCLUDES}/c2pa.hpp
        DESTINATION include)