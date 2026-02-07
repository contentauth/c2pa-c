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
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <stdexcept>
#include "unit_test.h"
#include "../include/c2pa.hpp"
#include "../tests/cmd_signer.hpp"

using namespace std;

// TODO-TMN: Update with context APIs
// TODO-TMN: Turn this into a simple main example too!
// TODO-TMN: move in folder app-test/cpp-apis

// assert that c2pa_str contains substr or exit
void assert_contains(const char *what, std::string str, const char *substr)
{
    if (strstr(str.c_str(), substr) == NULL)
    {
        fprintf(stderr, "FAILED %s: %s not found in %s\n", what, str.c_str(), substr);
        exit(1);
    }
    printf("PASSED: %s\n", what);
}

// assert that c2pa_str contains substr or exit
void assert_exists(const char *what, const char *file_path)
{
    if (std::filesystem::exists(file_path) == false)
    {
        fprintf(stderr, "FAILED %s: File %s does not exist\n", what, file_path);
        exit(1);
    }
    printf("PASSED: %s\n", what);
}

int main()
{
    // test v2 ManifestStoreReader apis
    try
    {

        std::ifstream ifs("tests/fixtures/C.jpg", std::ios::binary);

        if (!ifs)
        {
            throw std::runtime_error("Failed to open file");
        };

        auto reader = c2pa::Reader("image/jpeg", ifs);

        auto json = reader.json();
        assert_contains("c2pa::Reader.json", json, "C.jpg");

        auto thumb_path = "target/tmp/test_thumbail.jpg";
        std::remove(thumb_path);
        reader.get_resource("self#jumbf=c2pa.assertions/c2pa.thumbnail.claim.jpeg", thumb_path);
        assert_exists("c2pa::Reader.get_resource", thumb_path);
        ifs.close();
    }
    catch (c2pa::C2paException e)
    {
        cout << "Failed: C2pa::Reader: " << e.what() << endl;
        return (1);
    };

    // test v2 ManifestBuilder apis
    try
    {
        char *manifest = load_file("tests/fixtures/training.json");
        char *certs = load_file("tests/fixtures/es256_certs.pem");

        // create a signer
        c2pa::Signer signer = c2pa::Signer(&cmd_signer, Es256, certs, "http://timestamp.digicert.com");

        const char *signed_path = "target/tmp/C_signed.jpg";
        std::remove(signed_path); // remove the file if it exists

        auto builder = c2pa::Builder(manifest);
        builder.add_resource("thumbnail", "tests/fixtures/A.jpg");
        string ingredient_json = "{\"title\":\"Test Ingredient\"}";
        builder.add_ingredient(ingredient_json, "tests/fixtures/C.jpg");
        auto manifest_data = builder.sign("tests/fixtures/C.jpg", signed_path, signer);
        assert_exists("c2pa::Builder.sign", signed_path);
    }
    catch (c2pa::C2paException e)
    {
        cout << "Failed: C2pa::Builder: " << e.what() << endl;
        return (1);
    };

    // test v2 ManifestBuilder apis with cpp streams
    try
    {
        char *manifest = load_file("tests/fixtures/training.json");
        char *certs = load_file("tests/fixtures/es256_certs.pem");

        // create a signer
        c2pa::Signer signer = c2pa::Signer(&cmd_signer, Es256, certs, "http://timestamp.digicert.com");

        const char *signed_path = "target/tmp/C_signed-stream.jpg";
        std::remove(signed_path); // remove the file if it exists

        auto builder = c2pa::Builder(manifest);

        std::ifstream source("tests/fixtures/C.jpg", std::ios::binary);
        if (!source)
        {
            std::cerr << "Failed to open file: tests/fixtures/C.jpg" << std::endl;
            return 1;
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
        auto json = reader.json();
        assert_contains("c2pa::Builder.sign", json, "c2pa.training-mining");
        // cout << "Manifest: " << json << endl;
    }
    catch (c2pa::C2paException const &e)
    {
        cout << "Failed: C2pa::Builder: " << e.what() << endl;
        return (1);
    };
}
