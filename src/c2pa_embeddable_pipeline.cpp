// Copyright 2026 Adobe. All rights reserved.
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

/// @file   c2pa_embeddable_pipeline.cpp
/// @brief  EmbeddablePipeline class implementation.

#include <filesystem>
#include <fstream>
#include <sstream>

#include "c2pa.hpp"

namespace c2pa {
    const char* EmbeddablePipeline::state_name(State s) noexcept {
        switch (s) {
            case State::init: return "init";
            case State::placeholder_created: return "placeholder_created";
            case State::exclusions_configured: return "exclusions_configured";
            case State::hashed: return "hashed";
            case State::pipeline_signed: return "pipeline_signed";
        }
        return "unknown";
    }

    [[noreturn]] void EmbeddablePipeline::throw_wrong_state(
            const char* method, const std::string& expected) const {
        std::ostringstream msg;
        msg << method << " requires state " << expected
            << " but current state is '" << state_name(state_) << "'";
        throw C2paException(msg.str());
    }

    void EmbeddablePipeline::require_state(State expected, const char* method) const {
        if (state_ != expected) {
            std::ostringstream expected_str;
            expected_str << "'" << state_name(expected) << "'";
            throw_wrong_state(method, expected_str.str());
        }
    }

    void EmbeddablePipeline::require_state_at_least(State minimum, const char* method) const {
        if (state_ < minimum) {
            std::ostringstream expected;
            expected << "'" << state_name(minimum) << "' or later";
            throw_wrong_state(method, expected.str());
        }
    }

    void EmbeddablePipeline::require_state_in(
            std::initializer_list<State> allowed, const char* method) const {
        for (auto s : allowed) {
            if (state_ == s) return;
        }
        std::ostringstream expected;
        expected << "one of {";
        for (auto it = allowed.begin(); it != allowed.end(); ++it) {
            if (it != allowed.begin()) expected << ", ";
            expected << state_name(*it);
        }
        expected << "}";
        throw_wrong_state(method, expected.str());
    }

    void EmbeddablePipeline::require_not_faulted(const char* method) const {
        if (faulted_) {
            std::ostringstream msg;
            msg << method
                << " cannot be called: pipeline faulted during a prior operation"
                   " (create a new pipeline to retry)";
            throw C2paException(msg.str());
        }
    }

    EmbeddablePipeline::EmbeddablePipeline(Builder&& b, std::string format)
        : builder_(std::move(b))
        , format_(std::move(format))
    {
    }

    // Init-state forwarding

    void EmbeddablePipeline::add_ingredient(const std::string& json, const std::string& fmt, std::istream& source) {
        require_not_faulted("add_ingredient()");
        require_state(State::init, "add_ingredient()");
        try {
            builder_.add_ingredient(json, fmt, source);
        } catch (...) {
            faulted_ = true;
            throw;
        }
    }

    void EmbeddablePipeline::add_ingredient(const std::string& json, const std::filesystem::path& path) {
        require_not_faulted("add_ingredient()");
        require_state(State::init, "add_ingredient()");
        try {
            builder_.add_ingredient(json, path);
        } catch (...) {
            faulted_ = true;
            throw;
        }
    }

    void EmbeddablePipeline::add_resource(const std::string& uri, std::istream& source) {
        require_not_faulted("add_resource()");
        require_state(State::init, "add_resource()");
        try {
            builder_.add_resource(uri, source);
        } catch (...) {
            faulted_ = true;
            throw;
        }
    }

    void EmbeddablePipeline::add_resource(const std::string& uri, const std::filesystem::path& path) {
        require_not_faulted("add_resource()");
        require_state(State::init, "add_resource()");
        try {
            builder_.add_resource(uri, path);
        } catch (...) {
            faulted_ = true;
            throw;
        }
    }

    void EmbeddablePipeline::add_action(const std::string& json) {
        require_not_faulted("add_action()");
        require_state(State::init, "add_action()");
        try {
            builder_.add_action(json);
        } catch (...) {
            faulted_ = true;
            throw;
        }
    }

    EmbeddablePipeline& EmbeddablePipeline::with_definition(const std::string& json) {
        require_not_faulted("with_definition()");
        require_state(State::init, "with_definition()");
        try {
            builder_.with_definition(json);
        } catch (...) {
            faulted_ = true;
            throw;
        }
        return *this;
    }

    void EmbeddablePipeline::to_archive(std::ostream& dest) {
        require_not_faulted("to_archive()");
        require_state(State::init, "to_archive()");
        try {
            builder_.to_archive(dest);
        } catch (...) {
            faulted_ = true;
            throw;
        }
    }

    void EmbeddablePipeline::to_archive(const std::filesystem::path& path) {
        require_not_faulted("to_archive()");
        require_state(State::init, "to_archive()");
        try {
            builder_.to_archive(path);
        } catch (...) {
            faulted_ = true;
            throw;
        }
    }

    // Workflow transitions

    bool EmbeddablePipeline::needs_placeholder() {
        require_not_faulted("needs_placeholder()");
        try {
            return builder_.needs_placeholder(format_);
        } catch (...) {
            faulted_ = true;
            throw;
        }
    }

    const std::vector<unsigned char>& EmbeddablePipeline::create_placeholder() {
        require_not_faulted("create_placeholder()");
        require_state(State::init, "create_placeholder()");
        try {
            placeholder_ = builder_.placeholder(format_);
        } catch (...) {
            faulted_ = true;
            throw;
        }
        state_ = State::placeholder_created;
        return placeholder_;
    }

    void EmbeddablePipeline::set_data_hash_exclusions(
            const std::vector<std::pair<uint64_t, uint64_t>>& exclusions) {
        require_not_faulted("set_data_hash_exclusions()");
        require_state(State::placeholder_created, "set_data_hash_exclusions()");
        try {
            builder_.set_data_hash_exclusions(exclusions);
        } catch (...) {
            faulted_ = true;
            throw;
        }
        exclusions_ = exclusions;
        state_ = State::exclusions_configured;
    }

    void EmbeddablePipeline::hash_from_stream(std::istream& stream) {
        require_not_faulted("hash_from_stream()");
        require_state_in(
            {State::init, State::placeholder_created, State::exclusions_configured},
            "hash_from_stream()");
        if (state_ == State::init && needs_placeholder()) {
            throw C2paException(
                "hash_from_stream() cannot be called in 'init' state for this format "
                "because it requires a placeholder.");
        }
        try {
            builder_.update_hash_from_stream(format_, stream);
        } catch (...) {
            faulted_ = true;
            throw;
        }
        state_ = State::hashed;
    }

    const std::vector<unsigned char>& EmbeddablePipeline::sign() {
        require_not_faulted("sign()");
        require_state(State::hashed, "sign()");
        try {
            signed_manifest_ = builder_.sign_embeddable(format_);
        } catch (...) {
            faulted_ = true;
            throw;
        }
        state_ = State::pipeline_signed;
        return signed_manifest_;
    }

    // Accessors

    const std::vector<unsigned char>& EmbeddablePipeline::placeholder_bytes() const {
        require_state_at_least(State::placeholder_created, "placeholder_bytes()");
        if (placeholder_.empty()) {
            throw C2paException("placeholder_bytes() is not available because no placeholder was created on this pipeline path");
        }
        return placeholder_;
    }

    const std::vector<std::pair<uint64_t, uint64_t>>& EmbeddablePipeline::data_hash_exclusions() const {
        require_state_at_least(State::exclusions_configured, "data_hash_exclusions()");
        if (exclusions_.empty()) {
            throw C2paException("data_hash_exclusions() is not available because no exclusions were set on this pipeline path");
        }
        return exclusions_;
    }

    const std::vector<unsigned char>& EmbeddablePipeline::signed_bytes() const {
        require_state(State::pipeline_signed, "signed_bytes()");
        return signed_manifest_;
    }

    // Utilities

    const std::string& EmbeddablePipeline::format() const noexcept {
        return format_;
    }

    const char* EmbeddablePipeline::current_state() const noexcept {
        return state_name(state_);
    }

    bool EmbeddablePipeline::is_faulted() const noexcept {
        return faulted_;
    }

    Builder EmbeddablePipeline::into_builder() && {
        require_not_faulted("into_builder()");
        require_state(State::init, "into_builder()");
        return std::move(builder_);
    }

} // namespace c2pa
