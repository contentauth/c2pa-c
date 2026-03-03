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

/// @file test.cpp
/// @brief Simple C++ SDK example showing some C2PA operations

#include <iostream>
#include <fstream>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <vector>
#include <stdexcept>
#include "unit_test.h"
#include "../../include/c2pa.hpp"
#include "cmd_signer.hpp"

using namespace std;

// Helper: assert that string contains substring or exit
void assert_contains(const char *what, std::string str, const char *substr)
{
    if (strstr(str.c_str(), substr) == NULL)
    {
        fprintf(stderr, "FAILED %s: %s not found in %s\n", what, str.c_str(), substr);
        exit(1);
    }
    printf("PASSED: %s\n", what);
}

// Helper: assert that file exists or exit
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
    cout << "\n=== C2PA C++ SDK Example ===" << endl;
    cout << "Version: " << c2pa::version() << endl << endl;

    // ========================================================================
    // Example 1: Reading a manifest
    // ========================================================================
    cout << "--- Example 1: Reading a manifest ---" << endl;
    try
    {
        // Create default context (can be used for basic operations)
        auto default_context = c2pa::Context();

        std::ifstream ifs("tests/fixtures/C.jpg", std::ios::binary);
        if (!ifs)
        {
            throw std::runtime_error("Failed to open file");
        }

        // Create a Reader with default context
        auto reader = c2pa::Reader(default_context, "image/jpeg", ifs);

        auto json = reader.json();
        assert_contains("Reader.json", json, "C.jpg");

        // Extract a resource (thumbnail)
        auto thumb_path = "target/tmp/test_thumbnail.jpg";
        std::remove(thumb_path);
        reader.get_resource("self#jumbf=c2pa.assertions/c2pa.thumbnail.claim.jpeg", thumb_path);
        assert_exists("Reader.get_resource", thumb_path);

        ifs.close();
        cout << "Successfully read manifest and extracted thumbnail" << endl;
    }
    catch (c2pa::C2paException &e)
    {
        cout << "FAILED: Reader example: " << e.what() << endl;
        return (1);
    }

    // ========================================================================
    // Example 2: Signing with default context
    // ========================================================================
    cout << "\n--- Example 2: Signing with default context ---" << endl;
    try
    {
        char *manifest = load_file("tests/fixtures/training.json");
        char *certs = load_file("tests/fixtures/es256_certs.pem");

        // Create a signer using a callback function
        c2pa::Signer signer = c2pa::Signer(&cmd_signer, Es256, certs, "http://timestamp.digicert.com");

        const char *signed_path = "target/tmp/C_signed.jpg";
        std::remove(signed_path);

        // Create default context for basic operations
        auto default_context = c2pa::Context();

        // Create builder with context
        auto builder = c2pa::Builder(default_context, manifest);
        builder.add_resource("thumbnail", "tests/fixtures/A.jpg");

        string ingredient_json = "{\"title\":\"Test Ingredient\"}";
        builder.add_ingredient(ingredient_json, "tests/fixtures/C.jpg");

        auto manifest_data = builder.sign("tests/fixtures/C.jpg", signed_path, signer);
        assert_exists("Builder.sign", signed_path);

        free(manifest);
        free(certs);

        cout << "Successfully signed image with default context" << endl;
    }
    catch (c2pa::C2paException &e)
    {
        cout << "FAILED: Builder example: " << e.what() << endl;
        return (1);
    }

    // ========================================================================
    // Example 3: Stream-based signing
    // ========================================================================
    cout << "\n--- Example 3: Stream-based signing ---" << endl;
    try
    {
        char *manifest = load_file("tests/fixtures/training.json");
        char *certs = load_file("tests/fixtures/es256_certs.pem");

        c2pa::Signer signer = c2pa::Signer(&cmd_signer, Es256, certs, "http://timestamp.digicert.com");

        const char *signed_path = "target/tmp/C_signed-stream.jpg";
        std::remove(signed_path);

        auto default_context = c2pa::Context();
        auto builder = c2pa::Builder(default_context, manifest);

        std::ifstream source("tests/fixtures/C.jpg", std::ios::binary);
        if (!source)
        {
            std::cerr << "Failed to open file: tests/fixtures/C.jpg" << std::endl;
            return 1;
        }

        // Create a memory buffer for output
        std::stringstream memory_buffer(std::ios::in | std::ios::out | std::ios::binary);
        std::iostream &dest = memory_buffer;

        auto manifest_data = builder.sign("image/jpeg", source, dest, signer);
        source.close();

        // Verify by reading back from the stream
        dest.flush();
        dest.seekp(0, std::ios::beg);

        auto reader = c2pa::Reader(default_context, "image/jpeg", dest);
        auto json = reader.json();
        assert_contains("Builder.sign (stream)", json, "c2pa.training-mining");

        free(manifest);
        free(certs);

        cout << "Successfully signed using streams" << endl;
    }
    catch (c2pa::C2paException const &e)
    {
        cout << "FAILED: Stream signing example: " << e.what() << endl;
        return (1);
    }

    // ========================================================================
    // Example 4: Using trust anchors for validation
    // ========================================================================
    cout << "\n--- Example 4: Trust-based validation ---" << endl;
    try
    {
        // Load trust configuration from config file
        char *trust_settings = load_file("tests/fixtures/settings/test_settings_example.json");

        // Create a context with trust anchors
        auto trusted_context = c2pa::Context::ContextBuilder().with_settings(c2pa::Settings(trust_settings, "json")).create_context();
        free(trust_settings);

        cout << "Created context with trust anchors" << endl;

        // Sign an image with trust context
        char *manifest = load_file("tests/fixtures/training.json");
        char *certs = load_file("tests/fixtures/es256_certs.pem");
        char *p_key = load_file("tests/fixtures/es256_private.key");

        c2pa::Signer signer = c2pa::Signer("Es256", certs, p_key, "http://timestamp.digicert.com");

        const char *trusted_signed_path = "target/tmp/C_trusted_signed.jpg";
        std::remove(trusted_signed_path);

        auto builder = c2pa::Builder(trusted_context, manifest);
        builder.add_resource("thumbnail", "tests/fixtures/A.jpg");

        auto signed_manifest = builder.sign("tests/fixtures/C.jpg", trusted_signed_path, signer);
        assert_exists("Trusted Builder.sign", trusted_signed_path);

        cout << "Signed image with trust context" << endl;

        // Read back with trust context - should validate as "Trusted"
        auto trusted_reader = c2pa::Reader(trusted_context, trusted_signed_path);
        string trusted_json = trusted_reader.json();

        // In a scenario with valid trust chain, this would show "Trusted"
        // For test fixtures, it may show as "Valid" depending on the configured certificate chain
        cout << "Read manifest with trust validation" << endl;

        // Compare: read without trust context - will only show "Valid" at best
        auto default_context2 = c2pa::Context();
        auto basic_reader = c2pa::Reader(default_context2, trusted_signed_path);
        string basic_json = basic_reader.json();

        cout << "Trust example: Context with trust anchors provides trust validation" << endl;

        free(manifest);
        free(certs);
        free(p_key);
    }
    catch (c2pa::C2paException &e)
    {
        cout << "FAILED: Trust validation example: " << e.what() << endl;
        return (1);
    }

    // ========================================================================
    // Example 5: Context with settings
    // ========================================================================
    cout << "\n--- Example 5: Context with custom settings ---" << endl;
    try
    {
        // Build context using ContextBuilder for fine-grained control
        auto settings_json = R"({
            "builder": {
                "claim_generator_info": {
                    "name": "C2PA C++ SDK Example",
                    "version": "0.1.0"
                }
            }
        })";

        auto custom_context = c2pa::Context::ContextBuilder()
            .with_json(settings_json)
            .create_context();

        // Use this context for operations that need specific configuration
        char *manifest = load_file("tests/fixtures/training.json");
        auto builder = c2pa::Builder(custom_context, manifest);

        free(manifest);
    }
    catch (c2pa::C2paException &e)
    {
        cout << "FAILED: Custom settings example: " << e.what() << endl;
        return (1);
    }

    cout << "\n=== All examples completed successfully! ===" << endl;
    return 0;
}
