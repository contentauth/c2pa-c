cmake_minimum_required(VERSION 3.27)
project(c2pa_cpp)

include(FetchContent)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set DLL export for Windows
add_definitions(-DC2PA_DLL)

# Include directories
set(INCLUDES ${CMAKE_SOURCE_DIR}/include)
include_directories(${INCLUDES})

# Source files
set(SOURCES c2pa.cpp)

# Create the static library
add_library(c2pa_cpp STATIC ${SOURCES})

# Platform-independent library extension
if(APPLE)
    target_link_libraries(c2pa_cpp ${CMAKE_SOURCE_DIR}/target/release/libc2pa_c.dylib)
elseif(WIN32)
    target_link_libraries(c2pa_cpp ${CMAKE_SOURCE_DIR}/target/release/c2pa_c.lib ${CMAKE_SOURCE_DIR}/target/release/c2pa_c.dll)
else()
    target_link_libraries(c2pa_cpp ${CMAKE_SOURCE_DIR}/target/release/libc2pa_c.so)
endif()

target_include_directories(c2pa_cpp PUBLIC ${INCLUDES})

# Define the installation rules
install(TARGETS c2pa_cpp
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib
        RUNTIME DESTINATION bin)

install(FILES ${INCLUDES}/c2pa.hpp
        DESTINATION include)

# Add version check
cmake_minimum_required(VERSION 3.27)

# Use release tag instead of commit hash
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.14.0.zip
)

# Library extension handling
include(GNUInstallDirs)
set(CMAKE_INSTALL_RPATH "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")

if(MSVC)
    add_compile_options(/W4)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

include(CTest)
enable_testing()
