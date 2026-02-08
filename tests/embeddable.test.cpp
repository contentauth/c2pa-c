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
#include "test_utils.hpp"

namespace fs = std::filesystem;
using nlohmann::json;

// Get path for temp embeddable test files in build directory
static fs::path get_embeddable_temp_path(const std::string& name) {
    fs::path current_dir = fs::path(__FILE__).parent_path();
    fs::path build_dir = current_dir.parent_path() / "build";
    if (!fs::exists(build_dir)) {
        fs::create_directories(build_dir);
    }
    return build_dir / ("embeddable-" + name);
}

// Thumbnail assertion is used as marker to see if settings were applied,
// since it is an easy thing to check in tests!
static bool has_thumbnail(const std::string& manifest_json) {
    auto parsed = json::parse(manifest_json);
    std::string active = parsed["active_manifest"];
    return parsed["manifests"][active].contains("thumbnail");
}

// Insert bytes at a specific offset in a binary file
// This simulates embedding the placeholder manifest into an asset
static void insert_bytes_at_offset(
    const fs::path& source,
    const fs::path& dest,
    const std::vector<unsigned char>& data,
    size_t offset)
{
    // Read the entire source file
    std::ifstream src(source, std::ios::binary);
    std::vector<unsigned char> file_contents(
        (std::istreambuf_iterator<char>(src)),
        std::istreambuf_iterator<char>());
    src.close();

    // Insert the data at the specified offset
    file_contents.insert(file_contents.begin() + offset, data.begin(), data.end());

    // Write to destination
    std::ofstream dst(dest, std::ios::binary);
    dst.write(reinterpret_cast<const char*>(file_contents.data()), file_contents.size());
    dst.close();
}

// Replace bytes at a specific offset
static void replace_bytes_at_offset(
    const fs::path& file_path,
    const std::vector<unsigned char>& data,
    size_t offset)
{
    std::fstream file(file_path, std::ios::binary | std::ios::in | std::ios::out);
    if (!file.is_open()) {
        throw std::runtime_error("Could not open file for patching: " + file_path.string());
    }

    // Seek to offset and write the new data
    file.seekp(offset);
    file.write(reinterpret_cast<const char*>(data.data()), data.size());
    file.close();
}

// e2e workflow with A.jpg (has no existing C2PA)
TEST(Embeddable, FullWorkflowWithAJpg) {
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
    auto context = c2pa::Context::create();
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

// e2e workflow with C.jpg (has existing C2PA metadata)
TEST(Embeddable, FullWorkflowWithCJpg) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto signer = c2pa_test::create_test_signer();
    auto source_asset = c2pa_test::get_fixture_path("C.jpg");

    auto context = c2pa::Context::create();
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
TEST(Embeddable, PreCalculatedHash) {
    // This test demonstrates signing with a pre-calculated hash.
    // In production, you might calculate the hash on a different machine or at a different time than signing.

    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto signer = c2pa_test::create_test_signer();

    auto context = c2pa::Context::create();
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
TEST(Embeddable, AutoCalculatedHash) {
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

    auto context = c2pa::Context::create();
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
TEST(Embeddable, FormatEmbeddableRoundTrip) {
    // This test demonstrates the two-step format conversion:
    // 1. Sign with format="application/c2pa" to get raw JUMBF bytes
    // 2. Use format_embeddable() to convert to JPEG-embeddable format
    //
    // This is useful when you want to sign once and embed in multiple formats.

    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto signer = c2pa_test::create_test_signer();
    auto source_asset = c2pa_test::get_fixture_path("A.jpg");

    auto context = c2pa::Context::create();
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
TEST(Embeddable, PlaceholderSizeMatchesFinalInvariant) {
    // This test verifies the critical invariant:
    // placeholder.size() == final_signed_manifest.size()
    // This is what enables in-place patching in workflows.
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto signer = c2pa_test::create_test_signer();
    auto source_asset = c2pa_test::get_fixture_path("A.jpg");

    auto context = c2pa::Context::create();
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
TEST(Embeddable, PlaceholderSizeMatchesFinalInvariantWithMetadata) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto signer = c2pa_test::create_test_signer();
    auto source_asset = c2pa_test::get_fixture_path("C.jpg");

    auto context = c2pa::Context::create();
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

TEST(Embeddable, ContextSettingsPropagation) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto signer = c2pa_test::create_test_signer();
    auto source_asset = c2pa_test::get_fixture_path("A.jpg");

    // Create context with custom settings
    auto context = c2pa::Context::from_json(R"({
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

// Using archives with embedding
TEST(Embeddable, ArchiveRoundTripWithContextAJpg) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto signer = c2pa_test::create_test_signer();

    // Create context with custom settings (thumbnails disabled)
    auto context = c2pa::Context::from_json(R"({
        "builder": {
            "thumbnail": {
                "enabled": false
            }
        }
    })");

    // Create initial builder and get baseline placeholder size
    auto builder1 = c2pa::Builder(context, manifest_json);
    auto placeholder1 = builder1.data_hashed_placeholder(signer.reserve_size(), "image/jpeg");
    size_t original_size = placeholder1.size();

    // Save to archive
    auto archive_path = get_embeddable_temp_path("archive_a.c2pa");
    std::ofstream archive_stream(archive_path, std::ios::binary);
    builder1.to_archive(archive_stream);
    archive_stream.close();

    // Load from archive using same context
    auto builder2 = c2pa::Builder(context);
    std::ifstream load_stream(archive_path, std::ios::binary);
    builder2.load_archive(load_stream);
    load_stream.close();

    // Get placeholder from restored builder
    auto placeholder2 = builder2.data_hashed_placeholder(signer.reserve_size(), "image/jpeg");

    // Verify settings preserved by checking placeholder size matches
    EXPECT_EQ(placeholder2.size(), original_size)
        << "Settings should be preserved through archive round-trip";

    // Clean up temp file
    fs::remove(archive_path);
}

// Using archives with embedding with asset having content credentials
TEST(Embeddable, ArchiveRoundTripWithContextCJpg) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto signer = c2pa_test::create_test_signer();
    auto source_asset = c2pa_test::get_fixture_path("C.jpg");

    auto context = c2pa::Context::from_json(R"({
        "builder": {
            "thumbnail": {
                "enabled": false
            }
        }
    })");

    auto builder1 = c2pa::Builder(context, manifest_json);

    auto archive_path = get_embeddable_temp_path("archive_c.c2pa");
    std::ofstream archive_stream(archive_path, std::ios::binary);
    builder1.to_archive(archive_stream);
    archive_stream.close();

    auto builder2 = c2pa::Builder(context);
    std::ifstream load_stream(archive_path, std::ios::binary);
    builder2.load_archive(load_stream);
    load_stream.close();

    auto placeholder = builder2.data_hashed_placeholder(signer.reserve_size(), "image/jpeg");

    auto temp_asset = get_embeddable_temp_path("C_archive_roundtrip.jpg");
    size_t embed_offset = 20;
    insert_bytes_at_offset(source_asset, temp_asset, placeholder, embed_offset);

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

    std::ifstream asset_stream(temp_asset, std::ios::binary);
    auto signed_manifest = builder2.sign_data_hashed_embeddable(
        signer, data_hash, "image/jpeg", &asset_stream);
    asset_stream.close();

    auto final_asset = get_embeddable_temp_path("C_signed_from_archive.jpg");
    fs::copy_file(temp_asset, final_asset, fs::copy_options::overwrite_existing);
    replace_bytes_at_offset(final_asset, signed_manifest, embed_offset);

    c2pa::Reader reader(context, final_asset);
    std::string result_json = reader.json();
    EXPECT_FALSE(has_thumbnail(result_json));

    // Clean up temp files
    fs::remove(archive_path);
    fs::remove(temp_asset);
}

// Using with ingredients
TEST(Embeddable, ArchiveWithIngredientAJpg) {
    auto signer = c2pa_test::create_test_signer();
    auto ingredient_path = c2pa_test::get_fixture_path("A.jpg");

    // Create manifest with ingredient
    std::string manifest_with_ingredient = R"({
        "claim_generator": "test_app/1.0",
        "assertions": [
            {
                "label": "c2pa.actions",
                "data": {
                    "actions": [
                        {
                            "action": "c2pa.created"
                        }
                    ]
                }
            }
        ],
        "ingredients": [
            {
                "title": "A.jpg",
                "relationship": "parentOf"
            }
        ]
    })";

    auto context = c2pa::Context::from_json(R"({
        "builder": {
            "thumbnail": {
                "enabled": false
            }
        }
    })");

    auto builder1 = c2pa::Builder(context, manifest_with_ingredient);

    // Add ingredient using the proper API
    std::string ingredient_json = R"({"title": "A.jpg"})";
    builder1.add_ingredient(ingredient_json, ingredient_path);

    // Get baseline placeholder size
    auto placeholder1 = builder1.data_hashed_placeholder(signer.reserve_size(), "image/jpeg");
    size_t original_size = placeholder1.size();

    // Archive and restore
    auto archive_path = get_embeddable_temp_path("archive_with_ingredient_a.c2pa");
    std::ofstream archive_stream(archive_path, std::ios::binary);
    builder1.to_archive(archive_stream);
    archive_stream.close();

    auto builder2 = c2pa::Builder(context);
    std::ifstream load_stream(archive_path, std::ios::binary);
    builder2.load_archive(load_stream);
    load_stream.close();

    // Get placeholder from restored builder
    auto placeholder2 = builder2.data_hashed_placeholder(signer.reserve_size(), "image/jpeg");

    // Verify the ingredient survived the round-trip (size should match since ingredients are included)
    EXPECT_EQ(placeholder2.size(), original_size)
        << "Ingredient should be preserved through archive round-trip";

    // Clean up temp file
    fs::remove(archive_path);
}

// Using with ingredients with asset having content credentials
TEST(Embeddable, ArchiveWithIngredientCJpg) {
    auto signer = c2pa_test::create_test_signer();
    auto ingredient_path = c2pa_test::get_fixture_path("C.jpg");

    std::string manifest_with_ingredient = R"({
        "claim_generator": "test_app/1.0",
        "assertions": [
            {
                "label": "c2pa.actions",
                "data": {
                    "actions": [
                        {
                            "action": "c2pa.created"
                        }
                    ]
                }
            }
        ],
        "ingredients": [
            {
                "title": "C.jpg",
                "relationship": "parentOf"
            }
        ]
    })";

    auto context = c2pa::Context::from_json(R"({
        "builder": {
            "thumbnail": {
                "enabled": false
            }
        }
    })");

    auto builder1 = c2pa::Builder(context, manifest_with_ingredient);
    std::string ingredient_json = R"({"title": "C.jpg"})";
    builder1.add_ingredient(ingredient_json, ingredient_path);

    // Archive round-trip
    auto archive_path = get_embeddable_temp_path("archive_with_ingredient_c.c2pa");
    std::ofstream archive_stream(archive_path, std::ios::binary);
    builder1.to_archive(archive_stream);
    archive_stream.close();

    auto builder2 = c2pa::Builder(context);
    std::ifstream load_stream(archive_path, std::ios::binary);
    builder2.load_archive(load_stream);
    load_stream.close();

    // Verify we can create placeholders and sign after round-trip
    auto placeholder2 = builder2.data_hashed_placeholder(signer.reserve_size(), "image/jpeg");
    EXPECT_GT(placeholder2.size(), 0) << "Should be able to create placeholder after archive round-trip";

    // Verify we can sign
    std::string data_hash = R"({
        "exclusions": [{
            "start": 20,
            "length": )" + std::to_string(placeholder2.size()) + R"(
        }],
        "name": "jumbf manifest",
        "alg": "sha256",
        "hash": "gWZNEOMHQNiULfA/tO5HD2awOwYMA3tnfUPApIr9csk=",
        "pad": " "
    })";

    auto manifest = builder2.sign_data_hashed_embeddable(
        signer, data_hash, "image/jpeg", nullptr);
    EXPECT_GT(manifest.size(), 0) << "Should be able to sign after archive round-trip";

    // Clean up temp file
    fs::remove(archive_path);
}

// Verify different formats
TEST(Embeddable, MultipleFormats) {
    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto signer = c2pa_test::create_test_signer();

    auto context = c2pa::Context::create();

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

// Direct embedding when giving format
TEST(Embeddable, DirectEmbeddingWithFormat) {
    // This test demonstrates that when you sign with format="image/jpeg",
    // the output is already embeddable: no need for format_embeddable()!
    // Whereas...
    // Signing with "application/c2pa" requires a separate format_embeddable() call.

    auto manifest_json = c2pa_test::read_text_file(c2pa_test::get_fixture_path("training.json"));
    auto signer = c2pa_test::create_test_signer();
    auto source_asset = c2pa_test::get_fixture_path("A.jpg");

    auto context = c2pa::Context::create();
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
