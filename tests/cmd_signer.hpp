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

#include <vector>

// TODO-TMN: move in folder app-examples/cpp-apis

/// @brief signs the data using openssl and returns the signature
/// @details This function requires openssl to be installed as a command line tool
/// @param data std::vector<unsigned char> - the data to be signed
/// @return std::vector<unsigned char>  - the signature
std::vector<unsigned char> cmd_signer(const std::vector<unsigned char> &data);

#endif // CMD_SIGNER_H