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
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <stdexcept>
#include "unit_test.h"
#include "../include/c2pa.hpp"

using namespace std;

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

/// @brief signs the data using openssl and returns the signature
/// @details This function openssl to be installed as a command line tool
/// @param data std::vector<unsigned char> - the data to be signed
/// @return std::vector<unsigned char>  - the signature
std::vector<unsigned char> my_signer(const std::vector<unsigned char>& data) {
    if (data.empty()) {
        throw std::runtime_error("Signature data is empty");
    }

    std::ofstream source("target/cpp_data.bin", std::ios::binary);
    if (!source) {
        throw std::runtime_error("Failed to open temp signing file");
    }
    source.write(reinterpret_cast<const char*>(data.data()), data.size());

        // sign the temp file by calling openssl in a shell
    system("openssl dgst -sign tests/fixtures/es256_private.key -sha256 -out target/c_signature.sig target/c_data.bin");

    std::vector<uint8_t> signature;

    // Read the signature back into the output vector
    std::ifstream signature_file("target/c_signature.sig", std::ios::binary);
    if (!signature_file) {
        throw std::runtime_error("Failed to open signature file");
    }

    signature = std::vector<uint8_t>((std::istreambuf_iterator<char>(signature_file)), std::istreambuf_iterator<char>());

    return signature;
}

int main()
{
    auto version = c2pa::version();
    assert_contains("c2pa::version", version, "c2pa-c/0.");

    // test v2 ManifestStoreReader apis
    try {

        std::ifstream ifs("tests/fixtures/C.jpg", std::ios::binary);

        if (!ifs) {
            throw std::runtime_error("Failed to open file");
        };

        auto reader = c2pa::Reader("image/jpeg", ifs);

        // auto reader = c2pa::Reader("tests/fixtures/C.jpg");

        auto json = reader.json(); 
        assert_contains("c2pa::Reader.json", json, "C.jpg");

        auto thumb_path = "target/tmp/test_thumbail.jpg";
        std::remove(thumb_path);
        reader.get_resource("self#jumbf=c2pa.assertions/c2pa.thumbnail.claim.jpeg", thumb_path);
        assert_exists("c2pa::Reader.get_resource", thumb_path);
        ifs.close();
    }
    catch (c2pa::Exception e) {
        cout << "Failed: C2pa::Reader: " << e.what() << endl;
        return (1);
    };

    // test v2 ManifestBuilder apis
    try {
        char* manifest = load_file("tests/fixtures/training.json");
        char *certs = load_file("tests/fixtures/es256_certs.pem");

        // create a signer
        c2pa::Signer signer = c2pa::Signer( &my_signer, Es256, certs, "http://timestamp.digicert.com");

        const char *signed_path = "target/tmp/C_signed.jpg";
        std::remove(signed_path); // remove the file if it exists

        auto builder = c2pa::Builder(manifest);
        builder.add_resource("thumbnail", "tests/fixtures/A.jpg");
        string ingredient_json = "{\"title\":\"Test Ingredient\"}";
        builder.add_ingredient(ingredient_json, "tests/fixtures/C.jpg");
        auto manifest_data = builder.sign("tests/fixtures/C.jpg", signed_path, signer);
        assert_exists("c2pa::Builder.sign", signed_path);
    }
    catch (c2pa::Exception e) {
        cout << "Failed: C2pa::Builder: " << e.what() << endl;
        return (1);
    };

   // test v2 ManifestBuilder apis with cpp streams
    try {
        char *manifest = load_file("tests/fixtures/training.json");
        char *certs = load_file("tests/fixtures/es256_certs.pem");

        // create a signer
        c2pa::Signer signer = c2pa::Signer( &my_signer, Es256, certs, "http://timestamp.digicert.com");

        const char *signed_path = "target/tmp/C_signed-stream.jpg";
        std::remove(signed_path); // remove the file if it exists

        auto builder = c2pa::Builder(manifest);

        std::ifstream source("tests/fixtures/C.jpg", std::ios::binary);
        if (!source) {
            std::cerr << "Failed to open file: tests/fixtures/C.jpg" << std::endl;
            return 1;
        }

        // Create a memory buffer
        std::stringstream memory_buffer(std::ios::in | std::ios::out | std::ios::binary);
        std::iostream& dest = memory_buffer;
        auto manifest_data = builder.sign("image/jpeg", source, dest, signer);
        source.close();
    
        // Rewind dest to the start
        dest.flush();
        dest.seekp(0, std::ios::beg);
        auto reader = c2pa::Reader("image/jpeg", dest);
        auto json = reader.json();
        assert_contains("c2pa::Builder.sign", json, "c2pa.training-mining");
        //cout << "Manifest: " << json << endl;
    }
    catch (c2pa::Exception e) {
        cout << "Failed: C2pa::Builder: " << e.what() << endl;
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
