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
#include <algorithm>
#include <functional>
#include <iostream>
#include <memory>
#include <set>
#include <string>
#include <filesystem>
#include <thread>

#include "include/test_utils.hpp"

using namespace std;
namespace fs = std::filesystem;
using nlohmann::json;

// Test fixture for builder tests with cleanup
class BuilderTest : public ::testing::Test {
protected:
    std::vector<fs::path> temp_files;
    std::vector<fs::path> temp_dirs;
    bool cleanup_temp_files = true;  // Set to false to keep temp files for debugging

    // Get path for temp builder test files in build directory
    fs::path get_temp_path(const std::string& name) {
        fs::path current_dir = fs::path(__FILE__).parent_path();
        fs::path build_dir = current_dir.parent_path() / "build";
        if (!fs::exists(build_dir)) {
            fs::create_directories(build_dir);
        }
        fs::path temp_path = build_dir / ("builder-" + name);
        temp_files.push_back(temp_path);
        return temp_path;
    }

    // Get path for temp builder test directories in build directory
    fs::path get_temp_dir(const std::string& name) {
        fs::path current_dir = fs::path(__FILE__).parent_path();
        fs::path build_dir = current_dir.parent_path() / "build";
        if (!fs::exists(build_dir)) {
            fs::create_directories(build_dir);
        }
        fs::path temp_path = build_dir / ("builder-" + name);
        // Remove and recreate the temp dir
        if (fs::exists(temp_path)) {
            fs::remove_all(temp_path);
        }
        fs::create_directories(temp_path);
        temp_dirs.push_back(temp_path);
        return temp_path;
    }

    void TearDown() override {
        if (cleanup_temp_files) {
            for (const auto& path : temp_files) {
                if (fs::exists(path)) {
                    fs::remove(path);
                }
            }
            for (const auto& dir : temp_dirs) {
                if (fs::exists(dir)) {
                    fs::remove_all(dir);
                }
            }
        }
        temp_files.clear();
        temp_dirs.clear();
    }
};

TEST_F(BuilderTest, supported_mime_types_returns_types) {
  auto supported_types = c2pa::Builder::supported_mime_types();
  auto begin = supported_types.begin();
  auto end = supported_types.end();
  EXPECT_TRUE(std::find(begin, end, "image/jpeg") != end);
  EXPECT_TRUE(std::find(begin, end, "application/c2pa") != end);
}

TEST_F(BuilderTest, exposes_raw_pointer) {
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    auto manifest = c2pa_test::read_text_file(manifest_path);
    c2pa::Builder builder(manifest);
    ASSERT_NE(builder.c2pa_builder(), nullptr);
}

// Test fixture for basic signature validations with automatic cleanup
class BuilderSmokeSignTest : public BuilderTest, public ::testing::WithParamInterface<std::string> {
public:
  static std::string get_mime_type_from_extension(const std::string& filename) {
    std::string ext = fs::path(filename).extension().string();
    if (ext == ".jpg" || ext == ".jpeg") return "image/jpeg";
    if (ext == ".png") return "image/png";
    if (ext == ".gif") return "image/gif";
    if (ext == ".webp") return "image/webp";
    if (ext == ".svg") return "image/svg+xml";
    if (ext == ".dng") return "image/dng";
    if (ext == ".mp4") return "video/mp4";
    if (ext == ".mp3") return "audio/mpeg";
    if (ext == ".wav") return "audio/wav";
    return "application/octet-stream";
  }
};

INSTANTIATE_TEST_SUITE_P(
    BuilderSignCallToVerifyMiscFileTypes,
    BuilderSmokeSignTest,
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

TEST_P(BuilderSmokeSignTest, SignsFileTypes) {
  fs::path current_dir = fs::path(__FILE__).parent_path();

  // Construct the paths relative to the current directory
  fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
  fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
  fs::path asset_path = current_dir / "../tests/fixtures" / GetParam();

  // Use temp path with automatic cleanup
  fs::path output_path = get_temp_path(GetParam());

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

TEST_P(BuilderSmokeSignTest, SignsStreamTypes) {
  fs::path current_dir = fs::path(__FILE__).parent_path();

  // Construct the paths relative to the current directory
  fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
  fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
  fs::path asset_path = current_dir / "../tests/fixtures" / GetParam();

  auto manifest = c2pa_test::read_text_file(manifest_path);
  auto certs = c2pa_test::read_text_file(certs_path);
  auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

  // Get mimetype from file extension
  std::string mime_type = get_mime_type_from_extension(GetParam());

  // Create a signer
  auto signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");
  auto builder = c2pa::Builder(manifest);

  // Open input stream
  std::ifstream source(asset_path, std::ios::binary);
  ASSERT_TRUE(source) << "Failed to open source file: " << asset_path;

  // Create output stream
  std::stringstream output_buffer(std::ios::in | std::ios::out | std::ios::binary);
  std::iostream& dest = output_buffer;

  // Sign
  std::vector<unsigned char> manifest_data;
  ASSERT_NO_THROW(manifest_data = builder.sign(mime_type, source, dest, signer));
  ASSERT_FALSE(manifest_data.empty());

  source.close();

  // Rewind output stream and verify with Reader
  dest.flush();
  dest.seekg(0, std::ios::beg);

  auto reader = c2pa::Reader(mime_type, dest);
  std::string json_result;
  ASSERT_NO_THROW(json_result = reader.json());
  ASSERT_FALSE(json_result.empty());
}

TEST(BuilderErrorHandling, EmptyManifestJsonReturnsError)
{
    EXPECT_THROW(c2pa::Builder(""), c2pa::C2paException);
}

TEST(BuilderErrorHandling, MalformedJsonManifestReturnsError)
{
    EXPECT_THROW(c2pa::Builder("{ invalid json"), c2pa::C2paException);
}

TEST(BuilderErrorHandling, EmptyManifestJsonReturnsErrorWithContext)
{
    auto context = c2pa::Context();
    EXPECT_THROW(c2pa::Builder(context, ""), c2pa::C2paException);
}

TEST(BuilderErrorHandling, MalformedJsonManifestReturnsErrorWithContext)
{
    auto context = c2pa::Context();
    EXPECT_THROW(c2pa::Builder(context, "{ invalid json"), c2pa::C2paException);
}

TEST(BuilderErrorHandling, JsonErrorsBehaveSameWithAndWithoutContext)
{
    std::vector<std::string> bad_inputs = {
        "",
        "null",
        "[]",
        "{",
        "{ invalid }",
        "{\"key\": }",
    };

    for (const auto& bad_input : bad_inputs) {
        // Without context
        EXPECT_THROW({
            c2pa::Builder builder(bad_input);
        }, c2pa::C2paException)
            << "Without context, should throw for: " << bad_input;

        // With context
        auto ctx = c2pa::Context();
        EXPECT_THROW({
            c2pa::Builder builder(ctx, bad_input);
        }, c2pa::C2paException)
            << "With context, should throw for: " << bad_input;
    }
}

TEST(BuilderErrorHandling, ValidJsonWorksWithAndWithoutContext)
{
    std::string valid_json = R"({"claim_generator": "test"})";

    // Without context
    EXPECT_NO_THROW({
        c2pa::Builder builder(valid_json);
    });

    // With context
    auto ctx = c2pa::Context();
    EXPECT_NO_THROW({
        c2pa::Builder builder(ctx, valid_json);
    });
}

TEST(BuilderErrorHandling, FailedConstructionWithAndWithoutContext)
{
    for (int i = 0; i < 100; i++) {
        // Without context
        try {
            c2pa::Builder("");
            FAIL() << "Should have thrown";
        } catch (const c2pa::C2paException&) {
            // Expected
        }

        // With context
        try {
            auto ctx = c2pa::Context();
            c2pa::Builder(ctx, "");
            FAIL() << "Should have thrown";
        } catch (const c2pa::C2paException&) {
            // Expected
        }
    }
}

TEST(BuilderErrorHandling, ErrorMessagesWithAndWithoutContext)
{
    // Without context
    try {
        c2pa::Builder("");
        FAIL() << "Should have thrown";
    } catch (const c2pa::C2paException& e) {
        std::string msg = e.what();
        EXPECT_FALSE(msg.empty()) << "Error message should be present";
    }

    // With context
    try {
        auto ctx = c2pa::Context();
        c2pa::Builder(ctx, "");
        FAIL() << "Should have thrown";
    } catch (const c2pa::C2paException& e) {
        std::string msg = e.what();
        EXPECT_FALSE(msg.empty()) << "Error message should be present when using context API";
    }
}

TEST_F(BuilderTest, AddAnActionAndSign)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = get_temp_path("image_with_one_action.jpg");

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // Create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

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

TEST_F(BuilderTest, AddAnActionAndSignUsingContext)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = get_temp_path("image_with_one_action_context.jpg");

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // Create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    // Create a Context and pass it to the Builder
    auto context = c2pa::Context();
    auto builder = c2pa::Builder(context, manifest);

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

    // Read to verify (also using context)
    auto reader = c2pa::Reader(context, output_path);
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

TEST_F(BuilderTest, AddMultipleActionsAndSign)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = get_temp_path("image_with_multiple_actions.jpg");

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // Create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

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

TEST_F(BuilderTest, AddMultipleActionsAndSignUsingContext)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = get_temp_path("image_with_multiple_actions_context.jpg");

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // Create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    // Create a Context and pass it to the Builder
    auto context = c2pa::Context();
    auto builder = c2pa::Builder(context, manifest);

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

    // Read to verify (also using context)
    auto reader = c2pa::Reader(context, output_path);
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

TEST_F(BuilderTest, SignImageFileOnly)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = get_temp_path("training_image_only.jpg");

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    auto builder = c2pa::Builder(manifest);
    // the Builder returns manifest bytes
    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path, signer));

    // read to verify
    auto reader = c2pa::Reader(output_path);
    ASSERT_NO_THROW(reader.json());
    ASSERT_TRUE(std::filesystem::exists(output_path));
};

TEST_F(BuilderTest, SignImageFileNoThumbnailAutoGenThreadLocalSettings)
{
    // Run in separate thread for complete test isolation (thread-local settings won't leak)
    fs::path temp_output = get_temp_path("training_image_only_thread_local.jpg");
    std::thread test_thread([temp_output]() {
        fs::path current_dir = fs::path(__FILE__).parent_path();

        // Construct the paths relative to the current directory
        fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
        fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
        fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
        fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
        fs::path output_path = temp_output;

        auto manifest = c2pa_test::read_text_file(manifest_path);
        auto certs = c2pa_test::read_text_file(certs_path);
        auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

        // set settings to not generate thumbnails (thread-local)
        c2pa::load_settings("{\"builder\": { \"thumbnail\": {\"enabled\": false}}}", "json");

        // create a signer
        c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

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

TEST_F(BuilderTest, SignImageFileNoThumbnailAutoGen)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path_with_context = get_temp_path("settings_no_thumbnails.jpg");
    fs::path output_path_no_context = get_temp_path("settings_with_thumbnails.jpg");

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // Create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    // Test 1: Create context with specific settings via JSON
    auto context = c2pa::Context("{\"builder\": { \"thumbnail\": {\"enabled\": false}}}");

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

TEST_F(BuilderTest, SignImageThumbnailSettingsFileJson)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path_with_context = get_temp_path("image_context_settings_json.jpg");
    fs::path output_path_no_context = get_temp_path("image_no_context_json.jpg");

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // Create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    // Create context with specific settings via JSON, by loading the JSON file with the settings
    fs::path settings_path = current_dir / "../tests/fixtures/settings/test_settings_no_thumbnail.json";
    auto settings_json = c2pa_test::read_text_file(settings_path);
    auto context = c2pa::Context(settings_json);

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
    auto context2 = c2pa::Context(settings_json2);

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

TEST_F(BuilderTest, SignImageThumbnailSettingsObject)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path_no_thumbnail = get_temp_path("image_no_thumbnail_incremental.jpg");

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // Create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

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

TEST_F(BuilderTest, SignImageThumbnailSettingsIncrementalObject)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path_no_thumbnail = get_temp_path("image_no_thumbnail_incremental2.jpg");

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // Create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

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
    settings.update(updated_config, "json");

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

TEST_F(BuilderTest, SignImageFileWithResource)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = get_temp_path("training_resource_only.jpg");

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

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

TEST_F(BuilderTest, SignImageFileWithResourceUsingContext)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = get_temp_path("training_resource_only_context.jpg");

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    // Create a Context and pass it to the Builder
    auto context = c2pa::Context();
    auto builder = c2pa::Builder(context, manifest);
    // add a resource: a thumbnail
    builder.add_resource("thumbnail", image_path);

    // sign
    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path, signer));

    // read to verify signature (also using context)
    auto reader = c2pa::Reader(context, output_path);
    ASSERT_NO_THROW(reader.json());
    ASSERT_TRUE(std::filesystem::exists(output_path));
};

TEST_F(BuilderTest, SignWithMultipleResources)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path image_resource = current_dir / "../tests/fixtures/C.jpg";
    fs::path image_resource_other = current_dir / "../tests/fixtures/sample1.gif";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = get_temp_path("multiple_resources.jpg");

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

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

TEST_F(BuilderTest, SignImageFileWithIngredient)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = get_temp_path("training_ingredient_only.jpg");

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

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

TEST_F(BuilderTest, SignImageFileWithResourceAndIngredient)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = get_temp_path("training.jpg");

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");
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

TEST_F(BuilderTest, SignVideoFileWithMultipleIngredients)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_video_path = current_dir / "../tests/fixtures/video1.mp4";
    fs::path audio_ingredient = current_dir / "../tests/fixtures/sample1_signed.wav";
    fs::path image_ingredient = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = get_temp_path("video1_signed_multi_ingredients.mp4");

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

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

TEST_F(BuilderTest, SignVideoFileWithMultipleIngredientsUsingContext)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_video_path = current_dir / "../tests/fixtures/video1.mp4";
    fs::path audio_ingredient = current_dir / "../tests/fixtures/sample1_signed.wav";
    fs::path image_ingredient = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = get_temp_path("video1_signed_context.mp4");

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    // create the builder with context
    auto context = c2pa::Context();
    auto builder = c2pa::Builder(context, manifest);

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

    // read to verify signature (also using context)
    auto reader = c2pa::Reader(context, output_path);
    ASSERT_NO_THROW(reader.json());
    ASSERT_TRUE(std::filesystem::exists(output_path));
};

TEST_F(BuilderTest, SignVideoFileWithMultipleIngredientsAndResources)
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
    fs::path output_path = get_temp_path("video1_signed_with_ingredients_and_resources.mp4");

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

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

TEST_F(BuilderTest, SignVideoFileWithMultipleIngredientsAndResourcesInterleaved)
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
    fs::path output_path = get_temp_path("video1_signed_interleaved.mp4");

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // create a signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");
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

TEST_F(BuilderTest, SignImageStreamWithoutContext)
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

TEST_F(BuilderTest, SignImageStream)
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
    auto context = c2pa::Context();

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

TEST_F(BuilderTest, SignImageStreamBuilderReaderDifferentContext)
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
    auto write_context = c2pa::Context();

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
    auto read_context = c2pa::Context();

    auto reader = c2pa::Reader(read_context, "image/jpeg", dest);
    std::string json;
    ASSERT_NO_THROW(json = reader.json());

    // Verify the manifest contains expected data
    ASSERT_TRUE(json.find("cawg.training-mining") != std::string::npos);
    ASSERT_FALSE(manifest_data.empty());
}

TEST_F(BuilderTest, SignImageWithIngredientHavingManifestStream)
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

TEST_F(BuilderTest, SignStreamCloudUrl)
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

TEST_F(BuilderTest, SignDataHashedEmbedded)
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

TEST_F(BuilderTest, SignDataHashedEmbeddedUsingContext)
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

    // Create a Context and pass it to the Builder
    auto context = c2pa::Context();
    auto builder = c2pa::Builder(context, manifest);

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

TEST_F(BuilderTest, SignDataHashedEmbeddedWithAsset)
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

TEST_F(BuilderTest, SignDataHashedEmbeddedWithAssetUsingContext)
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

    // Create a Context and pass it to the Builder
    auto context = c2pa::Context();
    auto builder = c2pa::Builder(context, manifest);

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

TEST_F(BuilderTest, SignWithInvalidStream)
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

TEST_F(BuilderTest, SignWithoutTimestamping)
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

TEST_F(BuilderTest, ReadIngredientFile)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the path to the test fixture
    fs::path source_path = current_dir / "../tests/fixtures/A.jpg";

    // Get temp directory for ingredient data
    fs::path temp_dir = get_temp_dir("read_ingredient_a");

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

TEST_F(BuilderTest, ReadIngredientFileWhoHasAManifestStore)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the path to the test fixture
    // C has a manifest store attached
    fs::path source_path = current_dir / "../tests/fixtures/C.jpg";

    // Get temp directory for ingredient data
    fs::path temp_dir = get_temp_dir("read_ingredient_c");

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

TEST_F(BuilderTest, AddIngredientAsResourceToBuilder)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";

    // Construct the path to the test fixture
    fs::path ingredient_source_path = current_dir / "../tests/fixtures/A.jpg";
    std::string ingredient_source_path_str = ingredient_source_path.string();

    fs::path temp_dir = get_temp_dir("ingredient_as_resource");

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
    fs::path output_path = get_temp_path("signed_with_ingredient_and_resource_1.jpg");
    manifest_data = builder.sign(signed_image_path, output_path, signer);

    auto reader = c2pa::Reader(output_path);
    ASSERT_NO_THROW(reader.json());
}

TEST_F(BuilderTest, LinkIngredientsAndSign)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";

    // Construct the path to the test fixture
    fs::path ingredient_source_path = current_dir / "../tests/fixtures/A.jpg";
    std::string ingredient_source_path_str = ingredient_source_path.string();

    fs::path temp_dir = get_temp_dir("ingredient_linked_resource");

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
    fs::path output_path = get_temp_path("signed_with_ingredient_and_resource_2.jpg");

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

TEST_F(BuilderTest, LinkIngredientsAndSignUsingContext)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";

    // Construct the path to the test fixture
    fs::path ingredient_source_path = current_dir / "../tests/fixtures/A.jpg";
    std::string ingredient_source_path_str = ingredient_source_path.string();

    fs::path temp_dir = get_temp_dir("ingredient_linked_resource_context");

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

    // Create a Context and pass it to the Builder
    auto context = c2pa::Context();
    auto builder = c2pa::Builder(context, manifest_json.dump());

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
    fs::path output_path = get_temp_path("signed_with_ingredient_and_resource_context.jpg");

    std::vector<unsigned char> manifest_data;
    manifest_data = builder.sign(signed_image_path, output_path, signer);

    // Read to verify (also using context)
    auto reader = c2pa::Reader(context, output_path);
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

TEST_F(BuilderTest, AddIngredientToBuilderUsingBasePath)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";

    // Construct the path to the test fixture
    fs::path ingredient_source_path = current_dir / "../tests/fixtures/A.jpg";
    std::string ingredient_source_path_str = ingredient_source_path.string();

    // Use temp dir for ingredient data (data dir)
    fs::path temp_dir = get_temp_dir("base_ingredient_as_resource");

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
    fs::path output_path = get_temp_path("signed_with_ingredient_and_resource_3.jpg");
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path, signer));

    auto reader = c2pa::Reader(output_path);
    ASSERT_NO_THROW(reader.json());
}

TEST_F(BuilderTest, AddIngredientToBuilderUsingBasePathPlacedActionThreadLocalSettings)
{
    // Run in separate thread for complete test isolation (thread-local settings won't leak)
    std::thread test_thread([this]() {
        fs::path current_dir = fs::path(__FILE__).parent_path();

        // Construct the path to the test fixture
        fs::path ingredient_source_path = current_dir / "../tests/fixtures/A.jpg";
        std::string ingredient_source_path_str = ingredient_source_path.string();

        // Use temp dir for ingredient data
        fs::path build_dir = current_dir.parent_path() / "build";
        if (!fs::exists(build_dir)) {
            fs::create_directories(build_dir);
        }
        fs::path temp_dir = build_dir / "builder-ingredient_placed_as_resource";

        // Remove and recreate the temp data dir folder before using it
        if (fs::exists(temp_dir)) {
            fs::remove_all(temp_dir);
        }
        fs::create_directories(temp_dir);

        // set settings to not auto-add a placed action (thread-local)
        c2pa::load_settings("{\"builder\": { \"actions\": {\"auto_placed_action\": {\"enabled\": false}}}}", "json");

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
        fs::path output_path = get_temp_path("signed_with_ingredient_and_resource_4.jpg");
        ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path, signer));

        auto reader = c2pa::Reader(output_path);
        ASSERT_NO_THROW(reader.json());

        // Cleanup temp dir if needed
        if (cleanup_temp_files) {
            if (fs::exists(temp_dir)) {
                fs::remove_all(temp_dir);
            }
        }

        // No need to reset settings - thread-local settings destroyed with thread
    });

    // Wait for thread to complete
    test_thread.join();
}

TEST_F(BuilderTest, AddIngredientToBuilderUsingBasePathWithManifestContainingPlacedAction)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the path to the test fixture
    fs::path ingredient_source_path = current_dir / "../tests/fixtures/A.jpg";
    std::string ingredient_source_path_str = ingredient_source_path.string();

    // Use temp dir for ingredient data
    fs::path temp_dir = get_temp_dir("ingredient_placed_context");

    // Create context with auto_placed_action disabled via JSON settings
    auto context = c2pa::Context("{\"builder\": { \"actions\": {\"auto_placed_action\": {\"enabled\": false}}}}");

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
    fs::path output_path = get_temp_path("signed_with_ingredient_context.jpg");
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path, signer));

    // Read and verify
    auto reader = c2pa::Reader(output_path);
    std::string reader_json;
    ASSERT_NO_THROW(reader_json = reader.json());

    // Verify the placed action is in the manifest
    ASSERT_TRUE(reader_json.find("c2pa.placed") != std::string::npos);
    ASSERT_TRUE(reader_json.find(instance_id) != std::string::npos);
}

TEST_F(BuilderTest, AddIngredientWithProvenanceDataToBuilderUsingBasePath)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";

    // Construct the path to the test fixture
    fs::path ingredient_source_path = current_dir / "../tests/fixtures/C.jpg";
    std::string ingredient_source_path_str = ingredient_source_path.string();

    // Use temp data dir
    fs::path temp_dir = get_temp_dir("ingredient_with_provenance_as_resource");

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
    fs::path output_path = get_temp_path("signed_with_ingredient_and_resource_5.jpg");
    ASSERT_NO_THROW(manifest_data = builder.sign(signed_image_path, output_path, signer));

    auto reader = c2pa::Reader(output_path);
    ASSERT_NO_THROW(reader.json());
}

TEST_F(BuilderTest, MultipleBuildersDifferentThumbnailSettingsInterleaved)
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

    // Create one context with specific settings via JSON, by loading the JSON file with the settings
    fs::path settings_path = current_dir / "../tests/fixtures/settings/test_settings_no_thumbnail.json";
    auto settings_json = c2pa_test::read_text_file(settings_path);
    auto context_without_thumbnails = c2pa::Context(settings_json);

    // Now, create anothetrcontext, that sets thumbnails to be generated
    fs::path settings_path2 = current_dir / "../tests/fixtures/settings/test_settings_with_thumbnail.json";
    auto settings_json2 = c2pa_test::read_text_file(settings_path2);
    auto context_with_thumbnails = c2pa::Context(settings_json2);

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

TEST_F(BuilderTest, MultipleBuildersDifferentThumbnailSettingsInterleaved2)
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

    // Create one context with specific settings via JSON, by loading the JSON file with the settings
    fs::path settings_path = current_dir / "../tests/fixtures/settings/test_settings_no_thumbnail.json";
    auto settings_json = c2pa_test::read_text_file(settings_path);
    auto context_without_thumbnails = c2pa::Context(settings_json);

    // Now, create another context, that sets thumbnails to be generated
    fs::path settings_path2 = current_dir / "../tests/fixtures/settings/test_settings_with_thumbnail.json";
    auto settings_json2 = c2pa_test::read_text_file(settings_path2);
    auto context_with_thumbnails = c2pa::Context(settings_json2);

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

TEST_F(BuilderTest, TrustHandling)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();

    // All the test fixtures
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = get_temp_path("trust_handling_test.jpg");
    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");

    // Create our very own signer
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

    // Trust is based on a chain of trusted certificates. When signing, we may need to know
    // if the ingredients are trusted at time of signing, so we benefit from having a context
    // already configured with that trust to use with our Builder and Reader.
    fs::path settings_path = current_dir / "../tests/fixtures/settings/test_settings_example.json";
    auto settings = c2pa_test::read_text_file(settings_path);
    auto trusted_context = c2pa::Context::ContextBuilder().with_settings(c2pa::Settings(settings, "json")).create_context();

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
    auto no_trust_context = c2pa::Context(settings_without_trust);

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

TEST_F(BuilderTest, SignWithIStreamAndOStream_RoundTrip)
{
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = get_temp_path("stream_ostream_roundtrip.jpg");

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(certs_path);
    auto p_key = c2pa_test::read_text_file(current_dir / "../tests/fixtures/es256_private.key");
    c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

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

TEST_F(BuilderTest, SignWithIStreamAndIOStream_RoundTrip)
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

TEST_F(BuilderTest, ArchiveRoundTrip)
{
    auto manifest = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto context = c2pa::Context();
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

TEST_F(BuilderTest, ArchiveRoundTripSettingsBehavior)
{
    auto manifest = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));

    // Create context with settings that disable thumbnail generation
    auto settings_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("settings/test_settings_no_thumbnail.json"));
    auto context_no_thumbnail = c2pa::Context(settings_json);

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
    auto ctx_reader_direct = c2pa::Context();
    auto reader_direct = c2pa::Reader(ctx_reader_direct, "image/jpeg", dest_direct);
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
    auto ctx_reader_archive = c2pa::Context();
    auto reader_archive = c2pa::Reader(ctx_reader_archive, "image/jpeg", dest_archive);
    auto json_archive = json::parse(reader_archive.json());
    EXPECT_TRUE(json_archive.contains("active_manifest"));

    // Verify that the archive round-trip manifest HAS a thumbnail
    // (because from_archive() uses default settings which enable thumbnails)
    std::string active_archive = json_archive["active_manifest"];
    EXPECT_TRUE(json_archive["manifests"][active_archive].contains("thumbnail"))
        << "Archive round-trip (default context on Builder) should generate thumbnail with default settings";
}

TEST_F(BuilderTest, ArchiveRoundTripSettingsBehaviorRestoredCOntext)
{
    auto manifest = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));

    // Create context with settings that disable thumbnail generation
    auto settings_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("settings/test_settings_no_thumbnail.json"));
    auto context_no_thumbnail = c2pa::Context(settings_json);

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
    auto ctx_reader_direct = c2pa::Context();
    auto reader_direct = c2pa::Reader(ctx_reader_direct, "image/jpeg", dest_direct);
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
    auto ctx_reader_archive = c2pa::Context();
    auto reader_archive = c2pa::Reader(ctx_reader_archive, "image/jpeg", dest_archive);
    auto json_archive = json::parse(reader_archive.json());
    EXPECT_TRUE(json_archive.contains("active_manifest"));

    // Verify that the archive round-trip manifest HAS a thumbnail
    // (because from_archive() uses default settings which enable thumbnails)
    std::string active_archive = json_archive["active_manifest"];
    EXPECT_TRUE(json_archive["manifests"][active_archive].contains("thumbnail"))
        << "Archive round-trip (default context on Builder) should generate thumbnail with default settings";
}

TEST_F(BuilderTest, LoadArchiveWithContext)
{
    auto manifest = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));

    // Create a context with thumbnail generation disabled
    auto context = c2pa::Context(R"({"builder": {"thumbnail": {"enabled": false}}})");

    // Create a builder with the custom context
    auto builder1 = c2pa::Builder(context, manifest);

    // Add an ingredient
    std::string ingredient_json = R"({"title": "C.jpg Ingredient"})";
    builder1.add_ingredient(ingredient_json, c2pa_test::get_fixture_path("C.jpg"));

    // Export to archive
    std::stringstream archive_stream(std::ios::in | std::ios::out | std::ios::binary);
    EXPECT_NO_THROW({
        builder1.to_archive(archive_stream);
    });

    // Create a new builder with a context, then load the archive into it
    // This preserves the context's settings (thumbnail=false)
    auto context2 = c2pa::Context(R"({"builder": {"thumbnail": {"enabled": false}}})");

    archive_stream.seekg(0);
    c2pa::Builder builder2(context2);  // Create a builder with context
    EXPECT_NO_THROW({
        builder2.to_archive(archive_stream);  // Load an archive into the builder
    });

    // Sign with the restored builder (which has thumbnail=false from context2)
    auto signer = c2pa_test::create_test_signer();

    std::ifstream source2(c2pa_test::get_fixture_path("A.jpg"), std::ios::binary);
    ASSERT_TRUE(source2.is_open());
    std::stringstream dest_stream(std::ios::in | std::ios::out | std::ios::binary);
    EXPECT_NO_THROW({
        builder2.sign("image/jpeg", source2, dest_stream, signer);
    });

    // Verify the signed asset has NO thumbnail (because context2 had thumbnail=false)
    dest_stream.seekg(0);
    auto ctx_reader = c2pa::Context();
    auto reader = c2pa::Reader(ctx_reader, "image/jpeg", dest_stream);
    auto json_result = json::parse(reader.json());
    EXPECT_TRUE(json_result.contains("active_manifest"));

    std::string active_manifest = json_result["active_manifest"];
    EXPECT_FALSE(json_result["manifests"][active_manifest].contains("thumbnail"))
        << "Builder loaded with custom context should respect thumbnail=false setting";
}

// Test adding multiple builder archives that got signed as assets, back as ingredients
TEST_F(BuilderTest, MultipleArchivesAsIngredients)
{
    auto manifest = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto signer = c2pa_test::create_test_signer();

    // Create first builder and export to archive
    auto builder1 = c2pa::Builder(manifest);
    std::stringstream archive1_stream(std::ios::in | std::ios::out | std::ios::binary);
    EXPECT_NO_THROW({
        builder1.to_archive(archive1_stream);
    });

    // Create second builder and export to archive
    auto builder2 = c2pa::Builder(manifest);
    std::stringstream archive2_stream(std::ios::in | std::ios::out | std::ios::binary);
    EXPECT_NO_THROW({
        builder2.to_archive(archive2_stream);
    });

    // Create third builder and export to archive
    auto builder3 = c2pa::Builder(manifest);
    std::stringstream archive3_stream(std::ios::in | std::ios::out | std::ios::binary);
    EXPECT_NO_THROW({
        builder3.to_archive(archive3_stream);
    });

    // Load each archive and sign to create assets that can be reused as ingredients
    // Sign first archive: signed asset 1
    archive1_stream.seekg(0);
    auto loaded_builder1 = c2pa::Builder::from_archive(archive1_stream);
    std::ifstream source1(c2pa_test::get_fixture_path("C.jpg"), std::ios::binary);
    ASSERT_TRUE(source1.is_open());
    std::stringstream signed_asset1(std::ios::in | std::ios::out | std::ios::binary);
    EXPECT_NO_THROW({
        loaded_builder1.sign("image/jpeg", source1, signed_asset1, signer);
    });

    // Sign second archive: signed asset 2
    archive2_stream.seekg(0);
    auto loaded_builder2 = c2pa::Builder::from_archive(archive2_stream);
    std::ifstream source2(c2pa_test::get_fixture_path("A.jpg"), std::ios::binary);
    ASSERT_TRUE(source2.is_open());
    std::stringstream signed_asset2(std::ios::in | std::ios::out | std::ios::binary);
    EXPECT_NO_THROW({
        loaded_builder2.sign("image/jpeg", source2, signed_asset2, signer);
    });

    // Sign third archive: signed asset 3
    archive3_stream.seekg(0);
    auto loaded_builder3 = c2pa::Builder::from_archive(archive3_stream);
    std::ifstream source3(c2pa_test::get_fixture_path("sample1.gif"), std::ios::binary);
    ASSERT_TRUE(source3.is_open());
    std::stringstream signed_asset3(std::ios::in | std::ios::out | std::ios::binary);
    EXPECT_NO_THROW({
        loaded_builder3.sign("image/gif", source3, signed_asset3, signer);
    });

    // Create a final builder and add all three signed assets as ingredients with different relationships
    auto final_builder = c2pa::Builder(manifest);

    // Add first signed asset as parentOf ingredient
    signed_asset1.seekg(0);
    std::string ingredient1_json = R"({"title": "Archive 1 Ingredient", "relationship": "parentOf"})";
    EXPECT_NO_THROW({
        final_builder.add_ingredient(ingredient1_json, "image/jpeg", signed_asset1);
    });

    // Add second signed asset as componentOf ingredient
    signed_asset2.seekg(0);
    std::string ingredient2_json = R"({"title": "Archive 2 Ingredient", "relationship": "componentOf"})";
    EXPECT_NO_THROW({
        final_builder.add_ingredient(ingredient2_json, "image/jpeg", signed_asset2);
    });

    // Add third signed asset as componentOf ingredient
    signed_asset3.seekg(0);
    std::string ingredient3_json = R"({"title": "Archive 3 Ingredient", "relationship": "componentOf"})";
    EXPECT_NO_THROW({
        final_builder.add_ingredient(ingredient3_json, "image/gif", signed_asset3);
    });

    // Sign the final builder
    std::ifstream final_source(c2pa_test::get_fixture_path("A.jpg"), std::ios::binary);
    ASSERT_TRUE(final_source.is_open());
    std::stringstream dest_stream(std::ios::in | std::ios::out | std::ios::binary);
    EXPECT_NO_THROW({
        final_builder.sign("image/jpeg", final_source, dest_stream, signer);
    });

    // Verify all three ingredients are present with correct relationships
    dest_stream.seekg(0);
    auto ctx_reader = c2pa::Context();
    auto reader = c2pa::Reader(ctx_reader, "image/jpeg", dest_stream);
    auto json_result = json::parse(reader.json());
    EXPECT_TRUE(json_result.contains("active_manifest"));

    std::string active = json_result["active_manifest"];
    ASSERT_TRUE(json_result["manifests"][active].contains("ingredients"));

    auto ingredients = json_result["manifests"][active]["ingredients"];
    EXPECT_EQ(ingredients.size(), 3) << "Should have exactly 3 ingredients";

    // Verify ingredient titles and relationships are preserved
    std::map<std::string, std::string> ingredient_relationships;
    for (const auto& ingredient : ingredients) {
        EXPECT_TRUE(ingredient.contains("title"));
        EXPECT_TRUE(ingredient.contains("relationship"));
        ingredient_relationships[ingredient["title"]] = ingredient["relationship"];
    }

    EXPECT_EQ(ingredient_relationships["Archive 1 Ingredient"], "parentOf")
        << "First ingredient should have parentOf relationship";
    EXPECT_EQ(ingredient_relationships["Archive 2 Ingredient"], "componentOf")
        << "Second ingredient should have componentOf relationship";
    EXPECT_EQ(ingredient_relationships["Archive 3 Ingredient"], "componentOf")
        << "Third ingredient should have componentOf relationship";
}

TEST_F(BuilderTest, WithDefinitionUpdatesManifest) {
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path manifest_path = current_dir / "fixtures/training.json";

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto context = c2pa::Context();

    c2pa::Builder builder(context);
    builder.with_definition(manifest);

    fs::path asset_path = current_dir / "fixtures/A.jpg";
    fs::path dest_path = get_temp_path("test_with_definition_output.jpg");
    fs::path cert_path = current_dir / "fixtures/es256_certs.pem";
    fs::path key_path = current_dir / "fixtures/es256_private.key";

    auto certs = c2pa_test::read_text_file(cert_path);
    auto private_key = c2pa_test::read_text_file(key_path);
    c2pa::Signer signer("es256", certs, private_key);

    builder.sign(asset_path, dest_path, signer);

    c2pa::Reader reader(context, dest_path);
    auto json_result = json::parse(reader.json());

    std::string active = json_result["active_manifest"];
    auto claim_generator_info = json_result["manifests"][active]["claim_generator_info"];

    // Verify the manifest definition from training.json was applied
    ASSERT_TRUE(claim_generator_info.is_array());
    ASSERT_GT(claim_generator_info.size(), 0);
    EXPECT_EQ(claim_generator_info[0]["name"], "c2pa-c test");
}

TEST_F(BuilderTest, WithDefinitionChaining) {
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path manifest_path = current_dir / "fixtures/training.json";

    auto initial_manifest = R"({
      "claim_generator_info": [
        {
          "name": "initial-value",
          "version": "0.1"
        }
      ]
    })";
    auto updated_manifest = c2pa_test::read_text_file(manifest_path);
    auto context = c2pa::Context();

    c2pa::Builder builder(context, initial_manifest);
    builder.with_definition(updated_manifest);

    // Sign and verify the updated definition was applied
    fs::path asset_path = current_dir / "fixtures/A.jpg";
    fs::path dest_path = get_temp_path("test_with_definition_chaining_output.jpg");
    fs::path cert_path = current_dir / "fixtures/es256_certs.pem";
    fs::path key_path = current_dir / "fixtures/es256_private.key";

    auto certs = c2pa_test::read_text_file(cert_path);
    auto private_key = c2pa_test::read_text_file(key_path);
    c2pa::Signer signer("es256", certs, private_key);

    builder.sign(asset_path, dest_path, signer);

    c2pa::Reader reader(context, dest_path);
    auto json_result = json::parse(reader.json());

    std::string active = json_result["active_manifest"];
    auto claim_generator_info = json_result["manifests"][active]["claim_generator_info"];

    // Verify updated definition (training.json) was applied, not initial
    ASSERT_TRUE(claim_generator_info.is_array());
    ASSERT_GT(claim_generator_info.size(), 0);
    EXPECT_EQ(claim_generator_info[0]["name"], "c2pa-c test");
    EXPECT_NE(claim_generator_info[0]["name"], "initial-value");
}

TEST_F(BuilderTest, WithDefinitionInvalidJsonThrows) {
    auto context = c2pa::Context();
    c2pa::Builder builder(context);

    // Invalid JSON should throw C2paException
    auto invalid_manifest = "{ invalid json ]";
    EXPECT_THROW({
        builder.with_definition(invalid_manifest);
    }, c2pa::C2paException);

    // Builder should still be usable after failed with_definition
    // (the fix ensures old pointer is nulled before checking result,
    // preventing double-free in destructor)
}

TEST_F(BuilderTest, ArchiveToFilePath) {
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path manifest_path = current_dir / "fixtures/training.json";
    fs::path archive_path = get_temp_path("test_archive.c2pa");

    auto manifest = c2pa_test::read_text_file(manifest_path);
    c2pa::Builder builder(manifest);

    builder.to_archive(archive_path);

    EXPECT_TRUE(fs::exists(archive_path));
    EXPECT_GT(fs::file_size(archive_path), 0);
}

TEST_F(BuilderTest, ExtractIngredientsFromArchive) {
  // Helper function to transfer ingredients from an archive to a new builder
  auto create_builder_with_ingredients_from_archive = [](
      std::istream& archive_stream,
      const std::string& base_manifest_json) -> c2pa::Builder {

      // Create a Reader from the archive
      c2pa::Reader reader("application/c2pa", archive_stream);
      auto json_result = reader.json();

      // Parse the archive JSON to extract ingredient information
      auto parsed = json::parse(json_result);
      std::string active = parsed["active_manifest"];
      auto ingredients = parsed["manifests"][active]["ingredients"];

      // Create a builder with the ingredients injected into the manifest definition
      json manifest_json = json::parse(base_manifest_json);
      manifest_json["ingredients"] = ingredients;
      c2pa::Builder builder(manifest_json.dump());

      // Add all referenced resources for each ingredient
      for (size_t i = 0; i < ingredients.size(); i++) {
          const auto& ingredient = ingredients[i];
          std::string title = ingredient["title"];

          // Copy thumbnail resource if present
          if (ingredient.contains("thumbnail") && ingredient["thumbnail"].contains("identifier")) {
              std::string thumbnail_id = ingredient["thumbnail"]["identifier"];

              std::stringstream thumbnail_stream(std::ios::in | std::ios::out | std::ios::binary);
              reader.get_resource(thumbnail_id, thumbnail_stream);

              thumbnail_stream.seekg(0);
              builder.add_resource(thumbnail_id, thumbnail_stream);
          }

          // Copy manifest_data resource if present (C2PA-signed ingredients)
          if (ingredient.contains("manifest_data") &&
              ingredient["manifest_data"].is_object() &&
              ingredient["manifest_data"].contains("identifier")) {
              std::string manifest_data_id = ingredient["manifest_data"]["identifier"];

              std::stringstream manifest_data_stream(std::ios::in | std::ios::out | std::ios::binary);
              reader.get_resource(manifest_data_id, manifest_data_stream);

              manifest_data_stream.seekg(0);
              builder.add_resource(manifest_data_id, manifest_data_stream);
          }
      }

      return builder;
  };
  auto manifest = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));

  // Create a builder with 3 ingredients: A.jpg, C.jpg, and sample1.gif
  auto builder = c2pa::Builder(manifest);

  std::string ingredient1_json = R"({"title": "A.jpg", "relationship": "parentOf"})";
  builder.add_ingredient(ingredient1_json, c2pa_test::get_fixture_path("A.jpg"));

  std::string ingredient2_json = R"({"title": "C.jpg", "relationship": "componentOf"})";
  builder.add_ingredient(ingredient2_json, c2pa_test::get_fixture_path("C.jpg"));

  std::string ingredient3_json = R"({"title": "sample.gif", "relationship": "componentOf"})";
  builder.add_ingredient(ingredient3_json, c2pa_test::get_fixture_path("sample1.gif"));

  // Archive the builder
  std::stringstream archive_stream(std::ios::in | std::ios::out | std::ios::binary);
  EXPECT_NO_THROW({
      builder.to_archive(archive_stream);
  });

  // Skip verification since archives have placeholder signatures
  c2pa::load_settings(R"({"verify": {"verify_after_reading": false}})", "json");

  // Use the helper function to create a builder with ingredients from the archive
  archive_stream.seekg(0);
  auto merged_builder = create_builder_with_ingredients_from_archive(archive_stream, manifest);

  // Sign the merged builder
  auto signer = c2pa_test::create_test_signer();
  auto source_path = c2pa_test::get_fixture_path("A.jpg");
  auto output_path = get_temp_path("merged_output.jpg");

  std::vector<unsigned char> manifest_data;
  EXPECT_NO_THROW({
      manifest_data = merged_builder.sign(source_path, output_path, signer);
  });
  ASSERT_FALSE(manifest_data.empty());

  // Read and log the merged builder's manifest JSON
  auto merged_reader = c2pa::Reader(output_path);
  auto merged_json = merged_reader.json();

  // Verify all 3 ingredients are present in the merged builder
  auto merged_parsed = json::parse(merged_json);
  std::string merged_active = merged_parsed["active_manifest"];
  auto merged_ingredients = merged_parsed["manifests"][merged_active]["ingredients"];
  EXPECT_EQ(merged_ingredients.size(), 3) << "Merged builder should have all 3 ingredients";

  // Reset settings
  c2pa::load_settings(R"({"verify": {"verify_after_reading": true}})", "json");
}

TEST_F(BuilderTest, ExtractIngredientsFromArchiveToBuilder) {
  auto manifest = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));

  // Helper function that adds ingredients from an archived builder into an existing builder.
  // Can be called multiple times on the same builder to accumulate ingredients from multiple archives.
  auto create_builder_with_archived_ingredients = [&manifest](
      c2pa::Builder& builder,
      std::istream& archive_stream) {

      // Archive the current builder to capture its existing ingredients and resources
      std::stringstream builder_archive(std::ios::in | std::ios::out | std::ios::binary);
      builder.to_archive(builder_archive);
      builder_archive.seekg(0);

      c2pa::Reader builder_reader("application/c2pa", builder_archive);
      auto builder_parsed = json::parse(builder_reader.json());
      std::string builder_active = builder_parsed["active_manifest"];
      json current_ingredients = builder_parsed["manifests"][builder_active].value("ingredients", json::array());

      // Read the incoming archive to get its ingredients
      c2pa::Reader archive_reader("application/c2pa", archive_stream);
      auto archive_parsed = json::parse(archive_reader.json());
      std::string archive_active = archive_parsed["active_manifest"];
      auto new_ingredients = archive_parsed["manifests"][archive_active]["ingredients"];

      // Collect all resource identifiers already in use by current ingredients
      std::set<std::string> used_ids;
      for (const auto& ing : current_ingredients) {
          if (ing.contains("thumbnail") && ing["thumbnail"].contains("identifier"))
              used_ids.insert(ing["thumbnail"]["identifier"].get<std::string>());
          if (ing.contains("manifest_data") && ing["manifest_data"].is_object() && ing["manifest_data"].contains("identifier"))
              used_ids.insert(ing["manifest_data"]["identifier"].get<std::string>());
      }

      // Helper to generate a unique identifier by appending/incrementing a __N suffix
      int suffix_counter = static_cast<int>(current_ingredients.size());
      auto make_unique_id = [&](const std::string& original_id) -> std::string {
          if (used_ids.find(original_id) == used_ids.end()) {
              used_ids.insert(original_id);
              return original_id;
          }
          // Strip any existing __N suffix to get the base identifier
          std::string base = original_id;
          auto pos = base.rfind("__");
          if (pos != std::string::npos && pos + 2 < base.size()) {
              bool all_digits = std::all_of(base.begin() + static_cast<long>(pos) + 2, base.end(), ::isdigit);
              if (all_digits) base = base.substr(0, pos);
          }
          std::string new_id;
          do {
              new_id = base + "__" + std::to_string(suffix_counter++);
          } while (used_ids.find(new_id) != used_ids.end());
          used_ids.insert(new_id);
          return new_id;
      };

      // Remap conflicting identifiers in new_ingredients and collect (source_id, dest_id) pairs
      // for resource copying. source_id is the original ID in the archive reader, dest_id is the
      // (possibly remapped) ID that goes into the new builder.
      struct ResourceCopy { std::string source_id; std::string dest_id; };
      std::vector<ResourceCopy> new_resource_copies;

      for (auto& ingredient : new_ingredients) {
          if (ingredient.contains("thumbnail") && ingredient["thumbnail"].contains("identifier")) {
              std::string old_id = ingredient["thumbnail"]["identifier"].get<std::string>();
              std::string new_id = make_unique_id(old_id);
              if (new_id != old_id) {
                  ingredient["thumbnail"]["identifier"] = new_id;
              }
              new_resource_copies.push_back({old_id, new_id});
          }
          if (ingredient.contains("manifest_data") &&
              ingredient["manifest_data"].is_object() &&
              ingredient["manifest_data"].contains("identifier")) {
              std::string old_id = ingredient["manifest_data"]["identifier"].get<std::string>();
              std::string new_id = make_unique_id(old_id);
              if (new_id != old_id) {
                  ingredient["manifest_data"]["identifier"] = new_id;
              }
              new_resource_copies.push_back({old_id, new_id});
          }
      }

      // Merge all ingredients (current + remapped new)
      json all_ingredients = current_ingredients;
      for (const auto& ing : new_ingredients) {
          all_ingredients.push_back(ing);
      }

      // Create a new builder with the merged ingredients injected into the manifest definition
      json manifest_json = json::parse(manifest);
      manifest_json["ingredients"] = all_ingredients;
      c2pa::Builder new_builder(manifest_json.dump());

      // Copy resources from the current builder's archive (existing ingredients, no remapping needed)
      for (const auto& ingredient : current_ingredients) {
          if (ingredient.contains("thumbnail") && ingredient["thumbnail"].contains("identifier")) {
              std::string id = ingredient["thumbnail"]["identifier"];
              std::stringstream stream(std::ios::in | std::ios::out | std::ios::binary);
              builder_reader.get_resource(id, stream);
              stream.seekg(0);
              new_builder.add_resource(id, stream);
          }
          if (ingredient.contains("manifest_data") &&
              ingredient["manifest_data"].is_object() &&
              ingredient["manifest_data"].contains("identifier")) {
              std::string id = ingredient["manifest_data"]["identifier"];
              std::stringstream stream(std::ios::in | std::ios::out | std::ios::binary);
              builder_reader.get_resource(id, stream);
              stream.seekg(0);
              new_builder.add_resource(id, stream);
          }
      }

      // Copy resources from the new archive (using source_id to read, dest_id to write)
      for (const auto& rc : new_resource_copies) {
          std::stringstream stream(std::ios::in | std::ios::out | std::ios::binary);
          archive_reader.get_resource(rc.source_id, stream);
          stream.seekg(0);
          new_builder.add_resource(rc.dest_id, stream);
      }

      // Replace the original builder with the new one via move assignment
      builder = std::move(new_builder);
  };

  // Archive 1: A.jpg and C.jpg
  auto builder1 = c2pa::Builder(manifest);

  std::string ingredient1_json = R"({"title": "A.jpg", "relationship": "parentOf"})";
  builder1.add_ingredient(ingredient1_json, c2pa_test::get_fixture_path("A.jpg"));

  std::string ingredient2_json = R"({"title": "C.jpg", "relationship": "componentOf"})";
  builder1.add_ingredient(ingredient2_json, c2pa_test::get_fixture_path("C.jpg"));

  std::stringstream archive1_stream(std::ios::in | std::ios::out | std::ios::binary);
  EXPECT_NO_THROW({
      builder1.to_archive(archive1_stream);
  });

  // Archive 2: sample1.gif
  auto builder2 = c2pa::Builder(manifest);

  std::string ingredient3_json = R"({"title": "sample.gif", "relationship": "componentOf"})";
  builder2.add_ingredient(ingredient3_json, c2pa_test::get_fixture_path("sample1.gif"));

  std::stringstream archive2_stream(std::ios::in | std::ios::out | std::ios::binary);
  EXPECT_NO_THROW({
      builder2.to_archive(archive2_stream);
  });

  // Skip verification since archives have placeholder signatures
  c2pa::load_settings(R"({"verify": {"verify_after_reading": false}})", "json");

  auto merged_builder = c2pa::Builder(manifest);

  // First call: add ingredients from archive 1 (A.jpg, C.jpg)
  archive1_stream.seekg(0);
  create_builder_with_archived_ingredients(merged_builder, archive1_stream);

  // Second call: add ingredients from archive 2 (sample.gif) to the same builder
  archive2_stream.seekg(0);
  create_builder_with_archived_ingredients(merged_builder, archive2_stream);

  // Sign the merged builder
  auto signer = c2pa_test::create_test_signer();
  auto source_path = c2pa_test::get_fixture_path("A.jpg");
  auto output_path = get_temp_path("merged_output2.jpg");

  std::vector<unsigned char> manifest_data;
  EXPECT_NO_THROW({
      manifest_data = merged_builder.sign(source_path, output_path, signer);
  });
  ASSERT_FALSE(manifest_data.empty());

  // Read and log the merged builder's manifest JSON
  auto merged_reader = c2pa::Reader(output_path);
  auto merged_json = merged_reader.json();

  // Verify all 3 ingredients are present from both archives
  auto merged_parsed = json::parse(merged_json);
  std::string merged_active = merged_parsed["active_manifest"];
  auto merged_ingredients = merged_parsed["manifests"][merged_active]["ingredients"];
  EXPECT_EQ(merged_ingredients.size(), 3) << "Merged builder should have all 3 ingredients from both archives";

  // Reset settings
  c2pa::load_settings(R"({"verify": {"verify_after_reading": true}})", "json");
}

TEST_F(BuilderTest, ExtractIngredientsFromArchives) {
  // Helper that creates a builder from multiple archives, merging all their ingredients.
  auto create_builder_with_ingredients_from_archives = [](
      std::vector<std::reference_wrapper<std::istream>>& archive_streams,
      const std::string& base_manifest_json) -> c2pa::Builder {

      // Collect ingredients and their associated readers from all archives.
      // Each entry pairs an ingredient JSON (possibly with remapped identifiers)
      // with a pointer to the Reader it came from, so we can copy resources later.
      struct IngredientSource {
          json ingredient;          // ingredient JSON (with remapped IDs if needed)
          std::string source_id;    // original resource ID in the archive (for thumbnail)
          std::string dest_id;      // destination resource ID in the builder (possibly remapped)
          std::string md_source_id; // original manifest_data ID (empty if none)
          std::string md_dest_id;   // destination manifest_data ID (possibly remapped)
      };

      // Keep readers alive for the duration of resource copying
      std::vector<std::unique_ptr<c2pa::Reader>> readers;
      std::vector<std::vector<IngredientSource>> per_archive_sources;
      json all_ingredients = json::array();

      // Track used resource identifiers across all archives to avoid collisions
      std::set<std::string> used_ids;
      int suffix_counter = 0;

      auto make_unique_id = [&](const std::string& original_id) -> std::string {
          if (used_ids.find(original_id) == used_ids.end()) {
              used_ids.insert(original_id);
              return original_id;
          }
          std::string base = original_id;
          auto pos = base.rfind("__");
          if (pos != std::string::npos && pos + 2 < base.size()) {
              bool all_digits = std::all_of(base.begin() + static_cast<long>(pos) + 2, base.end(), ::isdigit);
              if (all_digits) base = base.substr(0, pos);
          }
          std::string new_id;
          do {
              new_id = base + "__" + std::to_string(suffix_counter++);
          } while (used_ids.find(new_id) != used_ids.end());
          used_ids.insert(new_id);
          return new_id;
      };

      // Process each archive: extract ingredients, remap conflicting IDs
      for (auto& stream_ref : archive_streams) {
          auto reader = std::make_unique<c2pa::Reader>("application/c2pa", stream_ref.get());
          auto parsed = json::parse(reader->json());
          std::string active = parsed["active_manifest"];
          auto ingredients = parsed["manifests"][active]["ingredients"];

          std::vector<IngredientSource> sources;
          for (auto& ingredient : ingredients) {
              IngredientSource src;

              // Remap thumbnail identifier if it conflicts
              if (ingredient.contains("thumbnail") && ingredient["thumbnail"].contains("identifier")) {
                  std::string old_id = ingredient["thumbnail"]["identifier"].get<std::string>();
                  std::string new_id = make_unique_id(old_id);
                  if (new_id != old_id) {
                      ingredient["thumbnail"]["identifier"] = new_id;
                  }
                  src.source_id = old_id;
                  src.dest_id = new_id;
              }

              // Remap manifest_data identifier if it conflicts
              if (ingredient.contains("manifest_data") &&
                  ingredient["manifest_data"].is_object() &&
                  ingredient["manifest_data"].contains("identifier")) {
                  std::string old_id = ingredient["manifest_data"]["identifier"].get<std::string>();
                  std::string new_id = make_unique_id(old_id);
                  if (new_id != old_id) {
                      ingredient["manifest_data"]["identifier"] = new_id;
                  }
                  src.md_source_id = old_id;
                  src.md_dest_id = new_id;
              }

              src.ingredient = ingredient;
              all_ingredients.push_back(ingredient);
              sources.push_back(std::move(src));
          }

          per_archive_sources.push_back(std::move(sources));
          readers.push_back(std::move(reader));
      }

      // Create the builder with all ingredients injected into the manifest definition
      json manifest_json = json::parse(base_manifest_json);
      manifest_json["ingredients"] = all_ingredients;
      c2pa::Builder builder(manifest_json.dump());

      // Copy all resources from each archive into the builder
      for (size_t a = 0; a < readers.size(); a++) {
          for (const auto& src : per_archive_sources[a]) {
              // Copy thumbnail
              if (!src.source_id.empty()) {
                  std::stringstream stream(std::ios::in | std::ios::out | std::ios::binary);
                  readers[a]->get_resource(src.source_id, stream);
                  stream.seekg(0);
                  builder.add_resource(src.dest_id, stream);
              }
              // Copy manifest_data
              if (!src.md_source_id.empty()) {
                  std::stringstream stream(std::ios::in | std::ios::out | std::ios::binary);
                  readers[a]->get_resource(src.md_source_id, stream);
                  stream.seekg(0);
                  builder.add_resource(src.md_dest_id, stream);
              }
          }
      }

      return builder;
  };

  auto manifest = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));

  // Archive 1: A.jpg and C.jpg
  auto builder1 = c2pa::Builder(manifest);

  std::string ingredient1_json = R"({"title": "A.jpg", "relationship": "parentOf"})";
  builder1.add_ingredient(ingredient1_json, c2pa_test::get_fixture_path("A.jpg"));

  std::string ingredient2_json = R"({"title": "C.jpg", "relationship": "componentOf"})";
  builder1.add_ingredient(ingredient2_json, c2pa_test::get_fixture_path("C.jpg"));

  std::stringstream archive1_stream(std::ios::in | std::ios::out | std::ios::binary);
  EXPECT_NO_THROW({
      builder1.to_archive(archive1_stream);
  });

  // Archive 2: sample1.gif
  auto builder2 = c2pa::Builder(manifest);

  std::string ingredient3_json = R"({"title": "sample.gif", "relationship": "componentOf"})";
  builder2.add_ingredient(ingredient3_json, c2pa_test::get_fixture_path("sample1.gif"));

  std::stringstream archive2_stream(std::ios::in | std::ios::out | std::ios::binary);
  EXPECT_NO_THROW({
      builder2.to_archive(archive2_stream);
  });

  // Skip verification since archives have placeholder signatures
  c2pa::load_settings(R"({"verify": {"verify_after_reading": false}})", "json");

  // Use the helper to create a builder from both archives at once
  archive1_stream.seekg(0);
  archive2_stream.seekg(0);
  std::vector<std::reference_wrapper<std::istream>> archives = { archive1_stream, archive2_stream };
  auto merged_builder = create_builder_with_ingredients_from_archives(archives, manifest);

  // Sign the merged builder
  auto signer = c2pa_test::create_test_signer();
  auto source_path = c2pa_test::get_fixture_path("A.jpg");
  auto output_path = get_temp_path("merged_from_archives.jpg");

  std::vector<unsigned char> manifest_data;
  EXPECT_NO_THROW({
      manifest_data = merged_builder.sign(source_path, output_path, signer);
  });
  ASSERT_FALSE(manifest_data.empty());

  // Read and verify the merged output
  auto merged_reader = c2pa::Reader(output_path);
  auto merged_json = merged_reader.json();

  // Verify all 3 ingredients are present from both archives
  auto merged_parsed = json::parse(merged_json);
  std::string merged_active = merged_parsed["active_manifest"];
  auto merged_ingredients = merged_parsed["manifests"][merged_active]["ingredients"];
  EXPECT_EQ(merged_ingredients.size(), 3) << "Merged builder should have all 3 ingredients from both archives";

  // Reset settings
  c2pa::load_settings(R"({"verify": {"verify_after_reading": true}})", "json");
}
