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
#include "test_signer.hpp"

using namespace std;
namespace fs = std::filesystem;

/// @brief Read a text file into a string
string read_text_file(const fs::path &path)
{
    ifstream file(path);
    if (!file.is_open())
    {
        throw runtime_error("Could not open file " + string(path));
    }
    string contents((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    file.close();
    return contents.data();
}

TEST(Builder, SignFile)
{

    fs::path current_dir = fs::path(__FILE__).parent_path();

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../target/example/training.jpg";

    try
    {
        auto manifest = read_text_file(manifest_path);
        auto certs = read_text_file(certs_path);

        // create a signer
        c2pa::Signer signer = c2pa::Signer(&test_signer, Es256, certs, "http://timestamp.digicert.com");

        std::remove(output_path.c_str()); // remove the file if it exists

        auto builder = c2pa::Builder(manifest);
        builder.add_resource("thumbnail", image_path);

        string ingredient_json = "{\"title\":\"Test Ingredient\"}";
        builder.add_ingredient(ingredient_json, signed_image_path);
        auto manifest_data = builder.sign(signed_image_path, output_path, signer);
        auto reader = c2pa::Reader(output_path);
        auto json = reader.json();
        ASSERT_TRUE(std::filesystem::exists(output_path));
    }
    catch (c2pa::C2paException const &e)
    {
        FAIL() << "Failed: C2pa::Builder: " << e.what() << endl;
    };
};

TEST(Builder, SignStream)
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

        // create a signer
        c2pa::Signer signer = c2pa::Signer(&test_signer, Es256, certs, "http://timestamp.digicert.com");

        auto builder = c2pa::Builder(manifest);

        std::ifstream source(signed_image_path, std::ios::binary);
        if (!source)
        {
            FAIL() << "Failed to open file: " << signed_image_path << std::endl;
        }

        // Create a memory buffer
        std::stringstream memory_buffer(std::ios::in | std::ios::out | std::ios::binary);
        std::iostream &dest = memory_buffer;
        auto _ = builder.sign("image/jpeg", source, dest, signer);
        source.close();

        // Rewind dest to the start
        dest.flush();
        dest.seekp(0, std::ios::beg);
        auto reader = c2pa::Reader("image/jpeg", dest);
        auto json = reader.json();
        ASSERT_TRUE(json.find("cawg.training-mining") != std::string::npos);
    }
    catch (c2pa::C2paException const &e)
    {
        FAIL() << "Failed: C2pa::Builder: " << e.what() << endl;
    };
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

        // create a signer
        c2pa::Signer signer = c2pa::Signer(&test_signer, Es256, certs, "http://timestamp.digicert.com");

        auto builder = c2pa::Builder(manifest);

        // very important to use a URL that does not exist, otherwise you may get a JumbfParseError or JumbfNotFound
        builder.set_remote_url("http://this_does_not_exist/foo.jpg");
        builder.set_no_embed();

        // auto manifest_data = builder.sign(signed_image_path, "target/dest.jpg", signer);
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
    }
    catch (c2pa::C2paException const &e)
    {
        std::string error_message = e.what();
        if (error_message.rfind("Remote ", 0) == 0)
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
    try
    {
        fs::path current_dir = fs::path(__FILE__).parent_path();

        // Construct the paths relative to the current directory
        fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
        fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
        // fs::path signed_image_path = current_dir / "../tests/fixtures/A.jpg";

        auto manifest = read_text_file(manifest_path);
        auto certs = read_text_file(certs_path);

        // create a signer
        c2pa::Signer signer = c2pa::Signer(&test_signer, Es256, certs, "http://timestamp.digicert.com");

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
        auto manifest_data = builder.sign_data_hashed_embeddable(signer, data_hash, "image/jpeg");
    }
    catch (c2pa::C2paException const &e)
    {
        FAIL() << "Failed: C2pa::Builder: " << e.what() << endl;
    };
}

TEST(Builder, SignDataHashedEmbeddedWithAsset)
{
    try
    {
        fs::path current_dir = fs::path(__FILE__).parent_path();

        // Construct the paths relative to the current directory
        fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
        fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
        fs::path image_path = current_dir / "../tests/fixtures/A.jpg";

        auto manifest = read_text_file(manifest_path);
        auto certs = read_text_file(certs_path);

        // create a signer
        c2pa::Signer signer = c2pa::Signer(&test_signer, Es256, certs, "http://timestamp.digicert.com");

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

        auto manifest_data = builder.sign_data_hashed_embeddable(signer, data_hash, "application/c2pa", &asset);

        auto embeddable_data = c2pa::Builder::format_embeddable("image/jpeg", manifest_data);

        ASSERT_TRUE(embeddable_data.size() > manifest_data.size());
    }
    catch (c2pa::C2paException const &e)
    {
        FAIL() << "Failed: C2pa::Builder: " << e.what() << endl;
    };
}
