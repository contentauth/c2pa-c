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

/// @file   c2pa.cpp
/// @brief  C++ wrapper for the C2PA C library.
/// @details This is used for creating and verifying C2PA manifests.
///          Thread safety is not guaranteed due to the use of errno and etc.
///          C++ standard: C++17

#include <cstring>
#include <fstream>
#include <iostream>
#include <string.h>
#include <optional>
#include <filesystem>
#include <system_error>
#include <utility>

#include "c2pa.hpp"

namespace {

/// @brief Converts a C array of C strings to a std::vector of std::string.
/// @param mime_types Pointer to an array of C strings (const char*).
/// @param count Number of elements in the array.
/// @return A std::vector containing the strings from the input array.
/// @details This function takes ownership of the input array and frees it
///          using c2pa_free_string_array().
std::vector<std::string> c_mime_types_to_vector(const char* const* mime_types, uintptr_t count) {
  std::vector<std::string> result;
  if (mime_types == nullptr) { return result; }

  result.reserve(count);
  for(uintptr_t i = 0; i < count; i++) {
    result.emplace_back(mime_types[i]);
  }

  c2pa_free_string_array(mime_types, count);
  return result;
}

intptr_t signer_passthrough(const void *context, const unsigned char *data, uintptr_t len, unsigned char *signature, uintptr_t sig_max_len)
{
  if (data == nullptr || signature == nullptr)
  {
    return c2pa::stream_error_return(c2pa::StreamError::InvalidArgument);
  }
  try
  {
    // the context is a pointer to the C++ callback function
    auto* callback = reinterpret_cast<c2pa::SignerFunc*>(const_cast<void*>(context));
    std::vector<uint8_t> data_vec(data, data + len);
    std::vector<uint8_t> signature_vec = (callback)(data_vec);
    if (signature_vec.size() > sig_max_len)
    {
      return c2pa::stream_error_return(c2pa::StreamError::NoBufferSpace);
    }
    std::copy(signature_vec.begin(), signature_vec.end(), signature);
    return signature_vec.size();
  }
  catch (std::exception const &e)
  {
    // todo pass exceptions to Rust error handling
    (void)e;
    // printf("Error: signer_passthrough - %s\n", e.what());
    return -1;
  }
  catch (...)
  {
    // printf("Error: signer_passthrough - unknown C2paException\n");
    return -1;
  }
}

}

namespace c2pa
{
namespace detail {

/// Maps C2PA seek mode to std::ios seek direction.
constexpr std::ios_base::seekdir whence_to_seekdir(C2paSeekMode whence) noexcept {
    switch (whence) {
        case C2paSeekMode::Start:   return std::ios_base::beg;
        case C2paSeekMode::Current: return std::ios_base::cur;
        case C2paSeekMode::End:     return std::ios_base::end;
        default:                    return std::ios_base::beg;
    }
}

/// Check if stream is in valid state for I/O operations
template<typename Stream>
inline bool is_stream_usable(Stream* s) noexcept {
    return s && !s->bad();
}

/// Traits (templated): how to seek and get position for a given stream type.
template<typename Stream>
struct StreamSeekTraits;

template<>
struct StreamSeekTraits<std::istream> {
    static void seek(std::istream* s, intptr_t offset, std::ios_base::seekdir dir) {
        s->seekg(offset, dir);
    }
    static int64_t tell(std::istream* s) {
        return static_cast<int64_t>(s->tellg());
    }
};

template<>
struct StreamSeekTraits<std::ostream> {
    static void seek(std::ostream* s, intptr_t offset, std::ios_base::seekdir dir) {
        s->seekp(offset, dir);
    }
    static int64_t tell(std::ostream* s) {
        return static_cast<int64_t>(s->tellp());
    }
};

template<>
struct StreamSeekTraits<std::iostream> {
    static void seek(std::iostream* s, intptr_t offset, std::ios_base::seekdir dir) {
        s->seekg(offset, dir);
        s->seekp(offset, dir);
    }
    static int64_t tell(std::iostream* s) {
        return static_cast<int64_t>(s->tellp());
    }
};

/// Seeker impl.
template<typename Stream>
intptr_t stream_seeker(StreamContext* context, intptr_t offset, C2paSeekMode whence) {
    auto* stream = reinterpret_cast<Stream*>(context);
    if (!is_stream_usable(stream)) {
        return stream_error_return(StreamError::IoError);
    }
    const std::ios_base::seekdir dir = whence_to_seekdir(whence);
    stream->clear();
    StreamSeekTraits<Stream>::seek(stream, offset, dir);
    if (stream->fail()) {
        return stream_error_return(StreamError::InvalidArgument);
    }
    if (stream->bad()) {
        return stream_error_return(StreamError::IoError);
    }
    const int64_t pos = StreamSeekTraits<Stream>::tell(stream);
    if (pos < 0) {
        return stream_error_return(StreamError::IoError);
    }
    return static_cast<intptr_t>(pos);
}

/// Reader impl.
template<typename Stream>
intptr_t stream_reader(StreamContext* context, uint8_t* buffer, intptr_t size) {
    if (!context || !buffer) {
        return stream_error_return(StreamError::InvalidArgument);
    }
    if (size < 0) {
        return stream_error_return(StreamError::InvalidArgument);
    }
    if (size == 0) {
        return 0;
    }
    auto* stream = reinterpret_cast<Stream*>(context);
    if (!is_stream_usable(stream)) {
        return stream_error_return(StreamError::IoError);
    }
    stream->read(reinterpret_cast<char*>(buffer), size);
    if (stream->fail()) {
        if (!stream->eof()) {
            return stream_error_return(StreamError::InvalidArgument);
        }
    }
    if (stream->bad()) {
        return stream_error_return(StreamError::IoError);
    }
    return static_cast<intptr_t>(stream->gcount());
}

/// Get stream from context, used by writer and flusher.
template<typename Stream, typename Op>
intptr_t stream_op(StreamContext* context, Op op) {
    auto* stream = reinterpret_cast<Stream*>(context);
    if (!is_stream_usable(stream)) {
        return stream_error_return(StreamError::IoError);
    }
    const intptr_t result = op(stream);
    if (stream->fail()) {
        return stream_error_return(StreamError::InvalidArgument);
    }
    if (stream->bad()) {
        return stream_error_return(StreamError::IoError);
    }
    return result;
}

/// Writer impl.
template<typename Stream>
intptr_t stream_writer(StreamContext* context, const uint8_t* buffer, intptr_t size) {
    return stream_op<Stream>(context, [buffer, size](Stream* s) {
        s->write(reinterpret_cast<const char*>(buffer), size);
        return size;
    });
}

/// Flusher impl.
template<typename Stream>
intptr_t stream_flusher(StreamContext* context) {
    return stream_op<Stream>(context, [](Stream* s) {
        s->flush();
        return 0;
    });
}

/// @brief Converts a path to a std::string in utf-8 format
inline std::string path_to_string(const std::filesystem::path &source_path)
{
    // Use u8string to ensure UTF-8 encoding across platforms. We have to convert
    // to std::string manually because std::string doesn't have a constructor accepting u8String until C++20.
    auto u8_str = source_path.u8string();
    return std::string(u8_str.begin(), u8_str.end());
}

/// @brief Open a binary file stream with error handling
/// @tparam StreamType std::ifstream or std::ofstream
/// @param path Path to the file
/// @return Unique pointer to opened stream
template<typename StreamType>
inline std::unique_ptr<StreamType> open_file_binary(const std::filesystem::path &path)
{
    auto path_str = path_to_string(path);
    auto stream = std::make_unique<StreamType>(
        path_str,
        std::ios_base::binary
    );
    if (!stream->is_open()) {
        throw C2paException("Failed to open file: " + path_str);
    }
    return stream;
}

/// @brief Extract file extension without the leading dot
/// @param path Filesystem path
/// @return Extension string (e.g., "jpg" not ".jpg")
inline std::string extract_file_extension(const std::filesystem::path &path) noexcept {
    auto ext = path.extension().string();
    return ext.empty() ? "" : ext.substr(1);
}

/// @brief Convert C string result to C++ string with cleanup
/// @param c_result Raw C string from C API
/// @return C++ string (throws if null)
template<typename T>
inline std::string c_string_to_string(T* c_result) {
    if (c_result == nullptr) {
        throw C2paException();
    }
    std::string str(c_result);
    c2pa_free(c_result);
    return str;
}

/// @brief Convert C byte array result to C++ vector
/// @param data Raw byte array from C API
/// @param size Size of the byte array (result from C API call)
/// @return Vector containing the bytes (throws if null or negative size)
/// @details This helper extracts the pattern of checking C API results,
///          copying to a vector, and freeing the C-allocated memory.
///          The C API contract is: if result < 0 or data == nullptr, the operation failed.
inline std::vector<unsigned char> to_byte_vector(const unsigned char* data, int64_t size) {
    if (size < 0 || data == nullptr) {
        c2pa_free(data);  // May be null or allocated, c2pa_free handles both
        throw C2paException();
    }

    auto result = std::vector<unsigned char>(data, data + size);
    c2pa_free(data);
    return result;
}

} // namespace detail

    /// C2paException class for C2PA errors.
    /// This class is used to throw exceptions for errors encountered by the C2PA library via c2pa_error().

    C2paException::C2paException()
    {
        auto result = c2pa_error();
        message_ = result ? std::string(result) : std::string();
        c2pa_free(result);
    }

    C2paException::C2paException(std::string message) : message_(std::move(message))
    {
    }

    const char* C2paException::what() const noexcept
    {
        return message_.c_str();
    }

    // Settings Implementation

    Settings::Settings() : settings_(c2pa_settings_new()) {
        if (!settings_) {
            throw C2paException("Failed to create settings");
        }
    }

    Settings::Settings(const std::string& data, const std::string& format) : settings_(c2pa_settings_new()) {
        if (!settings_) {
            throw C2paException("Failed to create settings");
        }
        if (c2pa_settings_update_from_string(settings_, data.c_str(), format.c_str()) != 0) {
            c2pa_free(settings_);
            throw C2paException();
        }
    }

    Settings::Settings(Settings&& other) noexcept
        : settings_(std::exchange(other.settings_, nullptr)) {
    }

    Settings& Settings::operator=(Settings&& other) noexcept {
        if (this != &other) {
            if (settings_) {
                c2pa_free(settings_);
            }
            settings_ = std::exchange(other.settings_, nullptr);
        }
        return *this;
    }

    Settings::~Settings() noexcept {
        if (settings_) {
            c2pa_free(settings_);
        }
    }

    Settings& Settings::set(const std::string& path, const std::string& json_value) {
        if (c2pa_settings_set_value(settings_, path.c_str(), json_value.c_str()) != 0) {
            throw C2paException();
        }
        return *this;
    }

    Settings& Settings::update(const std::string& data, const std::string& format) {
        if (c2pa_settings_update_from_string(settings_, data.c_str(), format.c_str()) != 0) {
            throw C2paException();
        }
        return *this;
    }

    C2paSettings* Settings::c_settings() const noexcept {
        return settings_;
    }

    // Context Implementation

    Context::Context(C2paContext* ctx) : context(ctx) {
        if (!context) {
            throw C2paException("Invalid context pointer");
        }
    }

    Context::Context() : context(c2pa_context_new()) {
        if (!context) {
            throw C2paException("Failed to create context");
        }
    }

    Context::Context(const Settings& settings) : context(nullptr) {
        auto builder = c2pa_context_builder_new();
        if (!builder) {
            throw C2paException("Failed to create context builder");
        }
        if (c2pa_context_builder_set_settings(builder, settings.c_settings()) != 0) {
            c2pa_free(builder);
            throw C2paException();
        }
        context = c2pa_context_builder_build(builder);
        if (!context) {
            throw C2paException("Failed to build context");
        }
    }

    Context::Context(Context&& other) noexcept : context(other.context) {
        other.context = nullptr;
    }

    Context& Context::operator=(Context&& other) noexcept {
        if (this != &other) {
            if (context) {
                c2pa_free(context);
            }
            context = other.context;
            other.context = nullptr;
        }
        return *this;
    }

    Context::~Context() noexcept {
        if (context) {
            c2pa_free(context);
        }
    }

    C2paContext* Context::c_context() const noexcept {
        return context;
    }

    bool Context::has_context() const noexcept {
        return context != nullptr;
    }

    Context::Context(const std::string& json) : Context(Settings(json, "json")) {
    }

    // Context::ContextBuilder

    Context::ContextBuilder::ContextBuilder() : context_builder(c2pa_context_builder_new()) {
        if (!context_builder) {
            throw C2paException("Failed to create context builder");
        }
    }

    Context::ContextBuilder::ContextBuilder(ContextBuilder&& other) noexcept
        : context_builder(std::exchange(other.context_builder, nullptr)) {
    }

    Context::ContextBuilder& Context::ContextBuilder::operator=(ContextBuilder&& other) noexcept {
        if (this != &other) {
            if (context_builder) {
                c2pa_free(context_builder);
            }
            context_builder = std::exchange(other.context_builder, nullptr);
        }
        return *this;
    }

    Context::ContextBuilder::~ContextBuilder() noexcept {
        if (context_builder) {
            c2pa_free(context_builder);
        }
    }

    bool Context::ContextBuilder::is_valid() const noexcept {
        return context_builder != nullptr;
    }

    Context::ContextBuilder& Context::ContextBuilder::with_settings(const Settings& settings) {
        if (!is_valid()) {
            throw C2paException("ContextBuilder is invalid (moved from)");
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

        // Open the file and read its content
        auto file = detail::open_file_binary<std::ifstream>(settings_path);
        std::string json_content((std::istreambuf_iterator<char>(*file)), std::istreambuf_iterator<char>());

        // Use the existing with_json method
        return with_json(json_content);
    }

    Context Context::ContextBuilder::create_context() {
        if (!is_valid()) {
            throw C2paException("ContextBuilder is invalid (moved from)");
        }

        // The C API consumes the builder on build
        C2paContext* ctx = c2pa_context_builder_build(context_builder);
        if (!ctx) {
            throw C2paException("Failed to build context");
        }

        // Builder is consumed by the C API
        context_builder = nullptr;

        return Context(ctx);
    }

    /// Returns the version of the C2PA library.
    std::string version()
    {
        return detail::c_string_to_string(c2pa_version());
    }

    /// Loads C2PA settings from a std::string in a given format.
    /// @param data the std::string to load.
    /// @param format the mime format of the string.
    /// @throws a C2pa::C2paException for errors encountered by the C2PA library.
    [[deprecated("Use Settings on Builder and Reader instead")]]
    void load_settings(const std::string &data, const std::string &format)
    {
        auto result = c2pa_load_settings(data.c_str(), format.c_str());
        if (result != 0)
        {
            throw c2pa::C2paException();
        }
    }

    /// Reads a file and returns the manifest json as a C2pa::String.
    /// @param source_path the path to the file to read.
    /// @param data_dir the directory to store binary resources (optional).
    /// @return a std::string containing the manifest json if a manifest was found.
    /// @throws a C2pa::C2paException for errors encountered by the C2PA library.
    [[deprecated("Use stream APIs instead: Reader to read data, combined with Builder to manage ingredients")]]
    std::optional<std::string> read_file(const std::filesystem::path &source_path, const std::optional<std::filesystem::path> data_dir)
    {
        const char* dir_ptr = nullptr;
        std::string dir_str;
        if (data_dir.has_value()) {
            dir_str = detail::path_to_string(data_dir.value());
            dir_ptr = dir_str.c_str();
        }

        char *result = c2pa_read_file(detail::path_to_string(source_path).c_str(), dir_ptr);

        if (result == nullptr)
        {
            auto C2paException = c2pa::C2paException();
            if (strstr(C2paException.what(), "ManifestNotFound") != nullptr)
            {
                return std::nullopt;
            }
            throw c2pa::C2paException();
        }
        std::string str(result);
        c2pa_free(result);
        return str;
    }

    /// Reads a file and returns an ingredient JSON as a C2pa::String.
    /// @param source_path the path to the file to read.
    /// @param data_dir the directory to store binary resources.
    /// @return a std::string containing the ingredient json.
    /// @throws a C2pa::C2paException for errors encountered by the C2PA library.
    [[deprecated("Use stream APIs instead: add_ingredient on the Builder")]]
    std::string read_ingredient_file(const std::filesystem::path &source_path, const std::filesystem::path &data_dir)
    {
        return detail::c_string_to_string(
            c2pa_read_ingredient_file(detail::path_to_string(source_path).c_str(),
                                     detail::path_to_string(data_dir).c_str()));
    }

    /// Adds the manifest and signs a file.
    // source_path: path to the asset to be signed
    // dest_path: the path to write the signed file to
    // manifest: the manifest json to add to the file
    // signer_info: the signer info to use for signing
    // data_dir: the directory to store binary resources (optional)
    // Throws a C2pa::C2paException for errors encountered by the C2PA library
    [[deprecated("Use stream APIs instead: sign on Builder")]]
    void sign_file(const std::filesystem::path &source_path,
                   const std::filesystem::path &dest_path,
                   const char *manifest,
                   c2pa::SignerInfo *signer_info,
                   const std::optional<std::filesystem::path> data_dir)
    {
        auto dir = data_dir.has_value() ? detail::path_to_string(data_dir.value()) : std::string();

        char *result = c2pa_sign_file(detail::path_to_string(source_path).c_str(), detail::path_to_string(dest_path).c_str(), manifest, signer_info, dir.c_str());
        if (result == nullptr)
        {
            throw c2pa::C2paException();
        }
        c2pa_free(result);
    }

    /// IStream Class wrapper for C2paStream.
    CppIStream::~CppIStream()
    {
        c2pa_release_stream(c_stream);
    }

    intptr_t CppIStream::reader(StreamContext *context, uint8_t *buffer, intptr_t size)
    {
        return detail::stream_reader<std::istream>(context, buffer, size);
    }

    intptr_t CppIStream::seeker(StreamContext *context, intptr_t offset, C2paSeekMode whence)
    {
        return detail::stream_seeker<std::istream>(context, offset, whence);
    }

    intptr_t CppIStream::writer(StreamContext *context, const uint8_t *buffer, intptr_t size)
    {
        (void)context;
        (void)buffer;
        (void)size;
        return stream_error_return(StreamError::InvalidArgument);
    }

    intptr_t CppIStream::flusher(StreamContext *context)
    {
        (void)context;
        return stream_error_return(StreamError::InvalidArgument);
    }

    /// Ostream Class wrapper for C2paStream implementation.
    CppOStream::~CppOStream()
    {
        c2pa_release_stream(c_stream);
    }

    intptr_t CppOStream::reader(StreamContext *context, uint8_t *buffer, intptr_t size)
    {
        (void) context;
        (void) buffer;
        (void) size;
        return stream_error_return(StreamError::InvalidArgument);
    }

    intptr_t CppOStream::seeker(StreamContext *context, intptr_t offset, C2paSeekMode whence)
    {
        return detail::stream_seeker<std::ostream>(context, offset, whence);
    }

    intptr_t CppOStream::writer(StreamContext *context, const uint8_t *buffer, intptr_t size)
    {
        return detail::stream_writer<std::ostream>(context, buffer, size);
    }

    intptr_t CppOStream::flusher(StreamContext *context)
    {
        return detail::stream_flusher<std::ostream>(context);
    }

    /// IOStream Class wrapper for C2paStream implementation.
    CppIOStream::~CppIOStream()
    {
        c2pa_release_stream(c_stream);
    }

    intptr_t CppIOStream::reader(StreamContext *context, uint8_t *buffer, intptr_t size)
    {
        return detail::stream_reader<std::iostream>(context, buffer, size);
    }

    intptr_t CppIOStream::seeker(StreamContext *context, intptr_t offset, C2paSeekMode whence)
    {
        return detail::stream_seeker<std::iostream>(context, offset, whence);
    }

    intptr_t CppIOStream::writer(StreamContext *context, const uint8_t *buffer, intptr_t size)
    {
        return detail::stream_writer<std::iostream>(context, buffer, size);
    }

    intptr_t CppIOStream::flusher(StreamContext *context)
    {
        return detail::stream_flusher<std::iostream>(context);
    }

    /// Reader class for reading manifests

    Reader::Reader(IContextProvider& context, const std::string &format, std::istream &stream)
        : c2pa_reader(nullptr), reader_context(&context)
    {
        if (!reader_context->has_context()) {
            throw C2paException("Invalid context provider");
        }

        c2pa_reader = c2pa_reader_from_context(reader_context->c_context());
        if (c2pa_reader == nullptr) {
            throw C2paException("Failed to create reader from context");
        }

        cpp_stream = std::make_unique<CppIStream>(stream);
        // Update reader with stream.
        // Note: c2pa_reader_with_stream always consumes the reader pointer,
        // so the original pointer is invalid after this call regardless of success/error.
        C2paReader* updated = c2pa_reader_with_stream(c2pa_reader, format.c_str(), cpp_stream->c_stream);
        c2pa_reader = nullptr;
        if (updated == nullptr) {
            throw C2paException();
        }
        c2pa_reader = updated;
    }

    Reader::Reader(IContextProvider& context, const std::filesystem::path &source_path)
        : c2pa_reader(nullptr), reader_context(&context)
    {
        if (!reader_context->has_context()) {
            throw C2paException("Invalid context provider");
        }

        c2pa_reader = c2pa_reader_from_context(reader_context->c_context());
        if (c2pa_reader == nullptr) {
            throw C2paException("Failed to create reader from context");
        }

        // Create owned stream that will live as long as the Reader
        owned_stream = std::make_unique<std::ifstream>(source_path, std::ios::binary);
        if (!owned_stream->is_open()) {
            c2pa_free(c2pa_reader);
            throw std::system_error(errno, std::system_category(), "Failed to open file: " + source_path.string());
        }

        std::string extension = detail::extract_file_extension(source_path);

        // CppIStream stores reference to owned_stream, which lives as long as Reader
        cpp_stream = std::make_unique<CppIStream>(*owned_stream);
        // Note: c2pa_reader_with_stream always consumes the reader pointer.
        C2paReader* updated = c2pa_reader_with_stream(c2pa_reader, extension.c_str(), cpp_stream->c_stream);
        c2pa_reader = nullptr;
        if (updated == nullptr) {
            throw C2paException();
        }
        c2pa_reader = updated;
    }

    Reader::Reader(const std::string &format, std::istream &stream)
        : reader_context(nullptr)
    {
        cpp_stream = std::make_unique<CppIStream>(stream);
        c2pa_reader = c2pa_reader_from_stream(format.c_str(), cpp_stream->c_stream);
        if (c2pa_reader == nullptr)
        {
            throw C2paException();
        }
    }

    Reader::Reader(const std::filesystem::path &source_path)
        : reader_context(nullptr)
    {
        // Create owned stream that will live as long as the Reader
        owned_stream = std::make_unique<std::ifstream>(source_path, std::ios::binary);
        if (!owned_stream->is_open()) {
            throw std::system_error(errno, std::system_category(), "Failed to open file: " + source_path.string());
        }

        std::string extension = detail::extract_file_extension(source_path);

        // CppIStream stores reference to owned_stream, which lives as long as Reader
        cpp_stream = std::make_unique<CppIStream>(*owned_stream);
        c2pa_reader = c2pa_reader_from_stream(extension.c_str(), cpp_stream->c_stream);
        if (c2pa_reader == nullptr)
        {
            throw C2paException();
        }
    }

    Reader::~Reader()
    {
        c2pa_free(c2pa_reader);
        // cpp_stream and owned_stream are automatically cleaned up by unique_ptr
    }

    std::string Reader::json() const
    {
        return detail::c_string_to_string(c2pa_reader_json(c2pa_reader));
    }

    [[nodiscard]] std::optional<std::string> Reader::remote_url() const {
        auto url = c2pa_reader_remote_url(c2pa_reader);
        if (url == nullptr) { return std::nullopt; }
        std::string url_str(url);
        // The C2PA library returns a `const char*` that needs to be released.
        // The underlying `char*` is mutable; however, to indicate the value
        // shouldn't be modified, it's returned as a const char*.
        //
        // TODO: Revisit after determining how we want c2pa-rs to handle
        //       strings that shouldn't be modified by our bindings.
        c2pa_free(url);
        return url_str;
    }

    int64_t Reader::get_resource(const std::string &uri, const std::filesystem::path &path)
    {
        auto file_stream = detail::open_file_binary<std::ofstream>(path);
        return get_resource(uri.c_str(), *file_stream);
    }

    int64_t Reader::get_resource(const std::string &uri, std::ostream &stream)
    {
        CppOStream output_stream(stream);
        int64_t result = c2pa_reader_resource_to_stream(c2pa_reader, uri.c_str(), output_stream.c_stream);
        if (result < 0)
        {
            throw C2paException();
        }
        return result;
    }

    std::vector<std::string> Reader::supported_mime_types() {
      uintptr_t count = 0;
      auto ptr = c2pa_reader_supported_mime_types(&count);
      return c_mime_types_to_vector(ptr, count);
    }

    const char *Signer::validate_tsa_uri(const std::string &tsa_uri)
    {
        return tsa_uri.empty() ? nullptr : tsa_uri.c_str();
    }

    const char *Signer::validate_tsa_uri(const std::optional<std::string> &tsa_uri)
    {
        return (tsa_uri && !tsa_uri->empty()) ? tsa_uri->c_str() : nullptr;
    }

    Signer::Signer(SignerFunc *callback, C2paSigningAlg alg, const std::string &sign_cert, const std::string &tsa_uri)
    {
        // Pass the C++ callback as a context to our static callback wrapper.
        signer = c2pa_signer_create((const void *)callback, &signer_passthrough, alg, sign_cert.c_str(), validate_tsa_uri(tsa_uri));
    }

    Signer::Signer(const std::string &alg, const std::string &sign_cert, const std::string &private_key, const std::optional<std::string> &tsa_uri)
    {
        auto info = C2paSignerInfo { alg.c_str(), sign_cert.c_str(), private_key.c_str(), validate_tsa_uri(tsa_uri) };
        signer = c2pa_signer_from_info(&info);
    }

    Signer::~Signer()
    {
        c2pa_free(signer);
    }

    /// @brief  Get the C2paSigner
    C2paSigner *Signer::c2pa_signer() const noexcept
    {
        return signer;
    }

    /// @brief  Get the size to reserve for a signature for this signer.
    uintptr_t Signer::reserve_size()
    {
        return c2pa_signer_reserve_size(signer);
    }

    /// @brief  Builder class for creating a manifest implementation.

    Builder::Builder(IContextProvider& context)
        : builder(nullptr), builder_context(&context)
    {
        if (!builder_context->has_context()) {
            throw C2paException("Invalid context provider");
        }

        builder = c2pa_builder_from_context(builder_context->c_context());
        if (builder == nullptr) {
            throw C2paException("Failed to create builder from context");
        }
    }

    Builder::Builder(IContextProvider& context, const std::string &manifest_json)
        : builder(nullptr), builder_context(&context)
    {
        if (!builder_context->has_context()) {
            throw C2paException("Invalid context provider");
        }

        // This creates the Builder using the context, eg. propagates settings
        builder = c2pa_builder_from_context(builder_context->c_context());
        if (builder == nullptr) {
            throw C2paException("Failed to create builder from context");
        }

        // Apply the manifest definition to the Builder.
        // Note: c2pa_builder_with_definition always consumes the builder pointer,
        // so the original pointer is invalid after this call regardless of success/error.
        C2paBuilder* updated = c2pa_builder_with_definition(builder, manifest_json.c_str());
        builder = nullptr;
        if (updated == nullptr) {
            throw C2paException();
        }
        builder = updated;
    }

    Builder::Builder(const std::string &manifest_json)
        : builder(nullptr), builder_context(nullptr)
    {
        builder = c2pa_builder_from_json(manifest_json.c_str());
        if (builder == nullptr)
        {
            throw C2paException();
        }
    }

    /// @brief Create a Builder from an archive.
    /// @param archive  The input stream to read the archive from.
    /// @throws C2pa::C2paException for errors encountered by the C2PA library.
    Builder::Builder(std::istream &archive)
        : builder(nullptr), builder_context(nullptr)
    {
        CppIStream c_archive(archive);
        builder = c2pa_builder_from_archive(c_archive.c_stream);
        if (builder == nullptr)
        {
            throw C2paException();
        }
    }

    Builder::~Builder()
    {
        c2pa_free(builder);
    }

    C2paBuilder *Builder::c2pa_builder() const noexcept
    {
        return builder;
    }

    void Builder::set_no_embed()
    {
        c2pa_builder_set_no_embed(builder);
    }

    void Builder::set_remote_url(const std::string &remote_url)
    {
        int result = c2pa_builder_set_remote_url(builder, remote_url.c_str());
        if (result < 0)
        {
            throw C2paException();
        }
    }

    Builder& Builder::with_definition(const std::string &manifest_json)
    {
        C2paBuilder* updated = c2pa_builder_with_definition(builder, manifest_json.c_str());
        builder = nullptr;
        if (updated == nullptr) {
            throw C2paException("Failed to set builder definition");
        }
        builder = updated;
        return *this;
    }

    void Builder::set_base_path(const std::string &base_path)
    {
        int result = c2pa_builder_set_base_path(builder, base_path.c_str());
        if (result < 0)
        {
            throw C2paException();
        }
    }

    void Builder::add_resource(const std::string &uri, std::istream &source)
    {
        CppIStream c_source(source);
        int result = c2pa_builder_add_resource(builder, uri.c_str(), c_source.c_stream);
        if (result < 0)
        {
            throw C2paException();
        }
    }

    void Builder::add_resource(const std::string &uri, const std::filesystem::path &source_path)
    {
        auto stream = detail::open_file_binary<std::ifstream>(source_path);
        add_resource(uri, *stream);
    }

    void Builder::add_ingredient(const std::string &ingredient_json, const std::string &format, std::istream &source)
    {
        CppIStream c_source(source);
        int result = c2pa_builder_add_ingredient_from_stream(builder, ingredient_json.c_str(), format.c_str(), c_source.c_stream);
        if (result < 0)
        {
            throw C2paException();
        }
    }

    void Builder::add_ingredient(const std::string &ingredient_json, const std::filesystem::path &source_path)
    {
        auto stream = detail::open_file_binary<std::ifstream>(source_path);
        auto format = detail::extract_file_extension(source_path);
        add_ingredient(ingredient_json, format.c_str(), *stream);
    }

    void Builder::from_ingredient_archive(const std::string &ingredient_json, std::istream &archive)
    {
        add_ingredient(ingredient_json, C2paMimeType::BinaryArchive, archive);
    }

    void Builder::from_ingredient_archive(const std::string &ingredient_json, const std::filesystem::path &archive_path)
    {
        auto stream = detail::open_file_binary<std::ifstream>(archive_path);
        from_ingredient_archive(ingredient_json, *stream);
    }

    void Builder::add_action(const std::string &action_json)
    {
        int result = c2pa_builder_add_action(builder, action_json.c_str());
        if (result < 0)
        {
            throw C2paException();
        }
    }

    std::vector<unsigned char> Builder::sign(const std::string &format, std::istream &source, std::ostream &dest, Signer &signer)
    {
        // Caller's source/dest streams must outlive this call
        // Stream wrappers are stack locals that wrap the caller's streams
        CppIStream c_source(source);
        CppOStream c_dest(dest);
        const unsigned char *c2pa_manifest_bytes = nullptr;

        // c2pa_builder_sign() uses streams synchronously and completes before returning
        auto result = c2pa_builder_sign(builder, format.c_str(), c_source.c_stream, c_dest.c_stream, signer.c2pa_signer(), &c2pa_manifest_bytes);
        return detail::to_byte_vector(c2pa_manifest_bytes, result);
    }

    std::vector<unsigned char> Builder::sign(const std::string &format, std::istream &source, std::iostream &dest, Signer &signer)
    {
        // Caller's source/dest streams must outlive this call
        // Stream wrappers are stack locals that wrap the caller's streams
        CppIStream c_source(source);
        CppIOStream c_dest(dest);
        const unsigned char *c2pa_manifest_bytes = nullptr;

        // c2pa_builder_sign() uses streams synchronously and completes before returning
        auto result = c2pa_builder_sign(builder, format.c_str(), c_source.c_stream, c_dest.c_stream, signer.c2pa_signer(), &c2pa_manifest_bytes);
        return detail::to_byte_vector(c2pa_manifest_bytes, result);
    }

    /// @brief Sign a file and write the signed data to an output file.
    /// @param source_path The path to the file to sign.
    /// @param dest_path The path to write the signed file to.
    /// @param signer A signer object to use when signing.
    /// @return A vector containing the signed manifest bytes.
    /// @throws C2pa::C2paException for errors encountered by the C2PA library.
    std::vector<unsigned char> Builder::sign(const std::filesystem::path &source_path, const std::filesystem::path &dest_path, Signer &signer)
    {
        auto source = detail::open_file_binary<std::ifstream>(source_path);
        // Ensure the destination directory exists
        auto dest_dir = dest_path.parent_path();
        if (!std::filesystem::exists(dest_dir))
        {
            std::filesystem::create_directories(dest_dir);
        }

        std::fstream dest(dest_path,
                          std::ios_base::binary | std::ios_base::trunc |
                              std::ios_base::in | std::ios_base::out);

        if (!dest.is_open())
        {
            throw std::runtime_error("Failed to open destination file: " + dest_path.string());
        }
        auto format = detail::extract_file_extension(dest_path);
        auto result = sign(format.c_str(), *source, dest, signer);
        return result;
    }

    /// @brief Create a Builder from an archive stream.
    /// @param archive The input stream to read the archive from.
    /// @throws C2pa::C2paException for errors encountered by the C2PA library.
    Builder Builder::from_archive(std::istream &archive)
    {
        return Builder(archive);
    }

    /// @brief Create a Builder from an archive file.
    /// @param archive The input path to read the archive from.
    /// @throws C2pa::C2paException for errors encountered by the C2PA library.
    Builder Builder::from_archive(const std::filesystem::path &archive_path)
    {
        auto path = detail::open_file_binary<std::ifstream>(archive_path);
        return from_archive(*path);
    }

    /// @brief Load an archive into a Builder instance, replacing its current manifest definition.
    /// @param archive The input stream to read the archive from.
    /// @return Reference to this builder for method chaining.
    /// @throws C2pa::C2paException for errors encountered by the C2PA library.
    Builder& Builder::with_archive(std::istream &archive)
    {
        CppIStream c_archive(archive);

        // c2pa_builder_with_archive consumes the builder pointer and returns a new one
        C2paBuilder* updated = c2pa_builder_with_archive(builder, c_archive.c_stream);
        builder = nullptr;
        if (updated == nullptr) {
            throw C2paException();
        }
        builder = updated;
        return *this;
    }

    /// @brief Write the builder to an archive stream.
    /// @param dest The output stream to write the archive to.
    /// @throws C2pa::C2paException for errors encountered by the C2PA library.
    void Builder::to_archive(std::ostream &dest)
    {
        CppOStream c_dest = CppOStream(dest);
        int result = c2pa_builder_to_archive(builder, c_dest.c_stream);
        if (result < 0)
        {
            throw C2paException();
        }
    }

    /// @brief Write the builder to an archive file.
    /// @param dest_path The path to write the archive file to.
    /// @throws C2pa::C2paException for errors encountered by the C2PA library.
    void Builder::to_archive(const std::filesystem::path &dest_path)
    {
        auto dest = detail::open_file_binary<std::ofstream>(dest_path);
        to_archive(*dest);
    }

    std::vector<unsigned char> Builder::data_hashed_placeholder(uintptr_t reserve_size, const std::string &format)
    {
        const unsigned char *c2pa_manifest_bytes = nullptr;
        auto result = c2pa_builder_data_hashed_placeholder(builder, reserve_size, format.c_str(), &c2pa_manifest_bytes);
        return detail::to_byte_vector(c2pa_manifest_bytes, result);
    }

    std::vector<unsigned char> Builder::sign_data_hashed_embeddable(Signer &signer, const std::string &data_hash, const std::string &format, std::istream *asset)
    {
        int64_t result;
        const unsigned char *c2pa_manifest_bytes = nullptr;
        if (asset)
        {
            CppIStream c_asset(*asset);
            result = c2pa_builder_sign_data_hashed_embeddable(builder, signer.c2pa_signer(), data_hash.c_str(), format.c_str(), c_asset.c_stream, &c2pa_manifest_bytes);
        }
        else
        {
            result = c2pa_builder_sign_data_hashed_embeddable(builder, signer.c2pa_signer(), data_hash.c_str(), format.c_str(), nullptr, &c2pa_manifest_bytes);
        }
        return detail::to_byte_vector(c2pa_manifest_bytes, result);
    }

    std::vector<unsigned char> Builder::format_embeddable(const std::string &format, std::vector<unsigned char> &data)
    {
        const unsigned char *c2pa_manifest_bytes = nullptr;
        auto result = c2pa_format_embeddable(format.c_str(), data.data(), data.size(), &c2pa_manifest_bytes);
        return detail::to_byte_vector(c2pa_manifest_bytes, result);
    }

    std::vector<std::string> Builder::supported_mime_types() {
      uintptr_t count = 0;
      auto ptr = c2pa_builder_supported_mime_types(&count);
      return c_mime_types_to_vector(ptr, count);
    }
} // namespace c2pa
