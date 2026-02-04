// Copyright 2024 Adobe. All rights reserved.
// This file is licensed to you under the Apache License,
// Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0)
// or the MIT license (http://opensource.org/licenses/MIT),
// at your option.
//
// Demonstration of the new context-based C2PA API

#include <c2pa.hpp>
#include <iostream>
#include <filesystem>

namespace fs = std::filesystem;

int main() {
    try {
        std::cout << "=== C2PA Context API Demo ===" << std::endl;
        std::cout << "C2PA Library Version: " << c2pa::version() << std::endl << std::endl;
        
        // 1. Create a default context
        std::cout << "1. Creating default context..." << std::endl;
        auto context = c2pa::Context::create();
        std::cout << "   Context created successfully!" << std::endl;
        std::cout << "   Has context: " << (context->has_context() ? "yes" : "no") << std::endl << std::endl;
        
        // 2. Create a context from JSON
        std::cout << "2. Creating context from JSON configuration..." << std::endl;
        auto json_context = c2pa::Context::from_json(R"({
            "verify": {
                "verify_after_reading": true
            }
        })");
        std::cout << "   JSON context created successfully!" << std::endl << std::endl;
        
        // 3. Create a context using Builder pattern
        std::cout << "3. Creating context using Builder..." << std::endl;
        auto builder_context = c2pa::Context::Builder()
            .with_json(R"({"verify": {"verify_after_sign": false}})")
            .build();
        std::cout << "   Builder context created successfully!" << std::endl << std::endl;
        
        // 4. Use context with Reader
        std::cout << "4. Using context with Reader..." << std::endl;
        fs::path test_file = fs::path(__FILE__).parent_path().parent_path() / "tests" / "fixtures" / "C.jpg";
        if (fs::exists(test_file)) {
            std::cout << "   Reading file: " << test_file << std::endl;
            c2pa::Reader reader(context, test_file);
            
            std::cout << "   Is embedded: " << (reader.is_embedded() ? "yes" : "no") << std::endl;
            
            // Get context from reader
            auto reader_context = reader.context();
            std::cout << "   Reader has context: " << (reader_context ? "yes" : "no") << std::endl;
            std::cout << "   Same context object: " << (reader_context == context ? "yes" : "no") << std::endl;
            
            // Get manifest
            auto manifest_json = reader.json();
            std::cout << "   Manifest size: " << manifest_json.size() << " bytes" << std::endl;
        } else {
            std::cout << "   Test file not found (expected for demo): " << test_file << std::endl;
        }
        std::cout << std::endl;
        
        // 5. Demonstrate thread-safe context sharing
        std::cout << "5. Context sharing demo..." << std::endl;
        auto shared_context = c2pa::Context::create();
        std::cout << "   Context can be safely shared via shared_ptr" << std::endl;
        std::cout << "   Use count: " << shared_context.use_count() << std::endl;
        
        // Make a copy
        auto shared_copy = shared_context;
        std::cout << "   After copying, use count: " << shared_context.use_count() << std::endl;
        std::cout << std::endl;
        
        // 6. Settings example
        std::cout << "6. Settings configuration demo..." << std::endl;
        c2pa::Settings settings;
        settings.set("verify.verify_after_sign", "true")
                .update(R"({"verify": {"verify_after_reading": false}})", "json");
        std::cout << "   Settings configured successfully!" << std::endl;
        
        auto settings_context = c2pa::Context::Builder()
            .with_settings(settings)
            .build();
        std::cout << "   Context created from settings!" << std::endl;
        std::cout << std::endl;
        
        std::cout << "=== All context API demos completed successfully! ===" << std::endl;
        return 0;
        
    } catch (const c2pa::C2paException& e) {
        std::cerr << "C2PA Error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
