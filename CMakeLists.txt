# Copyright 2024 Adobe. All rights reserved.
# This file is licensed to you under the Apache License,
# Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0)
# or the MIT license (http://opensource.org/licenses/MIT),
# at your option.
#
# Unless required by applicable law or agreed to in writing,
# this software is distributed on an "AS IS" BASIS, WITHOUT
# WARRANTIES OR REPRESENTATIONS OF ANY KIND, either express or
# implied. See the LICENSE-MIT and LICENSE-APACHE files for the
# specific language governing permissions and limitations under
# each license.

cmake_minimum_required(VERSION 3.27)

project(c2pa-c)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

# CMake Dependencies
include(FetchContent)

# Download GoogleTest testing framework.
FetchContent_Declare(
        googletest
        # Specify the commit you depend on and update it regularly.
        URL https://github.com/google/googletest/archive/5376968f6948923e2411081fd9372e71a59d8e77.zip
)

# For Windows: Prevent overriding the parent project's compiler/linker settings
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Download json
FetchContent_Declare(json URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz)
FetchContent_MakeAvailable(json)

# Create the unit test target.
file(GLOB unit_test_files CONFIGURE_DEPENDS "tests/*.test.cpp")
add_executable(unit_tests ${unit_test_files})
target_include_directories(unit_tests PUBLIC "${CMAKE_SOURCE_DIR}/include/")
target_link_libraries(unit_tests nlohmann_json::nlohmann_json)
target_link_libraries(unit_tests "${CMAKE_SOURCE_DIR}/target/debug/libc2pa_c.dylib")
target_link_libraries(unit_tests gtest_main) # Links in gtest_main so we can run tests.