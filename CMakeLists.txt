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

# This is the version of this C++ project
project(c2pa-c VERSION 0.12.1)

# Set the version of the c2pa_rs library used here
set(C2PA_VERSION "0.66.0")

set(CMAKE_POLICY_DEFAULT_CMP0135 NEW)
set(CMAKE_C_STANDARD 17)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_OSX_DEPLOYMENT_TARGET "13.3")

if(MSVC)
add_compile_options(/utf-8)
endif()

# Set RPATH settings for shared library discovery
set(CMAKE_SKIP_BUILD_RPATH FALSE)
set(CMAKE_BUILD_WITH_INSTALL_RPATH FALSE)
# Disable automatic RPATH from link paths since we copy libraries to executable directories
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH FALSE)

# Note: We don't set CMAKE_BUILD_RPATH to the prebuilt directory
# because we copy libraries to each executable's directory instead

# Set compiler-specific warning flags
if(MSVC)
    # MSVC warning flags
    add_compile_options(/W4 /WX)
else()
    # GCC/Clang warning flags
    add_compile_options(-Wall -Wextra -Werror)
endif()

enable_testing()

ADD_SUBDIRECTORY(src)
ADD_SUBDIRECTORY(tests)
ADD_SUBDIRECTORY(examples)
