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
};

// e2e workflow with A.jpg (has no existing C2PA)
TEST_F(EmbeddableTest, FullWorkflowWithAJpg) {
    // This test demonstrates the complete data-hashed embeddable workflow:
    // 1. Create placeholder
    // 2. Sign with auto-calculated hash
    // 3. Format for embedding
    // 4. Verify sizes match (critical invariant for in-place patching)
    //
    // Note: We don't actually embed and read back because manual byte-level
    // embedding requires format-specific knowledge of JPEG structure. The SDK's
    // normal sign() method handles this internally. This test verifies the
    // embeddable APIs work correctly.

    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto certs = c2pa_test::read_text_file(c2pa_test::get_fixture_path("es256_certs.pem"));
    auto private_key = c2pa_test::read_text_file(c2pa_test::get_fixture_path("es256_private.key"));
    auto source_asset = c2pa_test::get_fixture_path("A.jpg");

    // Create signing infra
    auto context = c2pa::Context();
    auto builder = c2pa::Builder(context, manifest_json);
    c2pa::Signer signer("Es256", certs, private_key, "http://timestamp.digicert.com");

    // 1: Get placeholder manifest
    // The placeholder is the exact size of the final signed manifest.
    // We specify "image/jpeg" to get JPEG-formatted placeholder bytes.
    auto placeholder = builder.data_hashed_placeholder(signer.reserve_size(), "image/jpeg");
    ASSERT_GT(placeholder.size(), 0) << "Placeholder should not be empty";
    size_t placeholder_size = placeholder.size();

    // 2: Build DataHash JSON
    // In production, you would embed the placeholder and note its location.
    // The exclusion range would match where you embedded it.
    size_t embed_offset = 20;  // Example offset
    std::string data_hash = R"({
        "exclusions": [{
            "start": )" + std::to_string(embed_offset) + R"(,
            "length": )" + std::to_string(placeholder.size()) + R"(
        }],
        "name": "jumbf manifest",
        "alg": "sha256",
        "hash": "",
        "pad": " "
    })";

    // 3: Sign with auto-calculated hash
    // The SDK reads the asset stream, skips the exclusion range,
    // calculates SHA-256 hash, and signs the manifest.
    std::ifstream asset_stream(source_asset, std::ios::binary);
    ASSERT_TRUE(asset_stream.is_open());

    auto raw_manifest = builder.sign_data_hashed_embeddable(
        signer, data_hash, "application/c2pa", &asset_stream);
    asset_stream.close();
    ASSERT_GT(raw_manifest.size(), 0);

    // 4: Format for JPEG embedding
    // Convert raw JUMBF to JPEG APP11 segments
    auto jpeg_embeddable = c2pa::Builder::format_embeddable("image/jpeg", raw_manifest);
    ASSERT_GT(jpeg_embeddable.size(), 0);

    // 5: Verify invariant (the placement size)
    // The formatted manifest must be exactly the same size as the placeholder.
    // This enables in-place patching in production workflows.
    EXPECT_EQ(jpeg_embeddable.size(), placeholder_size)
        << "Signed manifest size must match placeholder size for in-place patching";
}

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
    auto context = c2pa::Context::ContextBuilder()
        .with_signer(c2pa_test::create_test_signer())
        .create_context();

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
    auto signer = c2pa_test::create_test_signer();
    auto source_asset = c2pa_test::get_fixture_path("C.jpg");

    auto context = c2pa::Context();
    auto builder = c2pa::Builder(context, manifest_json);

    // Get placeholder
    auto placeholder = builder.data_hashed_placeholder(signer.reserve_size(), "image/jpeg");
    ASSERT_GT(placeholder.size(), 0);

    // Build data hash JSON
    size_t embed_offset = 20;
    std::string data_hash = R"({
        "exclusions": [{
            "start": )" + std::to_string(embed_offset) + R"(,
            "length": )" + std::to_string(placeholder.size()) + R"(
        }],
        "name": "jumbf manifest",
        "alg": "sha256",
        "hash": "",
        "pad": " "
    })";

    // Sign with the asset that has existing C2PA
    std::ifstream asset_stream(source_asset, std::ios::binary);
    auto raw_manifest = builder.sign_data_hashed_embeddable(
        signer, data_hash, "application/c2pa", &asset_stream);
    asset_stream.close();
    ASSERT_GT(raw_manifest.size(), 0);

    // Format for jpeg
    auto jpeg_embeddable = c2pa::Builder::format_embeddable("image/jpeg", raw_manifest);
    ASSERT_GT(jpeg_embeddable.size(), 0);

    // Verify invariant: size matches so we could place
    EXPECT_EQ(jpeg_embeddable.size(), placeholder.size());
}

// Pre-calculated hash
TEST_F(EmbeddableTest, PreCalculatedHash) {
    // This test demonstrates signing with a pre-calculated hash.
    // In production, you might calculate the hash on a different machine or at a different time than signing.

    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto signer = c2pa_test::create_test_signer();

    auto context = c2pa::Context();
    auto builder = c2pa::Builder(context, manifest_json);

    // Get placeholder
    auto placeholder = builder.data_hashed_placeholder(signer.reserve_size(), "image/jpeg");

    // Build data hash JSON with a KNOWN hash value
    // In this test, we use a dummy hash since we're not actually embedding.
    // In production, this would be a real SHA-256 hash of your asset.
    std::string data_hash = R"({
        "exclusions": [{
            "start": 20,
            "length": )" + std::to_string(placeholder.size()) + R"(
        }],
        "name": "jumbf manifest",
        "alg": "sha256",
        "hash": "gWZNEOMHQNiULfA/tO5HD2awOwYMA3tnfUPApIr9csk=",
        "pad": " "
    })";

    // Sign without providing an asset stream
    // The SDK uses the hash value from the data_hash JSON directly
    auto manifest = builder.sign_data_hashed_embeddable(
        signer, data_hash, "image/jpeg", nullptr);

    EXPECT_GT(manifest.size(), 0) << "Should produce signed manifest bytes";
    EXPECT_EQ(manifest.size(), placeholder.size()) << "Size should match placeholder";
}

// Auto-calculated hash
TEST_F(EmbeddableTest, AutoCalculatedHash) {
    // This test demonstrates the SDK calculating the hash automatically from an asset stream.
    // The hash field is left empty (""), and the SDK will:
    // 1. Read the asset stream
    // 2. Skip the exclusion ranges
    // 3. Calculate the SHA-256 hash of the remaining bytes
    //
    // Usually (not in a test), you would have already embedded the placeholder at the exclusion offset.
    // Here we just verify the API accepts an asset stream and auto-calculates the hash.

    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto signer = c2pa_test::create_test_signer();
    auto source_asset = c2pa_test::get_fixture_path("A.jpg");

    auto context = c2pa::Context();
    auto builder = c2pa::Builder(context, manifest_json);

    // Get placeholder
    auto placeholder = builder.data_hashed_placeholder(signer.reserve_size(), "image/jpeg");

    // Build data hash JSON with empty hash field
    // The exclusion range would match where you embedded the placeholder
    std::string data_hash = R"({
        "exclusions": [{
            "start": 20,
            "length": )" + std::to_string(placeholder.size()) + R"(
        }],
        "name": "jumbf manifest",
        "alg": "sha256",
        "hash": "",
        "pad": " "
    })";

    // Sign with asset stream: SDK will auto-calculate hash
    // This stream would be your asset with the placeholder already embedded
    std::ifstream asset_stream(source_asset, std::ios::binary);
    auto manifest = builder.sign_data_hashed_embeddable(
        signer, data_hash, "application/c2pa", &asset_stream);
    asset_stream.close();

    EXPECT_GT(manifest.size(), 0) << "Should produce signed manifest bytes";
}

// Embedding roundtrip
TEST_F(EmbeddableTest, FormatEmbeddableRoundTrip) {
    // This test demonstrates the two-step format conversion:
    // 1. Sign with format="application/c2pa" to get raw JUMBF bytes
    // 2. Use format_embeddable() to convert to JPEG-embeddable format
    //
    // This is useful when you want to sign once and embed in multiple formats.

    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto signer = c2pa_test::create_test_signer();
    auto source_asset = c2pa_test::get_fixture_path("A.jpg");

    auto context = c2pa::Context();
    auto builder = c2pa::Builder(context, manifest_json);

    auto placeholder = builder.data_hashed_placeholder(signer.reserve_size(), "image/jpeg");

    std::string data_hash = R"({
        "exclusions": [{
            "start": 20,
            "length": )" + std::to_string(placeholder.size()) + R"(
        }],
        "name": "jumbf manifest",
        "alg": "sha256",
        "hash": "",
        "pad": " "
    })";

    // 1: Sign with "application/c2pa" to get raw JUMBF
    std::ifstream asset_stream(source_asset, std::ios::binary);
    auto raw_jumbf = builder.sign_data_hashed_embeddable(
        signer, data_hash, "application/c2pa", &asset_stream);
    asset_stream.close();

    // 2: Convert raw JUMBF to JPEG-embeddable format
    auto jpeg_formatted = c2pa::Builder::format_embeddable("image/jpeg", raw_jumbf);

    // The JPEG format adds container overhead (APP11 markers, headers)
    // so the formatted output should be larger than raw JUMBF due to this
    EXPECT_GT(jpeg_formatted.size(), raw_jumbf.size())
        << "JPEG format should add container overhead";

    // But still matches placeholder size (which was already JPEG-formatted)
    EXPECT_EQ(jpeg_formatted.size(), placeholder.size());
}

// Verify placeholder size matches final size (asset has no existing C2PA)
TEST_F(EmbeddableTest, PlaceholderSizeMatchesFinalInvariant) {
    // This test verifies the critical invariant:
    // placeholder.size() == final_signed_manifest.size()
    // This is what enables in-place patching in workflows.
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto signer = c2pa_test::create_test_signer();
    auto source_asset = c2pa_test::get_fixture_path("A.jpg");

    auto context = c2pa::Context();
    auto builder = c2pa::Builder(context, manifest_json);

    // Get placeholder and record its size
    auto placeholder = builder.data_hashed_placeholder(signer.reserve_size(), "image/jpeg");
    size_t placeholder_size = placeholder.size();

    // Complete the signing flow
    size_t embed_offset = 20;
    std::string data_hash = R"({
        "exclusions": [{
            "start": )" + std::to_string(embed_offset) + R"(,
            "length": )" + std::to_string(placeholder.size()) + R"(
        }],
        "name": "jumbf manifest",
        "alg": "sha256",
        "hash": "",
        "pad": " "
    })";

    std::ifstream asset_stream(source_asset, std::ios::binary);
    auto raw_manifest = builder.sign_data_hashed_embeddable(
        signer, data_hash, "application/c2pa", &asset_stream);
    asset_stream.close();

    auto jpeg_embeddable = c2pa::Builder::format_embeddable("image/jpeg", raw_manifest);

    // The final manifest is exactly the same size as placeholder
    EXPECT_EQ(jpeg_embeddable.size(), placeholder_size)
        << "Final signed manifest MUST be same size as placeholder";
}

// Verify placeholder size matches final size (asset has existing C2PA metadata)
TEST_F(EmbeddableTest, PlaceholderSizeMatchesFinalInvariantWithMetadata) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto signer = c2pa_test::create_test_signer();
    auto source_asset = c2pa_test::get_fixture_path("C.jpg");

    auto context = c2pa::Context();
    auto builder = c2pa::Builder(context, manifest_json);

    auto placeholder = builder.data_hashed_placeholder(signer.reserve_size(), "image/jpeg");
    size_t placeholder_size = placeholder.size();

    size_t embed_offset = 20;
    std::string data_hash = R"({
        "exclusions": [{
            "start": )" + std::to_string(embed_offset) + R"(,
            "length": )" + std::to_string(placeholder.size()) + R"(
        }],
        "name": "jumbf manifest",
        "alg": "sha256",
        "hash": "",
        "pad": " "
    })";

    std::ifstream asset_stream(source_asset, std::ios::binary);
    auto signed_manifest = builder.sign_data_hashed_embeddable(
        signer, data_hash, "image/jpeg", &asset_stream);
    asset_stream.close();

    EXPECT_EQ(signed_manifest.size(), placeholder_size);
}

TEST_F(EmbeddableTest, ContextSettingsPropagation) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto signer = c2pa_test::create_test_signer();
    auto source_asset = c2pa_test::get_fixture_path("A.jpg");

    // Create context with custom settings
    auto context = c2pa::Context(R"({
        "builder": {
            "thumbnail": {
                "enabled": false
            }
        }
    })");

    // Use context for builder
    auto builder = c2pa::Builder(context, manifest_json);

    // Get placeholder
    auto placeholder = builder.data_hashed_placeholder(signer.reserve_size(), "image/jpeg");
    ASSERT_GT(placeholder.size(), 0);

    // Sign using the same context
    std::string data_hash = R"({
        "exclusions": [{
            "start": 20,
            "length": )" + std::to_string(placeholder.size()) + R"(
        }],
        "name": "jumbf manifest",
        "alg": "sha256",
        "hash": "",
        "pad": " "
    })";

    std::ifstream asset_stream(source_asset, std::ios::binary);
    auto manifest = builder.sign_data_hashed_embeddable(
        signer, data_hash, "application/c2pa", &asset_stream);
    asset_stream.close();

    EXPECT_GT(manifest.size(), 0) << "Context propagated successfully through workflow";
}

// Verify different formats
TEST_F(EmbeddableTest, MultipleFormats) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto signer = c2pa_test::create_test_signer();

    auto context = c2pa::Context();

    // jpeg
    {
        auto builder = c2pa::Builder(context, manifest_json);
        auto placeholder = builder.data_hashed_placeholder(
            signer.reserve_size(), "image/jpeg");
        EXPECT_GT(placeholder.size(), 0) << "JPEG placeholder should not be empty";

        // Build simple data hash with dummy values
        std::string data_hash = R"({
            "exclusions": [{
                "start": 20,
                "length": )" + std::to_string(placeholder.size()) + R"(
            }],
            "name": "jumbf manifest",
            "alg": "sha256",
            "hash": "gWZNEOMHQNiULfA/tO5HD2awOwYMA3tnfUPApIr9csk=",
            "pad": " "
        })";

        auto manifest = builder.sign_data_hashed_embeddable(
            signer, data_hash, "image/jpeg", nullptr);
        EXPECT_GT(manifest.size(), 0) << "JPEG signed manifest should not be empty";
    }

    // png
    {
        auto builder = c2pa::Builder(context, manifest_json);
        auto placeholder = builder.data_hashed_placeholder(
            signer.reserve_size(), "image/png");
        EXPECT_GT(placeholder.size(), 0) << "PNG placeholder should not be empty";

        std::string data_hash = R"({
            "exclusions": [{
                "start": 33,
                "length": )" + std::to_string(placeholder.size()) + R"(
            }],
            "name": "jumbf manifest",
            "alg": "sha256",
            "hash": "gWZNEOMHQNiULfA/tO5HD2awOwYMA3tnfUPApIr9csk=",
            "pad": " "
        })";

        auto manifest = builder.sign_data_hashed_embeddable(
            signer, data_hash, "image/png", nullptr);
        EXPECT_GT(manifest.size(), 0) << "PNG signed manifest should not be empty";
    }
}

// BoxHash: standard Builder::sign() flow with prefer_box_hash enabled
TEST_F(EmbeddableTest, BoxHashStandardSign) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto source_path = c2pa_test::get_fixture_path("A.jpg");
    auto output_path = get_temp_path("boxhash_standard.jpg");

    // Enable BoxHash via context settings
    auto context = c2pa::Context::ContextBuilder()
        .with_signer(c2pa_test::create_test_signer())
        .with_json(R"({
            "builder": {
                "prefer_box_hash": true
            }
        })")
        .create_context();

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

    auto context = c2pa::Context::ContextBuilder()
        .with_signer(c2pa_test::create_test_signer())
        .with_json(R"({
            "builder": {
                "prefer_box_hash": true
            }
        })")
        .create_context();

    auto builder = c2pa::Builder(context, manifest_json);

    // BoxHash-capable formats should not require a placeholder
    ASSERT_FALSE(builder.needs_placeholder("image/jpeg"))
        << "JPEG with prefer_box_hash should not require a placeholder";
}

// BoxHash: embeddable API workflow (update_hash_from_stream + sign_embeddable)
TEST_F(EmbeddableTest, BoxHashEmbeddableWorkflow) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto source_asset = c2pa_test::get_fixture_path("A.jpg");

    auto context = c2pa::Context::ContextBuilder()
        .with_signer(c2pa_test::create_test_signer())
        .with_json(R"({
            "builder": {
                "prefer_box_hash": true
            }
        })")
        .create_context();

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

    auto context = c2pa::Context::ContextBuilder()
        .with_signer(c2pa_test::create_test_signer())
        .with_json(R"({
            "builder": {
                "prefer_box_hash": true
            }
        })")
        .create_context();

    auto builder = c2pa::Builder(context, manifest_json);

    // BMFF formats always require a placeholder, even with prefer_box_hash
    ASSERT_TRUE(builder.needs_placeholder("video/mp4"))
        << "MP4 must always require a placeholder regardless of prefer_box_hash";
}

// Direct embedding when giving format
TEST_F(EmbeddableTest, DirectEmbeddingWithFormat) {
    // This test demonstrates that when you sign with format="image/jpeg",
    // the output is already embeddable: no need for format_embeddable()!
    // Whereas...
    // Signing with "application/c2pa" requires a separate format_embeddable() call.

    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto signer = c2pa_test::create_test_signer();
    auto source_asset = c2pa_test::get_fixture_path("A.jpg");

    auto context = c2pa::Context();
    auto builder = c2pa::Builder(context, manifest_json);

    auto placeholder = builder.data_hashed_placeholder(signer.reserve_size(), "image/jpeg");

    std::string data_hash = R"({
        "exclusions": [{
            "start": 20,
            "length": )" + std::to_string(placeholder.size()) + R"(
        }],
        "name": "jumbf manifest",
        "alg": "sha256",
        "hash": "",
        "pad": " "
    })";

    std::ifstream asset_stream(source_asset, std::ios::binary);

    // Sign directly with "image/jpeg" format
    // This output is already formatted: no format_embeddable() needed
    auto jpeg_manifest = builder.sign_data_hashed_embeddable(
        signer, data_hash, "image/jpeg", &asset_stream);
    asset_stream.close();

    EXPECT_GT(jpeg_manifest.size(), 0);
    EXPECT_EQ(jpeg_manifest.size(), placeholder.size())
        << "Direct JPEG format output matches placeholder size";
}

// ---------------------------------------------------------------------------
// EmbeddableWorkflowTest — type-state workflow API tests
// ---------------------------------------------------------------------------

class EmbeddableWorkflowTest : public ::testing::Test {
protected:
    std::vector<fs::path> temp_files;

    fs::path get_temp_path(const std::string& name) {
        fs::path current_dir = fs::path(__FILE__).parent_path();
        fs::path build_dir = current_dir.parent_path() / "build";
        if (!fs::exists(build_dir)) {
            fs::create_directories(build_dir);
        }
        fs::path temp_path = build_dir / ("embeddable_wf-" + name);
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
};

TEST_F(EmbeddableWorkflowTest, InitStateReportsCorrectly) {
    auto context = c2pa::Context::ContextBuilder()
        .with_signer(c2pa_test::create_test_signer())
        .create_context();
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto builder = c2pa::Builder(context, manifest_json);

    auto workflow= c2pa::EmbeddableWorkflow<c2pa::embeddable::Init>(
        std::move(builder), "image/jpeg");

    EXPECT_STREQ(workflow.get_current_state(), "Init");
    EXPECT_EQ(workflow.format(), "image/jpeg");
}

TEST_F(EmbeddableWorkflowTest, DataHashFullWorkflow) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto source_asset = c2pa_test::get_fixture_path("A.jpg");

    auto context = c2pa::Context::ContextBuilder()
        .with_signer(c2pa_test::create_test_signer())
        .create_context();
    auto builder = c2pa::Builder(context, manifest_json);

    // Enter the workflow
    auto workflow_init = c2pa::EmbeddableWorkflow<c2pa::embeddable::Init>(
        std::move(builder), "image/jpeg");

    ASSERT_TRUE(workflow_init.needs_placeholder());
    EXPECT_STREQ(workflow_init.get_current_state(), "Init");

    // Init -> PlaceholderCreated
    auto workflow_placeholder = std::move(workflow_init).create_placeholder();
    EXPECT_STREQ(workflow_placeholder.get_current_state(), "PlaceholderCreated");
    ASSERT_GT(workflow_placeholder.placeholder_bytes().size(), 0u);
    size_t placeholder_size = workflow_placeholder.placeholder_bytes().size();

    // PlaceholderCreated -> ExclusionsSet
    uint64_t embed_offset = 20;
    auto workflow_exclusions = std::move(workflow_placeholder)
        .set_data_hash_exclusions({{embed_offset, placeholder_size}});
    EXPECT_STREQ(workflow_exclusions.get_current_state(), "ExclusionsSet");

    // Verify exclusions are stored
    ASSERT_EQ(workflow_exclusions.data_hash_exclusions().size(), 1u);
    EXPECT_EQ(workflow_exclusions.data_hash_exclusions()[0].first, embed_offset);
    EXPECT_EQ(workflow_exclusions.data_hash_exclusions()[0].second, placeholder_size);

    // ExclusionsSet -> Hashed
    std::ifstream asset_stream(source_asset, std::ios::binary);
    ASSERT_TRUE(asset_stream.is_open());
    auto workflow_hashed = std::move(workflow_exclusions).hash_from_stream(asset_stream);
    asset_stream.close();
    EXPECT_STREQ(workflow_hashed.get_current_state(), "Hashed");

    // Verify exclusions survive the transition to Hashed
    ASSERT_EQ(workflow_hashed.data_hash_exclusions().size(), 1u);
    EXPECT_EQ(workflow_hashed.data_hash_exclusions()[0].first, embed_offset);

    // Hashed -> Signed
    auto workflow_signed = std::move(workflow_hashed).sign();
    EXPECT_STREQ(workflow_signed.get_current_state(), "Signed");
    ASSERT_GT(workflow_signed.signed_bytes().size(), 0u);
    EXPECT_EQ(workflow_signed.signed_bytes().size(), placeholder_size)
        << "Signed manifest must match placeholder size for in-place patching";

    // Verify exclusions survive through to Signed state
    ASSERT_EQ(workflow_signed.data_hash_exclusions().size(), 1u);
    EXPECT_EQ(workflow_signed.data_hash_exclusions()[0].first, embed_offset);
    EXPECT_EQ(workflow_signed.data_hash_exclusions()[0].second, placeholder_size);
}

TEST_F(EmbeddableWorkflowTest, BmffHashNeedsPlaceholder) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));

    auto context = c2pa::Context::ContextBuilder()
        .with_signer(c2pa_test::create_test_signer())
        .create_context();
    auto builder = c2pa::Builder(context, manifest_json);

    auto workflow= c2pa::EmbeddableWorkflow<c2pa::embeddable::Init>(
        std::move(builder), "video/mp4");

    ASSERT_TRUE(workflow.needs_placeholder())
        << "MP4 must always require a placeholder";
}

TEST_F(EmbeddableWorkflowTest, BoxHashFullWorkflow) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto source_asset = c2pa_test::get_fixture_path("A.jpg");

    auto context = c2pa::Context::ContextBuilder()
        .with_signer(c2pa_test::create_test_signer())
        .with_json(R"({
            "builder": {
                "prefer_box_hash": true
            }
        })")
        .create_context();
    auto builder = c2pa::Builder(context, manifest_json);

    auto workflow_init = c2pa::EmbeddableWorkflow<c2pa::embeddable::Init>(
        std::move(builder), "image/jpeg");

    ASSERT_FALSE(workflow_init.needs_placeholder());

    // Init -> Hashed (BoxHash: skip placeholder)
    std::ifstream asset_stream(source_asset, std::ios::binary);
    ASSERT_TRUE(asset_stream.is_open());
    auto workflow_hashed = std::move(workflow_init).hash_from_stream(asset_stream);
    asset_stream.close();
    EXPECT_STREQ(workflow_hashed.get_current_state(), "Hashed");

    // Hashed -> Signed
    auto workflow_signed = std::move(workflow_hashed).sign();
    EXPECT_STREQ(workflow_signed.get_current_state(), "Signed");
    ASSERT_GT(workflow_signed.signed_bytes().size(), 0u);
}

TEST_F(EmbeddableWorkflowTest, BoxHashBmffAlwaysRequiresPlaceholder) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));

    auto context = c2pa::Context::ContextBuilder()
        .with_signer(c2pa_test::create_test_signer())
        .with_json(R"({
            "builder": {
                "prefer_box_hash": true
            }
        })")
        .create_context();
    auto builder = c2pa::Builder(context, manifest_json);

    auto workflow= c2pa::EmbeddableWorkflow<c2pa::embeddable::Init>(
        std::move(builder), "video/mp4");

    ASSERT_TRUE(workflow.needs_placeholder())
        << "MP4 must require a placeholder regardless of prefer_box_hash";
}

TEST_F(EmbeddableWorkflowTest, ExposedBuilderMethodsWork) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));

    auto context = c2pa::Context::ContextBuilder()
        .with_signer(c2pa_test::create_test_signer())
        .create_context();
    auto builder = c2pa::Builder(context, manifest_json);

    auto workflow= c2pa::EmbeddableWorkflow<c2pa::embeddable::Init>(
        std::move(builder), "image/jpeg");

    // c2pa_builder() available via using declaration
    EXPECT_NE(workflow.c2pa_builder(), nullptr);

    // needs_placeholder() available in Init
    EXPECT_TRUE(workflow.needs_placeholder());

    // as_builder() provides raw Builder access
    EXPECT_NE(workflow.as_builder().c2pa_builder(), nullptr);
}

TEST_F(EmbeddableWorkflowTest, ToArchiveAvailableInInit) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));

    auto context = c2pa::Context::ContextBuilder()
        .with_signer(c2pa_test::create_test_signer())
        .create_context();
    auto builder = c2pa::Builder(context, manifest_json);

    auto workflow= c2pa::EmbeddableWorkflow<c2pa::embeddable::Init>(
        std::move(builder), "image/jpeg");

    auto archive_path = get_temp_path("archive_test.c2pa");
    ASSERT_NO_THROW(workflow.to_archive(archive_path));
    EXPECT_TRUE(fs::exists(archive_path));
}

TEST_F(EmbeddableWorkflowTest, IntoBuilderRecoversBuilder) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));

    auto context = c2pa::Context::ContextBuilder()
        .with_signer(c2pa_test::create_test_signer())
        .create_context();
    auto builder = c2pa::Builder(context, manifest_json);

    auto workflow= c2pa::EmbeddableWorkflow<c2pa::embeddable::Init>(
        std::move(builder), "image/jpeg");

    auto recovered = std::move(workflow).into_builder();
    EXPECT_NE(recovered.c2pa_builder(), nullptr);
}

TEST_F(EmbeddableWorkflowTest, FormatPreservedThroughTransitions) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));

    auto context = c2pa::Context::ContextBuilder()
        .with_signer(c2pa_test::create_test_signer())
        .create_context();
    auto builder = c2pa::Builder(context, manifest_json);

    auto workflow_init = c2pa::EmbeddableWorkflow<c2pa::embeddable::Init>(
        std::move(builder), "image/jpeg");
    EXPECT_EQ(workflow_init.format(), "image/jpeg");

    auto workflow_placeholder = std::move(workflow_init).create_placeholder();
    EXPECT_EQ(workflow_placeholder.format(), "image/jpeg");
}

TEST_F(EmbeddableWorkflowTest, PlaceholderBytesAvailableInLaterStates) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto source_asset = c2pa_test::get_fixture_path("A.jpg");

    auto context = c2pa::Context::ContextBuilder()
        .with_signer(c2pa_test::create_test_signer())
        .create_context();
    auto builder = c2pa::Builder(context, manifest_json);

    auto workflow_init = c2pa::EmbeddableWorkflow<c2pa::embeddable::Init>(
        std::move(builder), "image/jpeg");
    auto workflow_placeholder = std::move(workflow_init).create_placeholder();
    size_t placeholder_size = workflow_placeholder.placeholder_bytes().size();
    ASSERT_GT(placeholder_size, 0u);

    // placeholder_bytes() available in ExclusionsSet
    auto workflow_exclusions = std::move(workflow_placeholder).set_data_hash_exclusions({{2, placeholder_size}});
    EXPECT_EQ(workflow_exclusions.placeholder_bytes().size(), placeholder_size);

    // placeholder_bytes() available in Hashed
    std::ifstream stream(source_asset, std::ios::binary);
    auto workflow_hashed = std::move(workflow_exclusions).hash_from_stream(stream);
    stream.close();
    EXPECT_EQ(workflow_hashed.placeholder_bytes().size(), placeholder_size);

    // placeholder_bytes() available in Signed
    auto workflow_signed = std::move(workflow_hashed).sign();
    EXPECT_EQ(workflow_signed.placeholder_bytes().size(), placeholder_size);
}

TEST_F(EmbeddableWorkflowTest, AddIngredientInInitState) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));

    auto context = c2pa::Context::ContextBuilder()
        .with_signer(c2pa_test::create_test_signer())
        .create_context();
    auto builder = c2pa::Builder(context, manifest_json);

    auto workflow_init = c2pa::EmbeddableWorkflow<c2pa::embeddable::Init>(
        std::move(builder), "image/jpeg");

    // Add an ingredient from a file while in Init state
    workflow_init.add_ingredient(
        R"({"title": "A.jpg", "relationship": "parentOf"})",
        c2pa_test::get_fixture_path("A.jpg"));

    // Add an ingredient from a stream while in Init state
    std::ifstream ingredient_stream(c2pa_test::get_fixture_path("C.jpg"), std::ios::binary);
    ASSERT_TRUE(ingredient_stream.is_open());
    workflow_init.add_ingredient(
        R"({"title": "C.jpg", "relationship": "componentOf"})",
        "image/jpeg",
        ingredient_stream);
    ingredient_stream.close();

    // Placeholder creation succeeds with ingredients added
    auto workflow_placeholder = std::move(workflow_init).create_placeholder();
    EXPECT_STREQ(workflow_placeholder.get_current_state(), "PlaceholderCreated");
    ASSERT_GT(workflow_placeholder.placeholder_bytes().size(), 0u);
}

TEST_F(EmbeddableWorkflowTest, AddResourceInInitState) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto source_asset = c2pa_test::get_fixture_path("A.jpg");

    auto context = c2pa::Context::ContextBuilder()
        .with_signer(c2pa_test::create_test_signer())
        .create_context();
    auto builder = c2pa::Builder(context, manifest_json);

    auto workflow_init = c2pa::EmbeddableWorkflow<c2pa::embeddable::Init>(
        std::move(builder), "image/jpeg");

    // Add a resource from a file while in Init state
    workflow_init.add_resource("thumbnail", c2pa_test::get_fixture_path("C.jpg"));

    // Add a resource from a stream while in Init state
    std::ifstream resource_stream(c2pa_test::get_fixture_path("A.jpg"), std::ios::binary);
    ASSERT_TRUE(resource_stream.is_open());
    workflow_init.add_resource("source_image", resource_stream);
    resource_stream.close();

    // The workflow should still complete after adding resources
    auto workflow_placeholder = std::move(workflow_init).create_placeholder();
    ASSERT_GT(workflow_placeholder.placeholder_bytes().size(), 0u);
}

TEST_F(EmbeddableWorkflowTest, AddActionInInitState) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));

    auto context = c2pa::Context::ContextBuilder()
        .with_signer(c2pa_test::create_test_signer())
        .create_context();
    auto builder = c2pa::Builder(context, manifest_json);

    auto workflow_init = c2pa::EmbeddableWorkflow<c2pa::embeddable::Init>(
        std::move(builder), "image/jpeg");

    // Add an action while in Init state
    workflow_init.add_action(R"({
        "action": "c2pa.edited",
        "softwareAgent": "EmbeddableWorkflow Test"
    })");

    auto workflow_placeholder = std::move(workflow_init).create_placeholder();
    ASSERT_GT(workflow_placeholder.placeholder_bytes().size(), 0u);
}

// Strip fields that differ between two independent sign operations (instance IDs, timestamps,
// signature bytes, hash values) so the remaining manifest content can be compared.
static json strip_volatile_fields(json j) {
    std::vector<std::string> volatile_keys = {
        "instance_id", "instanceId",
        "time", "when",
        "signature", "pad",
        "hash", "value",
        "url"
    };

    if (j.is_object()) {
        for (const auto& key : volatile_keys) {
            j.erase(key);
        }
        for (auto& [key, val] : j.items()) {
            j[key] = strip_volatile_fields(val);
        }
    } else if (j.is_array()) {
        for (size_t i = 0; i < j.size(); ++i) {
            j[i] = strip_volatile_fields(j[i]);
        }
    }
    return j;
}

TEST_F(EmbeddableWorkflowTest, BuilderAndWorkflowProduceSameManifestContent) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto source_path = c2pa_test::get_fixture_path("A.jpg");
    auto ingredient_path = c2pa_test::get_fixture_path("C.jpg");

    auto ingredient_json = R"({"title": "C.jpg", "relationship": "parentOf"})";
    auto action_json = R"({"action": "c2pa.edited", "softwareAgent": "EquivalenceTest"})";

    // Path A: add ingredient and action on Builder, then sign
    auto dest_builder_path = get_temp_path("signed_via_builder.jpg");
    {
        auto context = c2pa::Context::ContextBuilder()
            .with_signer(c2pa_test::create_test_signer())
            .create_context();
        auto builder = c2pa::Builder(context, manifest_json);
        builder.add_ingredient(ingredient_json, ingredient_path);
        builder.add_action(action_json);
        builder.sign(source_path, dest_builder_path);
    }

    // Path B: add ingredient and action on EmbeddableWorkflow in Init, then sign
    auto dest_workflow_path = get_temp_path("signed_via_workflow.jpg");
    {
        auto context = c2pa::Context::ContextBuilder()
            .with_signer(c2pa_test::create_test_signer())
            .create_context();
        auto builder = c2pa::Builder(context, manifest_json);

        auto workflow = c2pa::EmbeddableWorkflow<c2pa::embeddable::Init>(
            std::move(builder), "image/jpeg");
        workflow.add_ingredient(ingredient_json, ingredient_path);
        workflow.add_action(action_json);
        workflow.sign(source_path, dest_workflow_path);
    }

    // Read both manifests back
    auto context_reader = c2pa::Context();
    auto reader_from_builder = c2pa::Reader(context_reader, dest_builder_path);
    auto reader_from_workflow = c2pa::Reader(context_reader, dest_workflow_path);

    auto parsed_builder = json::parse(reader_from_builder.json());
    auto parsed_workflow = json::parse(reader_from_workflow.json());

    // Extract active manifest from each using the active_manifest label
    ASSERT_TRUE(parsed_builder.contains("active_manifest"));
    ASSERT_TRUE(parsed_workflow.contains("active_manifest"));

    std::string label_builder = parsed_builder["active_manifest"];
    std::string label_workflow = parsed_workflow["active_manifest"];

    auto manifest_builder = parsed_builder["manifests"][label_builder];
    auto manifest_workflow = parsed_workflow["manifests"][label_workflow];

    // Compare assertions count
    ASSERT_TRUE(manifest_builder.contains("assertions"));
    ASSERT_TRUE(manifest_workflow.contains("assertions"));

    auto assertions_builder = manifest_builder["assertions"];
    auto assertions_workflow = manifest_workflow["assertions"];

    EXPECT_EQ(assertions_builder.size(), assertions_workflow.size())
        << "Both paths should produce the same number of assertions";

    // Find c2pa.actions assertions and compare
    json actions_builder, actions_workflow;
    for (const auto& assertion : assertions_builder) {
        if (assertion.contains("label") &&
            assertion["label"].get<std::string>().find("c2pa.actions") != std::string::npos) {
            actions_builder = assertion;
        }
    }
    for (const auto& assertion : assertions_workflow) {
        if (assertion.contains("label") &&
            assertion["label"].get<std::string>().find("c2pa.actions") != std::string::npos) {
            actions_workflow = assertion;
        }
    }

    ASSERT_FALSE(actions_builder.is_null())
        << "Builder manifest should contain a c2pa.actions assertion";
    ASSERT_FALSE(actions_workflow.is_null())
        << "Workflow manifest should contain a c2pa.actions assertion";
    EXPECT_EQ(strip_volatile_fields(actions_builder),
              strip_volatile_fields(actions_workflow))
        << "Action assertions should match between both paths";

    // Compare ingredients
    ASSERT_TRUE(manifest_builder.contains("ingredients"));
    ASSERT_TRUE(manifest_workflow.contains("ingredients"));

    auto ingredients_builder = manifest_builder["ingredients"];
    auto ingredients_workflow = manifest_workflow["ingredients"];

    ASSERT_EQ(ingredients_builder.size(), ingredients_workflow.size());

    // Compare ingredient titles and relationships (instance_id will differ)
    for (size_t i = 0; i < ingredients_builder.size(); ++i) {
        EXPECT_EQ(
            ingredients_builder[i].value("title", ""),
            ingredients_workflow[i].value("title", ""))
            << "Ingredient " << i << " title mismatch";
        EXPECT_EQ(
            ingredients_builder[i].value("relationship", ""),
            ingredients_workflow[i].value("relationship", ""))
            << "Ingredient " << i << " relationship mismatch";
    }
}
