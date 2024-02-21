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
#include <stdlib.h>
#include <string.h>
#include "unit_test.h"
#include "../include/c2pa.hpp"

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
void assert_exists(const char *what, const char* file_path)
{
    if (std::filesystem::exists(file_path) == false)
    {
        fprintf(stderr, "FAILED %s: File %s does not exist\n", what, file_path);
        exit(1);
    }
    printf("PASSED: %s\n", what);
}

using namespace std;

int main()
{
    auto version = c2pa::version();
    assert_contains("c2pa::version", version, "c2pa-c/0.");

    // test v2 ManifestStoreReader apis
    try {
        auto reader = c2pa::Reader("tests/fixtures/C.jpg");

        auto json = reader.json(); 
        assert_contains("c2pa::ManifestStoreReader.json", json, "C.jpg");

        auto thumb_path = "target/tmp/test_thumbail.jpg";
        std::remove(thumb_path);
        reader.get_resource("self#jumbf=c2pa.assertions/c2pa.thumbnail.claim.jpeg", thumb_path);
        assert_exists("c2pa::ManifestStoreReader.get_resource", thumb_path);
    }
    catch (c2pa::Exception e) {
        cout << "Failed: C2pa::ManifestStoreReader: " << e.what() << endl;
        return (1);
    };

    // test v2 ManifestBuilder apis
    try {
        char *manifest = load_file("tests/fixtures/training.json");
        char *certs = load_file("tests/fixtures/es256_certs.pem");
        char *private_key = load_file("tests/fixtures/es256_private.key");

        // create a sign_info struct
        C2paSignerInfo sign_info = {.alg = "es256", .sign_cert = certs, .private_key = private_key, .ta_url = "http://timestamp.digicert.com"};
        const char *signed_path = "target/tmp/C_signed.jpg";
        std::remove(signed_path); // remove the file if it exists
        auto builder = c2pa::Builder(manifest);
        auto manifest_data = builder.sign("tests/fixtures/C.jpg", signed_path, &sign_info);
        printf("manifest_size: %lu\n", manifest_data->size());
        free(manifest_data);
        assert_exists("c2pa::ManifestBuilder.sign", signed_path);
    }
    catch (c2pa::Exception e) {
        cout << "Failed: C2pa::ManifestBuilder: " << e.what() << endl;
        return (1);
    };

    try
    {
        // read a file with a valid manifest
        auto manifest_json = c2pa::read_file("tests/fixtures/C.jpg", "target/tmp");
        if (manifest_json.has_value())
        {
            assert_contains("c2pa::read_file", manifest_json.value(), "C.jpg");
        }
        else
        {
            cout << "Failed: c2pa::read_file_: manifest_json is empty" << endl;
            return (1);
        }
    }
    catch (c2pa::Exception e)
    {
        cout << "Failed: c2pa::read_file_: " << e.what() << endl;
        return (1);
    };

    try
    {
        // read a file with with no manifest and no data_dir
        auto manifest_json2 = c2pa::read_file("tests/fixtures/A.jpg");
        if (manifest_json2.has_value())
        {
            cout << "Failed: c2pa::read_file_no_manifest: manifest_json2 is not empty" << endl;
            return (1);
        }
    }
    catch (c2pa::Exception e)
    {
        cout << "Failed: c2pa::read_file_no_manifest: " << e.what() << endl;
    };

     try
    {
        // read a file with with no manifest and no data_dir
        auto manifest_json2 = c2pa::read_file("tests/fixtures/Z.jpg");
        cout << "Failed: c2pa::read_file_not_found";
        return (1);
    }
    catch (c2pa::Exception e)
    {
        assert_contains("c2pa::read_file_not_found", e.what(), "No such file or directory");
    };
}
