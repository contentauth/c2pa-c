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

#include <cerrno>
#include <filesystem>
#include <fstream>
#include <istream>
#include <ostream>
#include <string>
#include <vector>
#include <optional>
#include <memory>

#include <c2pa.h>

// NOOP for now, can use later to define static library
#define C2PA_CPP_API

namespace c2pa
{
    typedef C2paSignerInfo SignerInfo;

    // Forward declarations for context types
    class Settings;
    class Context;
    class IContextProvider;

    /// @brief Result codes for C API operations (matches C API return convention).
    enum class OperationResult : int {
        Success = 0,     ///< Operation succeeded
        Error = -1       ///< Operation failed (check C2paException for details)
    };

    /// @brief Stream/FFI error codes (maps to errno values used by the C layer).
    enum class StreamError : int {
        InvalidArgument = EINVAL,
        IoError = EIO,
        NoBufferSpace = ENOBUFS
    };

    /// @brief Set errno from StreamError and return error sentinel.
    /// @return OperationResult::Error (-1) for use as C API error return.
    inline int stream_error_return(StreamError e) noexcept {
        errno = static_cast<int>(e);
        return static_cast<int>(OperationResult::Error);
    }

    /// C2paException class for C2pa errors.
    /// This class is used to throw exceptions for errors encountered by the C2pa library via c2pa_error().
    class C2PA_CPP_API C2paException : public std::exception
    {
    public:
        C2paException();

        explicit C2paException(std::string message);

        ~C2paException() override = default;

        C2paException(const C2paException&) = default;
        C2paException& operator=(const C2paException&) = default;
        C2paException(C2paException&&) = default;
        C2paException& operator=(C2paException&&) = default;

        const char* what() const noexcept override;

    private:
        std::string message_;
    };

    /// @brief Interface for types that can provide C2PA context functionality.
    /// @details This interface can be implemented by external libraries to provide
    ///          custom context implementations (e.g. AdobeContext wrappers).
    ///          Reader and Builder take the context by reference and use it only at
    ///          construction; the underlying implementation copies context state
    ///          into the reader/builder, so the context does not need to outlive them.
    ///
    /// @par Move semantics
    /// Move construction and move assignment are defaulted. After move, the moved-from
    /// object is left in a valid but unspecified state: is_valid() may be false and
    /// c_context() may return nullptr. Implementations that own a C2paContext* (e.g. Context)
    /// must set the source's handle to nullptr on move to avoid double-free; callers must
    /// not use a moved-from provider without checking is_valid() first.
    ///
    /// @par Implementation Requirements for is_valid()
    /// The is_valid() method exists to support implementations that may have:
    /// - Optional or lazy context initialization
    /// - Contexts that can be invalidated or moved
    /// - A "no context" state as part of their lifecycle
    ///
    /// @par Why Both c_context() and is_valid()?
    /// While c_context() can return nullptr, is_valid() provides:
    /// 1. A boolean check without pointer inspection (yes/no answer for intialization)
    /// 2. Forward compatibility for implementations with complex context lifecycles (lazy load)
    ///
    /// @par Impact on Reader and Builder
    /// Reader and Builder constructors validate that a provider both exists and
    /// is_valid() returns true before using c_context(). This ensures that:
    /// - External implementations cannot be used in an uninitialized state
    /// - A consistent validation pattern exists across all context-using classes
    /// - Errors are caught early at construction time rather than during operations
    ///
    /// @par Standard Context Implementation
    /// The built-in Context class always returns true from is_valid() after
    /// successful construction, as it validates the context pointer in its constructor.
    /// External implementations may have different invariants.
    class C2PA_CPP_API IContextProvider {
    public:
        virtual ~IContextProvider() noexcept = default;

        /// @brief Get the underlying C2PA context pointer for FFI operations.
        /// @return Pointer to C2paContext, or nullptr if not available.
        /// @note Provider retains ownership; pointer valid for provider's lifetime.
        [[nodiscard]] virtual C2paContext* c_context() const noexcept = 0;

        /// @brief Check if this provider has a valid context.
        /// @return true if context is available, false otherwise.
        /// @note For standard Context objects, this always returns true after construction.
        ///       External implementations may return false to indicate uninitialized or
        ///       invalidated state. Reader and Builder constructors check this before use.
        /// @warning Implementations must ensure is_valid() == true implies c_context() != nullptr.
        [[nodiscard]] virtual bool is_valid() const noexcept = 0;

    protected:
        IContextProvider() = default;

        IContextProvider(const IContextProvider&) = delete;
        IContextProvider& operator=(const IContextProvider&) = delete;
    };

    /// @brief (C2PA SDK) Settings configuration object for creating contexts.
    /// @details Settings can be configured via JSON strings or programmatically
    ///          via set() and update() methods. Once passed to Context::ContextBuilder,
    ///          the settings are copied into the context and the Settings
    ///          object can be reused or discarded.
    ///
    /// @par Validity
    /// Settings uses is_valid() to indicate whether the object holds a valid underlying
    /// C settings handle. After move-from, is_valid() is false. set(), update(), and callers
    /// passing this object to the C API must ensure is_valid() is true or check before use.
    class C2PA_CPP_API Settings {
    public:
        /// @brief Create default settings.
        Settings();

        /// @brief Create settings from a configuration string.
        /// @param data Configuration data in JSON format.
        /// @param format Format of the data ("json").
        /// @throws C2paException if parsing fails.
        Settings(const std::string& data, const std::string& format);

        // Move semantics
        Settings(Settings&&) noexcept;
        Settings& operator=(Settings&&) noexcept;

        // Non-copyable
        Settings(const Settings&) = delete;
        Settings& operator=(const Settings&) = delete;

        ~Settings() noexcept;

        /// @brief Check if this Settings object is valid (holds a C settings handle).
        /// @return true if the object can be used (set, update, c_settings, or passed to Context/ContextBuilder).
        /// @return false if moved-from (or otherwise invalid). Callers must check before use.
        [[nodiscard]] bool is_valid() const noexcept;

        /// @brief Set a single configuration value by path.
        /// @param path Dot-separated path to the setting (e.g., "verify.verify_after_sign").
        /// @param json_value JSON-encoded value to set.
        /// @return Reference to this Settings for method chaining.
        /// @throws C2paException if the path or value is invalid.
        Settings& set(const std::string& path, const std::string& json_value);

        /// @brief Merge configuration from a JSON string (latest configuration wins).
        /// @param data Configuration data in JSON format.
        /// @return Reference to this Settings for method chaining.
        /// @throws C2paException if parsing fails, or if this object is invalid.
        /// @note This is the recommended overload when configuration is JSON.
        Settings& update(const std::string& data) { return update(data, "json"); }

        /// @brief Merge configuration from a std::string (latest configuration wins).
        /// @param data Configuration data in JSON or TOML format.
        /// @param format Format of the data ("json" or "toml").
        /// @return Reference to this Settings for method chaining.
        /// @throws C2paException if parsing fails, or if this object is invalid.
        Settings& update(const std::string& data, const std::string& format);

        /// @brief Get the raw C FFI settings pointer.
        /// @return Pointer to C2paSettings when is_valid() is true; nullptr when invalid.
        /// @note Callers passing this to the C API should check is_valid() first and treat nullptr as invalid.
        [[nodiscard]] C2paSettings* c_settings() const noexcept;

    private:
        C2paSettings* settings_ptr;
    };

    /// @brief C2PA context implementing IContextProvider.
    /// @details Context objects manage C2PA SDK configuration and state.
    ///          Contexts can be created via direct construction or the ContextBuilder:
    ///
    ///          Direct construction:
    ///          @code
    ///          c2pa::Context ctx;              // default
    ///          c2pa::Context ctx(settings);    // from Settings
    ///          c2pa::Context ctx(json);        // from JSON string
    ///          @endcode
    ///
    ///          ContextBuilder (for multi-step configuration):
    ///          @code
    ///          auto ctx = c2pa::Context::ContextBuilder()
    ///              .with_settings(settings)
    ///              .with_json(json)
    ///              .create_context();
    ///          @endcode
    ///
    ///          Builder and Reader take the context by reference (IContextProvider&).
    ///          The context object must outlive the Builder or Reader instance.
    class C2PA_CPP_API Context : public IContextProvider {
    public:
        /// @brief ContextBuilder for creating customized Context instances.
        /// @details Provides a builder pattern for configuring contexts with multiple settings.
        ///          Note: create_context() consumes the builder.
        /// @note For most use cases, prefer direct construction via the Context constructors.
        ///       The ContextBuilder is useful when you need to layer multiple configuration
        ///       sources (e.g. with_settings() followed by with_json()).
        class C2PA_CPP_API ContextBuilder {
        public:
            ContextBuilder();
            ~ContextBuilder() noexcept;

            // Move semantics
            ContextBuilder(ContextBuilder&&) noexcept;
            ContextBuilder& operator=(ContextBuilder&&) noexcept;

            // Non-copyable
            ContextBuilder(const ContextBuilder&) = delete;
            ContextBuilder& operator=(const ContextBuilder&) = delete;

            /// @brief Check if the builder is in a valid state.
            /// @return true if the builder can be used, false if moved from.
            [[nodiscard]] bool is_valid() const noexcept;

            /// @brief Configure with Settings object.
            /// @param settings Settings to use (will be copied into the context). Must be valid (is_valid() true).
            /// @return Reference to this ContextBuilder for method chaining.
            /// @throws C2paException if settings are invalid or settings.is_valid() is false.
            ContextBuilder& with_settings(const Settings& settings);

            /// @brief Configure settings with JSON string.
            /// @param json JSON configuration string.
            /// @return Reference to this ContextBuilder for method chaining.
            /// @throws C2paException if JSON is invalid.
            ContextBuilder& with_json(const std::string& json);

            /// @brief Configure settings from a JSON settings file.
            /// @param settings_path Full path to the JSON settings file.
            /// @return Reference to this ContextBuilder for method chaining.
            /// @throws C2paException if file cannot be read or JSON is invalid.
            ContextBuilder& with_json_settings_file(const std::filesystem::path& settings_path);

            /// @brief Create a Context from the current builder configuration.
            /// @return A new Context instance.
            /// @throws C2paException if context creation fails.
            /// @note This consumes the builder. After calling this, is_valid() returns false.
            [[nodiscard]] Context create_context();

        private:
            C2paContextBuilder* context_builder;
        };

        // Direct construction
        /// @brief Create a Context with default settings.
        /// @throws C2paException if context creation fails.
        Context();

        /// @brief Create a Context configured with a Settings object.
        /// @param settings Settings configuration to apply. Must be valid (settings.is_valid() true).
        /// @throws C2paException if settings are invalid, settings.is_valid() is false, or context creation fails.
        explicit Context(const Settings& settings);

        /// @brief Create a Context configured with a JSON string.
        /// @param json JSON configuration string.
        /// @throws C2paException if JSON is invalid or context creation fails.
        explicit Context(const std::string& json);

        // Non-copyable, moveable
        Context(const Context&) = delete;
        Context& operator=(const Context&) = delete;
        Context(Context&&) noexcept;
        Context& operator=(Context&&) noexcept;

        ~Context() noexcept override;

        // IContextProvider implementation
        /// @brief Get the underlying C2PA context pointer.
        /// @return C2paContext pointer when is_valid() is true; nullptr when moved-from (is_valid() false).
        /// @note Callers must check is_valid() before using the result; do not pass nullptr to the C API.
        [[nodiscard]] C2paContext* c_context() const noexcept override;

        /// @brief Check if this Context has a valid context (validity check for context-like types).
        /// @return true when the object holds a valid C context; false when moved-from.
        /// @note After move, is_valid() is false and c_context() returns nullptr.
        [[nodiscard]] bool is_valid() const noexcept override;

        /// @brief Internal constructor from raw FFI pointer (prefer public constructors).
        /// @param ctx Raw C2paContext pointer — Context takes ownership.
        /// @throws C2paException if ctx is nullptr.
        explicit Context(C2paContext* ctx);

    private:
        C2paContext* context;
    };

    /// Returns the version of the C2pa library.
    std::string C2PA_CPP_API version();

    /// Loads C2PA settings from a std::string in a given format.
    /// @param data the std::string to load.
    /// @param format the mime format of the string.
    /// @throws a C2pa::C2paException for errors encountered by the C2PA library.
    /// @deprecated Use Context::from_json() or Context::from_settings() instead for better thread safety.
    [[deprecated("Use Context::from_json() or Context::from_settings() instead")]]
    void C2PA_CPP_API load_settings(const std::string& data, const std::string& format);

    /// Reads a file and returns the manifest json as a C2pa::String.
    /// @param source_path the path to the file to read.
    /// @param data_dir the directory to store binary resources (optional).
    /// @return a std::string containing the manifest json if a manifest was found.
    /// @throws a C2pa::C2paException for errors encountered by the C2PA library.
    [[deprecated("Use Reader object instead")]]
    std::optional<std::string> C2PA_CPP_API read_file(const std::filesystem::path &source_path, const std::optional<std::filesystem::path> data_dir = std::nullopt);

    /// Reads a file and returns an ingredient JSON as a C2pa::String.
    /// @param source_path the path to the file to read.
    /// @param data_dir the directory to store binary resources.
    /// @return a std::string containing the ingredient json.
    /// @throws a C2pa::C2paException for errors encountered by the C2PA library.
    [[deprecated("Use Reader and Builder.add_ingredient")]]
    std::string C2PA_CPP_API read_ingredient_file(const std::filesystem::path &source_path, const std::filesystem::path &data_dir);

    /// Adds the manifest and signs a file.
    /// @param source_path the path to the asset to be signed.
    /// @param dest_path the path to write the signed file to.
    /// @param manifest the manifest json to add to the file.
    /// @param signer_info the signer info to use for signing.
    /// @param data_dir the directory to store binary resources (optional).
    /// @throws a C2pa::C2paException for errors encountered by the C2PA library.
    [[deprecated("Use Builder.sign instead")]]
    void C2PA_CPP_API sign_file(const std::filesystem::path &source_path,
                            const std::filesystem::path &dest_path,
                            const char *manifest,
                            SignerInfo *signer_info,
                            const std::optional<std::filesystem::path> data_dir = std::nullopt);

    /// @defgroup StreamWrappers Stream wrappers for C2PA C API
    /// @brief C++ stream types that adapt stream types to C2paStream.
    ///
    /// The C2PA C API expects a C2paStream with four callbacks: reader, writer, seeker, flusher.
    /// The contract for each callback is:
    /// - reader(context, buffer, size): read up to size bytes into buffer; return bytes read, or -1 on error (set errno).
    /// - writer(context, buffer, size): write size bytes from buffer; return bytes written, or -1 on error (set errno).
    /// - seeker(context, offset, whence): seek to offset (whence = Start/Current/End); return new position or -1 (set errno).
    /// - flusher(context): flush; return 0 on success, -1 on error (set errno).

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
            if (c_stream == nullptr) {
                throw C2paException("Failed to create input stream wrapper: is stream open and valid?");
            }
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
            if (c_stream == nullptr) {
                throw C2paException("Failed to create output stream wrapper: is stream open and valid?");
            }
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
            if (c_stream == nullptr) {
                throw C2paException("Failed to create I/O stream wrapper: is stream open and valid?");
            }
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
    ///          Resources are managed using RAII; member order ensures cpp_stream (which
    ///          holds a C stream pointing at the ifstream) is destroyed before owned_stream.
    class C2PA_CPP_API Reader
    {
    private:
        C2paReader *c2pa_reader;
        std::unique_ptr<std::ifstream> owned_stream;       // Owns file stream when created from path
        std::unique_ptr<CppIStream> cpp_stream;            // Wraps stream for C API; destroyed before owned_stream

    public:
        /// @brief Create a Reader from a context and stream.
        /// @param context Context provider; used at construction to configure settings
        /// @param format The mime format of the stream.
        /// @param stream The input stream to read from.
        /// @throws C2pa::C2paException if context.is_valid() returns false,
        ///         or for other errors encountered by the C2PA library.
        Reader(IContextProvider& context, const std::string &format, std::istream &stream);

        /// @brief Create a Reader from a context and file path.
        /// @param context Context provider; used at construction only to configure settings.
        /// @param source_path the path to the file to read.
        /// @throws C2pa::C2paException if context.is_valid() returns false,
        ///         or for other errors encountered by the C2PA library.
        /// @note Prefer using the streaming APIs if possible
        Reader(IContextProvider& context, const std::filesystem::path &source_path);

        /// @brief Create a Reader from a stream (will use global settings if any loaded).
        /// @details The validation_status field in the json contains validation results.
        /// @param format The mime format of the stream.
        /// @param stream The input stream to read from.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        /// @deprecated Use Reader(IContextProvider& context, format, stream) instead.
        [[deprecated("Use Reader(IContextProvider& context, format, stream) instead")]]
        Reader(const std::string &format, std::istream &stream);

        /// @brief Create a Reader from a file path (will use global settings if any loaded).
        /// @param source_path The path to the file to read.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        /// @deprecated Use Reader(IContextProvider& context, source_path) instead.
        /// @note Prefer using the streaming APIs if possible
        [[deprecated("Use Reader(IContextProvider& context, source_path) instead")]]
        Reader(const std::filesystem::path &source_path);

        // Non-copyable
        Reader(const Reader&) = delete;
        Reader& operator=(const Reader&) = delete;

        // Move semantics
        Reader(Reader&& other) noexcept
            : c2pa_reader(other.c2pa_reader),
              owned_stream(std::move(other.owned_stream)),
              cpp_stream(std::move(other.cpp_stream)) {
            other.c2pa_reader = nullptr;
        }
        Reader& operator=(Reader&& other) noexcept {
            if (this != &other) {
                if (c2pa_reader != nullptr) {
                    c2pa_free(c2pa_reader);
                }
                c2pa_reader = other.c2pa_reader;
                owned_stream = std::move(other.owned_stream);
                cpp_stream = std::move(other.cpp_stream);
                other.c2pa_reader = nullptr;
            }
            return *this;
        }

        ~Reader();

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
        std::string json() const;

        /// @brief  Get a resource from the reader and write it to a file.
        /// @param uri The uri of the resource.
        /// @param path The path to write the resource to.
        /// @return The number of bytes written.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        /// @note Prefer using the streaming APIs if possible
        int64_t get_resource(const std::string &uri, const std::filesystem::path &path);

        /// @brief  Get a resource from the reader  and write it to an output stream.
        /// @param uri The uri of the resource.
        /// @param stream The output stream to write the resource to.
        /// @return The number of bytes written.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        int64_t get_resource(const std::string &uri, std::ostream &stream);

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

        /// tsa_uri checks
        static const char *validate_tsa_uri(const std::string &tsa_uri);
        static const char *validate_tsa_uri(const std::optional<std::string> &tsa_uri);

    public:
        /// @brief Create a Signer from a callback function, signing algorithm, certificate, and TSA URI.
        /// @param callback  the callback function to use for signing.
        /// @param alg  The signing algorithm to use.
        /// @param sign_cert The certificate to use for signing.
        /// @param tsa_uri  The TSA URI to use for time-stamping.
        Signer(SignerFunc *callback, C2paSigningAlg alg, const std::string &sign_cert, const std::string &tsa_uri);

        /// @brief Create a signer from a signer pointer and takes ownership of that pointer
        /// @param c_signer The signer pointer to use here (should be non null)
        Signer(C2paSigner *c_signer) : signer(c_signer) {}

        /// @brief Crates a signer from signer information
        /// @param alg Signer algorithm
        /// @param sign_cert Signing certificate
        /// @param private_key Private key
        /// @param tsa_uri URL for timestamping authority
        Signer(const std::string &alg, const std::string &sign_cert, const std::string &private_key, const std::optional<std::string> &tsa_uri = std::nullopt);

        // Non-copyable
        Signer(const Signer&) = delete;
        Signer& operator=(const Signer&) = delete;

        // Move semantics (for ownership transfer)
        Signer(Signer&& other) noexcept : signer(other.signer) {
            other.signer = nullptr;
        }
        Signer& operator=(Signer&& other) noexcept {
            if (this != &other) {
                c2pa_free(signer);
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
    /// @details This class is used to create a manifest from a json std::string and add resources and ingredients to the manifest.
    class C2PA_CPP_API Builder
    {
    private:
        C2paBuilder *builder;

    public:
        /// @brief Create a Builder from a context with an empty manifest.
        /// @param context Context provider; used at construction to configure settings.
        /// @throws C2pa::C2paException if context.is_valid() returns false,
        ///         or for other errors encountered by the C2PA library.
        explicit Builder(IContextProvider& context);

        /// @brief Create a Builder from a context and manifest JSON string.
        /// @param context Context provider; used at construction to configure settings.
        /// @param manifest_json The manifest JSON string.
        /// @throws C2pa::C2paException if context.is_valid() returns false,
        ///         or for other errors encountered by the C2PA library.
        Builder(IContextProvider& context, const std::string &manifest_json);

        /// @brief Create a Builder from a manifest JSON std::string (will use global settings if any loaded).
        /// @param manifest_json The manifest JSON string.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        /// @deprecated Use Builder(IContextProvider& context, manifest_json) instead.
        [[deprecated("Use Builder(IContextProvider& context, manifest_json) instead")]]
        Builder(const std::string &manifest_json);

        // Non-copyable
        Builder(const Builder&) = delete;
        Builder& operator=(const Builder&) = delete;

        // Move semantics
        Builder(Builder&& other) noexcept : builder(other.builder) {
            other.builder = nullptr;
        }
        Builder& operator=(Builder&& other) noexcept {
            if (this != &other) {
                if (builder != nullptr)
                    c2pa_free(builder);
                builder = other.builder;
                other.builder = nullptr;
            }
            return *this;
        }

        ~Builder();

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
        void set_remote_url(const std::string &remote_url);

        /// @brief Set the base path for loading resources from files.
        ///        Loads from memory if this is not set.
        /// @param base_path The base path to set.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        /// @deprecated This method is planned to be deprecated in a future release.
        ///             Usage should be limited and temporary. Use `add_resource` instead.
        void set_base_path(const std::string &base_path);

        /// @brief Add a resource to the builder.
        /// @param uri The uri of the resource.
        /// @param source The input stream to read the resource from.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        void add_resource(const std::string &uri, std::istream &source);

        /// @brief Add a resource to the builder.
        /// @param uri The uri of the resource.
        /// @param source_path  The path to the resource file.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        /// @note Prefer using the streaming APIs if possible
        void add_resource(const std::string &uri, const std::filesystem::path &source_path);

        /// @brief Add an ingredient to the builder.
        /// @param ingredient_json  Any fields of the ingredient you want to define.
        /// @param format The format of the ingredient file.
        /// @param source The input stream to read the ingredient from.
        /// @throws C2pa::C2paException for errors encountered by the C2pa library.
        void add_ingredient(const std::string &ingredient_json, const std::string &format, std::istream &source);

        /// @brief Add an ingredient to the builder.
        /// @param ingredient_json  Any fields of the ingredient you want to define.
        /// @param source_path  The path to the ingredient file.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        /// @note Prefer using the streaming APIs if possible
        void add_ingredient(const std::string &ingredient_json, const std::filesystem::path &source_path);

        /// @brief Add an action to the manifest the Builder is constructing.
        /// @param action_json JSON std::string containing the action data.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        void add_action(const std::string &action_json);

        /// @brief Sign an input stream and write the signed data to an output stream.
        /// @param format The format of the output stream.
        /// @param source The input stream to sign.
        /// @param dest The output stream to write the signed data to.
        /// @param signer
        /// @return A vector containing the signed manifest bytes.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library
        /// @deprecated Use `sign(const string&, std::istream&, std::iostream&, Signer&)`
        std::vector<unsigned char> sign(const std::string &format, std::istream &source, std::ostream &dest, Signer &signer);

        /// @brief Sign an input stream and write the signed data to an output stream.
        /// @param format The format of the output stream.
        /// @param source The input stream to sign.
        /// @param dest The in/output stream to write the signed data to.
        /// @param signer
        /// @return A vector containing the signed manifest bytes.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        std::vector<unsigned char> sign(const std::string &format, std::istream &source, std::iostream &dest, Signer &signer);

        /// @brief Sign a file and write the signed data to an output file.
        /// @param source_path The path to the file to sign.
        /// @param dest_path The path to write the signed file to.
        /// @param signer A signer object to use when signing.
        /// @return A vector containing the signed manifest bytes.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        /// @note Prefer using the streaming APIs if possible
        std::vector<unsigned char> sign(const std::filesystem::path &source_path, const std::filesystem::path &dest_path, Signer &signer);

        /// @brief Create a Builder from an archived Builder.
        /// @param archive  The input stream to read the archive from.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        static Builder from_archive(std::istream &archive);

        /// @brief Create a Builder from an archive
        /// @param archive_path  the path to the archive file
        /// @throws C2pa::C2paException for errors encountered by the C2PA library
        /// @note Prefer using the streaming APIs if possible
        static Builder from_archive(const std::filesystem::path &archive_path);

        /// @brief Load an archive into this builder, replacing its current definition
        ///        with the reloaded archived builder state.
        /// @param archive The input stream to read the archive from.
        /// @return Reference to this builder for method chaining.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        /// @note This allows setting a context before loading the archive, preserving context settings.
        Builder& with_archive(std::istream &archive);

        /// @brief Write the builder to an archive stream.
        /// @param dest The output stream to write the archive to.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        void to_archive(std::ostream &dest);

        /// @brief Write the builder to an archive file.
        /// @param dest_path The path to write the archive file to.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        /// @note Prefer using the streaming APIs if possible
        void to_archive(const std::filesystem::path &dest_path);

        /// @brief Create a hashed placeholder from the builder.
        /// @param reserved_size The size required for a signature from the intended signer.
        /// @param format The format of the mime type or extension of the asset.
        /// @return A vector containing the hashed placeholder.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        std::vector<unsigned char> data_hashed_placeholder(uintptr_t reserved_size, const std::string &format);

        /// @brief Sign a Builder using the specified signer and data hash.
        /// @param signer The signer to use for signing.
        /// @param data_hash The data hash ranges to sign. This must contain hashes unless an asset is provided.
        /// @param format The mime format for embedding into.  Use "c2pa" for an unformatted result.
        /// @param asset An optional asset to hash according to the data_hash information.
        /// @return A vector containing the signed data.
        /// @throws C2pa::C2paException for errors encountered by the C2PA library.
        std::vector<unsigned char> sign_data_hashed_embeddable(Signer &signer, const std::string &data_hash, const std::string &format, std::istream *asset = nullptr);

        /// @brief convert an unformatted manifest data to an embeddable format.
        /// @param format The format for embedding into.
        /// @param data An unformatted manifest data block from sign_data_hashed_embeddable using "c2pa" format.
        /// @return A formatted copy of the data.
        static std::vector<unsigned char> format_embeddable(const std::string &format, std::vector<unsigned char> &data);

        /// @brief Returns a vector of mime types that the SDK is able to sign.
        static std::vector<std::string> supported_mime_types();

    private:
        // Private constructor for Builder from an archive (todo: find a better way to handle this)
        explicit Builder(std::istream &archive);
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
