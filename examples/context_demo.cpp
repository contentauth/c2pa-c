// Copyright 2024 Adobe. All rights reserved.
// This file is licensed to you under the Apache License,
// Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0)
// or the MIT license (http://opensource.org/licenses/MIT),
// at your option.
//
// Demonstration of the context-based C2PA API

#include <c2pa.hpp>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

int main() {
    try {
        std::cout << "C2PA Library Version: " << c2pa::version() << std::endl << std::endl;

        // 1. Example with a default context
        std::cout << "Creating default context..." << std::endl;
        auto context = c2pa::Context::create();
        std::cout << "   Context created successfully!" << std::endl;
        std::cout << "   Has context: " << (context->has_context() ? "yes" : "no") << std::endl << std::endl;

        // 2. Context from JSON, useful for settings direct parsing for instance
        std::cout << "Creating context from JSON configuration..." << std::endl;
        auto json_context = c2pa::Context::from_json(R"({
            "verify": {
                "verify_after_reading": true
            }
        })");
        std::cout << "   JSON context created successfully!" << std::endl << std::endl;

        // 3. Using a context builder to create the context
        std::cout << "Creating context using Builder..." << std::endl;
        auto dynamic_context = c2pa::Context::Builder()
            .with_json(R"({"verify": {"verify_after_sign": false}})")
            .build();
        std::cout << "   Dynamic context created successfully!" << std::endl << std::endl;

        // 4. Use context with the Reader API
        // THis propagates settings in the context to the Reader
        std::cout << "Using context with Reader..." << std::endl;
        fs::path test_file = fs::path(__FILE__).parent_path().parent_path() / "tests" / "fixtures" / "C.jpg";
        if (fs::exists(test_file)) {
            std::cout << "   Reading file: " << test_file << std::endl;
            c2pa::Reader reader(context, test_file);

            std::cout << "   Is embedded: " << (reader.is_embedded() ? "yes" : "no") << std::endl;

            // Get context from reader
            auto reader_context = reader.context();
            std::cout << "   Reader has context: " << (reader_context ? "yes" : "no") << std::endl;
            std::cout << "   Same context object: " << (reader_context == context ? "yes" : "no") << std::endl;

            // Get manifest, with the Reader using contextual settings (eg. for trust configurations)
            auto manifest_json = reader.json();
            std::cout << "   Manifest size: " << manifest_json.size() << " bytes" << std::endl;
        } else {
            std::cout << "   Test file not found: " << test_file << std::endl;
        }
        std::cout << std::endl;

        // Contexts are especially powerful to manage SDK settings
        std::cout << "Settings configuration through context..." << std::endl;
        c2pa::Settings settings;
        settings.set("verify.verify_after_sign", "true")
                .update(R"({"verify": {"verify_after_reading": false}})", "json");
        std::cout << "   Settings configured successfully!" << std::endl;

        auto settings_context = c2pa::Context::Builder()
            .with_settings(settings)
            .build();
        std::cout << "   Context created from settings (settings will propagate throught the context, not globally)" << std::endl;
        std::cout << std::endl;

        return 0;

    } catch (const c2pa::C2paException& e) {
        std::cerr << "C2PA Error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
