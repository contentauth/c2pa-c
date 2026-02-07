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

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <stdexcept>
#include <vector>
#include <unistd.h>

/// @brief Implementation of the command line signer function.
/// Hardened: uses mkstemp for secure temp files (S4), checks system() return (S1),
/// accepts raw pointer+size to avoid unnecessary vector copy (P1).
std::vector<unsigned char> cmd_signer(const uint8_t *data, size_t len)
{
    if (data == nullptr || len == 0)
    {
        throw std::runtime_error("Signature data is empty or null");
    }

    // Create secure temp files instead of predictable paths (S4)
    char data_template[] = "build/cpp_data_XXXXXX";
    char sig_template[] = "build/cpp_sig_XXXXXX";

    int data_fd = mkstemp(data_template);
    if (data_fd < 0)
    {
        throw std::runtime_error("Failed to create temp data file");
    }

    ssize_t written = write(data_fd, data, len);
    close(data_fd);
    if (written < 0 || static_cast<size_t>(written) != len)
    {
        unlink(data_template);
        throw std::runtime_error("Failed to write data to temp file");
    }

    // Build the openssl command with temp file paths
    char cmd[512];
    std::snprintf(cmd, sizeof(cmd),
        "openssl dgst -sign tests/fixtures/es256_private.key -sha256 -out %s %s",
        sig_template, data_template);

    // Sign the temp file (S1: check return value)
    int sys_ret = std::system(cmd);
    unlink(data_template);  // Clean up data temp file immediately
    if (sys_ret != 0)
    {
        unlink(sig_template);
        throw std::runtime_error("openssl signing command failed");
    }

    // Read the signature back into the output vector
    std::ifstream signature_file(sig_template, std::ios::binary);
    if (!signature_file)
    {
        unlink(sig_template);
        throw std::runtime_error("Failed to open signature file");
    }

    std::vector<uint8_t> signature(
        (std::istreambuf_iterator<char>(signature_file)),
        std::istreambuf_iterator<char>());
    signature_file.close();
    unlink(sig_template);  // Clean up signature temp file

    return signature;
}