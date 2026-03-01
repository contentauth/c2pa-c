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
inline fs::path get_fixture_path(const fs::path& filename)
{
    fs::path current_dir = fs::path(__FILE__).parent_path().parent_path();
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

/// @brief Create Settings that contain a signer configured in JSON.
/// @return Settings object with an es256 signer from test fixtures.
inline c2pa::Settings create_test_settings_with_signer()
{
    auto settings_json = read_text_file(
        get_fixture_path("settings/test_settings_with_signer.json"));
    return c2pa::Settings(settings_json, "json");
}

/// @brief Callback function that signs data using Ed25519 via the C API.
/// @details This wraps `c2pa_ed25519_sign` in a SignerFunc callback, allowing
///          tests to exercise the CallbackSigner code path without depending
///          on openssl. The callback reads data from the input, signs it using
///          the Ed25519 private key from test fixtures, and returns the signature.
/// @param data Data bytes to sign.
/// @return Signature bytes.
/// @throws std::runtime_error if signing fails.
inline std::vector<unsigned char> ed25519_callback_signer(
    const std::vector<unsigned char> &data)
{
    auto private_key = read_text_file(get_fixture_path("ed25519.pem"));

    const unsigned char *sig_ptr = c2pa_ed25519_sign(
        data.data(), data.size(), private_key.c_str());
    if (!sig_ptr) {
        throw std::runtime_error("c2pa_ed25519_sign failed");
    }

    // Ed25519 signatures are always 64 bytes
    std::vector<unsigned char> signature(sig_ptr, sig_ptr + 64);
    c2pa_signature_free(sig_ptr);
    return signature;
}

/// @brief Create a callback-based Signer for testing.
/// @details Wraps ed25519_callback_signer into a c2pa::Signer via the
///          callback constructor. This exercises the CallbackSigner code path
///          without requiring openssl in the test environment.
/// @return Callback-based Signer configured for Ed25519 with test credentials.
inline c2pa::Signer create_test_callback_signer()
{
    auto certs = read_text_file(get_fixture_path("ed25519.pub"));
    return c2pa::Signer(
        &ed25519_callback_signer,
        C2paSigningAlg::Ed25519,
        certs,
        "http://timestamp.digicert.com");
}

} // namespace c2pa_test

#endif // C2PA_TEST_UTILS_HPP
