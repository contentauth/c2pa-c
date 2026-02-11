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
#include <nlohmann/json.hpp>
#include <filesystem>

using nlohmann::json;
namespace fs = std::filesystem;

TEST(ReadFile, ReadFileWithNoManifestReturnsEmptyOptional) {
  fs::path current_dir = fs::path(__FILE__).parent_path();
  fs::path test_file = current_dir / "../tests/fixtures/A.jpg";
  auto result = c2pa::read_file(test_file);
  ASSERT_FALSE(result.has_value());
};

class ReadFileWithManifestTests
    : public ::testing::TestWithParam<std::string> {
public:
  static void test_read_file_with_manifest(const std::string& filename) {
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path test_file = current_dir / "../tests/fixtures" / filename;
    auto result = c2pa::read_file(test_file);
    ASSERT_TRUE(result.has_value());

    // parse result with json
    auto json = json::parse(result.value());
    EXPECT_TRUE(json.contains("manifests"));
    EXPECT_TRUE(json.contains("active_manifest"));
  }
};

INSTANTIATE_TEST_SUITE_P(ReadFileWithManifestTests, ReadFileWithManifestTests,
                         ::testing::Values(
                             // Files with manifests
                             "C.jpg",
                             "video1.mp4",
                             "C.dng",
                             "sample1_signed.wav"));

TEST_P(ReadFileWithManifestTests, ReadFileWithManifestReturnsSomeValue) {
    auto filename = GetParam();
    test_read_file_with_manifest(filename);
}

TEST(ReadFile, ReadFileWithDataDirReturnsSomeValue)
{
  fs::path current_dir = fs::path(__FILE__).parent_path();
  fs::path test_file = current_dir / "../tests/fixtures/C.jpg";
  auto result = c2pa::read_file(test_file, current_dir / "../build/read_file");
  ASSERT_TRUE(result.has_value());

  // parse result with json
  auto json = json::parse(result.value());

  EXPECT_TRUE(json.contains("manifests"));
  EXPECT_TRUE(json.contains("active_manifest"));

  // build/read_file should exist and contain a manifest.json file
  EXPECT_TRUE(fs::exists(current_dir / "../build/read_file"));
  EXPECT_TRUE(fs::exists(current_dir / "../build/read_file/manifest_store.json"));
};
