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
#include <thread>

#include "test_utils.hpp"

using namespace std;
namespace fs = std::filesystem;
using nlohmann::json;

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
    auto manifest = c2pa_test::read_text_file(manifest_path);
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
    fs::path output_path = current_dir / "../build/examples/image_with_one_action.jpg";

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

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
    fs::path output_path = current_dir / "../build/examples/image_with_multiple_actions.jpg";

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

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
    fs::path output_path = current_dir / "../build/examples/training_image_only.jpg";

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

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

TEST(Builder, SignImageFileNoThumbnailAutoGenThreadLocalSettings)
{
    // Run in separate thread for complete test isolation (thread-local settings won't leak)
    std::thread test_thread([]() {
        fs::path current_dir = fs::path(__FILE__).parent_path();

        // Construct the paths relative to the current directory
        fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
        fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
        fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
        fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
        fs::path output_path = current_dir / "../build/examples/training_image_only.jpg";

        auto manifest = c2pa_test::read_text_file(manifest_path);
        auto certs = c2pa_test::read_text_file(certs_path);
        auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

        // set settings to not generate thumbnails (thread-local)
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

        // No need to reset settings - thread-local settings destroyed with thread
    });

    // Wait for thread to complete
    test_thread.join();
};

TEST(Builder, SignImageFileNoThumbnailAutoGen)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path_with_context = current_dir / "../build/examples/settings_no_thumbnails.jpg";
    fs::path output_path_no_context = current_dir / "../build/examples/settings_with_thumbnails.jpg";

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // Create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::filesystem::remove(output_path_with_context.c_str());
    std::filesystem::remove(output_path_no_context.c_str());

    // Test 1: Create context with specific settings via JSON
    auto context = c2pa::Context::from_json("{\"builder\": { \"thumbnail\": {\"enabled\": false}}}");

    // Create builder using context containing settings
    auto builder_with_context = c2pa::Builder(context, manifest);
    std::vector<unsigned char> manifest_data_context;
    ASSERT_NO_THROW(manifest_data_context = builder_with_context.sign(signed_image_path, output_path_with_context, signer));

    // Verify the signed file with context exists and is readable
    ASSERT_TRUE(std::filesystem::exists(output_path_with_context));
    auto reader_with_context = c2pa::Reader(output_path_with_context);
    std::string json_with_context;
    ASSERT_NO_THROW(json_with_context = reader_with_context.json());
    EXPECT_FALSE(json_with_context.empty());

    // Test 2: Create builder WITHOUT context (uses default behavior)
    auto builder_no_context = c2pa::Builder(manifest);
    std::vector<unsigned char> manifest_data_no_context;
    ASSERT_NO_THROW(manifest_data_no_context = builder_no_context.sign(signed_image_path, output_path_no_context, signer));

    // Verify the signed file without context exists and is readable
    ASSERT_TRUE(std::filesystem::exists(output_path_no_context));
    auto reader_no_context = c2pa::Reader(output_path_no_context);
    std::string json_no_context;
    ASSERT_NO_THROW(json_no_context = reader_no_context.json());
    EXPECT_FALSE(json_no_context.empty());

    // Both builders should successfully create valid manifests,
    // but one should have thumbnails whereas the other shouldn't
    json parsed_context = json::parse(json_with_context);
    json parsed_no_context = json::parse(json_no_context);

    // Both should have valid structure
    EXPECT_TRUE(parsed_context.contains("active_manifest"));
    EXPECT_TRUE(parsed_no_context.contains("active_manifest"));

    std::string active_manifest_value_context = parsed_context["active_manifest"];
    EXPECT_FALSE(parsed_context["manifests"][active_manifest_value_context].contains("thumbnail"));
    std::string active_manifest_value_no_context = parsed_no_context["active_manifest"];
    EXPECT_TRUE(parsed_no_context["manifests"][active_manifest_value_no_context].contains("thumbnail"));
};

TEST(Builder, SignImageThumbnailSettingsFileToml)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path_with_context = current_dir / "../build/examples/image_context_settings_toml.jpg";
    fs::path output_path_no_context = current_dir / "../build/examples/image_no_context_toml.jpg";

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // Create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::filesystem::remove(output_path_with_context.c_str());
    std::filesystem::remove(output_path_no_context.c_str());

    // Create context with specific settings via toml, by loading the TOML file
    fs::path settings_path = current_dir / "../tests/fixtures/settings/test_settings_no_thumbnail.toml";
    auto settings_toml = c2pa_test::read_text_file(settings_path);
    auto context = c2pa::Context::from_toml(settings_toml);

    // Create builder using context containing settings (does not generate thumbnails)
    auto builder_no_thumbnail = c2pa::Builder(context, manifest);
    std::vector<unsigned char> manifest_data_no_thumbnail;
    ASSERT_NO_THROW(manifest_data_no_thumbnail = builder_no_thumbnail.sign(signed_image_path, output_path_with_context, signer));

    // Verify the signed file with context exists and is readable
    ASSERT_TRUE(std::filesystem::exists(output_path_with_context));
    auto reader_without_thumbnails = c2pa::Reader(output_path_with_context);
    std::string json_without_thumbnails;
    ASSERT_NO_THROW(json_without_thumbnails = reader_without_thumbnails.json());
    EXPECT_FALSE(json_without_thumbnails.empty());

    // Now, create builder with another context (settings generate a thumbnail)
    fs::path settings_path2 = current_dir / "../tests/fixtures/settings/test_settings_with_thumbnail.toml";
    auto settings_toml2 = c2pa_test::read_text_file(settings_path2);
    auto context2 = c2pa::Context::from_toml(settings_toml2);

    auto builder_with_thumbnail = c2pa::Builder(context2, manifest);
    std::vector<unsigned char> manifest_data_with_thumbnail;
    ASSERT_NO_THROW(manifest_data_with_thumbnail = builder_with_thumbnail.sign(signed_image_path, output_path_no_context, signer));

    // Verify the signed file without context exists and is readable
    ASSERT_TRUE(std::filesystem::exists(output_path_no_context));
    auto reader_with_thumbnails = c2pa::Reader(output_path_no_context);
    std::string json_with_thumbnails;
    ASSERT_NO_THROW(json_with_thumbnails = reader_with_thumbnails.json());
    EXPECT_FALSE(json_with_thumbnails.empty());

    // Both builders should successfully create valid manifests,
    // but one should have thumbnails whereas the other shouldn't
    json parsed_no_thumbnail = json::parse(json_without_thumbnails);
    json parsed_with_thumbnail = json::parse(json_with_thumbnails);

    // Both should have valid structure
    EXPECT_TRUE(parsed_no_thumbnail.contains("active_manifest"));
    EXPECT_TRUE(parsed_with_thumbnail.contains("active_manifest"));

    std::string active_manifest_value_context = parsed_no_thumbnail["active_manifest"];
    EXPECT_FALSE(parsed_no_thumbnail["manifests"][active_manifest_value_context].contains("thumbnail"));
    std::string active_manifest_value_no_context = parsed_with_thumbnail["active_manifest"];
    EXPECT_TRUE(parsed_with_thumbnail["manifests"][active_manifest_value_no_context].contains("thumbnail"));
};

TEST(Builder, SignImageThumbnailSettingsFileJson)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path_with_context = current_dir / "../build/examples/image_context_settings_json.jpg";
    fs::path output_path_no_context = current_dir / "../build/examples/image_no_context_json.jpg";

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // Create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::filesystem::remove(output_path_with_context.c_str());
    std::filesystem::remove(output_path_no_context.c_str());

    // Create context with specific settings via JSON, by loading the JSON file with the settings
    fs::path settings_path = current_dir / "../tests/fixtures/settings/test_settings_no_thumbnail.json";
    auto settings_json = c2pa_test::read_text_file(settings_path);
    auto context = c2pa::Context::from_json(settings_json);

    // Create builder using context containing settings (does not generate thumbnails)
    auto builder_no_thumbnail = c2pa::Builder(context, manifest);
    std::vector<unsigned char> manifest_data_no_thumbnail;
    ASSERT_NO_THROW(manifest_data_no_thumbnail = builder_no_thumbnail.sign(signed_image_path, output_path_with_context, signer));

    // Verify the signed file with context exists and is readable
    ASSERT_TRUE(std::filesystem::exists(output_path_with_context));
    auto reader_without_thumbnails = c2pa::Reader(output_path_with_context);
    std::string json_without_thumbnails;
    ASSERT_NO_THROW(json_without_thumbnails = reader_without_thumbnails.json());
    EXPECT_FALSE(json_without_thumbnails.empty());

    // Now, create builder with another context (settings generate a thumbnail)
    fs::path settings_path2 = current_dir / "../tests/fixtures/settings/test_settings_with_thumbnail.json";
    auto settings_json2 = c2pa_test::read_text_file(settings_path2);
    auto context2 = c2pa::Context::from_json(settings_json2);

    auto builder_with_thumbnail = c2pa::Builder(context2, manifest);
    std::vector<unsigned char> manifest_data_with_thumbnail;
    ASSERT_NO_THROW(manifest_data_with_thumbnail = builder_with_thumbnail.sign(signed_image_path, output_path_no_context, signer));

    // Verify the signed file without context exists and is readable
    ASSERT_TRUE(std::filesystem::exists(output_path_no_context));
    auto reader_with_thumbnails = c2pa::Reader(output_path_no_context);
    std::string json_with_thumbnails;
    ASSERT_NO_THROW(json_with_thumbnails = reader_with_thumbnails.json());
    EXPECT_FALSE(json_with_thumbnails.empty());

    // Both builders should successfully create valid manifests,
    // but one should have thumbnails whereas the other shouldn't
    json parsed_no_thumbnail = json::parse(json_without_thumbnails);
    json parsed_with_thumbnail = json::parse(json_with_thumbnails);

    // Both should have valid structure
    EXPECT_TRUE(parsed_no_thumbnail.contains("active_manifest"));
    EXPECT_TRUE(parsed_with_thumbnail.contains("active_manifest"));

    std::string active_manifest_value_context = parsed_no_thumbnail["active_manifest"];
    EXPECT_FALSE(parsed_no_thumbnail["manifests"][active_manifest_value_context].contains("thumbnail"));
    std::string active_manifest_value_no_context = parsed_with_thumbnail["active_manifest"];
    EXPECT_TRUE(parsed_with_thumbnail["manifests"][active_manifest_value_no_context].contains("thumbnail"));
};

TEST(Builder, SignImageThumbnailSettingsObject)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path_no_thumbnail = current_dir / "../build/examples/image_no_thumbnail_incremental.jpg";

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // Create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::filesystem::remove(output_path_no_thumbnail);

    // Here we are going to build settings, field by field
    // Start with default settings
    // If setting the same value multiple times, last one wins
    c2pa::Settings settings;
    settings
        .set("builder.thumbnail.enabled", "true")
        .set("builder.thumbnail.enabled", "false");

    // Build context from Settings object we just did
    auto context = c2pa::Context::ContextBuilder()
        .with_settings(settings)
        .create_context();

    auto builder = c2pa::Builder(context, manifest);
    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path_no_thumbnail, signer));

    // Verify the signed file exists and is readable
    ASSERT_TRUE(std::filesystem::exists(output_path_no_thumbnail));
    auto reader = c2pa::Reader(output_path_no_thumbnail);
    std::string json_result;
    ASSERT_NO_THROW(json_result = reader.json());
    EXPECT_FALSE(json_result.empty());

    // Verify NO thumbnail is present
    auto parsed = json::parse(json_result);
    EXPECT_TRUE(parsed.contains("active_manifest"));
    std::string active_manifest = parsed["active_manifest"];
    EXPECT_FALSE(parsed["manifests"][active_manifest].contains("thumbnail"));
}

TEST(Builder, SignImageThumbnailSettingsIncrementalObject)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path_no_thumbnail = current_dir / "../build/examples/image_no_thumbnail_incremental.jpg";

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // Create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::filesystem::remove(output_path_no_thumbnail);

    // Here we are going to update a settings object we just build
    c2pa::Settings settings;
    settings.set("builder.thumbnail.enabled", "true");
    std::string updated_config = R"({
        "builder": {
            "thumbnail": {
                "enabled": false
            }
        }
    })";
    settings.update(updated_config, c2pa::ConfigFormat::JSON);

    // Build context from Settings object we just did
    auto context = c2pa::Context::ContextBuilder()
        .with_settings(settings)
        .create_context();

    auto builder = c2pa::Builder(context, manifest);
    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path_no_thumbnail, signer));

    // Verify the signed file exists and is readable
    ASSERT_TRUE(std::filesystem::exists(output_path_no_thumbnail));
    auto reader = c2pa::Reader(output_path_no_thumbnail);
    std::string json_result;
    ASSERT_NO_THROW(json_result = reader.json());
    EXPECT_FALSE(json_result.empty());

    // Verify NO thumbnail is present
    auto parsed = json::parse(json_result);
    EXPECT_TRUE(parsed.contains("active_manifest"));
    std::string active_manifest = parsed["active_manifest"];
    EXPECT_FALSE(parsed["manifests"][active_manifest].contains("thumbnail"));
}

TEST(Builder, SignImageFileWithResource)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../build/examples/training_resource_only.jpg";

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

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
    fs::path output_path = current_dir / "../build/examples/multiple_resources.jpg";

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

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
    fs::path output_path = current_dir / "../build/examples/training_ingredient_only.jpg";

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

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
    fs::path output_path = current_dir / "../build/examples/training.jpg";

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

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
    fs::path output_path = current_dir / "../build/examples/video1_signed.mp4";

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

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
    fs::path output_path = current_dir / "../build/examples/video1_signed_with_ingredients_and_resources.mp4";

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

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
    fs::path output_path = current_dir / "../build/examples/video1_signed.mp4";

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

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

  fs::path output_path = current_dir / "../build/examples" / SimplePathSignTest::GetParam();
  std::filesystem::remove(output_path.c_str()); // remove the file if it exists

  auto manifest = c2pa_test::read_text_file(manifest_path);
  auto certs = c2pa_test::read_text_file(certs_path);
  auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

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

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

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

TEST(Builder, SignImageStream)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // Create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    // Create a default context (no custom settings)
    auto context = c2pa::Context::create();

    // Create builder with context
    auto builder = c2pa::Builder(context, manifest);

    std::ifstream source(signed_image_path, std::ios::binary);
    if (!source)
    {
        FAIL() << "Failed to open file: " << signed_image_path << std::endl;
    }

    // Create a memory buffer for the destination stream
    std::stringstream memory_buffer(std::ios::in | std::ios::out | std::ios::binary);
    std::iostream &dest = memory_buffer;

    // Sign using stream APIs
    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign("image/jpeg", source, dest, signer));
    source.close();

    // Rewind dest to the start for reading
    dest.flush();
    dest.seekp(0, std::ios::beg);
    dest.seekg(0, std::ios::beg);

    // Create reader with same context to read from stream
    auto reader = c2pa::Reader(context, "image/jpeg", dest);
    std::string json;
    ASSERT_NO_THROW(json = reader.json());

    // Verify the manifest contains expected data
    ASSERT_TRUE(json.find("cawg.training-mining") != std::string::npos);
    ASSERT_FALSE(manifest_data.empty());
}

TEST(Builder, SignImageStreamBuilderReaderDifferentContext)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // Create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    // Create a default context (no custom settings, defaults to SDK default settings)
    auto write_context = c2pa::Context::create();

    // Create builder with context
    auto builder = c2pa::Builder(write_context, manifest);

    std::ifstream source(signed_image_path, std::ios::binary);
    if (!source)
    {
        FAIL() << "Failed to open file: " << signed_image_path << std::endl;
    }

    // Create a memory buffer for the destination stream
    std::stringstream memory_buffer(std::ios::in | std::ios::out | std::ios::binary);
    std::iostream &dest = memory_buffer;

    // Sign using stream APIs
    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign("image/jpeg", source, dest, signer));
    source.close();

    // Rewind dest to the start for reading
    dest.flush();
    dest.seekp(0, std::ios::beg);
    dest.seekg(0, std::ios::beg);

    // Create a reader. The Reader can have a different context than the Builder,
    // which could enable different settings for reading vs writing!
    // (Here for this test we use a demo empty context again)

    // Create a default context (no custom settings)
    auto read_context = c2pa::Context::create();

    auto reader = c2pa::Reader(read_context, "image/jpeg", dest);
    std::string json;
    ASSERT_NO_THROW(json = reader.json());

    // Verify the manifest contains expected data
    ASSERT_TRUE(json.find("cawg.training-mining") != std::string::npos);
    ASSERT_FALSE(manifest_data.empty());
}

TEST(Builder, SignImageWithIngredientHavingManifestStream)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path ingredient_image_path = current_dir / "../tests/fixtures/C.jpg";

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

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

        auto manifest = c2pa_test::read_text_file(manifest_path);
        auto certs = c2pa_test::read_text_file(certs_path);
        auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

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
        auto json = reader.json();
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

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

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

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

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

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

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

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

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
    auto manifest = c2pa_test::read_text_file(manifest_path);

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
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::vector<unsigned char> manifest_data;
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../build/examples/signed_with_ingredient_and_resource.jpg";
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
    auto manifest = c2pa_test::read_text_file(manifest_path);

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
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../build/examples/signed_with_ingredient_and_resource.jpg";

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
    auto manifest = c2pa_test::read_text_file(manifest_path);

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
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::vector<unsigned char> manifest_data;
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../build/examples/signed_with_ingredient_and_resource.jpg";
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path, signer));

    auto reader = c2pa::Reader(output_path);
    ASSERT_NO_THROW(reader.json());
}

TEST(Builder, AddIngredientToBuilderUsingBasePathPlacedActionThreadLocalSettings)
{
    // Run in separate thread for complete test isolation (thread-local settings won't leak)
    std::thread test_thread([]() {
        fs::path current_dir = fs::path(__FILE__).parent_path();

        // Construct the path to the test fixture
        fs::path ingredient_source_path = current_dir / "../tests/fixtures/A.jpg";
        std::string ingredient_source_path_str = ingredient_source_path.string();

        // Use temp dir for ingredient data
        fs::path temp_dir = current_dir / "../build/ingredient_placed_as_resource_temp_dir";

        // set settings to not auto-add a placed action (thread-local)
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
        auto certs = c2pa_test::read_text_file(certs_path);
        auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");
        c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

        std::vector<unsigned char> manifest_data;
        fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
        fs::path output_path = current_dir / "../build/examples/signed_with_ingredient_and_resource.jpg";
        ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path, signer));

        auto reader = c2pa::Reader(output_path);
        ASSERT_NO_THROW(reader.json());

        // No need to reset settings - thread-local settings destroyed with thread
    });

    // Wait for thread to complete
    test_thread.join();
}

TEST(Builder, AddIngredientToBuilderUsingBasePathWithManifestContainingPlacedAction)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the path to the test fixture
    fs::path ingredient_source_path = current_dir / "../tests/fixtures/A.jpg";
    std::string ingredient_source_path_str = ingredient_source_path.string();

    // Use temp dir for ingredient data
    fs::path temp_dir = current_dir / "../build/ingredient_placed_context_temp_dir";

    // Remove and recreate the temp data dir folder before using it
    if (fs::exists(temp_dir)) {
        fs::remove_all(temp_dir);
    }
    fs::create_directories(temp_dir);

    // Create context with auto_placed_action disabled via JSON settings
    auto context = c2pa::Context::from_json("{\"builder\": { \"actions\": {\"auto_placed_action\": {\"enabled\": false}}}}");

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
                {"name", "c2pa-c test with context"},
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

    // Create a Builder with the context and manifest
    auto builder = c2pa::Builder(context, manifest_json.dump());

    // Set base path so builder can load ingredient resources
    builder.set_base_path(temp_dir.string());

    // Create a signer
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    // Sign the image
    std::vector<unsigned char> manifest_data;
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../build/examples/signed_with_ingredient_context.jpg";
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path, signer));

    // Read and verify
    auto reader = c2pa::Reader(output_path);
    std::string reader_json;
    ASSERT_NO_THROW(reader_json = reader.json());

    // Verify the placed action is in the manifest
    ASSERT_TRUE(reader_json.find("c2pa.placed") != std::string::npos);
    ASSERT_TRUE(reader_json.find(instance_id) != std::string::npos);
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
    auto manifest = c2pa_test::read_text_file(manifest_path);

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
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::vector<unsigned char> manifest_data;
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../build/examples/signed_with_ingredient_and_resource.jpg";
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path, signer));

    auto reader = c2pa::Reader(output_path);
    ASSERT_NO_THROW(reader.json());
}

TEST(Builder, MultipleBuildersDifferentThumbnailSettingsInterleaved)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // All the test fixtures
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path_no_thumbnail = current_dir / "../build/examples/no_thumbnail_interleaved_1.jpg";
    fs::path output_path_with_thumbnails = current_dir / "../build/examples/with_thumbnails_interleaved_1.jpg";
    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // Create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::filesystem::remove(output_path_no_thumbnail.c_str());
    std::filesystem::remove(output_path_with_thumbnails.c_str());

    // Create one context with specific settings via JSON, by loading the JSON file with the settings
    fs::path settings_path = current_dir / "../tests/fixtures/settings/test_settings_no_thumbnail.json";
    auto settings_json = c2pa_test::read_text_file(settings_path);
    auto context_without_thumbnails = c2pa::Context::from_json(settings_json);

    // Now, create anothetrcontext, that sets thumbnails to be generated
    fs::path settings_path2 = current_dir / "../tests/fixtures/settings/test_settings_with_thumbnail.json";
    auto settings_json2 = c2pa_test::read_text_file(settings_path2);
    auto context_with_thumbnails = c2pa::Context::from_json(settings_json2);

    // Create builder using context containing settings that does not generate thumbnails
    auto builder_no_thumbnail = c2pa::Builder(context_without_thumbnails, manifest);
    std::vector<unsigned char> manifest_data_no_thumbnail;
    ASSERT_NO_THROW(manifest_data_no_thumbnail = builder_no_thumbnail.sign(signed_image_path, output_path_no_thumbnail, signer));

    // Create builder using context containing settings that does generate thumbnails
    auto builder_with_thumbnail = c2pa::Builder(context_with_thumbnails, manifest);
    std::vector<unsigned char> manifest_data_with_thumbnail;
    ASSERT_NO_THROW(manifest_data_with_thumbnail = builder_with_thumbnail.sign(signed_image_path, output_path_with_thumbnails, signer));

    // Verify the signed file with context exists and is readable
    ASSERT_TRUE(std::filesystem::exists(output_path_no_thumbnail));
    auto reader_without_thumbnails = c2pa::Reader(output_path_no_thumbnail);
    std::string json_without_thumbnails;
    ASSERT_NO_THROW(json_without_thumbnails = reader_without_thumbnails.json());
    EXPECT_FALSE(json_without_thumbnails.empty());

    // Verify the signed file without context exists and is readable
    ASSERT_TRUE(std::filesystem::exists(output_path_with_thumbnails));
    auto reader_with_thumbnails = c2pa::Reader(output_path_with_thumbnails);
    std::string json_with_thumbnails;
    ASSERT_NO_THROW(json_with_thumbnails = reader_with_thumbnails.json());
    EXPECT_FALSE(json_with_thumbnails.empty());

    // Both builders should successfully create valid manifests,
    // but one should have thumbnails whereas the other shouldn't,
    // and we can even have different settings per Builder, as they trickle down!
    json parsed_no_thumbnail = json::parse(json_without_thumbnails);
    json parsed_with_thumbnail = json::parse(json_with_thumbnails);

    // Both should have valid structure
    EXPECT_TRUE(parsed_no_thumbnail.contains("active_manifest"));
    EXPECT_TRUE(parsed_with_thumbnail.contains("active_manifest"));

    std::string active_manifest_value_context = parsed_no_thumbnail["active_manifest"];
    EXPECT_FALSE(parsed_no_thumbnail["manifests"][active_manifest_value_context].contains("thumbnail"));
    std::string active_manifest_value_no_context = parsed_with_thumbnail["active_manifest"];
    EXPECT_TRUE(parsed_with_thumbnail["manifests"][active_manifest_value_no_context].contains("thumbnail"));
};

TEST(Builder, MultipleBuildersDifferentThumbnailSettingsInterleaved2)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // All the test fixtures
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path_no_thumbnail = current_dir / "../build/examples/no_thumbnail_interleaved_2.jpg";
    fs::path output_path_with_thumbnails = current_dir / "../build/examples/with_thumbnails_interleaved_2.jpg";
    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // Create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::filesystem::remove(output_path_no_thumbnail.c_str());
    std::filesystem::remove(output_path_with_thumbnails.c_str());

    // Create one context with specific settings via JSON, by loading the JSON file with the settings
    fs::path settings_path = current_dir / "../tests/fixtures/settings/test_settings_no_thumbnail.json";
    auto settings_json = c2pa_test::read_text_file(settings_path);
    auto context_without_thumbnails = c2pa::Context::from_json(settings_json);

    // Now, create another context, that sets thumbnails to be generated
    fs::path settings_path2 = current_dir / "../tests/fixtures/settings/test_settings_with_thumbnail.json";
    auto settings_json2 = c2pa_test::read_text_file(settings_path2);
    auto context_with_thumbnails = c2pa::Context::from_json(settings_json2);

    // Create builder using context containing settings that does generate thumbnails
    auto builder_with_thumbnail = c2pa::Builder(context_with_thumbnails, manifest);
    std::vector<unsigned char> manifest_data_with_thumbnail;
    ASSERT_NO_THROW(manifest_data_with_thumbnail = builder_with_thumbnail.sign(signed_image_path, output_path_with_thumbnails, signer));

    // Create builder using context containing settings that does not generate thumbnails
    auto builder_no_thumbnail = c2pa::Builder(context_without_thumbnails, manifest);
    std::vector<unsigned char> manifest_data_no_thumbnail;
    ASSERT_NO_THROW(manifest_data_no_thumbnail = builder_no_thumbnail.sign(signed_image_path, output_path_no_thumbnail, signer));

    // Verify the signed file with context exists and is readable
    ASSERT_TRUE(std::filesystem::exists(output_path_no_thumbnail));
    auto reader_without_thumbnails = c2pa::Reader(output_path_no_thumbnail);
    std::string json_without_thumbnails;
    ASSERT_NO_THROW(json_without_thumbnails = reader_without_thumbnails.json());
    EXPECT_FALSE(json_without_thumbnails.empty());

    // Verify the signed file without context exists and is readable
    ASSERT_TRUE(std::filesystem::exists(output_path_with_thumbnails));
    auto reader_with_thumbnails = c2pa::Reader(output_path_with_thumbnails);
    std::string json_with_thumbnails;
    ASSERT_NO_THROW(json_with_thumbnails = reader_with_thumbnails.json());
    EXPECT_FALSE(json_with_thumbnails.empty());

    // Both builders should successfully create valid manifests,
    // but one should have thumbnails whereas the other shouldn't,
    // and we can even have different settings per Builder, as they trickle down!
    json parsed_no_thumbnail = json::parse(json_without_thumbnails);
    json parsed_with_thumbnail = json::parse(json_with_thumbnails);

    // Both should have valid structure
    EXPECT_TRUE(parsed_no_thumbnail.contains("active_manifest"));
    EXPECT_TRUE(parsed_with_thumbnail.contains("active_manifest"));

    std::string active_manifest_value_context = parsed_no_thumbnail["active_manifest"];
    EXPECT_FALSE(parsed_no_thumbnail["manifests"][active_manifest_value_context].contains("thumbnail"));
    std::string active_manifest_value_no_context = parsed_with_thumbnail["active_manifest"];
    EXPECT_TRUE(parsed_with_thumbnail["manifests"][active_manifest_value_no_context].contains("thumbnail"));
};

TEST(Builder, TrustHandling)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // All the test fixtures
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../build/examples/trust_handling_test.jpg";
    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // Create our very own signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::filesystem::remove(output_path.c_str());

    // Trust is based on a chain of trusted certificates. When signing, we may need to know
    // if the ingredients are trusted at time of signing, so we benefit from having a context
    // already configured with that trust to use with our Builder and Reader.
    fs::path settings_path = current_dir / "../tests/fixtures/settings/test_settings_example.toml";
    auto settings = c2pa_test::read_text_file(settings_path);
    auto trusted_context = c2pa::Context::from_toml(settings);

    // Create builder using context containing settings that does generate thumbnails
    auto builder = c2pa::Builder(trusted_context, manifest);
    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path, signer));

    // Verify the signed file with context exists and is readable
    ASSERT_TRUE(std::filesystem::exists(output_path));

    // When reading, the Reader also needs to know about trust, to determine the manifest validation state
    // If there is a valid trust chain, the manifest will be in validation_state Trusted.
    auto reader = c2pa::Reader(trusted_context, output_path);
    std::string read_json_manifest;
    ASSERT_NO_THROW(read_json_manifest = reader.json());
    ASSERT_FALSE(read_json_manifest.empty());

    // Both builders should successfully create valid manifests,
    // but one should have thumbnails whereas the other shouldn't,
    // and we can even have different settings per Builder, as they trickle down!
    json parsed_manifest_json = json::parse(read_json_manifest);

    // Smoke check that the manifest has a valid structure
    ASSERT_TRUE(parsed_manifest_json["validation_state"] == "Trusted");

    // But if we don't know about the whole trust chain,
    // for instance it is not configured on the Reader's context,
    // then our manifest is "only" Valid.

    // If the Reader doesn't know about trust, we can only be "Valid" in the best case
    // For instance, a Reader without context won't know about trust
    auto reader2 = c2pa::Reader(output_path);
    std::string read_json_manifest2;
    ASSERT_NO_THROW(read_json_manifest2 = reader2.json());
    ASSERT_FALSE(read_json_manifest2.empty());

    // Both builders should successfully create valid manifests,
    // but one should have thumbnails whereas the other shouldn't,
    // and we can even have different settings per Builder, as they trickle down!
    json parsed_manifest_json2 = json::parse(read_json_manifest2);

    // Smoke check that the manifest has a valid structure
    ASSERT_TRUE(parsed_manifest_json2["validation_state"] == "Valid");

    // It is also important to make sure the proper trust chain is configured in settings...
    // If not, we won't be able to read the manifest as trusted!
    fs::path settings_without_trust_path = current_dir / "../tests/fixtures/settings/test_settings_no_thumbnail.json";
    auto settings_without_trust = c2pa_test::read_text_file(settings_without_trust_path);
    auto no_trust_context = c2pa::Context::from_json(settings_without_trust);

    auto reader3 = c2pa::Reader(no_trust_context, output_path);
    std::string read_json_manifest3;
    ASSERT_NO_THROW(read_json_manifest3 = reader3.json());
    ASSERT_FALSE(read_json_manifest3.empty());

    // Both builders should successfully create valid manifests,
    // but one should have thumbnails whereas the other shouldn't,
    // and we can even have different settings per Builder, as they trickle down!
    json parsed_manifest_json3 = json::parse(read_json_manifest3);
    // Smoke check that the manifest has a valid structure
    ASSERT_TRUE(parsed_manifest_json3["validation_state"] == "Valid");
};

TEST(Builder, SignWithIStreamAndOStream_RoundTrip)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../build/examples/stream_ostream_roundtrip.jpg";

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    std::filesystem::remove(output_path);
    std::filesystem::create_directories(output_path.parent_path());

    auto builder = c2pa::Builder(manifest);
    std::ifstream source(image_path, std::ios::binary);
    ASSERT_TRUE(source) << "Failed to open " << image_path;

    std::ofstream dest(output_path, std::ios::binary);
    ASSERT_TRUE(dest) << "Failed to open " << output_path;

    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign("image/jpeg", source, dest, signer));
    source.close();
    dest.close();

    ASSERT_FALSE(manifest_data.empty());
    ASSERT_TRUE(std::filesystem::exists(output_path));

    c2pa::Reader reader(output_path);
    std::string json_result;
    ASSERT_NO_THROW(json_result = reader.json());
    ASSERT_TRUE(json_result.find("cawg.training-mining") != std::string::npos);
}

TEST(Builder, SignWithIStreamAndIOStream_RoundTrip)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    auto builder = c2pa::Builder(manifest);
    std::ifstream source(image_path, std::ios::binary);
    ASSERT_TRUE(source) << "Failed to open " << image_path;

    std::stringstream buffer(std::ios::in | std::ios::out | std::ios::binary);
    std::iostream& dest = buffer;

    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign("image/jpeg", source, dest, signer));
    source.close();

    dest.flush();
    dest.seekg(0, std::ios::beg);
    dest.seekp(0, std::ios::beg);

    c2pa::Reader reader("image/jpeg", dest);
    std::string json_result;
    ASSERT_NO_THROW(json_result = reader.json());
    ASSERT_FALSE(manifest_data.empty());
    ASSERT_TRUE(json_result.find("cawg.training-mining") != std::string::npos);
}

TEST(Builder, ArchiveRoundTrip)
{
    auto manifest = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto context = c2pa::Context::create();
    auto builder1 = c2pa::Builder(context, manifest);

    // Export to archive
    std::stringstream archive_stream(std::ios::in | std::ios::out | std::ios::binary);
    EXPECT_NO_THROW({
        builder1.to_archive(archive_stream);
    });

    // Re-import from archive
    archive_stream.seekg(0);
    auto builder2 = c2pa::Builder::from_archive(archive_stream);

    // Sign with the restored builder
    auto signer = c2pa_test::create_test_signer();
    std::ifstream source(c2pa_test::get_fixture_path("A.jpg"), std::ios::binary);
    ASSERT_TRUE(source.is_open());
    std::stringstream dest(std::ios::in | std::ios::out | std::ios::binary);

    EXPECT_NO_THROW({
        builder2.sign("image/jpeg", source, dest, signer);
    });

    // Verify signed output has data
    dest.seekg(0, std::ios::end);
    EXPECT_GT(dest.tellg(), 0);
}

TEST(Builder, ArchiveRoundTripSettingsBehavior)
{
    auto manifest = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));

    // Create context with settings that disable thumbnail generation
    auto settings_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("settings/test_settings_no_thumbnail.json"));
    auto context_no_thumbnail = c2pa::Context::from_json(settings_json);

    // Verify the setting works when set on builder with context (baseline)
    auto builder_direct = c2pa::Builder(context_no_thumbnail, manifest);
    auto signer = c2pa_test::create_test_signer();
    std::ifstream source1(c2pa_test::get_fixture_path("A.jpg"), std::ios::binary);
    ASSERT_TRUE(source1.is_open());
    std::stringstream dest_direct(std::ios::in | std::ios::out | std::ios::binary);
    EXPECT_NO_THROW({
      builder_direct.sign("image/jpeg", source1, dest_direct, signer);
    });
    // Verify no thumbnail in direct sign
    dest_direct.seekg(0);
    auto reader_direct = c2pa::Reader(c2pa::Context::create(), "image/jpeg", dest_direct);
    auto json_direct = json::parse(reader_direct.json());
    std::string active_direct = json_direct["active_manifest"];
    EXPECT_FALSE(json_direct["manifests"][active_direct].contains("thumbnail"))
        << "Direct sign with thumbnail=false should not generate thumbnail";

    // from_archive does not preserve settings of the archived builder,
    // They would need to be restored... So here we expect a manifest with thumbnails then!
    auto builder1 = c2pa::Builder(context_no_thumbnail, manifest);
    // Export to archive
    std::stringstream archive_stream(std::ios::in | std::ios::out | std::ios::binary);
    EXPECT_NO_THROW({
        builder1.to_archive(archive_stream);
    });
    // Re-import from archive (creates builder with DEFAULT context/settings)
    archive_stream.seekg(0);
    auto builder2 = c2pa::Builder::from_archive(archive_stream);
    // Sign with the restored builder (reuse the same signer from above)
    std::ifstream source2(c2pa_test::get_fixture_path("A.jpg"), std::ios::binary);
    ASSERT_TRUE(source2.is_open());
    std::stringstream dest_archive(std::ios::in | std::ios::out | std::ios::binary);
    EXPECT_NO_THROW({
        builder2.sign("image/jpeg", source2, dest_archive, signer);
    });
    // Verify manifest structure is preserved
    dest_archive.seekg(0);
    auto reader_archive = c2pa::Reader(c2pa::Context::create(), "image/jpeg", dest_archive);
    auto json_archive = json::parse(reader_archive.json());
    EXPECT_TRUE(json_archive.contains("active_manifest"));

    // Verify that the archive round-trip manifest HAS a thumbnail
    // (because from_archive() uses default settings which enable thumbnails)
    std::string active_archive = json_archive["active_manifest"];
    EXPECT_TRUE(json_archive["manifests"][active_archive].contains("thumbnail"))
        << "Archive round-trip (default context on Builder) should generate thumbnail with default settings";
}

TEST(Builder, ArchiveRoundTripSettingsBehaviorRestoredCOntext)
{
    auto manifest = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));

    // Create context with settings that disable thumbnail generation
    auto settings_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("settings/test_settings_no_thumbnail.json"));
    auto context_no_thumbnail = c2pa::Context::from_json(settings_json);

    // Verify the setting works when set on builder with context (baseline)
    auto builder_direct = c2pa::Builder(context_no_thumbnail, manifest);
    auto signer = c2pa_test::create_test_signer();
    std::ifstream source1(c2pa_test::get_fixture_path("A.jpg"), std::ios::binary);
    ASSERT_TRUE(source1.is_open());
    std::stringstream dest_direct(std::ios::in | std::ios::out | std::ios::binary);
    EXPECT_NO_THROW({
      builder_direct.sign("image/jpeg", source1, dest_direct, signer);
    });
    // Verify no thumbnail is here (aka setting was applied)
    dest_direct.seekg(0);
    auto reader_direct = c2pa::Reader(c2pa::Context::create(), "image/jpeg", dest_direct);
    auto json_direct = json::parse(reader_direct.json());
    std::string active_direct = json_direct["active_manifest"];
    EXPECT_FALSE(json_direct["manifests"][active_direct].contains("thumbnail"))
        << "Direct sign with thumbnail=false should not generate thumbnail";

    // from_archive does not preserve settings of the archived builder,
    // They would need to be restored... So here we expect a manifest with thumbnails then!
    auto builder1 = c2pa::Builder(context_no_thumbnail, manifest);
    // Export to archive
    std::stringstream archive_stream(std::ios::in | std::ios::out | std::ios::binary);
    EXPECT_NO_THROW({
        builder1.to_archive(archive_stream);
    });
    // Re-import from archive (creates builder with DEFAULT context/settings)
    archive_stream.seekg(0);
    auto builder2 = c2pa::Builder::from_archive(archive_stream);
    // Sign with the restored builder (reuse the same signer from above)
    std::ifstream source2(c2pa_test::get_fixture_path("A.jpg"), std::ios::binary);
    ASSERT_TRUE(source2.is_open());
    std::stringstream dest_archive(std::ios::in | std::ios::out | std::ios::binary);
    EXPECT_NO_THROW({
        builder2.sign("image/jpeg", source2, dest_archive, signer);
    });
    // Verify manifest structure is preserved
    dest_archive.seekg(0);
    auto reader_archive = c2pa::Reader(c2pa::Context::create(), "image/jpeg", dest_archive);
    auto json_archive = json::parse(reader_archive.json());
    EXPECT_TRUE(json_archive.contains("active_manifest"));

    // Verify that the archive round-trip manifest HAS a thumbnail
    // (because from_archive() uses default settings which enable thumbnails)
    std::string active_archive = json_archive["active_manifest"];
    EXPECT_TRUE(json_archive["manifests"][active_archive].contains("thumbnail"))
        << "Archive round-trip (default context on Builder) should generate thumbnail with default settings";
}

// ============================================================================
// Builder Error Handling (error returned from C API, no panic)
// ============================================================================

TEST(BuilderErrorHandling, EmptyManifestJsonReturnsError)
{
    try {
        c2pa::Builder builder("");
        ADD_FAILURE() << "Expected C2paException for empty manifest JSON (error path, not panic)";
    } catch (const c2pa::C2paException& e) {
        std::string msg(e.what());
        EXPECT_FALSE(msg.empty()) << "Exception should carry an error message";
        // Underlying C API returns JsonError; message may contain "Json", "EOF", "parse", etc.
        EXPECT_TRUE(msg.find("Json") != std::string::npos || msg.find("EOF") != std::string::npos ||
                    msg.find("parse") != std::string::npos || msg.find("empty") != std::string::npos)
            << "Expected JSON-related error message, got: " << msg;
    } catch (...) {
        ADD_FAILURE() << "Expected C2paException, got other exception (possible panic path)";
    }
}

TEST(BuilderErrorHandling, MalformedJsonManifestReturnsError)
{
    try {
        c2pa::Builder builder("{ invalid json }");
        ADD_FAILURE() << "Expected C2paException for malformed JSON (error path, not panic)";
    } catch (const c2pa::C2paException& e) {
        std::string msg(e.what());
        EXPECT_FALSE(msg.empty()) << "Exception should carry an error message";
        EXPECT_TRUE(msg.find("Json") != std::string::npos || msg.find("parse") != std::string::npos ||
                    msg.find("invalid") != std::string::npos)
            << "Expected JSON-related error message, got: " << msg;
    } catch (...) {
        ADD_FAILURE() << "Expected C2paException, got other exception (possible panic path)";
    }
}
