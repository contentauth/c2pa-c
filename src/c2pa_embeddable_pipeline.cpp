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
/// @brief  EmbeddablePipeline and derived pipeline implementations.

#include <memory>
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

    EmbeddablePipeline::EmbeddablePipeline(Builder&& builder, std::string format)
        : builder_(std::move(builder))
        , format_(std::move(format))
    {
    }

    // Workflow methods

    void EmbeddablePipeline::do_hash(std::istream& stream) {
        require_not_faulted("hash_from_stream()");
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

    const std::vector<unsigned char>& EmbeddablePipeline::signed_bytes() const {
        require_state(State::pipeline_signed, "signed_bytes()");
        return signed_manifest_;
    }

    const std::string& EmbeddablePipeline::format() const noexcept {
        return format_;
    }

    const char* EmbeddablePipeline::current_state() const noexcept {
        return state_name(state_);
    }

    bool EmbeddablePipeline::is_faulted() const noexcept {
        return faulted_;
    }

    // Base class default implementations (throw for unsupported hash types)

    const std::vector<unsigned char>& EmbeddablePipeline::create_placeholder() {
        throw C2paException("create_placeholder() is not supported for this hash type");
    }

    void EmbeddablePipeline::set_exclusions(const std::vector<std::pair<uint64_t, uint64_t>>&) {
        throw C2paException("set_exclusions() is not supported for this hash type");
    }

    const std::vector<unsigned char>& EmbeddablePipeline::placeholder_bytes() const {
        throw C2paException("placeholder_bytes() is not supported for this hash type");
    }

    const std::vector<std::pair<uint64_t, uint64_t>>& EmbeddablePipeline::exclusion_ranges() const {
        throw C2paException("exclusion_ranges() is not supported for this hash type");
    }

    // Specialized class: DataHashPipeline

    DataHashPipeline::DataHashPipeline(Builder&& builder, std::string format)
        : EmbeddablePipeline(std::move(builder), std::move(format))
    {
    }

    HashType DataHashPipeline::hash_type() const { return HashType::Data; }

    const std::vector<unsigned char>& DataHashPipeline::create_placeholder() {
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

    void DataHashPipeline::set_exclusions(
            const std::vector<std::pair<uint64_t, uint64_t>>& exclusions) {
        require_not_faulted("set_exclusions()");
        require_state(State::placeholder_created, "set_exclusions()");
        try {
            builder_.set_data_hash_exclusions(exclusions);
        } catch (...) {
            faulted_ = true;
            throw;
        }
        exclusions_ = exclusions;
        state_ = State::exclusions_configured;
    }

    void DataHashPipeline::hash_from_stream(std::istream& stream) {
        require_not_faulted("hash_from_stream()");
        require_state(State::exclusions_configured, "hash_from_stream()");
        do_hash(stream);
    }

    const std::vector<unsigned char>& DataHashPipeline::placeholder_bytes() const {
        require_state_at_least(State::placeholder_created, "placeholder_bytes()");
        return placeholder_;
    }

    const std::vector<std::pair<uint64_t, uint64_t>>& DataHashPipeline::exclusion_ranges() const {
        require_state_at_least(State::exclusions_configured, "exclusion_ranges()");
        return exclusions_;
    }

    // Specialized class: BmffHashPipeline

    BmffHashPipeline::BmffHashPipeline(Builder&& builder, std::string format)
        : EmbeddablePipeline(std::move(builder), std::move(format))
    {
    }

    HashType BmffHashPipeline::hash_type() const { return HashType::Bmff; }

    const std::vector<unsigned char>& BmffHashPipeline::create_placeholder() {
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

    void BmffHashPipeline::hash_from_stream(std::istream& stream) {
        require_not_faulted("hash_from_stream()");
        require_state(State::placeholder_created, "hash_from_stream()");
        do_hash(stream);
    }

    const std::vector<unsigned char>& BmffHashPipeline::placeholder_bytes() const {
        require_state_at_least(State::placeholder_created, "placeholder_bytes()");
        return placeholder_;
    }

    // Specialized class: BoxHashPipeline

    BoxHashPipeline::BoxHashPipeline(Builder&& builder, std::string format)
        : EmbeddablePipeline(std::move(builder), std::move(format))
    {
    }

    HashType BoxHashPipeline::hash_type() const { return HashType::Box; }

    void BoxHashPipeline::hash_from_stream(std::istream& stream) {
        require_not_faulted("hash_from_stream()");
        require_state(State::init, "hash_from_stream()");
        do_hash(stream);
    }

    // Factory

    std::unique_ptr<EmbeddablePipeline> EmbeddablePipeline::create(
            Builder&& builder, const std::string& format) {
        // Query hash type before moving builder
        HashType ht = builder.hash_type(format);
        switch (ht) {
            case HashType::Data:
                return std::make_unique<DataHashPipeline>(std::move(builder), format);
            case HashType::Bmff:
                return std::make_unique<BmffHashPipeline>(std::move(builder), format);
            case HashType::Box:
                return std::make_unique<BoxHashPipeline>(std::move(builder), format);
        }
        throw C2paException("Unknown hash type");
    }

} // namespace c2pa
