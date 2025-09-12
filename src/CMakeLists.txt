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

if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64|aarch64")
    set(C2PA_PROCESSOR "aarch64")
else()
    set(C2PA_PROCESSOR "x86_64")
endif()

# Detect platform and architecture for the prebuilt zip
if(APPLE)
    set(C2PA_PREBUILT_ARCH "${C2PA_PROCESSOR}-apple-darwin")
    set(C2PA_LIB_NAME "libc2pa_c.dylib")
elseif(WIN32)
    set(C2PA_PREBUILT_ARCH "${C2PA_PROCESSOR}-pc-windows-msvc")
    set(C2PA_LIB_NAME "c2pa_c.dll")
    set(C2PA_IMPLIB_NAME "c2pa_c.dll.lib")

    # Warn about potential ABI issues with MinGW
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        message(WARNING "Using MinGW with MSVC-compiled DLL may cause ABI compatibility issues. Consider using MSVC compiler on Windows.")
    endif()
else()
    set(C2PA_PREBUILT_ARCH "${C2PA_PROCESSOR}-unknown-linux-gnu")
    set(C2PA_LIB_NAME "libc2pa_c.so")
endif()
set(C2PA_C_LIB "${C2PA_LIB_NAME}" PARENT_SCOPE)

set(C2PA_PREBUILT_ZIP "c2pa-v${C2PA_VERSION}-${C2PA_PREBUILT_ARCH}.zip")
set(C2PA_PREBUILT_URL "https://github.com/contentauth/c2pa-rs/releases/download/c2pa-v${C2PA_VERSION}/${C2PA_PREBUILT_ZIP}")

message(STATUS "fetching c2pa_prebuilt url: ${C2PA_PREBUILT_URL}")

FetchContent_Declare(
    c2pa_prebuilt
    URL ${C2PA_PREBUILT_URL}
)
FetchContent_MakeAvailable(c2pa_prebuilt)

set(C2PA_C_LIB "${c2pa_prebuilt_SOURCE_DIR}/lib/${C2PA_LIB_NAME}")
set(C2PA_C_LIB "${C2PA_C_LIB}" PARENT_SCOPE)
set(C2PA_PREBUILT_INCLUDE_DIR "${c2pa_prebuilt_SOURCE_DIR}/include" PARENT_SCOPE)

# Check if we got our prebuilt library
if(NOT EXISTS "${C2PA_C_LIB}")
    message(FATAL_ERROR
        "The c2pa prebuilt library was not downloaded or does not exist for this platform/version:\n"
        "  ${C2PA_PREBUILT_URL}\n"
        "Please check https://github.com/contentauth/c2pa-rs/releases for available binaries."
    )
endif()

# Fix hardcoded install name on macOS
if(APPLE)
    find_program(INSTALL_NAME_TOOL install_name_tool REQUIRED)
    
    # Check current install name before fixing
    execute_process(
        COMMAND otool -D "${C2PA_C_LIB}"
        OUTPUT_VARIABLE CURRENT_INSTALL_NAME
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    message(STATUS "Current install name: ${CURRENT_INSTALL_NAME}")
    
    # Fix the install name to use @rpath
    execute_process(
        COMMAND ${INSTALL_NAME_TOOL} -id "@rpath/${C2PA_LIB_NAME}" "${C2PA_C_LIB}"
        RESULT_VARIABLE INSTALL_NAME_RESULT
        OUTPUT_QUIET
        ERROR_QUIET
    )
    
    if(INSTALL_NAME_RESULT EQUAL 0)
        message(STATUS "Successfully fixed install name for ${C2PA_LIB_NAME}")
    else()
        message(WARNING "Failed to fix install name for ${C2PA_LIB_NAME} (result: ${INSTALL_NAME_RESULT})")
    endif()
endif()

# Define the C++ library and add sources
add_library(c2pa_cpp STATIC c2pa.cpp)

# Set compiler-specific flags for the C++ library
if(MSVC)
    target_compile_options(c2pa_cpp PRIVATE /WX)
else()
    target_compile_options(c2pa_cpp PRIVATE -Werror)
endif()

# Expose public headers and prebuilt C API headers to clients
target_include_directories(c2pa_cpp
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
        $<BUILD_INTERFACE:${c2pa_prebuilt_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

# Link the C++ library to the C library file directly to avoid RPATH issues
if(WIN32)
    # On Windows, link to the import library (.lib file)
    target_link_libraries(c2pa_cpp PRIVATE "${c2pa_prebuilt_SOURCE_DIR}/lib/${C2PA_IMPLIB_NAME}")
else()
    # On Unix systems, link directly to the shared library
    target_link_libraries(c2pa_cpp PRIVATE "${C2PA_C_LIB}")
endif()
target_include_directories(c2pa_cpp PUBLIC 
    $<BUILD_INTERFACE:${c2pa_prebuilt_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# Optionally, install rules for consumers
install(TARGETS c2pa_cpp
    EXPORT c2pa_cppTargets
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)
install(FILES ${CMAKE_SOURCE_DIR}/include/c2pa.hpp DESTINATION include)

# Install the export set (for build-tree and install-tree usage) - added from main
install(EXPORT c2pa_cppTargets
    FILE c2pa_cppTargets.cmake
    NAMESPACE c2pa_cpp::
    DESTINATION lib/cmake/c2pa_cpp
)

# Install prebuilt headers and libraries - added from main
install(DIRECTORY "${c2pa_prebuilt_SOURCE_DIR}/include/"
    DESTINATION include
    FILES_MATCHING PATTERN "*.h"
)
if(APPLE)
    install(FILES "${C2PA_C_LIB}" DESTINATION lib)
elseif(UNIX)
    install(FILES "${C2PA_C_LIB}" DESTINATION lib)
endif()
if(WIN32)
    install(FILES
        "${C2PA_C_LIB}"
        DESTINATION bin
    )
    install(FILES
        "${c2pa_prebuilt_SOURCE_DIR}/lib/${C2PA_IMPLIB_NAME}"
        DESTINATION lib
    )
endif()
