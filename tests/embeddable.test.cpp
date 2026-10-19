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
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <cstring>

#include "c2pa.hpp"
#include "include/test_utils.hpp"

namespace fs = std::filesystem;
using nlohmann::json;

// Test fixture for embeddable tests with automatic cleanup
class EmbeddableTest : public ::testing::Test {
protected:
    std::vector<fs::path> temp_files;

    // Get path for temp embeddable test files in build directory
    fs::path get_temp_path(const std::string& name) {
        fs::path current_dir = fs::path(__FILE__).parent_path();
        fs::path build_dir = current_dir.parent_path() / "build";
        if (!fs::exists(build_dir)) {
            fs::create_directories(build_dir);
        }
        fs::path temp_path = build_dir / ("embeddable-" + name);
        temp_files.push_back(temp_path);
        return temp_path;
    }

    void TearDown() override {
        for (const auto& path : temp_files) {
            if (fs::exists(path)) {
                fs::remove(path);
            }
        }
        temp_files.clear();
    }

    // Helper: Creates a Builder with a test manifest.
    c2pa::Builder make_builder() {
        auto manifest = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
        return c2pa::Builder(std::make_shared<c2pa::Context>(), manifest);
    }

    // Context with the test signer attached, plus optional settings JSON.
    static std::shared_ptr<c2pa::Context> signer_context(const std::string& settings_json = "") {
        c2pa::Context::ContextBuilder context_builder;
        if (!settings_json.empty()) {
            context_builder.with_json(settings_json);
        }
        context_builder.with_signer(c2pa_test::create_test_signer());
        return std::make_shared<c2pa::Context>(context_builder.create_context());
    }

    // Runs the DataHash embeddable flow on an asset, treating the placeholder as embedded at
    // embed_offset. Returns the placeholder and the signed manifest.
    static std::pair<std::vector<unsigned char>, std::vector<unsigned char>> sign_with_data_hash(
        c2pa::Builder& builder, const std::string& format, const fs::path& asset,
        uint64_t embed_offset = 20) {
        auto placeholder = builder.placeholder(format);
        builder.set_data_hash_exclusions({{embed_offset, placeholder.size()}});
        std::ifstream asset_stream(asset, std::ios::binary);
        EXPECT_TRUE(asset_stream.is_open()) << asset;
        builder.update_hash_from_stream(format, asset_stream);
        return {placeholder, builder.sign_embeddable(format)};
    }
};

// e2e workflow with A.jpg using new context-signer embeddable APIs
TEST_F(EmbeddableTest, FullWorkflowWithAJpgContextSigner) {
    // Demonstrates the new embeddable API where the signer lives on the context
    // rather than being passed explicitly to placeholder/sign methods.
    //
    // Workflow:
    //   1. needs_placeholder() — check whether a placeholder embed step is required
    //   2. placeholder()       — get the correctly-sized placeholder bytes to embed
    //   3. set_data_hash_exclusions() — register where the placeholder was embedded
    //   4. update_hash_from_stream()  — hash the asset (SDK skips exclusion ranges)
    //   5. sign_embeddable()          — sign; output size is exactly placeholder.size()

    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto source_asset  = c2pa_test::get_fixture_path("A.jpg");

    // Create a context with the signer attached programmatically
    auto context = std::make_shared<c2pa::Context>(c2pa::Context::ContextBuilder()
        .with_signer(c2pa_test::create_test_signer())
        .create_context());

    auto builder = c2pa::Builder(context, manifest_json);

    // 1: Check if a placeholder is required for JPEG
    ASSERT_TRUE(builder.needs_placeholder("image/jpeg"))
        << "JPEG always requires a placeholder";

    // 2: Get the placeholder bytes (size is committed internally for later validation)
    auto placeholder_bytes = builder.placeholder("image/jpeg");
    ASSERT_GT(placeholder_bytes.size(), 0) << "Placeholder must not be empty";
    size_t placeholder_size = placeholder_bytes.size();

    // 3: Register where we would embed the placeholder.
    //    In production this offset is where you physically write placeholder_bytes.
    size_t embed_offset = 20;
    builder.set_data_hash_exclusions({{embed_offset, placeholder_bytes.size()}});

    // 4: Hash the asset stream, skipping the exclusion range
    std::ifstream asset_stream(source_asset, std::ios::binary);
    ASSERT_TRUE(asset_stream.is_open());
    builder.update_hash_from_stream("image/jpeg", asset_stream);
    asset_stream.close();

    // 5: Sign — output must match the placeholder size (enables in-place patching)
    auto signed_manifest = builder.sign_embeddable("image/jpeg");
    ASSERT_GT(signed_manifest.size(), 0);
    EXPECT_EQ(signed_manifest.size(), placeholder_size)
        << "Signed manifest must be exactly the same size as the placeholder";
}

// e2e workflow with C.jpg (has existing C2PA metadata)
TEST_F(EmbeddableTest, FullWorkflowWithCJpg) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto builder = c2pa::Builder(signer_context(), manifest_json);

    auto [placeholder, signed_manifest] =
        sign_with_data_hash(builder, "image/jpeg", c2pa_test::get_fixture_path("C.jpg"));
    ASSERT_GT(placeholder.size(), 0);

    // The signed manifest fills the placeholder slot exactly, so it can be patched in place.
    EXPECT_EQ(signed_manifest.size(), placeholder.size());
}

TEST_F(EmbeddableTest, ContextSettingsPropagation) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto context = signer_context(R"({"builder": {"thumbnail": {"enabled": false}}})");
    auto builder = c2pa::Builder(context, manifest_json);

    auto [placeholder, signed_manifest] =
        sign_with_data_hash(builder, "image/jpeg", c2pa_test::get_fixture_path("A.jpg"));
    ASSERT_GT(placeholder.size(), 0);
    EXPECT_GT(signed_manifest.size(), 0) << "Context propagated successfully through workflow";
}

// Verify different formats
TEST_F(EmbeddableTest, MultipleFormats) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto context = signer_context();

    const std::vector<std::pair<std::string, std::string>> cases = {
        {"image/jpeg", "A.jpg"},
        {"image/gif", "sample1.gif"},
    };
    for (const auto& [format, fixture] : cases) {
        auto builder = c2pa::Builder(context, manifest_json);
        auto [placeholder, signed_manifest] =
            sign_with_data_hash(builder, format, c2pa_test::get_fixture_path(fixture));
        EXPECT_GT(placeholder.size(), 0) << format << " placeholder should not be empty";
        EXPECT_EQ(signed_manifest.size(), placeholder.size()) << format;
    }
}

// BoxHash: standard Builder::sign() flow with prefer_box_hash enabled
TEST_F(EmbeddableTest, BoxHashStandardSign) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto source_path = c2pa_test::get_fixture_path("A.jpg");
    auto output_path = get_temp_path("boxhash_standard.jpg");

    // Enable BoxHash via context settings
    auto context = std::make_shared<c2pa::Context>(c2pa::Context::ContextBuilder()
        .with_signer(c2pa_test::create_test_signer())
        .with_json(R"({
            "builder": {
                "prefer_box_hash": true
            }
        })")
        .create_context());

    auto builder = c2pa::Builder(context, manifest_json);

    // Standard sign() handles the entire BoxHash workflow internally
    std::vector<unsigned char> manifest_data;
    ASSERT_NO_THROW(manifest_data = builder.sign(source_path, output_path));
    ASSERT_FALSE(manifest_data.empty());

    // Verify the output can be read back with valid C2PA data
    auto reader = c2pa::Reader(context, output_path);
    std::string json_result;
    ASSERT_NO_THROW(json_result = reader.json());
    EXPECT_FALSE(json_result.empty());
}

// BoxHash: needs_placeholder() returns false when prefer_box_hash is enabled
TEST_F(EmbeddableTest, BoxHashNeedsPlaceholderReturnsFalse) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));

    auto context = std::make_shared<c2pa::Context>(c2pa::Context::ContextBuilder()
        .with_signer(c2pa_test::create_test_signer())
        .with_json(R"({
            "builder": {
                "prefer_box_hash": true
            }
        })")
        .create_context());

    auto builder = c2pa::Builder(context, manifest_json);

    // BoxHash-capable formats should not require a placeholder
    ASSERT_FALSE(builder.needs_placeholder("image/jpeg"))
        << "JPEG with prefer_box_hash should not require a placeholder";
}

// BoxHash: embeddable API workflow (update_hash_from_stream + sign_embeddable)
TEST_F(EmbeddableTest, BoxHashEmbeddableWorkflow) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto source_asset = c2pa_test::get_fixture_path("A.jpg");

    auto context = std::make_shared<c2pa::Context>(c2pa::Context::ContextBuilder()
        .with_signer(c2pa_test::create_test_signer())
        .with_json(R"({
            "builder": {
                "prefer_box_hash": true
            }
        })")
        .create_context());

    auto builder = c2pa::Builder(context, manifest_json);

    // No placeholder needed for BoxHash
    ASSERT_FALSE(builder.needs_placeholder("image/jpeg"));

    // Hash the original asset directly (no placeholder to embed first)
    std::ifstream asset_stream(source_asset, std::ios::binary);
    ASSERT_TRUE(asset_stream.is_open());
    builder.update_hash_from_stream("image/jpeg", asset_stream);
    asset_stream.close();

    // Sign — output is the actual manifest size (not padded to a placeholder)
    auto manifest_bytes = builder.sign_embeddable("image/jpeg");
    ASSERT_GT(manifest_bytes.size(), 0)
        << "BoxHash embeddable should produce signed manifest bytes";
}

// BoxHash: BMFF formats always require a placeholder regardless of prefer_box_hash
TEST_F(EmbeddableTest, BoxHashBmffAlwaysRequiresPlaceholder) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));

    auto context = std::make_shared<c2pa::Context>(c2pa::Context::ContextBuilder()
        .with_signer(c2pa_test::create_test_signer())
        .with_json(R"({
            "builder": {
                "prefer_box_hash": true
            }
        })")
        .create_context());

    auto builder = c2pa::Builder(context, manifest_json);

    // BMFF formats always require a placeholder, even with prefer_box_hash
    ASSERT_TRUE(builder.needs_placeholder("video/mp4"))
        << "MP4 must always require a placeholder regardless of prefer_box_hash";
}

class EmbeddableBlankFormatTest : public EmbeddableTest, public ::testing::WithParamInterface<std::string> {};
// needs_placeholder() and update_hash_from_stream() are the only embeddable steps when the core is silent on a blank format.

INSTANTIATE_TEST_SUITE_P(EmbeddableBlankFormats, EmbeddableBlankFormatTest,
                         ::testing::ValuesIn(c2pa_test::kBlankFormats));

TEST_P(EmbeddableBlankFormatTest, NeedsPlaceholderRejectsBlankFormat) {
    auto builder = make_builder();
    EXPECT_THROW({ builder.needs_placeholder(GetParam()); }, c2pa::C2paException);
}

TEST_F(EmbeddableTest, UpdateHashFromStreamRejectsBlankFormat) {
    auto builder = make_builder();
    std::ifstream asset(c2pa_test::get_fixture_path("A.jpg"), std::ios::binary);
    ASSERT_TRUE(asset.is_open());
    EXPECT_THROW({ builder.update_hash_from_stream("", asset); }, c2pa::C2paException);
}
