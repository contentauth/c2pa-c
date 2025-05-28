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

#include <c2pa.hpp>
#include <gtest/gtest.h>

TEST(Version, VersionReturnsInCorrectFormat) {
  auto version = c2pa::version();
  // Check that the version string is not empty and follows the expected format
  // print the version for debugging purposes
  ASSERT_FALSE(version.empty());
  ASSERT_TRUE(version.find("c_api/0.") != std::string::npos);
}