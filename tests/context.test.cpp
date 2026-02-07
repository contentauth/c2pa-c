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

/// @file   api_coverage.test.cpp
/// @brief  Tests for Context, Settings, Archive, and MIME type APIs.
/// @details Fills coverage gaps in the Context/Settings API, archive round-trip,
///          and MIME type support. See plans/04-context-settings-archive-mime.md

#include <gtest/gtest.h>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <string>

#include "c2pa.hpp"
#include "test_utils.hpp"

namespace fs = std::filesystem;

static fs::path fixture_path(const std::string &name)
{
    return c2pa_test::get_fixture_path(name);
}

static std::string load_fixture(const std::string &name)
{
    return c2pa_test::read_text_file(fixture_path(name));
}

// Can create a context
TEST(ContextAPI, ContextCreateReturnsValid)
{
    auto context = c2pa::Context::create();
    ASSERT_NE(context, nullptr);
    EXPECT_TRUE(context->has_context());
}

// Can create a context using JSON settings
TEST(ContextAPI, ContextFromJsonValid)
{
    std::string json = R"({"settings": {}})";
    auto context = c2pa::Context::from_json(json);
    ASSERT_NE(context, nullptr);
    EXPECT_TRUE(context->has_context());
}

// Context::from_json() with invalid JSON throws
TEST(ContextAPI, ContextFromJsonInvalidThrows)
{
    EXPECT_THROW(
        { auto context = c2pa::Context::from_json("{bad"); },
        c2pa::C2paException
    );
}

// Context::from_toml() with valid TOML returns valid context
TEST(ContextAPI, ContextFromTomlValid)
{
    std::string toml = "[settings]\n";
    auto context = c2pa::Context::from_toml(toml);
    ASSERT_NE(context, nullptr);
    EXPECT_TRUE(context->has_context());
}

// Context::from_toml() with invalid TOML throws
TEST(ContextAPI, ContextFromTomlInvalidThrows)
{
    EXPECT_THROW(
        { auto context = c2pa::Context::from_toml("bad toml [[[]"); },
        c2pa::C2paException
    );
}

// Default context can be used with a Builder
TEST(SettingsAPI, SettingsDefaultConstruction)
{
    c2pa::Settings settings;
    auto manifest = load_fixture("training.json");
    auto context = c2pa::Context::create();

    // Should not crash when building with default settings
    EXPECT_NO_THROW({
        auto builder = c2pa::Builder(context, manifest);
    });
}

// Can update settings with any valid JSON
TEST(SettingsAPI, SettingsUpdateJson)
{
    c2pa::Settings settings;
    EXPECT_NO_THROW({
        settings.update(R"({"key": "val"})", "json");
    });
}

// ContextBuilder empty build returns default/empty context
TEST(ContextBuilderAPI, ContextBuilderEmptyBuild)
{
    auto builder = c2pa::Context::ContextBuilder();
    auto context = builder.create_context();

    ASSERT_NE(context, nullptr);
    EXPECT_TRUE(context->has_context());
}
