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
#include <string>
#include <filesystem>

using namespace std;
namespace fs = std::filesystem;
using nlohmann::json;

// TODO-TMN
// Other settings to try out from https://github.com/contentauth/c2pa-rs/blob/main/sdk/tests/fixtures/test_settings.toml:
// (Also do the JSON edition)
// - Signer in settings
// - tsa_url
// - claim generator
// - all_actions_included

/// @brief Read a text file into a string
static string read_text_file(const fs::path &path)
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

TEST(Builder, exposes_raw_pointer) {
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    auto manifest = read_text_file(manifest_path);
    c2pa::Builder builder(manifest);
    ASSERT_NE(builder.c2pa_builder(), nullptr);
}

TEST(Builder, AddAnActionAndSign)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../build/example/image_with_one_action.jpg";

    auto manifest = read_text_file(manifest_path);
    auto certs = read_text_file(certs_path);
    auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // Create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::filesystem::remove(output_path.c_str()); // remove the file if it exists

    auto builder = c2pa::Builder(manifest);

    // Add an action to the builder
    string action_json = R"({
        "action": "c2pa.color_adjustments",
        "parameters": {
            "name": "brightnesscontrast"
        }
    })";
    builder.add_action(action_json);

    // Sign with the added actions. The Builder returns manifest bytes
    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path, signer));

    // Read to verify
    auto reader = c2pa::Reader(output_path);
    string json_result;
    ASSERT_NO_THROW(json_result = reader.json());
    ASSERT_TRUE(std::filesystem::exists(output_path));

    // Parse the JSON result for structured validation
    json manifest_json;
    ASSERT_NO_THROW(manifest_json = json::parse(json_result));

    // Get the active manifest
    ASSERT_TRUE(manifest_json.contains("manifests"));
    ASSERT_TRUE(manifest_json.contains("active_manifest"));
    string active_manifest_label = manifest_json["active_manifest"];
    ASSERT_TRUE(manifest_json["manifests"].contains(active_manifest_label));
    json active_manifest = manifest_json["manifests"][active_manifest_label];
    ASSERT_TRUE(active_manifest.contains("assertions"));
    ASSERT_TRUE(active_manifest["assertions"].is_array());

    // Find the c2pa.actions assertion
    json actions_assertion;
    bool found_actions_assertion = false;
    for (const auto& assertion : active_manifest["assertions"]) {
        if (assertion.contains("label") && assertion["label"] == "c2pa.actions.v2") {
            actions_assertion = assertion;
            found_actions_assertion = true;
            break;
        }
    }
    ASSERT_TRUE(found_actions_assertion);

    // Verify the actions assertion has the expected structure
    ASSERT_TRUE(actions_assertion.contains("data"));
    ASSERT_TRUE(actions_assertion["data"].contains("actions"));
    ASSERT_TRUE(actions_assertion["data"]["actions"].is_array());
    json actions_array = actions_assertion["data"]["actions"];
    ASSERT_FALSE(actions_array.empty());

    // Verify the action we added is here...
    json our_action;
    bool found_our_action = false;
    for (const auto& action : actions_array) {
        if (action.contains("action") && action["action"] == "c2pa.color_adjustments") {
            our_action = action;
            found_our_action = true;
            break;
        }
    }
    ASSERT_TRUE(found_our_action);

    // Verify the action structure
    ASSERT_TRUE(our_action.contains("action"));
    ASSERT_EQ(our_action["action"], "c2pa.color_adjustments");
    ASSERT_TRUE(our_action.contains("parameters"));
    ASSERT_TRUE(our_action["parameters"].contains("name"));
    ASSERT_EQ(our_action["parameters"]["name"], "brightnesscontrast");
};

TEST(Builder, AddMultipleActionsAndSign)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../build/example/image_with_multiple_actions.jpg";

    auto manifest = read_text_file(manifest_path);
    auto certs = read_text_file(certs_path);
    auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // Create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::filesystem::remove(output_path.c_str()); // remove the file if it exists

    auto builder = c2pa::Builder(manifest);

    // Add multiple actions to the builder
    string action_json_1 = R"({
        "action": "c2pa.color_adjustments",
        "parameters": {
            "name": "brightnesscontrast"
        }
    })";
    builder.add_action(action_json_1);
    string action_json_2 = R"({
        "action": "c2pa.filtered",
        "parameters": {
            "name": "A filter"
        },
        "description": "Filtering applied"
    })";
    builder.add_action(action_json_2);

    // Sign with the added actions
    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path, signer));

    // Read to verify
    auto reader = c2pa::Reader(output_path);
    string json_result;
    ASSERT_NO_THROW(json_result = reader.json());
    ASSERT_TRUE(std::filesystem::exists(output_path));

    // Parse the JSON result for structured validation
    json manifest_json;
    ASSERT_NO_THROW(manifest_json = json::parse(json_result));

    // Get the active manifest
    ASSERT_TRUE(manifest_json.contains("manifests"));
    ASSERT_TRUE(manifest_json.contains("active_manifest"));
    string active_manifest_label = manifest_json["active_manifest"];
    ASSERT_TRUE(manifest_json["manifests"].contains(active_manifest_label));
    json active_manifest = manifest_json["manifests"][active_manifest_label];
    ASSERT_TRUE(active_manifest.contains("assertions"));
    ASSERT_TRUE(active_manifest["assertions"].is_array());

    // Find the c2pa.actions assertion
    json actions_assertion;
    bool found_actions_assertion = false;
    for (const auto& assertion : active_manifest["assertions"]) {
        if (assertion.contains("label") && assertion["label"] == "c2pa.actions.v2") {
            actions_assertion = assertion;
            found_actions_assertion = true;
            break;
        }
    }
    ASSERT_TRUE(found_actions_assertion);

    // Verify the actions assertion structure
    ASSERT_TRUE(actions_assertion.contains("data"));
    ASSERT_TRUE(actions_assertion["data"].contains("actions"));
    ASSERT_TRUE(actions_assertion["data"]["actions"].is_array());

    // Verify both actions we added are in the actions array
    json actions_array = actions_assertion["data"]["actions"];
    ASSERT_FALSE(actions_array.empty());
    ASSERT_GE(actions_array.size(), 2);

    // Find our added actions...
    json color_adjustments_action;
    json filtered_action;
    bool found_color_adjustments = false;
    bool found_filtered = false;

    for (const auto& action : actions_array) {
        if (action.contains("action")) {
            if (action["action"] == "c2pa.color_adjustments") {
                color_adjustments_action = action;
                found_color_adjustments = true;
            } else if (action["action"] == "c2pa.filtered") {
                filtered_action = action;
                found_filtered = true;
            }
        }
    }

    ASSERT_TRUE(found_color_adjustments);
    ASSERT_TRUE(found_filtered);

    // Verify the color_adjustments action structure
    ASSERT_TRUE(color_adjustments_action.contains("action"));
    ASSERT_EQ(color_adjustments_action["action"], "c2pa.color_adjustments");
    ASSERT_TRUE(color_adjustments_action.contains("parameters"));
    ASSERT_TRUE(color_adjustments_action["parameters"].contains("name"));
    ASSERT_EQ(color_adjustments_action["parameters"]["name"], "brightnesscontrast");

    // Verify the filtered action structure
    ASSERT_TRUE(filtered_action.contains("action"));
    ASSERT_EQ(filtered_action["action"], "c2pa.filtered");
    ASSERT_TRUE(filtered_action.contains("parameters"));
    ASSERT_TRUE(filtered_action["parameters"].contains("name"));
    ASSERT_EQ(filtered_action["parameters"]["name"], "A filter");
    ASSERT_TRUE(filtered_action.contains("description"));
    ASSERT_EQ(filtered_action["description"], "Filtering applied");
};

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

TEST(Builder, SignImageFileWithoutThumbnailAutoGenerationThroughThreadLocalSharedSettings)
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

TEST(Builder, SignImageFileWithoutThumbnailAutoGenerationThroughSettings)
{
    // TODO-TMN Write similar to SignImageFileWithoutThumbnailAutoGenerationThroughThreadLocalSharedSettings, but use the context API
    // TODO: Make sure another builder without the context behaves diffrently, aka by default the thumbnails are on and verify settings doesn't propagate to other one
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

TEST(Builder, SignImageStreamWithoutContext)
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

TEST(Builder, SignImageStreamWithContext)
{
  // TODO-TMN
}

TEST(Builder, SignImageStreamWithTrustSettings)
{
  // TODO-TMN
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
        printf("READER OK \n");
        auto json = reader.json();
        printf("JSON \n");
        printf("%s\n", json.c_str());
    }
    catch (c2pa::C2paException const &e)
    {
        printf("ERROR \n");
        printf("%s\n", e.what());
        std::string error_message = e.what();
        if (error_message.rfind("Other: could not fetch the remote manifest", 0) == 0)
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

TEST(Builder, SignWithoutTimestamping)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";

    auto manifest = read_text_file(manifest_path);
    auto certs = read_text_file(certs_path);
    auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // Create a signer without tsa uri
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key);

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
    string json_result;
    ASSERT_NO_THROW(json_result = reader.json());

    // Parse the JSON result for validation
    json result_json = json::parse(json_result);

    // Get the active manifest signature info
    ASSERT_TRUE(result_json.contains("active_manifest"));
    ASSERT_TRUE(result_json.contains("manifests"));
    string active_manifest_label = result_json["active_manifest"];
    ASSERT_TRUE(result_json["manifests"].contains(active_manifest_label));
    json active_manifest = result_json["manifests"][active_manifest_label];
    ASSERT_TRUE(active_manifest.contains("signature_info"));
    ASSERT_TRUE(active_manifest["signature_info"].is_object());

    // Expect no signature timestamp when tsa uri is not provided
    ASSERT_FALSE(active_manifest["signature_info"].contains("time"));
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

    // Parse ingredient JSON and extract the identifier
    json ingredient_json = json::parse(result);
    std::string identifier = ingredient_json["thumbnail"]["identifier"];

    // Create the builder using a manifest JSON
    auto manifest = read_text_file(manifest_path);

    // Parse the manifest and add ingredients array
    json manifest_json = json::parse(manifest);
    manifest_json["ingredients"] = json::array({ingredient_json});

    auto builder = c2pa::Builder(manifest_json.dump());

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

TEST(Builder, LinkIngredientsAndSign)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";

    // Construct the path to the test fixture
    fs::path ingredient_source_path = current_dir / "../tests/fixtures/A.jpg";
    std::string ingredient_source_path_str = ingredient_source_path.string();

    fs::path temp_dir = current_dir / "../build/ingredient_linked_resource_temp_dir";

    // Remove and recreate the build/ingredient_as_resource_temp_dir folder before using it
    // This is technically a clean-up in-between tests
    if (fs::exists(temp_dir)) {
        fs::remove_all(temp_dir);
    }
    fs::create_directories(temp_dir);

    // Create the builder using a manifest JSON
    auto manifest = read_text_file(manifest_path);

    // Parse the manifest and modify it
    json manifest_json = json::parse(manifest);

    // Find the c2pa.actions assertion and add ingredientIds to c2pa.created action
    // The values in ingredientIds are going to be used to link ingredients to actions
    // Those values should be valid UUIDs
    for (auto& assertion : manifest_json["assertions"]) {
        if (assertion["label"] == "c2pa.actions") {
            for (auto& action : assertion["data"]["actions"]) {
                if (action["action"] == "c2pa.created") {
                    action["parameters"]["ingredientIds"] = json::array({"test:iid:939a4c48-0dff-44ec-8f95-61f52b11618f"});
                    break;
                }
            }
            break;
        }
    }

    auto builder = c2pa::Builder(manifest_json.dump());

    // add an ingredient
    // instance_id, if found in an action's ingredientIds array,
    // will be used to link the ingredient to the action
    json ingredient_obj = {
        {"title", "Test Ingredient"},
        {"relationship", "parentOf"},
        {"instance_id", "test:iid:939a4c48-0dff-44ec-8f95-61f52b11618f"}
    };
    string ingredient_json = ingredient_obj.dump();
    builder.add_ingredient(ingredient_json, ingredient_source_path);

    // Create a signer
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    auto certs = read_text_file(certs_path);
    auto p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key");
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../build/example/signed_with_ingredient_and_resource.jpg";

    std::vector<unsigned char> manifest_data;
    manifest_data = builder.sign(signed_image_path, output_path, signer);

    auto reader = c2pa::Reader(output_path);
    string json_result;
    ASSERT_NO_THROW(json_result = reader.json());

    // Parse the JSON result for validation
    json result_json = json::parse(json_result);

    // Get the active manifest
    ASSERT_TRUE(result_json.contains("active_manifest"));
    ASSERT_TRUE(result_json.contains("manifests"));
    string active_manifest_label = result_json["active_manifest"];
    ASSERT_TRUE(result_json["manifests"].contains(active_manifest_label));
    json active_manifest = result_json["manifests"][active_manifest_label];

    // Verify ingredients array has exactly one ingredient with label "c2pa.ingredient.v3"
    ASSERT_TRUE(active_manifest.contains("ingredients"));
    ASSERT_TRUE(active_manifest["ingredients"].is_array());
    ASSERT_EQ(active_manifest["ingredients"].size(), 1);
    ASSERT_TRUE(active_manifest["ingredients"][0].contains("label"));
    ASSERT_EQ(active_manifest["ingredients"][0]["label"], "c2pa.ingredient.v3");

    // Find the c2pa.actions assertion
    ASSERT_TRUE(active_manifest.contains("assertions"));
    ASSERT_TRUE(active_manifest["assertions"].is_array());

    json actions_assertion;
    bool found_actions = false;
    for (const auto& assertion : active_manifest["assertions"]) {
        if (assertion.contains("label") && assertion["label"] == "c2pa.actions.v2") {
            actions_assertion = assertion;
            found_actions = true;
            break;
        }
    }
    ASSERT_TRUE(found_actions);

    // Find the c2pa.created action
    ASSERT_TRUE(actions_assertion.contains("data"));
    ASSERT_TRUE(actions_assertion["data"].contains("actions"));
    ASSERT_TRUE(actions_assertion["data"]["actions"].is_array());

    json created_action;
    bool found_created = false;
    for (const auto& action : actions_assertion["data"]["actions"]) {
        if (action.contains("action") && action["action"] == "c2pa.created") {
            created_action = action;
            found_created = true;
            break;
        }
    }
    ASSERT_TRUE(found_created);

    // Verify the c2pa.created action has an ingredients array with exactly one object
    // that has url: "self#jumbf=c2pa.assertions/c2pa.ingredient.v3"
    ASSERT_TRUE(created_action.contains("parameters"));
    ASSERT_TRUE(created_action["parameters"].contains("ingredients"));
    ASSERT_TRUE(created_action["parameters"]["ingredients"].is_array());
    ASSERT_EQ(created_action["parameters"]["ingredients"].size(), 1);
    ASSERT_TRUE(created_action["parameters"]["ingredients"][0].contains("url"));
    ASSERT_EQ(created_action["parameters"]["ingredients"][0]["url"], "self#jumbf=c2pa.assertions/c2pa.ingredient.v3");
}

TEST(Builder, AddIngredientToBuilderUsingBasePath)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";

    // Construct the path to the test fixture
    fs::path ingredient_source_path = current_dir / "../tests/fixtures/A.jpg";
    std::string ingredient_source_path_str = ingredient_source_path.string();

    // Use temp dir for ingredient data (data dir)
    fs::path temp_dir = current_dir / "../build/base_ingredient_as_resource_temp_dir";

    // Remove and recreate the temp data dir folder before using it
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

TEST(Builder, AddIngredientToBuilderUsingBasePathWithManifestContainingPlacedActionThroughThreadLocalSharedSettings)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the path to the test fixture
    fs::path ingredient_source_path = current_dir / "../tests/fixtures/A.jpg";
    std::string ingredient_source_path_str = ingredient_source_path.string();

    // Use temp dir for ingredient data
    fs::path temp_dir = current_dir / "../build/ingredient_placed_as_resource_temp_dir";

    // set settings to not auto-add a placed action
    c2pa::load_settings("{\"builder\": { \"actions\": {\"auto_placed_action\": {\"enabled\": false}}}}", "json");

    // Remove and recreate the temp data dir folder before using it
    // This is technically a clean-up in-between tests
    if (fs::exists(temp_dir)) {
        fs::remove_all(temp_dir);
    }
    fs::create_directories(temp_dir);

    // Get the needed JSON for the ingredient
    std::string result;
    result = c2pa::read_ingredient_file(ingredient_source_path, temp_dir);

    // Parse ingredient JSON and extract the instance_id
    json ingredient_json = json::parse(result);
    std::string instance_id = ingredient_json["instance_id"];

    // Create the manifest JSON structure with the placed action
    json manifest_json = {
        {"vendor", "a-vendor"},
        {"claim_generator_info", json::array({
            {
                {"name", "c2pa-c test"},
                {"version", "1.0.0"}
            }
        })},
        {"assertions", json::array({
            {
                {"label", "c2pa.actions"},
                {"data", {
                    {"actions", json::array({
                        {
                            {"action", "c2pa.created"},
                            {"description", "Created a new file or content"},
                            {"parameters", {
                                {"com.vendor.tool", "new"}
                            }},
                            {"digitalSourceType", "http://cv.iptc.org/newscodes/digitalsourcetype/digitalCreation"}
                        },
                        {
                            {"action", "c2pa.placed"},
                            {"description", "Added pre-existing content to this file"},
                            {"parameters", {
                                {"com.vendor.tool", "place_embedded_object"},
                                {"ingredientIds", json::array({instance_id})}
                            }}
                        }
                    })},
                    {"metadata", {
                        {"dateTime", "2025-09-25T20:59:48.262Z"}
                    }}
                }}
            }
        })},
        {"ingredients", json::array({ingredient_json})}
    };

    // Now we can create a Builder with the manifest
    auto builder = c2pa::Builder(manifest_json.dump());

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

TEST(Builder, AddIngredientToBuilderUsingBasePathWithManifestContainingPlacedAction)
{
    // TODO-TMN: Write similar as AddIngredientToBuilderUsingBasePathWithManifestContainingPlacedActionThroughThreadLocalSharedSettings, but with context API
}

TEST(Builder, AddIngredientWithProvenanceDataToBuilderUsingBasePath)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";

    // Construct the path to the test fixture
    fs::path ingredient_source_path = current_dir / "../tests/fixtures/C.jpg";
    std::string ingredient_source_path_str = ingredient_source_path.string();

    // Use temp data dir
    fs::path temp_dir = current_dir / "../build/ingredient_with_prevenance_as_resource_temp_dir";

    // Remove and recreate the temp data dir folder before using it
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
