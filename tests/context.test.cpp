// Copyright 2026 Adobe. All rights reserved.
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

#include <gtest/gtest.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <nlohmann/json.hpp>

#include "c2pa.hpp"
#include "include/test_utils.hpp"

namespace fs = std::filesystem;

// Test fixture for context tests with automatic cleanup
class ContextTest : public ::testing::Test {
protected:
    std::vector<fs::path> temp_files;
    bool cleanup_temp_files = true;  // Set to false to keep temp files for debugging

    // Get path for temp context test files in build directory
    fs::path get_temp_path(const std::string& name) {
        fs::path current_dir = fs::path(__FILE__).parent_path();
        fs::path build_dir = current_dir.parent_path() / "build";
        if (!fs::exists(build_dir)) {
            fs::create_directories(build_dir);
        }
        fs::path temp_path = build_dir / ("context-" + name);
        temp_files.push_back(temp_path);
        return temp_path;
    }

    void TearDown() override {
        if (cleanup_temp_files) {
            for (const auto& path : temp_files) {
                if (fs::exists(path)) {
                    fs::remove(path);
                }
            }
        }
        temp_files.clear();
    }
};

static fs::path fixture_path(const std::string &name)
{
    return c2pa_test::get_fixture_path(name);
}

static std::string load_fixture(const std::string &name)
{
    return c2pa_test::read_text_file(fixture_path(name));
}

// Can create a context using JSON settings
TEST(Context, ContextFromJsonValid)
{
    std::string json = R"({"settings": {}})";
    c2pa::Context context(json);
    EXPECT_TRUE(context.has_context());
}

// Can create a context using Settings object
TEST(Context, ContextFromSettingsValid)
{
    c2pa::Settings settings;
    settings.set("builder.thumbnail.enabled", "false");
    c2pa::Context context(settings);
    EXPECT_TRUE(context.has_context());
}

// Context with invalid JSON throws
TEST(Context, ContextFromJsonInvalidThrows)
{
    EXPECT_THROW(
        { c2pa::Context context("{bad"); },
        c2pa::C2paException
    );
}

// Default context can be used with a Builder
TEST(Context, SettingsDefaultConstruction)
{
    c2pa::Settings settings;
    auto manifest = load_fixture("training.json");
    c2pa::Context context;

    // Should not crash when building with default settings
    EXPECT_NO_THROW({
        c2pa::Builder builder(context, manifest);
    });
}

// Can update settings with any valid JSON
TEST(Context, SettingsUpdateJson)
{
    c2pa::Settings settings;
    EXPECT_NO_THROW({
        settings.update(R"({"key": "val"})", "json");
    });
}

// ContextBuilder empty build returns default/empty context
TEST(Context, ContextBuilderEmptyBuild)
{
    auto builder = c2pa::Context::ContextBuilder();
    auto context = builder.create_context();

    EXPECT_TRUE(context.has_context());
}

// Helper function to check if thumbnail is present in signed manifest
static bool has_thumbnail(const std::string& manifest_json) {
    auto parsed = nlohmann::json::parse(manifest_json);
    std::string active = parsed["active_manifest"];
    return parsed["manifests"][active].contains("thumbnail");
}

// Helper function to sign with context and return manifest JSON
static std::string sign_with_context(c2pa::IContextProvider& context, const fs::path& dest_path) {
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path manifest_path = current_dir / "fixtures/training.json";
    fs::path asset_path = current_dir / "fixtures/A.jpg";
    fs::path cert_path = current_dir / "fixtures/es256_certs.pem";
    fs::path key_path = current_dir / "fixtures/es256_private.key";

    auto manifest = c2pa_test::read_text_file(manifest_path);
    auto certs = c2pa_test::read_text_file(cert_path);
    auto private_key = c2pa_test::read_text_file(key_path);

    c2pa::Builder builder(context, manifest);
    c2pa::Signer signer("es256", certs, private_key);

    builder.sign(asset_path, dest_path, signer);

    c2pa::Reader reader(context, dest_path);
    auto result = reader.json();

    return result;
}

TEST_F(ContextTest, SetOverridesLastWins) {
    c2pa::Settings settings;
    settings.set("builder.thumbnail.enabled", "true");
    settings.set("builder.thumbnail.enabled", "false");

    auto context = c2pa::Context::ContextBuilder().with_settings(settings).create_context();
    auto manifest_json = sign_with_context(context, get_temp_path("set_overrides_last_wins.jpg"));

    EXPECT_FALSE(has_thumbnail(manifest_json));
}

TEST_F(ContextTest, UpdateOverridesSetJson) {
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path settings_path = current_dir / "fixtures/settings/test_settings_no_thumbnail.json";
    auto settings_json = c2pa_test::read_text_file(settings_path);

    c2pa::Settings settings;
    settings.set("builder.thumbnail.enabled", "true");
    settings.update(settings_json, "json");

    auto context = c2pa::Context::ContextBuilder().with_settings(settings).create_context();
    auto manifest_json = sign_with_context(context, get_temp_path("update_overrides_set_json.jpg"));

    EXPECT_FALSE(has_thumbnail(manifest_json));
}

TEST_F(ContextTest, SetOverridesUpdateJson) {
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path settings_path = current_dir / "fixtures/settings/test_settings_no_thumbnail.json";
    auto settings_json = c2pa_test::read_text_file(settings_path);

    c2pa::Settings settings;
    settings.update(settings_json, "json");
    settings.set("builder.thumbnail.enabled", "true");

    auto context = c2pa::Context::ContextBuilder().with_settings(settings).create_context();
    auto manifest_json = sign_with_context(context, get_temp_path("set_overrides_update_json.jpg"));

    EXPECT_TRUE(has_thumbnail(manifest_json));
}

TEST_F(ContextTest, WithSettingsThenWithJson) {
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path json_path = current_dir / "fixtures/settings/test_settings_no_thumbnail.json";
    auto settings_json = c2pa_test::read_text_file(json_path);

    c2pa::Settings settings;
    settings.set("builder.thumbnail.enabled", "true");

    auto context = c2pa::Context::ContextBuilder()
        .with_settings(settings)
        .with_json(settings_json)
        .create_context();

    auto manifest_json = sign_with_context(context, get_temp_path("with_settings_then_json.jpg"));
    EXPECT_FALSE(has_thumbnail(manifest_json));
}

TEST_F(ContextTest, WithJsonThenWithSettings) {
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path json_path = current_dir / "fixtures/settings/test_settings_with_thumbnail.json";
    auto settings_json = c2pa_test::read_text_file(json_path);

    c2pa::Settings settings;
    settings.set("builder.thumbnail.enabled", "false");

    auto context = c2pa::Context::ContextBuilder()
        .with_json(settings_json)
        .with_settings(settings)
        .create_context();

    auto manifest_json = sign_with_context(context, get_temp_path("with_json_then_settings.jpg"));
    EXPECT_FALSE(has_thumbnail(manifest_json));
}

TEST(Context, ContextBuilderMoveConstructor) {
    auto b1 = c2pa::Context::ContextBuilder();
    auto b2 = std::move(b1);

    EXPECT_FALSE(b1.is_valid());
    EXPECT_TRUE(b2.is_valid());

    auto context = b2.create_context();
    EXPECT_TRUE(context.has_context());
}

TEST(Context, ContextBuilderMoveAssignment) {
    auto b1 = c2pa::Context::ContextBuilder();
    auto b2 = c2pa::Context::ContextBuilder();

    b1 = std::move(b2);

    EXPECT_FALSE(b2.is_valid());
    EXPECT_TRUE(b1.is_valid());

    auto context = b1.create_context();
    EXPECT_TRUE(context.has_context());
}

TEST(Context, SettingsMoveConstructor) {
    c2pa::Settings s1;
    s1.set("builder.thumbnail.enabled", "false");

    auto s2 = std::move(s1);

    EXPECT_EQ(s1.c_settings(), nullptr);

    // Verify s2 is functional
    auto context = c2pa::Context::ContextBuilder().with_settings(s2).create_context();
    EXPECT_TRUE(context.has_context());
}

TEST(Context, SettingsMoveAssignment) {
    c2pa::Settings s1;
    c2pa::Settings s2;
    s2.set("builder.thumbnail.enabled", "true");

    s1 = std::move(s2);

    EXPECT_EQ(s2.c_settings(), nullptr);

    // Verify s1 is functional and can call set()
    EXPECT_NO_THROW(s1.set("builder.thumbnail.enabled", "false"));
}

TEST(Context, ContextBuilderUseAfterMoveThrows) {
    auto b1 = c2pa::Context::ContextBuilder();
    auto b2 = std::move(b1);

    EXPECT_THROW(b1.with_json("{}"), c2pa::C2paException);
}

TEST(Context, UseAfterConsumeThrows) {
    auto builder = c2pa::Context::ContextBuilder();
    auto context = builder.create_context();

    EXPECT_THROW(builder.with_json("{}"), c2pa::C2paException);
}

TEST(Context, DoubleConsumeThrows) {
    auto builder = c2pa::Context::ContextBuilder();
    auto context1 = builder.create_context();

    EXPECT_THROW({
        auto context2 = builder.create_context();
        (void)context2; // Suppress unused variable warning
    }, c2pa::C2paException);
}

// Default constructor creates a valid context
TEST(Context, DirectConstructDefault) {
    c2pa::Context context;
    EXPECT_TRUE(context.has_context());
    EXPECT_NE(context.c_context(), nullptr);
}

// Constructor with Settings creates a valid context
TEST(Context, DirectConstructWithSettings) {
    c2pa::Settings settings;
    settings.set("builder.thumbnail.enabled", "false");

    c2pa::Context context(settings);
    EXPECT_TRUE(context.has_context());
}

// Default constructor can be used with Builder
TEST(Context, DirectConstructDefaultWithBuilder) {
    auto manifest = load_fixture("training.json");
    c2pa::Context context;

    EXPECT_NO_THROW({
        c2pa::Builder builder(context, manifest);
    });
}

// 1) Direct construction with Settings: sign and verify thumbnail is disabled
TEST_F(ContextTest, DirectConstructSettingsSignVerify) {
    c2pa::Settings settings;
    settings.set("builder.thumbnail.enabled", "false");

    c2pa::Context context(settings);
    auto manifest_json = sign_with_context(context, get_temp_path("direct_construct_settings.jpg"));

    EXPECT_FALSE(has_thumbnail(manifest_json));
}

// 2) Direct default construction: sign and verify thumbnail is enabled (default)
TEST_F(ContextTest, DirectConstructDefaultSignVerify) {
    c2pa::Context context;
    auto manifest_json = sign_with_context(context, get_temp_path("direct_construct_default.jpg"));

    EXPECT_TRUE(has_thumbnail(manifest_json));
}

// 3) JSON string constructor: sign and verify thumbnail is disabled
TEST_F(ContextTest, JsonConstructorSignVerify) {
    c2pa::Context context(R"({"builder": {"thumbnail": {"enabled": false}}})");
    auto manifest_json = sign_with_context(context, get_temp_path("json_constructor.jpg"));

    EXPECT_FALSE(has_thumbnail(manifest_json));
}

// 4) ContextBuilder with Settings: sign and verify thumbnail is disabled
TEST_F(ContextTest, ContextBuilderWithSettingsSignVerify) {
    c2pa::Settings settings;
    settings.set("builder.thumbnail.enabled", "false");

    auto context = c2pa::Context::ContextBuilder()
        .with_settings(settings)
        .create_context();
    auto manifest_json = sign_with_context(context, get_temp_path("builder_with_settings.jpg"));

    EXPECT_FALSE(has_thumbnail(manifest_json));
}

// 5) ContextBuilder with JSON: sign and verify thumbnail is disabled
TEST_F(ContextTest, ContextBuilderWithJsonSignVerify) {
    auto context = c2pa::Context::ContextBuilder()
        .with_json(R"({"builder": {"thumbnail": {"enabled": false}}})")
        .create_context();
    auto manifest_json = sign_with_context(context, get_temp_path("builder_with_json.jpg"));

    EXPECT_FALSE(has_thumbnail(manifest_json));
}

// 6) ContextBuilder empty (default): sign and verify thumbnail is enabled (default)
TEST_F(ContextTest, ContextBuilderDefaultSignVerify) {
    auto context = c2pa::Context::ContextBuilder().create_context();
    auto manifest_json = sign_with_context(context, get_temp_path("builder_default.jpg"));

    EXPECT_TRUE(has_thumbnail(manifest_json));
}

// 7) Direct construction with Settings enabling thumbnail: sign and verify
TEST_F(ContextTest, DirectConstructSettingsEnableThumbnailSignVerify) {
    c2pa::Settings settings;
    settings.set("builder.thumbnail.enabled", "true");

    c2pa::Context context(settings);
    auto manifest_json = sign_with_context(context, get_temp_path("direct_construct_enable_thumb.jpg"));

    EXPECT_TRUE(has_thumbnail(manifest_json));
}

// Test with_json_settings_file method: loads settings from file path
TEST_F(ContextTest, ContextBuilderWithJsonSettingsFile) {
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path settings_path = current_dir / "fixtures/settings/test_settings_no_thumbnail.json";

    auto context = c2pa::Context::ContextBuilder()
        .with_json_settings_file(settings_path)
        .create_context();

    auto manifest_json = sign_with_context(context, get_temp_path("with_json_settings_file.jpg"));
    EXPECT_FALSE(has_thumbnail(manifest_json));
}

// Test with_json_settings_file with invalid path throws exception
TEST(Context, ContextBuilderWithJsonSettingsFileInvalidPath) {
    auto builder = c2pa::Context::ContextBuilder();
    EXPECT_THROW({
        builder.with_json_settings_file("/nonexistent/path/to/settings.json");
    }, c2pa::C2paException);
}

// Test with_json_settings_file can be chained with other methods
TEST_F(ContextTest, ContextBuilderWithJsonSettingsFileChaining) {
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path settings_path = current_dir / "fixtures/settings/test_settings_with_thumbnail.json";

    // File enables thumbnail, then we disable it with set()
    c2pa::Settings override_settings;
    override_settings.set("builder.thumbnail.enabled", "false");

    auto context = c2pa::Context::ContextBuilder()
        .with_json_settings_file(settings_path)
        .with_settings(override_settings)
        .create_context();

    auto manifest_json = sign_with_context(context, get_temp_path("with_json_settings_file_chained.jpg"));
    EXPECT_FALSE(has_thumbnail(manifest_json));
}
