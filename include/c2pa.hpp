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

/// @file   c2pa.hpp
/// @brief  C++ wrapper for the C2PA C library.
/// @details This is used for creating and verifying C2PA manifests.
///          This is an early version, and has not been fully tested.
///          Thread safety is not guaranteed due to the use of errno and etc.

#ifndef C2PA_H
#define C2PA_H

// Suppress unused function warning for GCC/Clang
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

// Suppress unused function warning for MSVC
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4505)
#endif

#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>
#include <optional>
#include <memory>

#include <c2pa.h>

// NOOP for now, can use later to define static library
#define C2PA_CPP_API

namespace c2pa
{
    using namespace std;

    typedef C2paSignerInfo SignerInfo;

    // Forward declarations for context types
    class Settings;
    class Context;
    class IContextProvider;

    /// @brief Shared pointer to context provider for polymorphic usage.
    using ContextProviderPtr = std::shared_ptr<IContextProvider>;

    /// C2paException class for C2pa errors.
    /// This class is used to throw exceptions for errors encountered by the C2pa library via c2pa_error().
    class C2PA_CPP_API C2paException : public exception
    {
    private:
        string message;

    public:
        C2paException();

        C2paException(string what);

        virtual const char *what() const noexcept;
    };

    /// @brief Interface for types that can provide C2PA context functionality.
    /// @details Implement this interface to make your own context
    class C2PA_CPP_API IContextProvider {
    public:
        virtual ~IContextProvider() = default;
        
        /// @brief Get the underlying C2PA context pointer for FFI operations.
        /// @return Pointer to C2paContext, or nullptr if not available.
        /// @note Provider retains ownership; pointer valid for provider's lifetime.
        [[nodiscard]] virtual C2paContext* c_context() const = 0;
        
        /// @brief Check if this provider has a valid context.
        /// @return true if context is available, false otherwise.
        [[nodiscard]] virtual bool has_context() const noexcept = 0;
        
    protected:
        IContextProvider() = default;
        IContextProvider(const IContextProvider&) = default;
        IContextProvider& operator=(const IContextProvider&) = default;
        IContextProvider(IContextProvider&&) = default;
        IContextProvider& operator=(IContextProvider&&) = default;
    };

    /// @brief Mutable settings configuration object for building contexts.
    /// @details Settings can be configured via JSON/TOML strings or programmatically
    ///          via set() and update() methods. Once passed to Context::Builder,
    ///          the settings are copied and the Settings object can be reused or discarded.
    class C2PA_CPP_API Settings {
    public:
        /// @brief Create default settings.
        Settings();
        
        /// @brief Create settings from a configuration string.
        /// @param data Configuration data in JSON or TOML format.
        /// @param format Format of the data ("json" or "toml").
        /// @throws C2paException if parsing fails.
        Settings(const string& data, const string& format);
        
        // Move semantics
        Settings(Settings&&) noexcept;
        Settings& operator=(Settings&&) noexcept;
        
        // Non-copyable
        Settings(const Settings&) = delete;
        Settings& operator=(const Settings&) = delete;
        
        ~Settings();
        
        /// @brief Set a single configuration value by path.
        /// @param path Dot-separated path to the setting (e.g., "verify.verify_after_sign").
        /// @param json_value JSON-encoded value to set.
        /// @return Reference to this Settings for method chaining.
        /// @throws C2paException if the path or value is invalid.
        Settings& set(const string& path, const string& json_value);
        
        /// @brief Merge configuration from a string.
        /// @param data Configuration data in JSON or TOML format.
        /// @param format Format of the data ("json" or "toml").
        /// @return Reference to this Settings for method chaining.
        /// @throws C2paException if parsing fails.
        Settings& update(const string& data, const string& format);
        
        /// @brief Get the raw C FFI settings pointer.
        /// @return Pointer to C2paSettings, or nullptr if not initialized.
        [[nodiscard]] C2paSettings* c_settings() const noexcept;
        
    private:
        C2paSettings* settings_;
    };

    /// @brief Immutable C2PA context implementing IContextProvider.
    /// @details Context objects are immutable after construction and can be safely
    ///          shared across threads via shared_ptr. Create contexts using static
    ///          factory methods or the Builder pattern.
    class C2PA_CPP_API Context : public IContextProvider {
    public:
        /// @brief ContextBuilder for creating customized Context instances.
        class C2PA_CPP_API ContextBuilder {
        public:
            ContextBuilder();
            ~ContextBuilder();
            
            // Move semantics
            ContextBuilder(ContextBuilder&&) noexcept;
            ContextBuilder& operator=(ContextBuilder&&) noexcept;
            
            // Non-copyable
            ContextBuilder(const ContextBuilder&) = delete;
            ContextBuilder& operator=(const ContextBuilder&) = delete;
            
            /// @brief Configure with Settings object.
            /// @param settings Settings to use (will be copied).
            /// @return Reference to this ContextBuilder for method chaining.
            ContextBuilder& with_settings(const Settings& settings);
            
            /// @brief Configure with JSON string.
            /// @param json JSON configuration string.
            /// @return Reference to this ContextBuilder for method chaining.
            /// @throws C2paException if JSON is invalid.
            ContextBuilder& with_json(const string& json);
            
            /// @brief Configure with TOML string.
            /// @param toml TOML configuration string.
            /// @return Reference to this ContextBuilder for method chaining.
            /// @throws C2paException if TOML is invalid.
            ContextBuilder& with_toml(const string& toml);
            
            /// @brief Build the immutable Context.
            /// @return Shared pointer to the new Context.
            /// @throws C2paException if context creation fails.
            /// @note This consumes the builder. After calling create_context(), the builder is in a moved-from state.
            [[nodiscard]] ContextProviderPtr create_context();
            
        private:
            C2paContextBuilder* context_builder_;
        };

        /// @brief Create a Context with default settings.
        /// @return Shared pointer to the new Context.
        /// @throws C2paException if context creation fails.
        [[nodiscard]] static ContextProviderPtr create();
        
        /// @brief Create a Context from JSON configuration.
        /// @param json JSON configuration string.
        /// @return Shared pointer to the new Context.
        /// @throws C2paException if JSON is invalid or context creation fails.
        [[nodiscard]] static ContextProviderPtr from_json(const string& json);
        
        /// @brief Create a Context from TOML configuration.
        /// @param toml TOML configuration string.
        /// @return Shared pointer to the new Context.
        /// @throws C2paException if TOML is invalid or context creation fails.
        [[nodiscard]] static ContextProviderPtr from_toml(const string& toml);
        
        // Non-copyable, non-moveable (managed via shared_ptr)
        Context(const Context&) = delete;
        Context& operator=(const Context&) = delete;
        Context(Context&&) = delete;
        Context& operator=(Context&&) = delete;
        
        ~Context() override;
        
        // IContextProvider implementation
        [[nodiscard]] C2paContext* c_context() const override;
        [[nodiscard]] bool has_context() const noexcept override;
        
        /// @brief Internal constructor (use static factory methods instead).
        /// @note This is public to allow std::make_shared but should not be called directly.
        explicit Context(C2paContext* ctx);
        
    private:
        C2paContext* context_;
        
        friend class Builder;
    };

    /// Returns the version of the C2pa library.
    string C2PA_CPP_API version();

    /// Loads C2PA settings from a string in a given format.
    /// @param data the string to load.
    /// @param format the mime format of the string.
    /// @throws a C2pa::C2paException for errors encountered by the C2PA library.
    /// @deprecated Use Context::from_json() or Context::from_toml() instead for better thread safety.
    [[deprecated("Use Context::from_json() or Context::from_toml() instead")]]
    void C2PA_CPP_API load_settings(const string& data, const string& format);

    /// Reads a file and returns the manifest json as a C2pa::String.
    /// @param source_path the path to the file to read.
    /// @param data_dir the directory to store binary resources (optional).
    /// @return a string containing the manifest json if a manifest was found.
    /// @throws a C2pa::C2paException for errors encountered by the C2PA library.
    optional<string> C2PA_CPP_API read_file(const std::filesystem::path &source_path, const optional<std::filesystem::path> data_dir = nullopt);

    /// Reads a file and returns an ingredient JSON as a C2pa::String.
    /// @param source_path the path to the file to read.
    /// @param data_dir the directory to store binary resources.
    /// @return a string containing the ingredient json.
    /// @throws a C2pa::C2paException for errors encountered by the C2PA library.
    std::string C2PA_CPP_API read_ingredient_file(const std::filesystem::path &source_path, const std::filesystem::path &data_dir);

    /// Adds the manifest and signs a file.
    /// @param source_path the path to the asset to be signed.
    /// @param dest_path the path to write the signed file to.
    /// @param manifest the manifest json to add to the file.
    /// @param signer_info the signer info to use for signing.
    /// @param data_dir the directory to store binary resources (optional).
    /// @throws a C2pa::C2paException for errors encountered by the C2PA library.
    void C2PA_CPP_API sign_file(const std::filesystem::path &source_path,
                            const std::filesystem::path &dest_path,
                            const char *manifest,
                            SignerInfo *signer_info,
                            const std::optional<std::filesystem::path> data_dir = std::nullopt);

    /// @brief Istream Class wrapper for C2paStream.
    /// @details This class is used to wrap an input stream for use with the C2PA library.
    class C2PA_CPP_API CppIStream : public C2paStream
    {
    public:
        C2paStream *c_stream;
        template <typename IStream>
        explicit CppIStream(IStream &istream) {
            static_assert(std::is_base_of<std::istream, IStream>::value,
                      "Stream must be derived from std::istream");
            c_stream = c2pa_create_stream(reinterpret_cast<StreamContext *>(&istream), reader, seeker, writer, flusher);	 	 
        }

        CppIStream(const CppIStream &) = delete;
        CppIStream &operator=(const CppIStream &) = delete;
        CppIStream(CppIStream &&) = delete;
        CppIStream &operator=(CppIStream &&) = delete;

        ~CppIStream();

    private:
        static intptr_t reader(StreamContext *context, uint8_t *buffer, intptr_t size);
        static intptr_t writer(StreamContext *context, const uint8_t *buffer, intptr_t size);
        static intptr_t seeker(StreamContext *context, intptr_t offset, C2paSeekMode whence);
        static intptr_t flusher(StreamContext *context);

        friend class Reader;
    };

    /// @brief Ostream Class wrapper for C2paStream.
    /// @details This class is used to wrap an output stream for use with the C2PA library.
    class C2PA_CPP_API CppOStream : public C2paStream
    {
    public:
        C2paStream *c_stream;
        template <typename OStream>
        explicit CppOStream(OStream &ostream) {
            static_assert(std::is_base_of<std::ostream, OStream>::value, "Stream must be derived from std::ostream");
            c_stream = c2pa_create_stream(reinterpret_cast<StreamContext *>(&ostream), reader, seeker, writer, flusher);
        }

        CppOStream(const CppOStream &) = delete;
        CppOStream &operator=(const CppOStream &) = delete;
        CppOStream(CppOStream &&) = delete;
        CppOStream &operator=(CppOStream &&) = delete;

        ~CppOStream();

    private:
        static intptr_t reader(StreamContext *context, uint8_t *buffer, intptr_t size);
        static intptr_t writer(StreamContext *context, const uint8_t *buffer, intptr_t size);
        static intptr_t seeker(StreamContext *context, intptr_t offset, C2paSeekMode whence);
        static intptr_t flusher(StreamContext *context);
    };

    /// @brief IOStream Class wrapper for C2paStream.
    /// @details This class is used to wrap an input/output stream for use with the C2PA library.
    class C2PA_CPP_API CppIOStream : public C2paStream
    {
    public:
        C2paStream *c_stream;
        template <typename IOStream>
        CppIOStream(IOStream &iostream) {
            static_assert(std::is_base_of<std::iostream, IOStream>::value, "Stream must be derived from std::iostream");
            c_stream = c2pa_create_stream(reinterpret_cast<StreamContext *>(&iostream), reader, seeker, writer, flusher);
        }

        CppIOStream(const CppIOStream &) = delete;
        CppIOStream &operator=(const CppIOStream &) = delete;
        CppIOStream(CppIOStream &&) = delete;
        CppIOStream &operator=(CppIOStream &&) = delete;

        ~CppIOStream();

    private:
        static intptr_t reader(StreamContext *context, uint8_t *buffer, intptr_t size);
        static intptr_t writer(StreamContext *context, const uint8_t *buffer, intptr_t size);
        static intptr_t seeker(StreamContext *context, intptr_t offset, C2paSeekMode whence);
        static intptr_t flusher(StreamContext *context);
    };

    /// @brief Reader class for reading a manifest.
    /// @details This class is used to read and validate a manifest from a stream or file.
    class C2PA_CPP_API Reader
    {
    private:
        C2paReader *c2pa_reader;
        CppIStream *cpp_stream = nullptr;
        ContextProviderPtr context_;  // Keeps context alive

    public:
        // ===== Context-based constructors (NEW, RECOMMENDED) =====
        
        /// @brief Create a Reader from a context and stream.
        /// @param context Context provider to use for this reader.
        /// @param format The mime format of the stream.
        /// @param stream The input stream to read from.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        Reader(ContextProviderPtr context, const std::string &format, std::istream &stream);
        
        /// @brief Create a Reader from a context and file path.
        /// @param context Context provider to use for this reader.
        /// @param source_path The path to the file to read.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        Reader(ContextProviderPtr context, const std::filesystem::path &source_path);
        
        // ===== Legacy constructors (DEPRECATED) =====
        
        /// @brief Create a Reader from a stream (uses global settings).
        /// @details The validation_status field in the json contains validation results.
        /// @param format The mime format of the stream.
        /// @param stream The input stream to read from.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        /// @deprecated Use Reader(ContextProviderPtr, format, stream) instead.
        [[deprecated("Use Reader(ContextProviderPtr, format, stream) for better thread safety")]]
        Reader(const std::string &format, std::istream &stream);

        /// @brief Create a Reader from a file path (uses global settings).
        /// @param source_path The path to the file to read.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        /// @deprecated Use Reader(ContextProviderPtr, source_path) instead.
        [[deprecated("Use Reader(ContextProviderPtr, source_path) for better thread safety")]]
        Reader(const std::filesystem::path &source_path);

        // Non-copyable
        Reader(const Reader&) = delete;
        Reader& operator=(const Reader&) = delete;

        // Move semantics
        Reader(Reader&& other) noexcept
            : c2pa_reader(other.c2pa_reader), cpp_stream(other.cpp_stream), context_(std::move(other.context_)) {
            other.c2pa_reader = nullptr;
            other.cpp_stream = nullptr;
        }
        Reader& operator=(Reader&& other) noexcept {
            if (this != &other) {
                c2pa_reader_free(c2pa_reader);
                delete cpp_stream;
                c2pa_reader = other.c2pa_reader;
                cpp_stream = other.cpp_stream;
                context_ = std::move(other.context_);
                other.c2pa_reader = nullptr;
                other.cpp_stream = nullptr;
            }
            return *this;
        }

        ~Reader();
        
        /// @brief Get the context associated with this Reader.
        /// @return Shared pointer to the context, or nullptr if using legacy API.
        [[nodiscard]] inline ContextProviderPtr context() const noexcept {
            return context_;
        }

        /// @brief Returns if the reader was created from an embedded manifest.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        [[nodiscard]] inline bool is_embedded() const {
            return c2pa_reader_is_embedded(c2pa_reader);
        }

        /// @brief Returns the remote url of the manifest if this `Reader`
        ///        obtained the manifest remotely.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        [[nodiscard]] std::optional<std::string> remote_url() const;

        /// @brief Get the manifest as a json string.
        /// @return The manifest as a json string.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        string json() const;

        /// @brief  Get a resource from the reader and write it to a file.
        /// @param uri The uri of the resource.
        /// @param path The path to write the resource to.
        /// @return The number of bytes written.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        int64_t get_resource(const string &uri, const std::filesystem::path &path);

        /// @brief  Get a resource from the reader  and write it to an output stream.
        /// @param uri The uri of the resource.
        /// @param stream The output stream to write the resource to.
        /// @return The number of bytes written.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        int64_t get_resource(const string &uri, std::ostream &stream);

        /// @brief Get the raw C2paReader pointer.
        /// @return The raw C2paReader pointer.
        /// @note This is intended for internal API use and compatibility with C APIs.
        C2paReader* get_api_internal_raw_reader() const { return c2pa_reader; }

        /// @brief Returns a vector of mime types that the SDK is able to
        /// read manifests from.
        static std::vector<std::string> supported_mime_types();
    };

    /// @brief  Signer Callback function type.
    /// @param  data the data to sign.
    /// @return the signature as a vector of bytes.
    /// @details This function type is used to create a callback function for signing.
    using SignerFunc = std::vector<unsigned char>(const std::vector<unsigned char> &);

    /// @brief  Signer class for creating a signer.
    /// @details This class is used to create a signer from a signing algorithm, certificate, and TSA URI.
    class C2PA_CPP_API Signer
    {
    private:
        C2paSigner *signer;

    public:
        /// @brief Create a Signer from a callback function, signing algorithm, certificate, and TSA URI.
        /// @param callback  the callback function to use for signing.
        /// @param alg  The signing algorithm to use.
        /// @param sign_cert The certificate to use for signing.
        /// @param tsa_uri  The TSA URI to use for time-stamping.
        Signer(SignerFunc *callback, C2paSigningAlg alg, const string &sign_cert, const string &tsa_uri);

        /// @brief Create a signer from a signer pointer and takes ownership of that pointer
        /// @param c_signer The signer pointer to use here (should be non null)
        Signer(C2paSigner *c_signer) : signer(c_signer) {}

        /// @brief Crates a signer from signer information
        /// @param alg Signer algorithm
        /// @param sign_cert Signing certificate
        /// @param private_key Private key
        /// @param tsa_uri URL for timestamping authority
        Signer(const string &alg, const string &sign_cert, const string &private_key, const optional<string> &tsa_uri = nullopt);

        // Non-copyable
        Signer(const Signer&) = delete;
        Signer& operator=(const Signer&) = delete;

        // Move semantics (for ownership transfer)
        Signer(Signer&& other) noexcept : signer(other.signer) {
            other.signer = nullptr;
        }
        Signer& operator=(Signer&& other) noexcept {
            if (this != &other) {
                c2pa_signer_free(signer);
                signer = other.signer;
                other.signer = nullptr;
            }
            return *this;
        }

        ~Signer();

        /// @brief  Get the size to reserve for a signature for this signer.
        /// @return Reserved size for the signature.
        uintptr_t reserve_size();

        /// @brief  Get the C2paSigner
        C2paSigner *c2pa_signer() const noexcept;
    };

    /// @brief Builder class for creating a manifest.
    /// @details This class is used to create a manifest from a json string and add resources and ingredients to the manifest.
    class C2PA_CPP_API Builder
    {
    private:
        C2paBuilder *builder;
        ContextProviderPtr context_;  // Keeps context alive

    public:
        // ===== Context-based constructors (NEW, RECOMMENDED) =====
        
        /// @brief Create a Builder from a context with an empty manifest.
        /// @param context Context provider to use for this builder.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        explicit Builder(ContextProviderPtr context);
        
        /// @brief Create a Builder from a context and manifest JSON string.
        /// @param context Context provider to use for this builder.
        /// @param manifest_json The manifest JSON string.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        Builder(ContextProviderPtr context, const std::string &manifest_json);
        
        // ===== Legacy constructor (DEPRECATED) =====
        
        /// @brief Create a Builder from a manifest JSON string (uses global settings).
        /// @param manifest_json The manifest JSON string.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        /// @deprecated Use Builder(ContextProviderPtr, manifest_json) instead.
        [[deprecated("Use Builder(ContextProviderPtr, manifest_json) for better thread safety")]]
        Builder(const std::string &manifest_json);

        // Non-copyable
        Builder(const Builder&) = delete;
        Builder& operator=(const Builder&) = delete;

        // Move semantics
        Builder(Builder&& other) noexcept : builder(other.builder), context_(std::move(other.context_)) {
            other.builder = nullptr;
        }
        Builder& operator=(Builder&& other) noexcept {
            if (this != &other) {
                c2pa_builder_free(builder);
                builder = other.builder;
                context_ = std::move(other.context_);
                other.builder = nullptr;
            }
            return *this;
        }

        ~Builder();
        
        /// @brief Get the context associated with this Builder.
        /// @return Shared pointer to the context, or nullptr if using legacy API.
        [[nodiscard]] inline ContextProviderPtr context() const noexcept {
            return context_;
        }

        /// @brief  Get the underlying C2paBuilder pointer.
        /// @return Pointer managed by this wrapper.
        C2paBuilder *c2pa_builder() const noexcept;
        
        /// @brief Set or update the manifest definition.
        /// @param manifest_json The manifest JSON string.
        /// @return Reference to this Builder for method chaining.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        Builder& with_definition(const std::string &manifest_json);

        /// @brief  Set the no embed flag.
        void set_no_embed();

        /// @brief Set the remote URL.
        /// @param remote_url  The remote URL to set.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        void set_remote_url(const string &remote_url);

        /// @brief Set the base path for loading resources from files.
        ///        Loads from memory if this is not set.
        /// @param base_path The base path to set.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        /// @deprecated This method is planned to be deprecated in a future release.
        ///             Usage should be limited and temporary. Use `add_resource` instead.
        void set_base_path(const string &base_path);

        /// @brief Add a resource to the builder.
        /// @param uri The uri of the resource.
        /// @param source The input stream to read the resource from.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        void add_resource(const string &uri, istream &source);

        /// @brief Add a resource to the builder.
        /// @param uri The uri of the resource.
        /// @param source_path  The path to the resource file.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        void add_resource(const string &uri, const std::filesystem::path &source_path);

        /// @brief Add an ingredient to the builder.
        /// @param ingredient_json  Any fields of the ingredient you want to define.
        /// @param format The format of the ingredient file.
        /// @param source The input stream to read the ingredient from.
        /// @throws C2pa::C2paException for errors encountered by the C2pa library.
        void add_ingredient(const string &ingredient_json, const string &format, istream &source);

        /// @brief Add an ingredient to the builder.
        /// @param ingredient_json  Any fields of the ingredient you want to define.
        /// @param source_path  The path to the ingredient file.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        void add_ingredient(const string &ingredient_json, const std::filesystem::path &source_path);

        /// @brief Add an action to the manifest the Builder is constructing.
        /// @param action_json JSON string containing the action data.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        void add_action(const string &action_json);

        /// @brief Sign an input stream and write the signed data to an output stream.
        /// @param format The format of the output stream.
        /// @param source The input stream to sign.
        /// @param dest The output stream to write the signed data to.
        /// @param signer
        /// @return A vector containing the signed manifest bytes.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library
        /// @deprecated Use `sign(const string&, istream&, iostream&, Signer&)`
        std::vector<unsigned char> sign(const string &format, istream &source, ostream &dest, Signer &signer);

        /// @brief Sign an input stream and write the signed data to an output stream.
        /// @param format The format of the output stream.
        /// @param source The input stream to sign.
        /// @param dest The in/output stream to write the signed data to.
        /// @param signer
        /// @return A vector containing the signed manifest bytes.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        std::vector<unsigned char> sign(const string &format, istream &source, iostream &dest, Signer &signer);

        /// @brief Sign a file and write the signed data to an output file.
        /// @param source_path The path to the file to sign.
        /// @param dest_path The path to write the signed file to.
        /// @param signer A signer object to use when signing.
        /// @return A vector containing the signed manifest bytes.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        std::vector<unsigned char> sign(const std::filesystem::path &source_path, const std::filesystem::path &dest_path, Signer &signer);

        /// @brief Create a Builder from an archive.
        /// @param archive  The input stream to read the archive from.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        static Builder from_archive(istream &archive);

        /// @brief Create a Builder from an archive
        /// @param archive_path  the path to the archive file
        /// @throws C2pa::C2paException for errors encountered by the C2PA library
        static Builder from_archive(const std::filesystem::path &archive_path);

        /// @brief Write the builder to an archive stream.
        /// @param dest The output stream to write the archive to.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        void to_archive(ostream &dest);

        /// @brief Write the builder to an archive file.
        /// @param dest_path The path to write the archive file to.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        void to_archive(const std::filesystem::path &dest_path);

        /// @brief Create a hashed placeholder from the builder.
        /// @param reserved_size The size required for a signature from the intended signer.
        /// @param format The format of the mime type or extension of the asset.
        /// @return A vector containing the hashed placeholder.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        std::vector<unsigned char> data_hashed_placeholder(uintptr_t reserved_size, const string &format);

        /// @brief Sign a Builder using the specified signer and data hash.
        /// @param signer The signer to use for signing.
        /// @param data_hash The data hash ranges to sign. This must contain hashes unless an asset is provided.
        /// @param format The mime format for embedding into.  Use "c2pa" for an unformatted result.
        /// @param asset An optional asset to hash according to the data_hash information.
        /// @return A vector containing the signed data.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        std::vector<unsigned char> sign_data_hashed_embeddable(Signer &signer, const string &data_hash, const string &format, istream *asset = nullptr);

        /// @brief convert an unformatted manifest data to an embeddable format.
        /// @param format The format for embedding into.
        /// @param data An unformatted manifest data block from sign_data_hashed_embeddable using "c2pa" format.
        /// @return A formatted copy of the data.
        static std::vector<unsigned char> format_embeddable(const string &format, std::vector<unsigned char> &data);

        /// @brief Returns a vector of mime types that the SDK is able to sign.
        static std::vector<std::string> supported_mime_types();

    private:
        // Private constructor for Builder from an archive (todo: find a better way to handle this)
        explicit Builder(istream &archive);
    };
}

// Restore warnings
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif

#ifdef _MSC_VER
#pragma warning(pop)
#endif

#endif // C2PA_H
