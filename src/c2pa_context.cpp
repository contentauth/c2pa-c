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

/// @file   c2pa_context.cpp
/// @brief  Context and ContextBuilder implementation.

#include <utility>
#include <fstream>

#include "c2pa.hpp"
#include "c2pa_internal.hpp"

namespace c2pa
{
    Context::Context(C2paContext* ctx) : context(ctx) {
        if (!context) {
            throw C2paException("Invalid context pointer");
        }
    }

    Context::Context() : context(c2pa_context_new()) {
        if (!context) {
            throw C2paException("Failed to create Context");
        }
    }

    Context::Context(const Settings& settings)
        : Context(ContextBuilder().with_settings(settings).create_context()) {
    }

    Context::Context(Context&& other) noexcept
        : context(std::exchange(other.context, nullptr)) {
    }

    Context& Context::operator=(Context&& other) noexcept {
        if (this != &other) {
            c2pa_free(context);
            context = std::exchange(other.context, nullptr);
        }
        return *this;
    }

    Context::~Context() noexcept {
        c2pa_free(context);
    }

    C2paContext* Context::c_context() const noexcept {
        return context;
    }

    bool Context::is_valid() const noexcept {
        return context != nullptr;
    }

    Context::Context(const std::string& json) : Context(Settings(json, "json")) {
    }

    Context::Context(const Settings& settings, Signer&& signer)
        : Context(ContextBuilder()
            .with_settings(settings)
            .with_signer(std::move(signer))
            .create_context()) {
    }

    // Context::ContextBuilder

    Context::ContextBuilder::ContextBuilder() : context_builder(c2pa_context_builder_new()) {
        if (!context_builder) {
            throw C2paException("Failed to create Context builder");
        }
    }

    Context::ContextBuilder::ContextBuilder(ContextBuilder&& other) noexcept
        : context_builder(std::exchange(other.context_builder, nullptr)) {
    }

    Context::ContextBuilder& Context::ContextBuilder::operator=(ContextBuilder&& other) noexcept {
        if (this != &other) {
            c2pa_free(context_builder);
            context_builder = std::exchange(other.context_builder, nullptr);
        }
        return *this;
    }

    Context::ContextBuilder::~ContextBuilder() noexcept {
        c2pa_free(context_builder);
    }

    bool Context::ContextBuilder::is_valid() const noexcept {
        return context_builder != nullptr;
    }

    Context::ContextBuilder& Context::ContextBuilder::with_settings(const Settings& settings) {
        if (!is_valid()) {
            throw C2paException("ContextBuilder is invalid (moved from)");
        }
        if (!settings.is_valid()) {
            throw C2paException("Settings object is invalid");
        }
        if (c2pa_context_builder_set_settings(context_builder, settings.c_settings()) != 0) {
            throw C2paException();
        }
        return *this;
    }

    Context::ContextBuilder& Context::ContextBuilder::with_json(const std::string& json) {
        if (!is_valid()) {
            throw C2paException("ContextBuilder is invalid (moved from)");
        }
        return with_settings(Settings(json, "json"));
    }

    Context::ContextBuilder& Context::ContextBuilder::with_json_settings_file(const std::filesystem::path& settings_path) {
        if (!is_valid()) {
            throw C2paException("ContextBuilder is invalid (moved from)");
        }

        auto file = detail::open_file_binary<std::ifstream>(settings_path);

        file->seekg(0, std::ios::end);
        auto file_size = file->tellg();
        file->seekg(0, std::ios::beg);
        if (file_size < 0) {
            throw C2paException("Settings file is not readable");
        }
        constexpr std::streamoff max_settings_size = 1024 * 1024; // 1 MB max, similar to c2pa-rs
        if (file_size > max_settings_size) {
            throw C2paException("Settings file is too large (>1MB)");
        }

        std::string json_content((std::istreambuf_iterator<char>(*file)), std::istreambuf_iterator<char>());

        return with_json(json_content);
    }

    Context::ContextBuilder& Context::ContextBuilder::with_signer(Signer&& signer) {
        if (!is_valid()) {
            throw C2paException("ContextBuilder is invalid (moved from)");
        }
        C2paSigner* raw = signer.release();
        if (!raw) {
            throw C2paException("Signer is not valid");
        }
        c2pa_context_builder_set_signer(context_builder, raw);
        return *this;
    }

    C2paContextBuilder* Context::ContextBuilder::c2pa_context_builder() const noexcept {
        return context_builder;
    }

    C2paContextBuilder* Context::ContextBuilder::release() noexcept {
        return std::exchange(context_builder, nullptr);
    }

    Context Context::ContextBuilder::create_context() {
        if (!is_valid()) {
            throw C2paException("ContextBuilder is invalid (moved from)");
        }

        // The C API consumes the context builder on build
        C2paContext* ctx = c2pa_context_builder_build(context_builder);
        if (!ctx) {
            throw C2paException("Failed to build context");
        }

        // Builder is consumed by the C API
        context_builder = nullptr;

        return Context(ctx);
    }
} // namespace c2pa
