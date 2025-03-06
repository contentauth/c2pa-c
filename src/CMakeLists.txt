cmake_minimum_required(VERSION 3.27)
project(c2pa_cpp)

include(FetchContent)

# CMake Dependencies
include(FetchContent)
FetchContent_Declare(
  googletest
  GIT_REPOSITORY https://github.com/google/googletest.git
  GIT_TAG v1.16.0
  OVERRIDE_FIND_PACKAGE)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Download json
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

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

file(GLOB unit_test_files CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tests/*.test.cpp")
add_executable(c2pa_c_tests ${unit_test_files})

# Create the static library
add_library(c2pa_cpp STATIC ${SOURCES})

if(MINGW)
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=gnu++17")
else()
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++17")
endif()

# Platform-independent library extension
if(APPLE)
  target_link_libraries(c2pa_cpp
    ${CMAKE_SOURCE_DIR}/target/release/libc2pa_c.dylib
  )
elseif(WIN32)
  target_link_libraries(c2pa_c_tests
    ${CMAKE_SOURCE_DIR}/target/release/c2pa_c.dll.lib
  )

  target_link_libraries(c2pa_cpp
    ${CMAKE_SOURCE_DIR}/target/release/c2pa_c.dll.lib
  )

  add_custom_command(TARGET c2pa_c_tests POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_SOURCE_DIR}/target/release/c2pa_c.dll"
    $<TARGET_FILE_DIR:c2pa_c_tests>
    COMMENT "Copying required c2pa_c.dll to executing directory of tests."
  )
else()
  target_link_libraries(c2pa_cpp
    ${CMAKE_SOURCE_DIR}/target/release/libc2pa_c.so
  )
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

target_include_directories(c2pa_c_tests PUBLIC "${CMAKE_SOURCE_DIR}/include/")

target_link_libraries(c2pa_c_tests nlohmann_json::nlohmann_json)
target_link_libraries(c2pa_c_tests c2pa_cpp)
target_link_libraries(c2pa_c_tests gtest_main)

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
