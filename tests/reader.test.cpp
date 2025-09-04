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
#include <fstream>

using nlohmann::json;
namespace fs = std::filesystem;

TEST(Reader, SupportedTypes) {
  auto supported_types = c2pa::Reader::supported_mime_types();
  EXPECT_TRUE(std::find(supported_types.begin(), supported_types.end(), "image/jpeg") != supported_types.end());
  EXPECT_TRUE(std::find(supported_types.begin(), supported_types.end(), "image/png") != supported_types.end());
};

TEST(Reader, StreamWithManifest) {
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path test_file = current_dir.parent_path() / "tests" / "fixtures" / L"CÖÄ_.jpg";
    ASSERT_TRUE(std::filesystem::exists(test_file)) << "Test file does not exist: " << test_file;

    // read the new manifest and display the JSON
    std::ifstream file_stream(test_file, std::ios::binary);
    ASSERT_TRUE(file_stream.is_open()) << "Failed to open file: " << test_file;

    auto reader = c2pa::Reader("image/jpeg", file_stream);
    auto manifest_store_json = reader.json();
    EXPECT_TRUE(manifest_store_json.find("C.jpg") != std::string::npos);
};

TEST(Reader, VideoStreamWithManifest) {
  fs::path current_dir = fs::path(__FILE__).parent_path();
  fs::path test_file = current_dir.parent_path() / "tests" / "fixtures" / "video1.mp4";
  ASSERT_TRUE(std::filesystem::exists(test_file)) << "Test file does not exist: " << test_file;

  // read the new manifest and display the JSON
  std::ifstream file_stream(test_file, std::ios::binary);
  ASSERT_TRUE(file_stream.is_open()) << "Failed to open video file: " << test_file;

  auto reader = c2pa::Reader("video/mp4", file_stream);
  auto manifest_store_json = reader.json();
  EXPECT_TRUE(manifest_store_json.find("My Title") != std::string::npos);
};

TEST(Reader, VideoStreamWithManifestUsingExtension) {
  fs::path current_dir = fs::path(__FILE__).parent_path();
  fs::path test_file = current_dir.parent_path() / "tests" / "fixtures" / "video1.mp4";
  ASSERT_TRUE(std::filesystem::exists(test_file)) << "Test file does not exist: " << test_file;

  // read the new manifest and display the JSON
  std::ifstream file_stream(test_file, std::ios::binary);
  ASSERT_TRUE(file_stream.is_open()) << "Failed to open video file: " << test_file;

  auto reader = c2pa::Reader("mp4", file_stream);
  auto manifest_store_json = reader.json();
  EXPECT_TRUE(manifest_store_json.find("My Title") != std::string::npos);
};

TEST(Reader, DngStreamWithManifest) {
  fs::path current_dir = fs::path(__FILE__).parent_path();
  fs::path test_file = current_dir.parent_path() / "tests" / "fixtures" / "C.dng";
  ASSERT_TRUE(std::filesystem::exists(test_file)) << "Test file does not exist: " << test_file;

  // read the new manifest and display the JSON
  std::ifstream file_stream(test_file, std::ios::binary);
  ASSERT_TRUE(file_stream.is_open()) << "Failed to open video file: " << test_file;

  auto reader = c2pa::Reader("DNG", file_stream);
  auto manifest_store_json = reader.json();
  // C.jpg because the DNG file was created from the C.jpg file
  EXPECT_TRUE(manifest_store_json.find("C.jpg") != std::string::npos);
};

TEST(Reader, FileWithManifest)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path test_file = current_dir / "../tests/fixtures/C.jpg";

    // read the new manifest and display the JSON
    auto reader = c2pa::Reader(test_file);
    auto manifest_store_json = reader.json();
    EXPECT_TRUE(manifest_store_json.find("C.jpg") != std::string::npos);
};

TEST(Reader, FileNoManifest)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path test_file = current_dir / "../tests/fixtures/A.jpg";
    EXPECT_THROW({ auto reader = c2pa::Reader(test_file); }, c2pa::C2paException);
};

class RemoteUrlTests
    : public ::testing::TestWithParam<std::tuple<std::string, bool>> {
public:
  static c2pa::Reader reader_from_fixture(const std::string &file_name) {
    auto current_dir = fs::path(__FILE__).parent_path();
    auto fixture = current_dir / "../tests/fixtures" / file_name;
    std::ifstream stream(fixture, std::ios::binary);
    return { "image/jpeg", stream  };
  }
};

INSTANTIATE_TEST_SUITE_P(ReaderRemoteUrlTests, RemoteUrlTests,
                         ::testing::Values(
                             // (fixture filename, is_remote_manifest)
                             std::make_tuple("cloud.jpg", true),
                             std::make_tuple("C.jpg", false)));

TEST_P(RemoteUrlTests, RemoteUrl) {
    auto reader = reader_from_fixture(std::get<0>(GetParam()));
    auto expected_is_remote = std::get<1>(GetParam());
    EXPECT_EQ(reader.remote_url().has_value(), expected_is_remote);
}

TEST_P(RemoteUrlTests, IsEmbeddedTest) {
    auto reader = reader_from_fixture(std::get<0>(GetParam()));
    auto expected_is_remote = std::get<1>(GetParam());
    EXPECT_EQ(reader.is_embedded(), !expected_is_remote);
}

TEST(Reader, HasManifestUtf8Path) {
    auto current_dir = fs::path(__FILE__).parent_path();
    auto test_file = current_dir.parent_path() / "tests" / "fixtures" / L"CÖÄ_.jpg";
    ASSERT_TRUE(std::filesystem::exists(test_file)) << "Test file does not exist: " << test_file;

    std::ifstream stream(test_file, std::ios::binary);
    auto reader = c2pa::Reader("image/jpeg", stream);

    EXPECT_FALSE(reader.remote_url());
    EXPECT_TRUE(reader.is_embedded());
}

/* remove this until we resolve CAWG Identity testing
TEST(Reader, FileWithCawgIdentityManifest)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path test_file = current_dir / "../tests/fixtures/C_with_CAWG_data.jpg";

    // read the new manifest and display the JSON
    auto reader = c2pa::Reader(test_file);
    auto manifest_store_string = reader.json();
    //printf("Manifest Store JSON: %s\n", manifest_store_string.c_str());
    auto manifest_store_json = json::parse(manifest_store_string);
    auto active_manifest_label = manifest_store_json["active_manifest"];
    auto active_manifest = manifest_store_json["manifests"][active_manifest_label];
    EXPECT_EQ(active_manifest["assertions"][1]["label"], "cawg.identity");
    // verify that the CAWG assertion was decoded into json correctly
    EXPECT_EQ(active_manifest["assertions"][1]["data"]["verifiedIdentities"][0]["type"], "cawg.social_media");

    EXPECT_EQ(manifest_store_json["validation_state"], "Valid");
    // verify that we successfully validated the CAWG assertion
    EXPECT_EQ(manifest_store_json["validation_results"]["activeManifest"]["success"][8]["code"], "cawg.ica.credential_valid");
};
*/

TEST(Reader, FileNotFound)
{
    try
    {
        auto reader = c2pa::Reader("foo/xxx.xyz");
        FAIL() << "Expected c2pa::C2paException";
    }
    catch (const c2pa::C2paException &e)
    {
        EXPECT_TRUE(std::string(e.what()).rfind("Failed to open file", 0) == 0);
    }
    catch (...)
    {
        FAIL() << "Expected c2pa::C2paException Failed to open file";
    }
};
