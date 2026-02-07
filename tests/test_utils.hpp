// Copyright 2024 Adobe. All rights reserved.
// This file is licensed to you under the Apache License,
// Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0)
// or the MIT license (http://opensource.org/licenses/MIT),
// at your option.
// Unless required by applicable law or agreed to in writing,
// this software is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR REPRESENTATIONS OF ANY KIND, either express or
// implied. See the LICENSE-MIT and LICENSE-APACHE files for the
// specific language governing permissions and limitations under
// each license.

/// @file   test_utils.hpp
/// @brief  Shared utilities for C++ tests

#ifndef C2PA_TEST_UTILS_HPP
#define C2PA_TEST_UTILS_HPP

#include <filesystem>
#include <fstream>
#include <string>
#include <stdexcept>

#include "c2pa.hpp"

namespace c2pa_test {

namespace fs = std::filesystem;

/// @brief Read a text file into a string
/// @param path Path to the file to read
/// @return Contents of the file as a string
/// @throws std::runtime_error if file cannot be opened
inline std::string read_text_file(const fs::path &path)
{
    std::ifstream file(path);
    if (!file.is_open())
    {
        throw std::runtime_error("Could not open file " + path.string());
    }
    std::string contents((std::istreambuf_iterator<char>(file)),
                         std::istreambuf_iterator<char>());
    file.close();
    return contents;
}

/// @brief Get path to a test fixture file
/// @param filename Name of the fixture file (e.g., "training.json")
/// @return Full path to the fixture
inline fs::path get_fixture_path(const std::string& filename)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();
    return current_dir / "fixtures" / filename;
}

/// @brief Create an ES256 signer with test credentials
/// @return Configured signer for testing
inline c2pa::Signer create_test_signer()
{
    auto fixtures_dir = get_fixture_path("");
    auto certs = read_text_file(fixtures_dir / "es256_certs.pem");
    auto private_key = read_text_file(fixtures_dir / "es256_private.key");
    return c2pa::Signer("Es256", certs, private_key,
                        "http://timestamp.digicert.com");
}

} // namespace c2pa_test

#endif // C2PA_TEST_UTILS_HPP
