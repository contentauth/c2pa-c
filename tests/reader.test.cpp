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

using nlohmann::json;

TEST(Reader, StreamWithManifest)
{
    // read the new manifest and display the JSON
    std::ifstream file_stream("../../tests/fixtures/C.jpg", std::ios::binary);
    auto reader = c2pa::Reader("image/jpeg", file_stream);
    auto manifest_store_json = reader.json();
    EXPECT_TRUE(manifest_store_json.find("C.jpg") != std::string::npos);
};

TEST(Reader, FileWithManifest)
{
    // read the new manifest and display the JSON
    auto reader = c2pa::Reader("../../tests/fixtures/C.jpg");
    auto manifest_store_json = reader.json();
    EXPECT_TRUE(manifest_store_json.find("C.jpg") != std::string::npos);
};

TEST(Reader, FileNoManifest)
{
    EXPECT_THROW({ auto reader = c2pa::Reader("../../tests/fixtures/A.jpg"); }, c2pa::Exception);
};

TEST(Reader, FileNotFound)
{
    try
    {
        auto reader = c2pa::Reader("foo/xxx.xyz");
        FAIL() << "Expected c2pa::Exception";
    }
    catch (const c2pa::Exception &e)
    {
        // EXPECT_STREQ(e.what(), "File not found");
        EXPECT_TRUE(std::string(e.what()).rfind("Failed to open file", 0) == 0);
    }
    catch (...)
    {
        FAIL() << "Expected c2pa::Exception Failed to open file";
    }
};
