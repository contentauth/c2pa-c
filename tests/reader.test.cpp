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

class StreamWithManifestTests
    : public ::testing::TestWithParam<std::tuple<std::string, std::string, std::string>> {
public:
  static void test_stream_with_manifest(const std::string& filename, const std::string& mime_type, const std::string& expected_content) {
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path test_file = current_dir.parent_path() / "tests" / "fixtures" / filename;
    ASSERT_TRUE(std::filesystem::exists(test_file)) << "Test file does not exist: " << test_file;

    // read the new manifest and display the JSON
    std::ifstream file_stream(test_file, std::ios::binary);
    ASSERT_TRUE(file_stream.is_open()) << "Failed to open file: " << test_file;

    auto reader = c2pa::Reader(mime_type, file_stream);
    auto manifest_store_json = reader.json();
    EXPECT_TRUE(manifest_store_json.find(expected_content) != std::string::npos);
  }
};

INSTANTIATE_TEST_SUITE_P(ReaderStreamWithManifestTests, StreamWithManifestTests,
                         ::testing::Values(
                             // (filename, type or mimetype, expected_content = Title from the manifest)
                             std::make_tuple("CÖÄ_.jpg", "image/jpeg", "C.jpg"),
                             std::make_tuple("video1.mp4", "video/mp4", "My Title"),
                             std::make_tuple("sample1_signed.wav", "wav", "sample1_signed.wav"),
                             std::make_tuple("C.dng", "DNG", "C.jpg")));

TEST_P(StreamWithManifestTests, StreamWithManifest) {
    auto filename = std::get<0>(GetParam());
    auto mime_type = std::get<1>(GetParam());
    auto expected_content = std::get<2>(GetParam());
    test_stream_with_manifest(filename, mime_type, expected_content);
}

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

class FileWithManifestTests
    : public ::testing::TestWithParam<std::tuple<std::string, std::string>> {
public:
  static void test_file_with_manifest(const std::string& filename, const std::string& expected_content) {
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path test_file = current_dir / "../tests/fixtures" / filename;

    // Read the manifest from the file
    auto reader = c2pa::Reader(test_file);
    auto manifest_store_json = reader.json();

    // Simple content checks
    EXPECT_TRUE(manifest_store_json.find(expected_content) != std::string::npos);
  }
};

INSTANTIATE_TEST_SUITE_P(ReaderFileWithManifestTests, FileWithManifestTests,
                         ::testing::Values(
                             // (filename, expected_content = Title from the manifest)
                             std::make_tuple("C.jpg", "C.jpg"),
                             std::make_tuple("video1.mp4", "My Title"),
                             std::make_tuple("sample1_signed.wav", "sample1_signed.wav"),
                             std::make_tuple("C.dng", "C.jpg")));

TEST_P(FileWithManifestTests, FileWithManifest) {
    auto filename = std::get<0>(GetParam());
    auto expected_content = std::get<1>(GetParam());
    test_file_with_manifest(filename, expected_content);
}

TEST(Reader, ImageFileWithManifestMultipleCalls)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path test_file = current_dir / "../tests/fixtures/C.jpg";

    // read the new manifest and display the JSON
    auto reader = c2pa::Reader(test_file);
    auto manifest_store_json = reader.json();
    EXPECT_TRUE(manifest_store_json.find("C.jpg") != std::string::npos);

    auto manifest_store_json_2 = reader.json();
    EXPECT_TRUE(manifest_store_json_2.find("C.jpg") != std::string::npos);

    auto manifest_store_json_3 = reader.json();
    EXPECT_TRUE(manifest_store_json_3.find("C.jpg") != std::string::npos);
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

TEST(Reader, StreamClosed)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path test_file = current_dir / "../tests/fixtures/C.jpg";
    ASSERT_TRUE(std::filesystem::exists(test_file)) << "Test file does not exist: " << test_file;

    // Create a stream and close it before creating the reader
    std::ifstream file_stream(test_file, std::ios::binary);
    ASSERT_TRUE(file_stream.is_open()) << "Failed to open file: " << test_file;
    file_stream.close(); // Close the stream before creating reader

    // Attempt to create reader with closed stream should throw exception
    EXPECT_THROW({
        auto reader = c2pa::Reader("image/jpeg", file_stream);
    }, c2pa::C2paException);
};

TEST(Reader, MultipleReadersSameFile)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path test_file = current_dir / "../tests/fixtures/C.jpg";
    ASSERT_TRUE(std::filesystem::exists(test_file)) << "Test file does not exist: " << test_file;

    // Create multiple readers from the same file
    auto reader1 = c2pa::Reader(test_file);
    auto reader2 = c2pa::Reader(test_file);
    auto reader3 = c2pa::Reader(test_file);

    // All readers should be able to read the manifest independently
    auto manifest1 = reader1.json();
    auto manifest2 = reader2.json();
    auto manifest3 = reader3.json();

    // All manifests should be identical
    EXPECT_EQ(manifest1, manifest2);
    EXPECT_EQ(manifest2, manifest3);
    EXPECT_EQ(manifest1, manifest3);

    // All readers should report the same embedded status
    EXPECT_EQ(reader1.is_embedded(), reader2.is_embedded());
    EXPECT_EQ(reader2.is_embedded(), reader3.is_embedded());

    // All readers should report the same remote URL status
    EXPECT_EQ(reader1.remote_url().has_value(), reader2.remote_url().has_value());
    EXPECT_EQ(reader2.remote_url().has_value(), reader3.remote_url().has_value());

    // Verify the manifest contains expected content
    EXPECT_TRUE(manifest1.find("C.jpg") != std::string::npos);
    EXPECT_TRUE(manifest2.find("C.jpg") != std::string::npos);
    EXPECT_TRUE(manifest3.find("C.jpg") != std::string::npos);
};
