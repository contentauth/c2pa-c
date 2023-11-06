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
#include "../include/c2pa.hpp"
#include "json.hpp"

// this example uses nlohmann json for parsing the manifest
using json = nlohmann::json;
using namespace std;

/// @brief Read a text file into a string
string read_text_file(const char *path)
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

/// @brief Example of signing a file with a manifest and reading the manifest back
/// @details This shows how to write a do not train assertion and read the status back
/// @return 0 on success, 1 on failure
int main()
{
    cout << "The C2pa library version is " << C2pa::version() << endl;

    try
    {
        // load the manifest, certs, and private key
        string manifest_json = read_text_file("tests/fixtures/training.json").data();
        string certs = read_text_file("tests/fixtures/es256_certs.pem").data();
        string private_key = read_text_file("tests/fixtures/es256_private.key").data();

        // create a sign_info struct
        C2pa::SignerInfo sign_info = {.alg = "es256", .sign_cert = certs.c_str(), .private_key = private_key.c_str(), .ta_url = "http://timestamp.digicert.com"};

        // sign the file
        C2pa::sign_file("tests/fixtures/A.jpg", "target/example/training.jpg", manifest_json.c_str(), sign_info);

        // read the new manifest and display the JSON
        auto new_manifest_json = C2pa::read_file("target/example/training.jpg");
        cout << "The new manifest is " << new_manifest_json << endl;

        // parse the manifest and display the AI training status

        bool allowed = true; // default to allowed
        json manifest_store = json::parse(new_manifest_json.c_str());

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
    catch (C2pa::Exception e)
    {
        cout << "C2PA Error: " << e.what() << endl;
    }
    catch (runtime_error e)
    {
        cout << "setup error" << e.what() << endl;
    }
    catch (json::parse_error &e)
    {
        cout << "parse error " << e.what() << endl;
    }
}
