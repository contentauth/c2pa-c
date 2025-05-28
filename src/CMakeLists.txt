cmake_minimum_required(VERSION 3.27)
project(c2pa_cpp)

include(FetchContent)

# Download json (if needed by the library)
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set DLL export for Windows
add_definitions(-DC2PA_DLL)

# Detect platform and architecture for the prebuilt zip
if(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
        set(C2PA_PREBUILT_ARCH "aarch64-apple-darwin")
    else()
        set(C2PA_PREBUILT_ARCH "x86_64-apple-darwin")
    endif()
elseif(WIN32)
    set(C2PA_PREBUILT_ARCH "x86_64-pc-windows-msvc")
else()
    set(C2PA_PREBUILT_ARCH "x86_64-unknown-linux-gnu")
endif()

set(C2PA_PREBUILT_ZIP "c2pa-v${C2PA_VERSION}-${C2PA_PREBUILT_ARCH}.zip")
set(C2PA_PREBUILT_URL "https://github.com/contentauth/c2pa-rs/releases/download/c2pa-v${C2PA_VERSION}/${C2PA_PREBUILT_ZIP}")

message(STATUS "fetching c2pa_prebuilt url: ${C2PA_PREBUILT_URL}")

FetchContent_Declare(
    c2pa_prebuilt
    URL ${C2PA_PREBUILT_URL}
)
FetchContent_MakeAvailable(c2pa_prebuilt)

if(APPLE)
    set(C2PA_C_LIB "${c2pa_prebuilt_SOURCE_DIR}/lib/libc2pa_c.dylib")
elseif(WIN32)
    set(C2PA_C_LIB "${c2pa_prebuilt_SOURCE_DIR}/lib/c2pa_c.dll.lib")
else()
    set(C2PA_C_LIB "${c2pa_prebuilt_SOURCE_DIR}/lib/libc2pa_c.so")
endif()

# Check if we got our prebuilt library
if(NOT EXISTS "${C2PA_C_LIB}")
    message(FATAL_ERROR
        "The c2pa prebuilt library was not downloaded or does not exist for this platform/version:\n"
        "  ${C2PA_PREBUILT_URL}\n"
        "Please check https://github.com/contentauth/c2pa-rs/releases for available binaries."
    )
endif()

# Define the C++ library and add sources
add_library(c2pa_cpp STATIC c2pa.cpp)

# Expose public headers and prebuilt C API headers to clients
target_include_directories(c2pa_cpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
        ${c2pa_prebuilt_SOURCE_DIR}/include
)

# Create an imported target for the prebuilt C library (so clients can just link to c2pa_cpp)
add_library(c2pa_c SHARED IMPORTED GLOBAL)
set_target_properties(c2pa_c PROPERTIES
    IMPORTED_LOCATION "${C2PA_C_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${c2pa_prebuilt_SOURCE_DIR}/include"
)

# Link the C++ library to the C library (PUBLIC so clients get it automatically)
target_link_libraries(c2pa_cpp PUBLIC c2pa_c)

# Optionally, install rules for consumers
install(TARGETS c2pa_cpp
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_SOURCE_DIR}/include/c2pa.hpp DESTINATION include)

#  copy the prebuilt library to the build directory after building for testing
if(WIN32)
    add_custom_command(TARGET c2pa_cpp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${c2pa_prebuilt_SOURCE_DIR}/lib/c2pa_c.dll"
            $<TARGET_FILE_DIR:c2pa_cpp>
    )
elseif(UNIX AND NOT APPLE)
    add_custom_command(TARGET c2pa_cpp POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${c2pa_prebuilt_SOURCE_DIR}/lib/libc2pa_c.so"
            $<TARGET_FILE_DIR:c2pa_cpp>
    )
endif()