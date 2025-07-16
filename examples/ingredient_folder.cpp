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

#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <stdexcept>
#include "c2pa.hpp"
#include <nlohmann/json.hpp>

#include <sstream>
#include <algorithm>

// this example uses nlohmann json for parsing the manifest
using json = nlohmann::json;
using namespace std;
namespace fs = std::filesystem;
using namespace c2pa;

/// @brief Read a text file into a string
string read_text_file(const fs::path &path)
{
    ifstream file(path);
    if (!file.is_open())
    {
        throw runtime_error("Could not open file " + path.string());
    }
    string contents((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    file.close();
    return contents.data();
}

// Helper function to get the directory of the current file
fs::path get_current_directory(const char *file_path)
{
    return fs::path(file_path).parent_path();
}

std::string uri_to_path(string uri, string manifest_label) {
    std::string path = uri;

    string prefix = "self#jumbf=";
    if (path.rfind(prefix, 0) == 0) { // starts with "self#jumbf="
        path = path.substr(prefix.length());

        string c2pa_prefix = "/c2pa/";
        if (path.rfind(c2pa_prefix, 0) == 0) {
            path = path.substr(c2pa_prefix.length());
        } else {
            path = manifest_label + "/" + path;
        }

        // Replace ':' with '_'
        std::replace(path.begin(), path.end(), ':', '_');
    }

    return path;
}

/// @brief Example of signing a file with a manifest and reading the manifest back
/// @details This shows how to write a do not train assertion and read the status back
/// @return 0 on success, 1 on failure
int main()
{
    // Get the current directory of this file
    fs::path current_dir = get_current_directory(__FILE__);

    // Construct the paths relative to the current directory
    fs::path manifest_path = current_dir / "../tests/fixtures/training.json";
    fs::path certs_path = current_dir / "../tests/fixtures/es256_certs.pem";
    fs::path image_path = current_dir / "../tests/fixtures/A.jpg";
    fs::path output_path = current_dir / "../build/example/training.jpg";
    fs::path thumbnail_path = current_dir / "../build/example/thumbnail.jpg";

    fs::path ingredient_path = current_dir / "../tests/fixtures/ingredient_c";
    std::stringstream empty_stream; 
    try
    {
        // load the manifest, certs, ingredient, and private key
        json manifest_json = json::parse(read_text_file(manifest_path).data());
        string certs = read_text_file(certs_path).data();
        string p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key").data();
        string ingredient_json = read_text_file(ingredient_path / "ingredient.json");
        manifest_json["ingredients"].push_back(json::parse(ingredient_json));
        // create a signer
        Signer signer = Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

        auto builder = Builder(manifest_json.dump());
                
        json ing_json = json::parse(ingredient_json);

        //TODO: This can be replaced with just setting the base_path
        if (ing_json.contains("manifest_data")) {
            string identifier = ing_json["manifest_data"]["identifier"];
            ifstream manifest(ingredient_path / identifier);
            builder.add_resource(identifier, manifest);
        }

        if (ing_json.contains("thumbnail")) {
            string identifier = ing_json["thumbnail"]["identifier"];
            identifier = uri_to_path(identifier, "unknown");
            ifstream thumbnail(ingredient_path / identifier);
            builder.add_resource(identifier, thumbnail);
        }

        builder.to_archive(ingredient_path / "test.archive");

        auto builder_from = builder.from_archive(ingredient_path / "test.archive");

        auto manifest_data = builder_from.sign(image_path, output_path, signer);
        // read the new manifest and display the JSON
        auto reader = Reader(output_path);

        auto manifest_store_json = reader.json();
        cout << "The new manifest is " << manifest_store_json << endl;

        // get the active manifest
        json manifest_store = json::parse(manifest_store_json);
        if (manifest_store.contains("active_manifest"))
        {
            string active_manifest = manifest_store["active_manifest"];
            json &manifest = manifest_store["manifests"][active_manifest];

            string identifer = manifest["ingredients"][0]["thumbnail"]["identifier"];

            reader.get_resource(identifer, thumbnail_path);

            cout << "thumbnail written to" << thumbnail_path << endl;
        }
    }
    catch (c2pa::C2paException const &e)
    {
        cout << "C2PA Error: " << e.what() << endl;
    }
    catch (runtime_error const &e)
    {
        cout << "setup error" << e.what() << endl;
    }
    catch (json::parse_error const &e)
    {
        cout << "parse error " << e.what() << endl;
    }
}
