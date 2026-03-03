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

#include <fstream>
#include <stdlib.h>
#include <vector>

/// @brief Implementation of the command line signer function
std::vector<unsigned char> cmd_signer(const std::vector<unsigned char> &data)
{
    if (data.empty())
    {
        throw std::runtime_error("Signature data is empty");
    }

    std::ofstream source("build/cpp_data.bin", std::ios::binary);
    if (!source)
    {
        throw std::runtime_error("Failed to open temp signing file");
    }
    source.write(reinterpret_cast<const char *>(data.data()), data.size());

    // sign the temp file by calling openssl in a shell
    system("openssl dgst -sign tests/fixtures/es256_private.key -sha256 -out build/c_signature.sig build/c_data.bin");

    std::vector<uint8_t> signature;

    // Read the signature back into the output vector
    std::ifstream signature_file("build/c_signature.sig", std::ios::binary);
    if (!signature_file)
    {
        throw std::runtime_error("Failed to open signature file");
    }

    signature = std::vector<uint8_t>((std::istreambuf_iterator<char>(signature_file)), std::istreambuf_iterator<char>());

    return signature;
}