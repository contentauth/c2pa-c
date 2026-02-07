// Copyright 2023 Adobe. All rights reserved.
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

// this example uses nlohmann json for parsing the manifest
using json = nlohmann::json;
using namespace std;
namespace fs = std::filesystem;

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

// vector<unsigned char> my_signer(const std::vector<unsigned char> &data)
// {
//     fs::path private_key_path = get_current_directory(__FILE__) / "../tests/fixtures/es256_private.key";
//     return es25519_signer(data, private_key_path.c_str());
// };

/// @brief Example of signing a file with a manifest and reading the manifest back
/// @details This shows how to write a do not train assertion and read the status back
/// @return 0 on success, 1 on failure
int main()
{
    // Get the current directory of this file
    fs::path current_dir = get_current_directory(__FILE__);
    fs::path project_root = current_dir.parent_path(); // Go up one level to project root

    // Construct the paths relative to the project root
    fs::path manifest_path = project_root / "tests/fixtures/training.json";
    fs::path certs_path = project_root / "tests/fixtures/es256_certs.pem";
    fs::path image_path = project_root / "tests/fixtures/A.jpg";
    fs::path output_path = project_root / "build/examples/training.jpg";

    cout << "The C2pa library version is " << c2pa::version() << endl;
    cout << "RUNNING EXAMPLE training.cpp " << endl;

    try
    {
        // load the manifest, certs, and private key
        string manifest_json = read_text_file(manifest_path).data();
        string certs = read_text_file(certs_path).data();
        string p_key = read_text_file(current_dir / "../tests/fixtures/es256_private.key").data();

        // create a signer
        c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

        auto builder = c2pa::Builder(manifest_json);
        auto manifest_data = builder.sign(image_path, output_path, signer);

        // read the new manifest and display the JSON
        auto reader = c2pa::Reader(output_path);
        auto new_manifest_json = reader.json();
        cout << "The new manifest is " << new_manifest_json << endl;

        // parse the manifest and display the AI training status
        bool allowed = true; // default to allowed
        json manifest_store = json::parse(new_manifest_json);

        // get the active manifest
        string active_manifest = manifest_store["active_manifest"];
        json &manifest = manifest_store["manifests"][active_manifest];

        // scan the assertions for the training-mining assertion
        for (auto &assertion : manifest["assertions"])
        {
            if (assertion["label"] == "c2pa.training-mining")
            {
                for (json &entry : assertion["data"]["entries"])
                {
                    if (entry["use"] == "notAllowed")
                    {
                        allowed = false;
                    }
                }
            }
        }
        cout << "AI training is " << (allowed ? "allowed" : "not allowed") << endl;
    }
    catch (c2pa::C2paException const &e)
    {
        cout << "C2PA Error: " << e.what() << endl;
    }
    catch (runtime_error const &e)
    {
        cout << "setup error: " << e.what() << endl;
    }
    catch (json::parse_error const &e)
    {
        cout << "parse error " << e.what() << endl;
    }
}
