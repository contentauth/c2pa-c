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
#include <system_error> // For std::system_error

#include "c2pa.hpp"

using namespace std;
using namespace c2pa;

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
    errno = EINVAL;
    return -1;
  }
  try
  {
    // the context is a pointer to the C++ callback function
    SignerFunc *callback = (SignerFunc *)context;
    std::vector<uint8_t> data_vec(data, data + len);
    std::vector<uint8_t> signature_vec = (callback)(data_vec);
    if (signature_vec.size() > sig_max_len)
    {
      errno = ENOBUFS;
      return -1;
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
    /// C2paException class for C2PA errors.
    /// This class is used to throw exceptions for errors encountered by the C2PA library via c2pa_error().

    C2paException::C2paException()
    {
        auto result = c2pa_error();
        message = string(result);
        c2pa_string_free(result);
    }

    C2paException::C2paException(string what) : message(std::move(what))
    {
    }

    const char *C2paException::what() const noexcept
    {
        return message.c_str();
    }

    // ===== Settings Implementation =====
    
    Settings::Settings() : settings_(c2pa_settings_new()) {
        if (!settings_) {
            throw C2paException("Failed to create settings");
        }
    }
    
    Settings::Settings(const string& data, const string& format) : settings_(c2pa_settings_new()) {
        if (!settings_) {
            throw C2paException("Failed to create settings");
        }
        if (c2pa_settings_update_from_string(settings_, data.c_str(), format.c_str()) != 0) {
            c2pa_free(settings_);
            throw C2paException();
        }
    }
    
    Settings::Settings(Settings&& other) noexcept : settings_(other.settings_) {
        other.settings_ = nullptr;
    }
    
    Settings& Settings::operator=(Settings&& other) noexcept {
        if (this != &other) {
            if (settings_) {
                c2pa_free(settings_);
            }
            settings_ = other.settings_;
            other.settings_ = nullptr;
        }
        return *this;
    }
    
    Settings::~Settings() {
        if (settings_) {
            c2pa_free(settings_);
        }
    }
    
    Settings& Settings::set(const string& path, const string& json_value) {
        if (c2pa_settings_set_value(settings_, path.c_str(), json_value.c_str()) != 0) {
            throw C2paException();
        }
        return *this;
    }
    
    Settings& Settings::update(const string& data, const string& format) {
        if (c2pa_settings_update_from_string(settings_, data.c_str(), format.c_str()) != 0) {
            throw C2paException();
        }
        return *this;
    }
    
    C2paSettings* Settings::c_settings() const noexcept {
        return settings_;
    }
    
    // ===== Context Implementation =====
    
    Context::Context(C2paContext* ctx) : context_(ctx) {
        if (!context_) {
            throw C2paException("Invalid context pointer");
        }
    }
    
    Context::~Context() {
        if (context_) {
            c2pa_free(context_);
        }
    }
    
    C2paContext* Context::c_context() const {
        return context_;
    }
    
    bool Context::has_context() const noexcept {
        return context_ != nullptr;
    }
    
    ContextProviderPtr Context::create() {
        C2paContext* ctx = c2pa_context_new();
        if (!ctx) {
            throw C2paException("Failed to create context");
        }
        return std::make_shared<Context>(ctx);
    }
    
    ContextProviderPtr Context::from_json(const string& json) {
        return ContextCreator().with_json(json).create_context();
    }
    
    ContextProviderPtr Context::from_toml(const string& toml) {
        return ContextCreator().with_toml(toml).create_context();
    }
    
    // ===== Context::Builder Implementation =====
    
    Context::ContextCreator::ContextCreator() : builder_(c2pa_context_builder_new()) {
        if (!builder_) {
            throw C2paException("Failed to create context builder");
        }
    }
    
    Context::ContextCreator::ContextCreator(ContextCreator&& other) noexcept : builder_(other.builder_) {
        other.builder_ = nullptr;
    }
    
    Context::ContextCreator& Context::ContextCreator::operator=(ContextCreator&& other) noexcept {
        if (this != &other) {
            if (builder_) {
                c2pa_free(builder_);
            }
            builder_ = other.builder_;
            other.builder_ = nullptr;
        }
        return *this;
    }
    
    Context::ContextCreator::~ContextCreator() {
        if (builder_) {
            c2pa_free(builder_);
        }
    }
    
    Context::ContextCreator& Context::ContextCreator::with_settings(const Settings& settings) {
        if (c2pa_context_builder_set_settings(builder_, settings.c_settings()) != 0) {
            throw C2paException();
        }
        return *this;
    }
    
    Context::ContextCreator& Context::ContextCreator::with_json(const string& json) {
        Settings settings(json, "json");
        return with_settings(settings);
    }
    
    Context::ContextCreator& Context::ContextCreator::with_toml(const string& toml) {
        Settings settings(toml, "toml");
        return with_settings(settings);
    }
    
    ContextProviderPtr Context::ContextCreator::create_context() {
        if (!builder_) {
            throw C2paException("Builder already consumed");
        }
        C2paContext* ctx = c2pa_context_builder_build(builder_);
        builder_ = nullptr;  // Builder is consumed
        if (!ctx) {
            throw C2paException("Failed to build context");
        }
        return std::make_shared<Context>(ctx);
    }

    /// Returns the version of the C2PA library.
    string version()
    {
        auto result = c2pa_version();
        std::string str(result);
        c2pa_string_free(result);
        return str;
    }

    /// Loads C2PA settings from a string in a given format.
    /// @param data the string to load.
    /// @param format the mime format of the string.
    /// @throws a C2pa::C2paException for errors encountered by the C2PA library.
    void load_settings(const string &data, const string &format)
    {
        auto result = c2pa_load_settings(data.c_str(), format.c_str());
        if (result != 0)
        {
            throw c2pa::C2paException();
        }
    }

    /// converts a path to a string in utf-8 format
    inline std::string path_to_string(const std::filesystem::path &source_path)
    {
		// Use u8string to ensure UTF-8 encoding across platforms. We have to convert
		// to std::string manually because std::string doesn't have a constructor accepting u8String until C++20.
		auto u8_str = source_path.u8string();
        return std::string(u8_str.begin(), u8_str.end());
    }

    /// Reads a file and returns the manifest json as a C2pa::String.
    /// @param source_path the path to the file to read.
    /// @param data_dir the directory to store binary resources (optional).
    /// @return a string containing the manifest json if a manifest was found.
    /// @throws a C2pa::C2paException for errors encountered by the C2PA library.
    optional<string> read_file(const std::filesystem::path &source_path, const optional<std::filesystem::path> data_dir)
    {
        const char* dir_ptr = nullptr;
        std::string dir_str;
        if (data_dir.has_value()) {
            dir_str = path_to_string(data_dir.value());
            dir_ptr = dir_str.c_str();
        }

        char *result = c2pa_read_file(path_to_string(source_path).c_str(), dir_ptr);

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
        c2pa_string_free(result);
        return str;
    }

    /// Reads a file and returns an ingredient JSON as a C2pa::String.
    /// @param source_path the path to the file to read.
    /// @param data_dir the directory to store binary resources.
    /// @return a string containing the ingredient json.
    /// @throws a C2pa::C2paException for errors encountered by the C2PA library.
    string read_ingredient_file(const std::filesystem::path &source_path, const std::filesystem::path &data_dir)
    {
        char *result = c2pa_read_ingredient_file(path_to_string(source_path).c_str(), path_to_string(data_dir).c_str());
        if (result == nullptr)
        {
            throw c2pa::C2paException();
        }
        std::string str(result);
        c2pa_string_free(result);
        return str;
    }

    /// Adds the manifest and signs a file.
    // source_path: path to the asset to be signed
    // dest_path: the path to write the signed file to
    // manifest: the manifest json to add to the file
    // signer_info: the signer info to use for signing
    // data_dir: the directory to store binary resources (optional)
    // Throws a C2pa::C2paException for errors encountered by the C2PA library
    void sign_file(const std::filesystem::path &source_path,
                   const std::filesystem::path &dest_path,
                   const char *manifest,
                   c2pa::SignerInfo *signer_info,
                   const std::optional<std::filesystem::path> data_dir)
    {
        auto dir = data_dir.has_value() ? path_to_string(data_dir.value()) : string();

        char *result = c2pa_sign_file(path_to_string(source_path).c_str(), path_to_string(dest_path).c_str(), manifest, signer_info, dir.c_str());
        if (result == nullptr)
        {
            // NULL means error occurred
            throw c2pa::C2paException();
        }
        // Result contains JSON manifest on success
        c2pa_string_free(result);
    }

    /// IStream Class wrapper for C2paStream.
    CppIStream::~CppIStream()
    {
        c2pa_release_stream(c_stream);
    }

    intptr_t CppIStream::reader(StreamContext *context, uint8_t *buffer, intptr_t size)
    {
        std::istream *istream = (std::istream *)context;
        istream->read((char *)buffer, size);
        if (istream->fail())
        {
            if (!istream->eof())
            {                   // do not report eof as an error
                errno = EINVAL; // Invalid argument
                // std::perror("Error: Invalid argument");
                return -1;
            }
        }
        if (istream->bad())
        {
            errno = EIO; // Input/output error
            // std::perror("Error: Input/output error");
            return -1;
        }
        size_t gcount = istream->gcount();
        return gcount;
    }

    intptr_t CppIStream::seeker(StreamContext *context, intptr_t offset, C2paSeekMode whence)
    {
        std::istream *istream = (std::istream *)context;

        std::ios_base::seekdir dir = std::ios_base::beg;
        switch (whence)
        {
        case C2paSeekMode::Start:
            dir = std::ios_base::beg;
            break;
        case C2paSeekMode::Current:
            dir = std::ios_base::cur;
            break;
        case C2paSeekMode::End:
            dir = std::ios_base::end;
            break;
        };

        istream->clear(); // important: clear any flags
        istream->seekg(offset, dir);
        if (istream->fail())
        {
            errno = EINVAL;
            return -1;
        }
        else if (istream->bad())
        {
            errno = EIO;
            return -1;
        }
        // Use int64_t instead of long to avoid 2GB limit on systems where long is 32-bit
        int64_t pos = static_cast<int64_t>(istream->tellg());
        if (pos < 0)
        {
            errno = EIO;
            return -1;
        }
        // printf("seeker offset= %ld pos = %lld whence = %d\n", offset, (long long)pos, dir);
        return static_cast<intptr_t>(pos);
    }

    intptr_t CppIStream::writer(StreamContext *context, const uint8_t *buffer, intptr_t size)
    {
        std::iostream *stream = (std::iostream *)context;
        stream->write((const char *)buffer, size);
        if (stream->fail())
        {
            errno = EINVAL; // Invalid argument
            return -1;
        }
        else if (stream->bad())
        {
            errno = EIO; // I/O error
            return -1;
        }
        return size;
    }

    intptr_t CppIStream::flusher(StreamContext *context)
    {
        std::iostream *stream = (std::iostream *)context;
        stream->flush();
        if (stream->fail() || stream->bad())
          {
              errno = EIO;
              return -1;
          }
        return 0;
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
        errno = EINVAL; // Invalid argument
        return -1;
    }

    intptr_t CppOStream::seeker(StreamContext *context, intptr_t offset, C2paSeekMode whence)
    {
        std::ostream *ostream = (std::ostream *)context;
        // printf("seeker ofstream = %p\n", ostream);

        if (!ostream)
        {
            errno = EINVAL; // Invalid argument
            return -1;
        }

        std::ios_base::seekdir dir = ios_base::beg;
        switch (whence)
        {
        case C2paSeekMode::Start:
            dir = ios_base::beg;
            break;
        case C2paSeekMode::Current:
            dir = ios_base::cur;
            break;
        case C2paSeekMode::End:
            dir = ios_base::end;
            break;
        };

        ostream->clear(); // Clear any error flags
        ostream->seekp(offset, dir);
        if (ostream->fail())
        {
            errno = EINVAL; // Invalid argument
            return -1;
        }
        else if (ostream->bad())
        {
            errno = EIO; // Input/output error
            return -1;
        }
        // Use int64_t instead of long to avoid 2GB limit on systems where long is 32-bit
        int64_t pos = static_cast<int64_t>(ostream->tellp());
        if (pos < 0)
        {
            errno = EIO; // Input/output error
            return -1;
        }
        return static_cast<intptr_t>(pos);
    }

    intptr_t CppOStream::writer(StreamContext *context, const uint8_t *buffer, intptr_t size)
    {
        std::ostream *ofstream = (std::ostream *)context;
        // printf("writer ofstream = %p\n", ofstream);
        ofstream->write((const char *)buffer, size);
        if (ofstream->fail())
        {
            errno = EINVAL; // Invalid argument
            return -1;
        }
        else if (ofstream->bad())
        {
            errno = EIO; // I/O error
            return -1;
        }
        return size;
    }

    intptr_t CppOStream::flusher(StreamContext *context)
    {
        std::ostream *ofstream = (std::ostream *)context;
        ofstream->flush();
        if (ofstream->fail() || ofstream->bad()) {
            errno = EIO;
            return -1;
        }
        return 0;
    }

    /// IOStream Class wrapper for C2paStream implementation.
    CppIOStream::~CppIOStream()
    {
        c2pa_release_stream(c_stream);
    }

    intptr_t CppIOStream::reader(StreamContext *context, uint8_t *buffer, intptr_t size)
    {
        std::iostream *iostream = (std::iostream *)context;
        iostream->read((char *)buffer, size);
        if (iostream->fail())
        {
            if (!iostream->eof())
            {                   // do not report eof as an error, but as
                errno = EINVAL; // invalid argument instead
                // std::perror("Error: Invalid argument");
                return -1;
            }
        }
        if (iostream->bad())
        {
            errno = EIO; // Input/output error
            // std::perror("Error: Input/output error");
            return -1;
        }
        size_t gcount = iostream->gcount();
        return gcount;
    }

    intptr_t CppIOStream::seeker(StreamContext *context, intptr_t offset, C2paSeekMode whence)
    {
        iostream *iostream = (std::iostream *)context;

        if (!iostream)
        {
            errno = EINVAL; // Invalid argument
            return -1;
        }

        std::ios_base::seekdir dir = std::ios_base::beg;
        switch (whence)
        {
        case C2paSeekMode::Start:
            dir = std::ios_base::beg;
            break;
        case C2paSeekMode::Current:
            dir = std::ios_base::cur;
            break;
        case C2paSeekMode::End:
            dir = std::ios_base::end;
            break;
        };
        // seek for both read and write since rust does it that way

        iostream->clear(); // Clear any error flags
        iostream->seekg(offset, dir);
        if (iostream->fail())
        {
            errno = EINVAL; // Invalid argument
            return -1;
        }
        else if (iostream->bad())
        {
            errno = EIO; // Input/output error
            return -1;
        }
        // Use int64_t instead of long to avoid 2GB limit on systems where long is 32-bit
        int64_t pos = static_cast<int64_t>(iostream->tellg());
        if (pos < 0)
        {
            errno = EIO; // Input/output error
            return -1;
        }

        iostream->seekp(offset, dir);
        if (iostream->fail())
        {
            errno = EINVAL; // Invalid argument
            return -1;
        }
        else if (iostream->bad())
        {
            errno = EIO; // Input/output error
            return -1;
        }
        pos = static_cast<int64_t>(iostream->tellp());
        if (pos < 0)
        {
            errno = EIO; // Input/output error
            return -1;
        }
        return static_cast<intptr_t>(pos);
    }

    intptr_t CppIOStream::writer(StreamContext *context, const uint8_t *buffer, intptr_t size)
    {
        std::iostream *iostream = (std::iostream *)context;
        iostream->write((const char *)buffer, size);
        if (iostream->fail())
        {
            errno = EINVAL; // Invalid argument
            return -1;
        }
        else if (iostream->bad())
        {
            errno = EIO; // I/O error
            return -1;
        }
        return size;
    }

    intptr_t CppIOStream::flusher(StreamContext *context)
    {
        std::iostream *iostream = (std::iostream *)context;
        iostream->flush();
        if (iostream->fail() || iostream->bad())
          {
              errno = EIO;
              return -1;
          }
        return 0;
    }

    /// Reader class for reading a manifest implementation.
    // ===== Context-based Reader constructors =====
    
    Reader::Reader(ContextProviderPtr context, const string &format, std::istream &stream)
        : context_(std::move(context))
    {
        if (!context_ || !context_->has_context()) {
            throw C2paException("Invalid context provider");
        }
        
        c2pa_reader = c2pa_reader_from_context(context_->c_context());
        if (c2pa_reader == nullptr) {
            throw C2paException("Failed to create reader from context");
        }
        
        cpp_stream = new CppIStream(stream);
        // Update reader with stream
        C2paReader* updated = c2pa_reader_with_stream(c2pa_reader, format.c_str(), cpp_stream->c_stream);
        if (updated == nullptr) {
            delete cpp_stream;
            c2pa_reader_free(c2pa_reader);
            throw C2paException("Failed to configure reader with stream");
        }
        c2pa_reader = updated;
    }
    
    Reader::Reader(ContextProviderPtr context, const std::filesystem::path &source_path)
        : context_(std::move(context))
    {
        if (!context_ || !context_->has_context()) {
            throw C2paException("Invalid context provider");
        }
        
        c2pa_reader = c2pa_reader_from_context(context_->c_context());
        if (c2pa_reader == nullptr) {
            throw C2paException("Failed to create reader from context");
        }
        
        std::ifstream file_stream(source_path, std::ios::binary);
        if (!file_stream.is_open()) {
            c2pa_reader_free(c2pa_reader);
            throw std::system_error(errno, std::system_category(), "Failed to open file: " + source_path.string());
        }
        
        string extension = source_path.extension().string();
        if (!extension.empty()) {
            extension = extension.substr(1); // Skip the dot
        }
        
        cpp_stream = new CppIStream(file_stream);
        C2paReader* updated = c2pa_reader_with_stream(c2pa_reader, extension.c_str(), cpp_stream->c_stream);
        if (updated == nullptr) {
            delete cpp_stream;
            c2pa_reader_free(c2pa_reader);
            throw C2paException("Failed to configure reader with stream");
        }
        c2pa_reader = updated;
    }
    
    // ===== Legacy Reader constructors (deprecated) =====
    
    Reader::Reader(const string &format, std::istream &stream)
        : context_(nullptr)
    {
        cpp_stream = new CppIStream(stream); // keep this allocated for life of Reader
        c2pa_reader = c2pa_reader_from_stream(format.c_str(), cpp_stream->c_stream);
        if (c2pa_reader == nullptr)
        {
            delete cpp_stream;
            throw C2paException();
        }
    }

    Reader::Reader(const std::filesystem::path &source_path)
        : context_(nullptr)
    {
		const auto source_str = path_to_string(source_path);
        std::ifstream file_stream(source_str, std::ios_base::binary);
        if (!file_stream.is_open())
        {
            // Use std::system_error for cross-platform error handling
            throw C2paException("Failed to open file: " + source_path.string() + " - " +
                               std::system_error(errno, std::system_category()).what());
        }
        string extension = source_path.extension().string();
        if (!extension.empty())
        {
            extension = extension.substr(1); // Skip the dot
        }

        cpp_stream = new CppIStream(file_stream); // keep this allocated for life of Reader
        c2pa_reader = c2pa_reader_from_stream(extension.c_str(), cpp_stream->c_stream);
        if (c2pa_reader == nullptr)
        {
            delete cpp_stream;
            throw C2paException();
        }
    }

    Reader::~Reader()
    {
        c2pa_reader_free(c2pa_reader);
        if (cpp_stream != nullptr)
        {
            delete cpp_stream;
        }
    }

    string Reader::json() const
    {
        char *result = c2pa_reader_json(c2pa_reader);
        if (result == nullptr)
        {
            throw C2paException();
        }
        std::string str(result);
        c2pa_string_free(result);
        return str;
    }

    [[nodiscard]] std::optional<std::string> Reader::remote_url() const {
        auto url = c2pa_reader_remote_url(c2pa_reader);
        if (url == nullptr) { return nullopt; }
        std::string url_str(url);
        // The C2PA library returns a `const char*` that needs to be released.
        // The underlying `char*` is mutable; however, to indicate the value
        // shouldn't be modified, it's returned as a const char*.
        //
        // TODO: Revisit after determining how we want c2pa-rs to handle
        //       strings that shouldn't be modified by our bindings.
        c2pa_string_free(const_cast<char *>(url));
        return url_str;
    }

    int64_t Reader::get_resource(const string &uri, const std::filesystem::path &path)
    {
		const auto path_str = path_to_string(path);
        std::ofstream file_stream(path_str, std::ios_base::binary);
        if (!file_stream.is_open())
        {
            throw C2paException(); // Handle file open error appropriately
        }
        return get_resource(uri.c_str(), file_stream);
    }

    int64_t Reader::get_resource(const string &uri, std::ostream &stream)
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

    Signer::Signer(SignerFunc *callback, C2paSigningAlg alg, const string &sign_cert, const string &tsa_uri)
    {
        // Pass the C++ callback as a context to our static callback wrapper.
        signer = c2pa_signer_create((const void *)callback, &signer_passthrough, alg, sign_cert.c_str(), tsa_uri.c_str());
    }

    Signer::Signer(const string &alg, const string &sign_cert, const string &private_key, const optional<string> &tsa_uri)
    {
        auto info = C2paSignerInfo { alg.c_str(), sign_cert.c_str(), private_key.c_str(), tsa_uri ? tsa_uri->c_str() : nullptr };
        signer = c2pa_signer_from_info(&info);
    }

    Signer::~Signer()
    {
        c2pa_signer_free(signer);
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

    // ===== Context-based Builder constructors =====
    
    Builder::Builder(ContextProviderPtr context)
        : builder(nullptr), context_(std::move(context))
    {
        if (!context_ || !context_->has_context()) {
            throw C2paException("Invalid context provider");
        }
        
        builder = c2pa_builder_from_context(context_->c_context());
        if (builder == nullptr) {
            throw C2paException("Failed to create builder from context");
        }
    }
    
    Builder::Builder(ContextProviderPtr context, const string &manifest_json)
        : builder(nullptr), context_(std::move(context))
    {
        if (!context_ || !context_->has_context()) {
            throw C2paException("Invalid context provider");
        }
        
        builder = c2pa_builder_from_context(context_->c_context());
        if (builder == nullptr) {
            throw C2paException("Failed to create builder from context");
        }
        
        // Apply the manifest definition
        C2paBuilder* updated = c2pa_builder_with_definition(builder, manifest_json.c_str());
        if (updated == nullptr) {
            c2pa_builder_free(builder);
            throw C2paException("Failed to set builder definition");
        }
        builder = updated;
    }
    
    // ===== Legacy Builder constructors (deprecated) =====
    
    Builder::Builder(const string &manifest_json)
        : builder(nullptr), context_(nullptr)
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
    Builder::Builder(istream &archive)
        : builder(nullptr), context_(nullptr)
    {
        CppIStream c_archive = CppIStream(archive);
        builder = c2pa_builder_from_archive(c_archive.c_stream);
        if (builder == nullptr)
        {
            throw C2paException();
        }
    }

    Builder::~Builder()
    {
        c2pa_builder_free(builder);
    }

    C2paBuilder *Builder::c2pa_builder() const noexcept
    {
        return builder;
    }

    void Builder::set_no_embed()
    {
        c2pa_builder_set_no_embed(builder);
    }

    void Builder::set_remote_url(const string &remote_url)
    {
        int result = c2pa_builder_set_remote_url(builder, remote_url.c_str());
        if (result < 0)
        {
            throw C2paException();
        }
    }
    
    Builder& Builder::with_definition(const string &manifest_json)
    {
        C2paBuilder* updated = c2pa_builder_with_definition(builder, manifest_json.c_str());
        if (updated == nullptr) {
            throw C2paException("Failed to set builder definition");
        }
        builder = updated;
        return *this;
    }

    void Builder::set_base_path(const string &base_path)
    {
        int result = c2pa_builder_set_base_path(builder, base_path.c_str());
        if (result < 0)
        {
            throw C2paException();
        }
    }

    void Builder::add_resource(const string &uri, istream &source)
    {
        CppIStream c_source = CppIStream(source);
        int result = c2pa_builder_add_resource(builder, uri.c_str(), c_source.c_stream);
        if (result < 0)
        {
            throw C2paException();
        }
    }

    void Builder::add_resource(const string &uri, const std::filesystem::path &source_path)
    {
		const auto source_str = path_to_string(source_path);
        ifstream stream = ifstream(source_str, std::ios_base::binary);
        if (!stream.is_open())
        {
            throw std::runtime_error("Failed to open source file: " + source_path.string());
        }
        add_resource(uri, stream);
    }

    void Builder::add_ingredient(const string &ingredient_json, const string &format, istream &source)
    {
        CppIStream c_source = CppIStream(source);
        int result = c2pa_builder_add_ingredient_from_stream(builder, ingredient_json.c_str(), format.c_str(), c_source.c_stream);
        if (result < 0)
        {
            throw C2paException();
        }
    }

    void Builder::add_ingredient(const string &ingredient_json, const std::filesystem::path &source_path)
    {
		const auto source_str = path_to_string(source_path);
        ifstream stream = ifstream(source_str, std::ios_base::binary);
        if (!stream.is_open())
        {
            throw std::runtime_error("Failed to open source file: " + source_path.string());
        }
        auto format = source_path.extension().string();
        if (!format.empty())
        {
            format = format.substr(1); // Skip the dot.
        }
        add_ingredient(ingredient_json, format.c_str(), stream);
    }

    void Builder::add_action(const string &action_json)
    {
        int result = c2pa_builder_add_action(builder, action_json.c_str());
        if (result < 0)
        {
            throw C2paException();
        }
    }

    std::vector<unsigned char> Builder::sign(const string &format, istream &source, ostream &dest, Signer &signer)
    {
        CppIStream c_source(source);
        CppOStream c_dest(dest);
        const unsigned char *c2pa_manifest_bytes = nullptr;
        auto result = c2pa_builder_sign(builder, format.c_str(), c_source.c_stream, c_dest.c_stream, signer.c2pa_signer(), &c2pa_manifest_bytes);
        if (result < 0 || c2pa_manifest_bytes == nullptr)
        {
          throw C2paException();
        }

        auto manifest_bytes = std::vector<unsigned char>(c2pa_manifest_bytes, c2pa_manifest_bytes + result);
        c2pa_manifest_bytes_free(c2pa_manifest_bytes);
        return manifest_bytes;
    }

    std::vector<unsigned char> Builder::sign(const string &format, istream &source, iostream &dest, Signer &signer)
    {
        CppIStream c_source(source);
        CppIOStream c_dest(dest);
        const unsigned char *c2pa_manifest_bytes = nullptr;
        auto result = c2pa_builder_sign(builder, format.c_str(), c_source.c_stream, c_dest.c_stream, signer.c2pa_signer(), &c2pa_manifest_bytes);
        if (result < 0 || c2pa_manifest_bytes == nullptr)
        {
            throw C2paException();
        }

        auto manifest_bytes = std::vector<unsigned char>(c2pa_manifest_bytes, c2pa_manifest_bytes + result);
        c2pa_manifest_bytes_free(c2pa_manifest_bytes);
        return manifest_bytes;
    }

    /// @brief Sign a file and write the signed data to an output file.
    /// @param source_path The path to the file to sign.
    /// @param dest_path The path to write the signed file to.
    /// @param signer A signer object to use when signing.
    /// @return A vector containing the signed manifest bytes.
    /// @throws C2pa::C2paException for errors encountered by the C2PA library.
    std::vector<unsigned char> Builder::sign(const std::filesystem::path &source_path, const std::filesystem::path &dest_path, Signer &signer)
    {
		const auto source_str = path_to_string(source_path);
        std::ifstream source(source_str, std::ios_base::binary);
        if (!source.is_open())
        {
            throw std::runtime_error("Failed to open source file: " + source_path.string());
        }
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
        auto format = dest_path.extension().string();
        if (!format.empty())
        {
            format = format.substr(1); // Skip the dot
        }
        auto result = sign(format.c_str(), source, dest, signer);
        return result;
    }

    /// @brief Create a Builder from an archive stream.
    /// @param archive The input stream to read the archive from.
    /// @throws C2pa::C2paException for errors encountered by the C2PA library.
    Builder Builder::from_archive(istream &archive)
    {
        return Builder(archive);
    }

    /// @brief Create a Builder from an archive file.
    /// @param archive The input path to read the archive from.
    /// @throws C2pa::C2paException for errors encountered by the C2PA library.
    Builder Builder::from_archive(const std::filesystem::path &archive_path)
    {
		const auto archive_str = path_to_string(archive_path);
        std::ifstream path(archive_str, std::ios_base::binary);
        if (!path.is_open())
        {
            throw std::runtime_error("Failed to open archive file: " + archive_path.string());
        }
        return from_archive(path);
    }

    /// @brief Write the builder to an archive stream.
    /// @param dest The output stream to write the archive to.
    /// @throws C2pa::C2paException for errors encountered by the C2PA library.
    void Builder::to_archive(ostream &dest)
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
		const auto dest_str = path_to_string(dest_path);
        std::ofstream dest(dest_str, std::ios_base::binary);
        if (!dest.is_open())
        {
            throw std::runtime_error("Failed to open destination file: " + dest_path.string());
        }
        to_archive(dest);
    }

    std::vector<unsigned char> Builder::data_hashed_placeholder(uintptr_t reserve_size, const string &format)
    {
        const unsigned char *c2pa_manifest_bytes = nullptr;
        auto result = c2pa_builder_data_hashed_placeholder(builder, reserve_size, format.c_str(), &c2pa_manifest_bytes);
        if (result < 0 || c2pa_manifest_bytes == nullptr)
        {
            throw(C2paException());
        }

        auto data = std::vector<unsigned char>(c2pa_manifest_bytes, c2pa_manifest_bytes + result);
        c2pa_manifest_bytes_free(c2pa_manifest_bytes);
        return data;
    }

    std::vector<unsigned char> Builder::sign_data_hashed_embeddable(Signer &signer, const string &data_hash, const string &format, istream *asset)
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
        if (result < 0 || c2pa_manifest_bytes == nullptr)
        {
            throw(C2paException());
        }

        auto data = std::vector<unsigned char>(c2pa_manifest_bytes, c2pa_manifest_bytes + result);
        c2pa_manifest_bytes_free(c2pa_manifest_bytes);
        return data;
    }

    std::vector<unsigned char> Builder::format_embeddable(const string &format, std::vector<unsigned char> &data)
    {
        const unsigned char *c2pa_manifest_bytes = nullptr;
        auto result = c2pa_format_embeddable(format.c_str(), data.data(), data.size(), &c2pa_manifest_bytes);
        if (result < 0 || c2pa_manifest_bytes == nullptr)
        {
            throw(C2paException());
        }

        auto formatted_data = std::vector<unsigned char>(c2pa_manifest_bytes, c2pa_manifest_bytes + result);
        c2pa_manifest_bytes_free(c2pa_manifest_bytes);
        return formatted_data;
    }

    std::vector<std::string> Builder::supported_mime_types() {
      uintptr_t count = 0;
      auto ptr = c2pa_builder_supported_mime_types(&count);
      return c_mime_types_to_vector(ptr, count);
    }
} // namespace c2pa
