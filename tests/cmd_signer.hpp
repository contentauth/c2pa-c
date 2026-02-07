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

#ifndef CMD_SIGNER_H
#define CMD_SIGNER_H

#include <cstdint>
#include <vector>

/// @brief signs the data using openssl and returns the signature
/// @details This function requires openssl to be installed as a command line tool.
///          Accepts raw pointer+size to avoid unnecessary vector copies (P1).
/// @param data pointer to the data to be signed
/// @param len length of the data in bytes
/// @return std::vector<unsigned char>  - the signature
std::vector<unsigned char> cmd_signer(const uint8_t *data, size_t len);

#endif // CMD_SIGNER_H