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
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3")


INCLUDE_DIRECTORIES(include)

ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(tests)
ADD_SUBDIRECTORY(examples)

# Add the Rust library
set(RUST_LIB ${CMAKE_SOURCE_DIR}/target/release/libc2pa_c.dylib)

# Ensure the Rust library is built before linking
add_custom_command(
    OUTPUT ${RUST_LIB}
    COMMAND cargo build --release
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    COMMENT "Building Rust library"
)

# Create a custom target for the Rust library
add_custom_target(build_rust_lib ALL DEPENDS ${RUST_LIB})

# create the cpptest target
#add_executable(cpptest tests/test.cpp "${CMAKE_SOURCE_DIR}/src/c2pa.cpp" "${CMAKE_SOURCE_DIR}/tests/cmd_signer.cpp")
#target_include_directories(cpptest PUBLIC "${CMAKE_SOURCE_DIR}/include/" "${CMAKE_SOURCE_DIR}/tests/")
#target_link_libraries(cpptest nlohmann_json::nlohmann_json)
#target_link_libraries(cpptest "${CMAKE_SOURCE_DIR}/target/release/libc2pa_c.dylib")
