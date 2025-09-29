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
#include <string>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;

/// @brief Read a text file into a string
string read_text_file(const fs::path &path)
{
    ifstream file(path);
    if (!file.is_open())
    {
        throw runtime_error("Could not open file " + path.string());
    }
    string contents((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    file.close();
    return contents.data();
}

TEST(Builder, supported_mime_types_returns_types) {
  auto supported_types = c2pa::Builder::supported_mime_types();
  auto begin = supported_types.begin();
  auto end = supported_types.end();
  EXPECT_TRUE(std::find(begin, end, "image/jpeg") != end);
  EXPECT_TRUE(std::find(begin, end, "application/c2pa") != end);
}

TEST(Builder, SignImageFileOnly)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../build/example/training_image_only.jpg";

    auto manifest = read_text_file(manifest_path);
    auto certs = read_text_file(certs_path);
    auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::filesystem::remove(output_path.c_str()); // remove the file if it exists

    auto builder = c2pa::Builder(manifest);
    // the Builder returns manifest bytes
    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path, signer));

    // read to verify
    auto reader = c2pa::Reader(output_path);
    ASSERT_NO_THROW(reader.json());
    ASSERT_TRUE(std::filesystem::exists(output_path));
};

TEST(Builder, SignImageFileWithoutThumbnailAutoGenerationThroughSettings)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../build/example/training_image_only.jpg";

    auto manifest = read_text_file(manifest_path);
    auto certs = read_text_file(certs_path);
    auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // set settings to not generate thumbnails
    c2pa::load_settings("{\"builder\": { \"thumbnail\": {\"enabled\": false}}}", "json");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::filesystem::remove(output_path.c_str()); // remove the file if it exists

    auto builder = c2pa::Builder(manifest);
    // the Builder returns manifest bytes
    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path, signer));

    // read to verify
    ASSERT_TRUE(std::filesystem::exists(output_path));
    auto reader = c2pa::Reader(output_path);
    ASSERT_NO_THROW(reader.json());

    // reset settings to defaults, because settings are thread-local
    c2pa::load_settings("{\"builder\": { \"thumbnail\": {\"enabled\": true}}}", "json");
};

TEST(Builder, SignImageFileWithResource)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../build/example/training_resource_only.jpg";

    auto manifest = read_text_file(manifest_path);
    auto certs = read_text_file(certs_path);
    auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::filesystem::remove(output_path.c_str()); // remove the file if it exists

    auto builder = c2pa::Builder(manifest);
    // add a resource: a thumbnail
    builder.add_resource("thumbnail", image_path);

    // sign
    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path, signer));

    // read to verify signature
    auto reader = c2pa::Reader(output_path);
    ASSERT_NO_THROW(reader.json());
    ASSERT_TRUE(std::filesystem::exists(output_path));
};

TEST(Builder, SignWithMultipleResources)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path image_resource = current_dir / "../tests/fixtures/C.jpg";
    fs::path image_resource_other = current_dir / "../tests/fixtures/sample1.gif";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../build/example/multiple_resources.jpg";

    auto manifest = read_text_file(manifest_path);
    auto certs = read_text_file(certs_path);
    auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::filesystem::remove(output_path.c_str()); // remove the file if it exists

    auto builder = c2pa::Builder(manifest);

    // add multiple resources
    builder.add_resource("thumbnail1", image_path);
    builder.add_resource("thumbnail2", image_resource);
    builder.add_resource("thumbnail3", image_resource_other);

    // sign
    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path, signer));

    // read to verify signature
    auto reader = c2pa::Reader(output_path);
    ASSERT_NO_THROW(reader.json());
    ASSERT_TRUE(std::filesystem::exists(output_path));
}

TEST(Builder, SignImageFileWithIngredient)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../build/example/training_ingredient_only.jpg";

    auto manifest = read_text_file(manifest_path);
    auto certs = read_text_file(certs_path);
    auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::filesystem::remove(output_path.c_str()); // remove the file if it exists

    auto builder = c2pa::Builder(manifest);

    // add an ingredient
    string ingredient_json = "{\"title\":\"Test Ingredient\"}";
    // the signed image can be used as an ingredient too
    builder.add_ingredient(ingredient_json, signed_image_path);

    // sign
    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path, signer));

    // read to verify signature
    auto reader = c2pa::Reader(output_path);
    ASSERT_NO_THROW(reader.json());
    ASSERT_TRUE(std::filesystem::exists(output_path));
};

TEST(Builder, SignImageFileWithResourceAndIngredient)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../build/example/training.jpg";

    auto manifest = read_text_file(manifest_path);
    auto certs = read_text_file(certs_path);
    auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");
    std::filesystem::remove(output_path.c_str()); // remove the file if it exists

    auto builder = c2pa::Builder(manifest);
    // add a resource: a thumbnail
    builder.add_resource("thumbnail", image_path);

    // add an ingredient
    string ingredient_json = "{\"title\":\"Test Ingredient\"}";
    // the signed image can be used as an ingredient too
    builder.add_ingredient(ingredient_json, signed_image_path);

    // sign
    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path, signer));

    // read to verify signature
    auto reader = c2pa::Reader(output_path);
    ASSERT_NO_THROW(reader.json());
    ASSERT_TRUE(std::filesystem::exists(output_path));
};

TEST(Builder, SignVideoFileWithMultipleIngredients)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_video_path = current_dir / "../tests/fixtures/video1.mp4";
    fs::path audio_ingredient = current_dir / "../tests/fixtures/sample1_signed.wav";
    fs::path image_ingredient = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../build/example/video1_signed.mp4";

    auto manifest = read_text_file(manifest_path);
    auto certs = read_text_file(certs_path);
    auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::filesystem::remove(output_path.c_str()); // remove the file if it exists

    // create the builder
    auto builder = c2pa::Builder(manifest);

    // add the ingredients for the video
    string ingredient_video_json = "{\"title\":\"Test Video Ingredient\", \"relationship\": \"parentOf\"}";
    builder.add_ingredient(ingredient_video_json, signed_video_path);
    string ingredient_audio_json = "{\"title\":\"Test Audio Ingredient\", \"relationship\": \"componentOf\"}";
    builder.add_ingredient(ingredient_audio_json, audio_ingredient);
    string ingredient_image_json = "{\"title\":\"Test Image Ingredient\", \"relationship\": \"componentOf\"}";
    builder.add_ingredient(ingredient_image_json, image_ingredient);

    // sign
    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_video_path, output_path, signer));

    // read to verify signature
    auto reader = c2pa::Reader(output_path);
    ASSERT_NO_THROW(reader.json());
    ASSERT_TRUE(std::filesystem::exists(output_path));
};

TEST(Builder, SignVideoFileWithMultipleIngredientsAndResources)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_video_path = current_dir / "../tests/fixtures/video1.mp4";
    fs::path audio_ingredient = current_dir / "../tests/fixtures/sample1_signed.wav";
    fs::path image_ingredient = current_dir / "../tests/fixtures/A.jpg";
    fs::path image_resource = current_dir / "../tests/fixtures/C.jpg";
    fs::path image_resource_other = current_dir / "../tests/fixtures/sample1.gif";
    fs::path output_path = current_dir / "../build/example/video1_signed_with_ingredients_and_resources.mp4";

    auto manifest = read_text_file(manifest_path);
    auto certs = read_text_file(certs_path);
    auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::filesystem::remove(output_path.c_str()); // remove the file if it exists

    // create the builder
    auto builder = c2pa::Builder(manifest);

    // add the ingredients for the video
    string ingredient_video_json = "{\"title\":\"Test Video Ingredient\", \"relationship\": \"parentOf\"}";
    builder.add_ingredient(ingredient_video_json, signed_video_path);
    string ingredient_audio_json = "{\"title\":\"Test Audio Ingredient\", \"relationship\": \"componentOf\"}";
    builder.add_ingredient(ingredient_audio_json, audio_ingredient);
    string ingredient_image_json = "{\"title\":\"Test Image Ingredient\", \"relationship\": \"componentOf\"}";
    builder.add_ingredient(ingredient_image_json, image_ingredient);

    // add multiple resources
    builder.add_resource("thumbnail1", image_ingredient);
    builder.add_resource("thumbnail2", image_resource);
    builder.add_resource("thumbnail3", image_resource_other);

    // sign
    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_video_path, output_path, signer));

    // read to verify signature
    auto reader = c2pa::Reader(output_path);
    ASSERT_NO_THROW(reader.json());
    ASSERT_TRUE(std::filesystem::exists(output_path));
};

TEST(Builder, SignVideoFileWithMultipleIngredientsAndResourcesInterleaved)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_video_path = current_dir / "../tests/fixtures/video1.mp4";
    fs::path audio_ingredient = current_dir / "../tests/fixtures/sample1_signed.wav";
    fs::path image_ingredient = current_dir / "../tests/fixtures/A.jpg";
    fs::path image_resource = current_dir / "../tests/fixtures/C.jpg";
    fs::path image_resource_other = current_dir / "../tests/fixtures/sample1.gif";
    fs::path output_path = current_dir / "../build/example/video1_signed.mp4";

    auto manifest = read_text_file(manifest_path);
    auto certs = read_text_file(certs_path);
    auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");
    std::filesystem::remove(output_path.c_str()); // remove the file if it exists

    // create the builder
    auto builder = c2pa::Builder(manifest);

    // add ingredients and resources
    string ingredient_video_json = "{\"title\":\"Test Video Ingredient\", \"relationship\": \"parentOf\"}";
    builder.add_ingredient(ingredient_video_json, signed_video_path);
    builder.add_resource("thumbnail1", image_ingredient);
    string ingredient_audio_json = "{\"title\":\"Test Audio Ingredient\", \"relationship\": \"componentOf\"}";
    builder.add_ingredient(ingredient_audio_json, audio_ingredient);
    builder.add_resource("thumbnail2", image_resource);
    builder.add_resource("thumbnail3", image_resource_other);
    string ingredient_image_json = "{\"title\":\"Test Image Ingredient\", \"relationship\": \"componentOf\"}";
    builder.add_ingredient(ingredient_image_json, image_ingredient);

    // sign
    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_video_path, output_path, signer));

    // read to verify signature
    auto reader = c2pa::Reader(output_path);
    ASSERT_NO_THROW(reader.json());
    ASSERT_TRUE(std::filesystem::exists(output_path));
};

class SimplePathSignTest : public ::testing::TestWithParam<std::string> {};
INSTANTIATE_TEST_SUITE_P(
    BuilderSignCallToVerifyMiscFileTypes,
    SimplePathSignTest,
    ::testing::Values(
        "A.jpg",
        "C.jpg",
        "C.dng",
        "C_with_CAWG_data.jpg",
        "sample1.gif",
        "sample1.mp3",
        "sample1.wav",
        "sample1.webp",
        "sample2.svg",
        "video1.mp4"
        )
);

TEST_P(SimplePathSignTest, SignsFileTypes) {
  fs::path current_dir = fs::path(__FILE__).parent_path();

  // Construct the paths relative to the current directory
  fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
  fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
  fs::path asset_path = current_dir / "../tests/fixtures" / SimplePathSignTest::GetParam();

  fs::path output_path = current_dir / "../build/example" / SimplePathSignTest::GetParam();
  std::filesystem::remove(output_path.c_str()); // remove the file if it exists

  auto manifest = read_text_file(manifest_path);
  auto certs = read_text_file(certs_path);
  auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");

  // create a signer
  auto signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");
  auto builder = c2pa::Builder(manifest);

  std::vector<unsigned char> manifest_data;
  ASSERT_NO_THROW(manifest_data = builder.sign(asset_path, output_path, signer));
  ASSERT_FALSE(manifest_data.empty());

  ASSERT_TRUE(std::filesystem::exists(output_path));

  auto reader = c2pa::Reader(output_path);
  ASSERT_NO_THROW(reader.json());
}

TEST(Builder, SignImageStream)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";

    auto manifest = read_text_file(manifest_path);
    auto certs = read_text_file(certs_path);
    auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    auto builder = c2pa::Builder(manifest);

    std::ifstream source(signed_image_path, std::ios::binary);
    if (!source)
    {
        FAIL() << "Failed to open file: " << signed_image_path << std::endl;
    }

    // Create a memory buffer
    std::stringstream memory_buffer(std::ios::in | std::ios::out | std::ios::binary);
    std::iostream &dest = memory_buffer;
    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign("image/jpeg", source, dest, signer));
    source.close();

    // Rewind dest to the start
    dest.flush();
    dest.seekp(0, std::ios::beg);
    auto reader = c2pa::Reader("image/jpeg", dest);
    std::string json;
    ASSERT_NO_THROW(json = reader.json());
    ASSERT_TRUE(json.find("cawg.training-mining") != std::string::npos);
}

TEST(Builder, SignImageWithIngredientHavingManifestStream)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path ingredient_image_path = current_dir / "../tests/fixtures/C.jpg";

    auto manifest = read_text_file(manifest_path);
    auto certs = read_text_file(certs_path);
    auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    auto builder = c2pa::Builder(manifest);

    // there can only be one parent ingredient
    string ingredient_json = "{\"title\":\"Test Ingredient\", \"relationship\": \"parentOf\"}";
    builder.add_ingredient(ingredient_json, signed_image_path);
    // other ingredients can be components
    string ingredient_json_2 = "{\"title\":\"Test Ingredient 2\", \"relationship\": \"componentOf\"}";
    builder.add_ingredient(ingredient_json_2, ingredient_image_path);

    std::ifstream source(signed_image_path, std::ios::binary);
    if (!source)
    {
        FAIL() << "Failed to open file: " << signed_image_path << std::endl;
    }

    // Create a memory buffer
    std::stringstream memory_buffer(std::ios::in | std::ios::out | std::ios::binary);
    std::iostream &dest = memory_buffer;
    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign("image/jpeg", source, dest, signer));
    source.close();

    // Rewind dest to the start
    dest.flush();
    dest.seekp(0, std::ios::beg);
    auto reader = c2pa::Reader("image/jpeg", dest);
    std::string json;
    ASSERT_NO_THROW(json = reader.json());
    ASSERT_TRUE(json.find("cawg.training-mining") != std::string::npos);
}

TEST(Builder, NoSignOnInvalidDoubleParentsManifest)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path ingredient_image_path = current_dir / "../tests/fixtures/C.jpg";

    auto manifest = read_text_file(manifest_path);
    auto certs = read_text_file(certs_path);
    auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    auto builder = c2pa::Builder(manifest);

    // there can only be one parent ingredient
    string ingredient_json = "{\"title\":\"Test Ingredient\", \"relationship\": \"parentOf\"}";
    builder.add_ingredient(ingredient_json, signed_image_path);
    // adding a second parent ingredient will make the manifest invalid
    string ingredient_json_2 = "{\"title\":\"Test Ingredient 2\", \"relationship\": \"parentOf\"}";
    builder.add_ingredient(ingredient_json_2, ingredient_image_path);

    std::ifstream source(signed_image_path, std::ios::binary);
    if (!source)
    {
        FAIL() << "Failed to open file: " << signed_image_path << std::endl;
    }

    // Create a memory buffer
    std::stringstream memory_buffer(std::ios::in | std::ios::out | std::ios::binary);
    std::iostream &dest = memory_buffer;

    // Expect the sign operation to fail due to invalid manifest (multiple parent ingredients)
    EXPECT_THROW(builder.sign("image/jpeg", source, dest, signer), c2pa::C2paException);
    source.close();
}

TEST(Builder, SignStreamCloudUrl)
{
    try
    {
        fs::path current_dir = fs::path(__FILE__).parent_path();

        // Construct the paths relative to the current directory
        fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
        fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
        fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";

        auto manifest = read_text_file(manifest_path);
        auto certs = read_text_file(certs_path);
        auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");

        // create a signer
        c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

        auto builder = c2pa::Builder(manifest);

        // very important to use a URL that does not exist, otherwise you may get a JumbfParseError or JumbfNotFound
        builder.set_remote_url("http://this_does_not_exist/foo.jpg");
        builder.set_no_embed();

        // auto manifest_data = builder.sign(signed_image_path, "build/dest.jpg", signer);
        std::ifstream source(signed_image_path, std::ios::binary);
        if (!source)
        {
            FAIL() << "Failed to open file: " << signed_image_path << std::endl;
        }

        // Create a memory buffer
        std::stringstream memory_buffer(std::ios::in | std::ios::out | std::ios::binary);
        std::iostream &dest = memory_buffer;
        auto manifest_data = builder.sign("image/jpeg", source, dest, signer);
        source.close();

        // Rewind dest to the start
        dest.flush();
        dest.seekp(0, std::ios::beg);
        auto reader = c2pa::Reader("image/jpeg", dest);
    }
    catch (c2pa::C2paException const &e)
    {
        std::string error_message = e.what();
        if (error_message.rfind("Remote:", 0) == 0)
        {
            SUCCEED();
        }
        else
        {
            FAIL() << "Failed: C2pa::Builder: " << e.what() << endl;
        }
    };
}

TEST(Builder, SignDataHashedEmbedded)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    // fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";

    auto manifest = read_text_file(manifest_path);
    auto certs = read_text_file(certs_path);
    auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    auto builder = c2pa::Builder(manifest);

    auto placeholder = builder.data_hashed_placeholder(signer.reserve_size(), "image/jpeg");

    std::string data_hash = R"({
      "exclusions": [
        {
          "start": 20,
          "length": 45884
        }
      ],
      "name": "jumbf manifest",
      "alg": "sha256",
      "hash": "gWZNEOMHQNiULfA/tO5HD2awOwYMA3tnfUPApIr9csk=",
      "pad": " "
    })";
    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign_data_hashed_embeddable(signer, data_hash, "image/jpeg"));
}

TEST(Builder, SignDataHashedEmbeddedWithAsset)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";

    auto manifest = read_text_file(manifest_path);
    auto certs = read_text_file(certs_path);
    auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    auto builder = c2pa::Builder(manifest);

    auto placeholder = builder.data_hashed_placeholder(signer.reserve_size(), "image/jpeg");

    std::string data_hash = R"({
      "exclusions": [
        {
          "start": 20,
          "length": 45884
        }
      ],
      "name": "jumbf manifest",
      "alg": "sha256",
      "hash": "",
      "pad": " "
    })";

    std::ifstream asset(image_path, std::ios::binary);
    if (!asset)
    {
        FAIL() << "Failed to open file: " << image_path << std::endl;
    }

    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign_data_hashed_embeddable(signer, data_hash, "application/c2pa", &asset));

    auto embeddable_data = c2pa::Builder::format_embeddable("image/jpeg", manifest_data);

    ASSERT_TRUE(embeddable_data.size() > manifest_data.size());
}

TEST(Builder, SignWithInvalidStream)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";

    auto manifest = read_text_file(manifest_path);
    auto certs = read_text_file(certs_path);
    auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    auto builder = c2pa::Builder(manifest);

    // create an empty/invalid stream
    std::stringstream empty_stream;

    std::stringstream dest;

    // expect the sign operation to fail due to invalid/empty stream
    EXPECT_THROW(builder.sign("image/jpeg", empty_stream, dest, signer), c2pa::C2paException);
}

TEST(Builder, ReadIngredientFile)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the path to the test fixture
    fs::path source_path = current_dir / "../tests/fixtures/A.jpg";

    // Use target/tmp like other tests
    fs::path temp_dir = "target/tmp";

    // Remove and recreate the target/tmp folder before using it
    // This is technically a clean-up in-between tests
    if (fs::exists(temp_dir)) {
        fs::remove_all(temp_dir);
    }
    fs::create_directories(temp_dir);

    // Test that the function can read ingredient file successfully
    std::string result;
    ASSERT_NO_THROW(result = c2pa::read_ingredient_file(source_path, temp_dir));

    // Verify that the result is not empty
    ASSERT_FALSE(result.empty());

    // Expected JSON structure:
    // The result should contain at least the following structure:
    // {
    //   "title": "A.jpg",
    //   "format": "image/jpeg",
    //   "thumbnail": {
    //     ... more goes here (identifier and hash)
    //     ... but we don't check these in this test
    //   },
    //   "relationship": "componentOf"
    // }

    ASSERT_TRUE(result.find("\"title\"") != std::string::npos);
    ASSERT_TRUE(result.find("\"A.jpg\"") != std::string::npos);

    ASSERT_TRUE(result.find("\"format\"") != std::string::npos);
    ASSERT_TRUE(result.find("\"image/jpeg\"") != std::string::npos);

    ASSERT_TRUE(result.find("\"thumbnail\"") != std::string::npos);

    ASSERT_TRUE(result.find("\"relationship\"") != std::string::npos);
    ASSERT_TRUE(result.find("\"componentOf\"") != std::string::npos);
}

TEST(Builder, ReadIngredientFileWhoHasAManifestStore)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the path to the test fixture
    // C has a manifest store attached
    fs::path source_path = current_dir / "../tests/fixtures/C.jpg";

    // Use target/tmp like other tests
    fs::path temp_dir = "target/tmp";

    // Remove and recreate the target/tmp folder before using it
    // This is technically a clean-up in-between tests
    if (fs::exists(temp_dir)) {
        fs::remove_all(temp_dir);
    }
    fs::create_directories(temp_dir);

    // Test that the function can read ingredient file successfully
    std::string result;
    ASSERT_NO_THROW(result = c2pa::read_ingredient_file(source_path, temp_dir));

    // The expected JSON structure for an ingredient with a manifest
    // is extended, as there are additional fields.
    // So, the result should contain at least the following structure:
    // {
    //   "title": "C.jpg",
    //   "format": "image/jpeg",
    //   "thumbnail": {
    //     "format": "image/jpeg",
    //     ... more goes here (identifier and hash)
    //     ... but we don't check these in this test
    //   },
    //   "relationship": "componentOf",
    //   "active_manifest": "contentauth:urn:uuid:c85a2b90-f1a0-4aa4-b17f-f938b475804e",
    //   "validation_results": { ... more goes here ... }
    //   "manifest_data": {
    //     "format": "application/c2pa",
    //     "identifier": not checked in the test, value changes after multiple
    //                  runs during debugs to be unique because we reuse the
    //                  same dir holding resources
    //   }
    // }

    ASSERT_TRUE(result.find("\"title\"") != std::string::npos);
    ASSERT_TRUE(result.find("\"C.jpg\"") != std::string::npos);

    ASSERT_TRUE(result.find("\"format\"") != std::string::npos);
    ASSERT_TRUE(result.find("\"image/jpeg\"") != std::string::npos);

    ASSERT_TRUE(result.find("\"thumbnail\"") != std::string::npos);

    ASSERT_TRUE(result.find("\"relationship\"") != std::string::npos);
    ASSERT_TRUE(result.find("\"componentOf\"") != std::string::npos);

    // Additional fields because the ingredient has a manifest store attached
    ASSERT_TRUE(result.find("\"active_manifest\"") != std::string::npos);
    ASSERT_TRUE(result.find("\"contentauth:urn:uuid:c85a2b90-f1a0-4aa4-b17f-f938b475804e\"") != std::string::npos);

    ASSERT_TRUE(result.find("\"validation_results\"") != std::string::npos);

    ASSERT_TRUE(result.find("\"manifest_data\"") != std::string::npos);
    ASSERT_TRUE(result.find("\"application/c2pa\"") != std::string::npos);
}

TEST(Builder, AddIngredientAsResourceToBuilder)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";

    // Construct the path to the test fixture
    fs::path ingredient_source_path = current_dir / "../tests/fixtures/A.jpg";
    std::string ingredient_source_path_str = ingredient_source_path.string();

    fs::path temp_dir = current_dir / "../build/ingredient_as_resource_temp_dir";

    // Remove and recreate the build/ingredient_as_resource_temp_dir folder before using it
    // This is technically a clean-up in-between tests
    if (fs::exists(temp_dir)) {
        fs::remove_all(temp_dir);
    }
    fs::create_directories(temp_dir);

    // Get the needed JSON for the ingredient from the ingredient file using `read_ingredient_file`
    std::string result;
    // The data_dir is the location where binary resources will be stored
    // eg. thumbnails
    result = c2pa::read_ingredient_file(ingredient_source_path, temp_dir);

    // Extract the identifier from the ingredient JSON, as we will use it to add the resource
    // by building the path to the ingredient's thumbnail.
    // (The identifier may change in between runs because we reuse the tmp folder)
    std::string identifier;
    size_t identifier_start = result.find("\"identifier\":");
    if (identifier_start != std::string::npos) {
        identifier_start = result.find("\"", identifier_start + 13); // Skip "identifier":
        if (identifier_start != std::string::npos) {
            identifier_start++; // Skip the opening quote
            size_t identifier_end = result.find("\"", identifier_start);
            if (identifier_end != std::string::npos) {
                identifier = result.substr(identifier_start, identifier_end - identifier_start);
            }
        }
    }

    // Create the builder using a manifest JSON
    auto manifest = read_text_file(manifest_path);

    // Add ingredients array to the manifest JSON, since our demo manifest doesn't have it,
    // and we are adding ingredients more manually than through the Builder.add_ingredient call.
    // Parse the JSON and add ingredients array
    std::string modified_manifest = manifest;
    // Find the last closing brace and insert ingredients array before it
    size_t last_brace = modified_manifest.find_last_of('}');
    if (last_brace != std::string::npos) {
        std::string ingredients_array = ",\n  \"ingredients\": [\n    " + result + "\n  ]";
        modified_manifest.insert(last_brace, ingredients_array);
    }

    auto builder = c2pa::Builder(modified_manifest);

    // Add a resource: a thumbnail for the ingredient
    // for the thumbnail path, we use what was put in the data_dir by the read_ingredient_file call.
    fs::path ingredient_thumbnail_path = temp_dir / identifier;
    builder.add_resource(identifier, ingredient_thumbnail_path);

    // Create a signer
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    auto certs = read_text_file(certs_path);
    auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::vector<unsigned char> manifest_data;
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../build/example/signed_with_ingredient_and_resource.jpg";
    manifest_data = builder.sign(signed_image_path, output_path, signer);

    auto reader = c2pa::Reader(output_path);
    ASSERT_NO_THROW(reader.json());
}

TEST(Builder, AddIngredientToBuilderUsingBasePath)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";

    // Construct the path to the test fixture
    fs::path ingredient_source_path = current_dir / "../tests/fixtures/A.jpg";
    std::string ingredient_source_path_str = ingredient_source_path.string();

    // Use temp dir for ingredient data (data dir)
    fs::path temp_dir = current_dir / "../build/ingredient_as_resource_temp_dir";

    // Remove and recreate the build/ingredient_as_resource_temp_dir folder before using it
    // This is technically a clean-up in-between tests
    if (fs::exists(temp_dir)) {
        fs::remove_all(temp_dir);
    }
    fs::create_directories(temp_dir);

    // Get the needed JSON for the ingredient
    std::string result;
    // The data_dir is the location where binary resources will be stored
    // eg. thumbnails
    result = c2pa::read_ingredient_file(ingredient_source_path, temp_dir);

    // Create the builder using a manifest JSON
    auto manifest = read_text_file(manifest_path);

    // Add ingredients array to the manifest JSON, since our demo manifest doesn't have it,
    // and we are adding ingredients more manually than through the Builder.add_ingredient call.

    // Parse the JSON and add ingredients array
    std::string modified_manifest = manifest;
    // Find the last closing brace and insert ingredients array before it
    size_t last_brace = modified_manifest.find_last_of('}');
    if (last_brace != std::string::npos) {
        std::string ingredients_array = ",\n  \"ingredients\": [\n    " + result + "\n  ]";
        modified_manifest.insert(last_brace, ingredients_array);
    }

    auto builder = c2pa::Builder(modified_manifest);

    // a Builder can load resources from a base path
    // eg. ingredients from a data directory.
    // Here, we can reuse the data directory from the read_ingredient_file call,
    // so the builder properly loads the ingredient data using that directory.
    builder.set_base_path(temp_dir.string());

    // Create a signer
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    auto certs = read_text_file(certs_path);
    auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::vector<unsigned char> manifest_data;
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../build/example/signed_with_ingredient_and_resource.jpg";
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path, signer));

    auto reader = c2pa::Reader(output_path);
    ASSERT_NO_THROW(reader.json());
}

TEST(Builder, AddIngredientToBuilderUsingBasePathWithManifestContainingPlacedAction)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the path to the test fixture
    fs::path ingredient_source_path = current_dir / "../tests/fixtures/A.jpg";
    std::string ingredient_source_path_str = ingredient_source_path.string();

    // Use temp dir for ingredient data
    fs::path temp_dir = current_dir / "../build/ingredient_placed_as_resource_temp_dir";

    // set settings to not auto-add a placed action
    c2pa::load_settings("{\"builder\": { \"actions\": {\"auto_placed_action\": {\"enabled\": false}}}}", "json");

    // Remove and recreate the build/ingredient_as_resource_temp_dir folder before using it
    // This is technically a clean-up in-between tests
    if (fs::exists(temp_dir)) {
        fs::remove_all(temp_dir);
    }
    fs::create_directories(temp_dir);

    // Get the needed JSON for the ingredient
    std::string result;
    result = c2pa::read_ingredient_file(ingredient_source_path, temp_dir);

    // Extract the instance_id from the ingredient JSON, we need it to link the ingredient
    std::string instance_id;
    size_t instance_id_start = result.find("\"instance_id\":");
    if (instance_id_start != std::string::npos) {
        instance_id_start = result.find("\"", instance_id_start + 14); // Skip "instance_id":
        if (instance_id_start != std::string::npos) {
            instance_id_start++; // Skip the opening quote
            size_t instance_id_end = result.find("\"", instance_id_start);
            if (instance_id_end != std::string::npos) {
                instance_id = result.substr(instance_id_start, instance_id_end - instance_id_start);
            }
        }
    }

    // Initialize the near-complete manifest JSON (contains ingredient JSON and placed action)
    // We are going to replace the placeholder value in the "ingredientIds" array with 
    // the ingredientId we got by reading the ingredient from file.
    std::string manifest = R"({
        "vendor": "a-vendor",
        "claim_generator_info": [
            {
                "name": "c2pa-c test",
                "version": "1.0.0"
            }
        ],
        "assertions": [
            {
                "label": "c2pa.actions",
                "data": {
                    "actions": [
                        {
                            "action": "c2pa.created",
                            "description": "Created a new file or content",
                            "parameters": {
                                "com.vendor.tool": "new"
                            },
                            "digitalSourceType": "http://cv.iptc.org/newscodes/digitalsourcetype/digitalCreation"
                        },
                        {
                            "action": "c2pa.placed",
                            "description": "Added pre-existing content to this file",
                            "parameters": {
                                "com.vendor.tool": "place_embedded_object",
                                "ingredientIds": ["placeholder_ingredient_id"]
                            }
                        }
                    ],
                    "metadata": {
                        "dateTime": "2025-09-25T20:59:48.262Z"
                    }
                }
            }
        ]
    })";

    // Note: Fragile JSON parsing, but OK for testing purposes.
    std::string modified_manifest = manifest;
    // Find the last closing brace and insert ingredients array before it
    size_t last_brace = modified_manifest.find_last_of('}');
    if (last_brace != std::string::npos) {
        std::string ingredients_array = ",\n  \"ingredients\": [\n    " + result + "\n  ]";
        modified_manifest.insert(last_brace, ingredients_array);
    }
    // Update the ingredientIds parameter in the c2pa.placed action to reference the actual ingredient
    if (!instance_id.empty()) {
        // Find the c2pa.placed action and update its ingredientIds parameter
        size_t placed_action_start = modified_manifest.find("\"action\": \"c2pa.placed\"");
        if (placed_action_start != std::string::npos) {
            // Find the parameters section of this action
            size_t parameters_start = modified_manifest.find("\"parameters\": {", placed_action_start);
            if (parameters_start != std::string::npos) {
                // Find the ingredientIds parameter and replace its value
                size_t ingredient_ids_start = modified_manifest.find("\"ingredientIds\":", parameters_start);
                if (ingredient_ids_start != std::string::npos) {
                    // Find the array value of the ingredientIds parameter
                    size_t array_start = modified_manifest.find("[", ingredient_ids_start);
                    if (array_start != std::string::npos) {
                        size_t array_end = modified_manifest.find("]", array_start);
                        if (array_end != std::string::npos) {
                            // Replace the placeholder ingredientId value with the actual retrieved instance_id
                            std::string ingredient_ids_array = "[\"" + instance_id + "\"]";
                            modified_manifest.replace(array_start, array_end - array_start + 1, ingredient_ids_array);
                        }
                    }
                }
            }
        }
    }

    // Now we can create a Builder with the manually adjusted manifest
    auto builder = c2pa::Builder(modified_manifest);

    // A Builder can load resources from a base path eg. ingredients from a data directory.
    // Here, we reuse the data directory from the read_ingredient_file call,
    // so the builder properly loads the ingredient data using that directory.
    builder.set_base_path(temp_dir.string());

    // Create a signer
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    auto certs = read_text_file(certs_path);
    auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::vector<unsigned char> manifest_data;
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../build/example/signed_with_ingredient_and_resource.jpg";
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path, signer));

    auto reader = c2pa::Reader(output_path);
    ASSERT_NO_THROW(reader.json());

    // reset settings to auto-add a placed action
    c2pa::load_settings("{\"builder\": { \"actions\": {\"auto_placed_action\": {\"enabled\": true}}}}", "json");
}


TEST(Builder, AddIngredientWithProvenanceDataToBuilderUsingBasePath)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";

    // Construct the path to the test fixture
    fs::path ingredient_source_path = current_dir / "../tests/fixtures/C.jpg";
    std::string ingredient_source_path_str = ingredient_source_path.string();

    // Use target/tmp like other tests
    fs::path temp_dir = current_dir / "../build/ingredient_as_resource_temp_dir";

    // Remove and recreate the build/ingredient_as_resource_temp_dir folder before using it
    // This is technically a clean-up in-between tests
    if (fs::exists(temp_dir)) {
        fs::remove_all(temp_dir);
    }
    fs::create_directories(temp_dir);

    // Get the needed JSON for the ingredient
    std::string result;
    // The data_dir is the location where binary resources will be stored
    // eg. thumbnails, but also additional c2pa data
    result = c2pa::read_ingredient_file(ingredient_source_path, temp_dir);

    // Create the builder using a manifest JSON
    auto manifest = read_text_file(manifest_path);

    // Add ingredients array to the manifest JSON, since our demo manifest doesn't have it,
    // and we are adding ingredients more manually than through the Builder.add_ingredient call.

    // Parse the JSON and add ingredients array
    std::string modified_manifest = manifest;
    // Find the last closing brace and insert ingredients array before it
    size_t last_brace = modified_manifest.find_last_of('}');
    if (last_brace != std::string::npos) {
        std::string ingredients_array = ",\n  \"ingredients\": [\n    " + result + "\n  ]";
        modified_manifest.insert(last_brace, ingredients_array);
    }

    auto builder = c2pa::Builder(modified_manifest);

    // a Builder can load resources from a base path
    // eg. ingredients from a data directory.
    // Here, we reuse the data directory from the read_ingredient_file call,
    // so the builder properly loads the ingredient data using that directory.
    builder.set_base_path(temp_dir.string());

    // Create a signer
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    auto certs = read_text_file(certs_path);
    auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::vector<unsigned char> manifest_data;
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../build/example/signed_with_ingredient_and_resource.jpg";
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path, signer));

    auto reader = c2pa::Reader(output_path);
    ASSERT_NO_THROW(reader.json());
}
