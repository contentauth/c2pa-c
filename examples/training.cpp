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

using namespace std;


string read_text_file(const char* path) {
    ifstream file(path);
    if (!file.is_open()) {
        throw runtime_error("Could not open file " + string(path));
    }
    string contents((istreambuf_iterator<char>(file)), istreambuf_iterator<char>());
    file.close();
    return contents.data();
}


/// @brief Example of signing a file with a manifest and reading the manifest back
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
        C2paSignerInfo sign_info = {.alg = "es256", .tsa_url = "http://timestamp.digicert.com", .signcert = certs.c_str(), .pkey = private_key.c_str()};
   
        // sign the file
        C2pa::sign_file("tests/fixtures/A.jpg", "target/example/training.jpg", manifest_json.c_str(), sign_info, NULL);

        // read the manifest back and display the JSON
        cout << "Manifest is " << C2pa::read_file("target/example/training.jpg", "target/tmp") << endl;
    }
    catch (C2pa::Exception e)
    {
        cout << "C2PA Error: " << e.what() << endl;
    }
    catch(runtime_error e)
    {
        cout << "setup error" << e.what() << endl;
    }
}
