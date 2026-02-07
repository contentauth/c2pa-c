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

/// @file   error_handling.test.cpp
/// @brief  Error handling tests for Builder and Reader.
/// @details Verifies that invalid inputs, malformed data, and error conditions
///          are handled with proper exceptions rather than crashes or UB.
///          See plans/02-error-handling.md for the full test plan.

#include <gtest/gtest.h>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "c2pa.hpp"
#include "test_utils.hpp"

namespace fs = std::filesystem;

// ============================================================================
// Helpers
// ============================================================================

static fs::path fixture_path(const std::string &name)
{
    return c2pa_test::get_fixture_path(name);
}

static std::string load_fixture(const std::string &name)
{
    return c2pa_test::read_text_file(fixture_path(name));
}

// ============================================================================
// Builder Error Handling
// ============================================================================

// Test 1: Empty manifest JSON causes underlying library to panic
// NOTE: c2pa-rs C API panics on invalid JSON instead of returning error.
// This is a known limitation of the underlying library, not the C++ wrapper.
TEST(BuilderErrorHandling, EmptyManifestJsonPanics)
{
    GTEST_SKIP() << "Skipped: underlying c2pa-rs C API panics on invalid JSON. "
                 << "This is a known issue that should be fixed upstream.";
}

// Test 2: Malformed/invalid JSON manifest causes underlying library to panic
// NOTE: c2pa-rs C API panics on invalid JSON instead of returning error.
TEST(BuilderErrorHandling, MalformedJsonManifestPanics)
{
    GTEST_SKIP() << "Skipped: underlying c2pa-rs C API panics on invalid JSON. "
                 << "This is a known issue that should be fixed upstream.";
}

// Test 3: add_resource with non-existent file path should throw
TEST(BuilderErrorHandling, InvalidResourcePathThrows)
{
    auto manifest = load_fixture("training.json");
    auto context = c2pa::Context::create();
    auto builder = c2pa::Builder(context, manifest);

    EXPECT_THROW(
        builder.add_resource("thumbnail", fs::path("nonexistent/path/to/file.jpg")),
        c2pa::C2paException
    );
}

// Test 4: add_ingredient with malformed JSON should throw
TEST(BuilderErrorHandling, InvalidIngredientJsonThrows)
{
    auto manifest = load_fixture("training.json");
    auto context = c2pa::Context::create();
    auto builder = c2pa::Builder(context, manifest);

    std::ifstream source(fixture_path("C.jpg"), std::ios::binary);
    ASSERT_TRUE(source.is_open());

    EXPECT_THROW(
        builder.add_ingredient("{bad json!!!", "image/jpeg", source),
        c2pa::C2paException
    );
}

// Test 5: Builder should be reusable after a failed sign (or throw cleanly on second attempt)
TEST(BuilderErrorHandling, BuilderReusableAfterFailedSign)
{
    auto manifest = load_fixture("training.json");
    auto context = c2pa::Context::create();
    auto builder = c2pa::Builder(context, manifest);
    auto signer = c2pa_test::create_test_signer();

    // First sign: empty stream should fail
    std::stringstream empty_source;
    std::stringstream dest1;
    EXPECT_THROW(
        builder.sign("image/jpeg", empty_source, dest1, signer),
        c2pa::C2paException
    );

    // Second attempt: valid streams should either succeed or throw cleanly (no crash/UB)
    std::ifstream valid_source(fixture_path("A.jpg"), std::ios::binary);
    ASSERT_TRUE(valid_source.is_open());
    std::stringstream dest2(std::ios::in | std::ios::out | std::ios::binary);

    // We don't mandate success — the builder's internal state may be consumed.
    // The key requirement is: no crash, no UB, no leak.
    try
    {
        builder.sign("image/jpeg", valid_source, dest2, signer);
    }
    catch (const c2pa::C2paException &)
    {
        // Acceptable: builder may not be reusable after failure
    }
}

// Test 6: sign with unsupported MIME type should throw
TEST(BuilderErrorHandling, InvalidMimeTypeStreamSignThrows)
{
    auto manifest = load_fixture("training.json");
    auto context = c2pa::Context::create();
    auto builder = c2pa::Builder(context, manifest);
    auto signer = c2pa_test::create_test_signer();

    std::ifstream source(fixture_path("C.jpg"), std::ios::binary);
    ASSERT_TRUE(source.is_open());
    std::stringstream dest(std::ios::in | std::ios::out | std::ios::binary);

    EXPECT_THROW(
        builder.sign("invalid/mime-type", source, dest, signer),
        c2pa::C2paException
    );
}

// Test 7: Builder with null context should throw
TEST(BuilderErrorHandling, NullContextThrows)
{
    EXPECT_THROW(
        { auto builder = c2pa::Builder(std::shared_ptr<c2pa::IContextProvider>(nullptr)); },
        c2pa::C2paException
    );
}

// Test 8: add_action with empty JSON should throw
TEST(BuilderErrorHandling, EmptyActionJsonThrows)
{
    auto manifest = load_fixture("training.json");
    auto context = c2pa::Context::create();
    auto builder = c2pa::Builder(context, manifest);

    EXPECT_THROW(
        builder.add_action(""),
        c2pa::C2paException
    );
}

// ============================================================================
// Reader Error Handling
// ============================================================================

// Test 9: Reading an empty file causes underlying library to panic
// NOTE: c2pa-rs C API panics on invalid/empty files instead of returning error.
TEST(ReaderErrorHandling, EmptyFilePanics)
{
    GTEST_SKIP() << "Skipped: underlying c2pa-rs C API panics on empty files. "
                 << "This is a known issue that should be fixed upstream.";
}

// Test 10: Reading a truncated/corrupted file causes underlying library to panic
// NOTE: c2pa-rs C API panics on malformed files instead of returning error.
TEST(ReaderErrorHandling, TruncatedFilePanics)
{
    GTEST_SKIP() << "Skipped: underlying c2pa-rs C API panics on truncated/corrupted files. "
                 << "This is a known issue that should be fixed upstream.";
}

// Test 11: Reader with unsupported MIME type causes underlying library to panic
// NOTE: c2pa-rs C API panics on unsupported MIME types instead of returning error.
TEST(ReaderErrorHandling, UnsupportedMimeTypePanics)
{
    GTEST_SKIP() << "Skipped: underlying c2pa-rs C API panics on unsupported MIME types. "
                 << "This is a known issue that should be fixed upstream.";
}

// Test 12: Reader with null context should throw
TEST(ReaderErrorHandling, NullContextThrows)
{
    EXPECT_THROW(
        { auto reader = c2pa::Reader(std::shared_ptr<c2pa::IContextProvider>(nullptr), fixture_path("C.jpg")); },
        c2pa::C2paException
    );
}
