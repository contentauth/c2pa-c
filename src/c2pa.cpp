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
///          This is an early version, and has not been fully tested.
///          Thread safety is not guaranteed due to the use of errno and etc.

#include <fstream>
#include <iostream>
#include <string.h>
#include <optional>   // C++17
#include <filesystem> // C++17

#include "c2pa.hpp"
#include "cpp_io_stream.h"

using path = std::filesystem::path;

using namespace std;
using namespace c2pa;

namespace c2pa
{
    /// Exception class for C2PA errors.
    /// This class is used to throw exceptions for errors encountered by the C2PA library via c2pa_error().

    Exception::Exception() : message(c2pa_error())
    {
        auto result = c2pa_error();
        message = string(result);
        c2pa_release_string(result);
    }

    Exception::Exception(string what) : message(what)
    {
    }

    const char *Exception::what() const throw()
    {
        return message.c_str();
    }

    /// Returns the version of the C2PA library.
    string version()
    {
        auto result = c2pa_version();
        string str = string(result);
        c2pa_release_string(result);
        return str;
    }

    /// Loads C2PA settings from a string in a given format.
    /// @param format the mime format of the string.
    /// @param data the string to load.
    /// @throws a C2pa::Exception for errors encountered by the C2PA library.
    void load_settings(const string &format, const string &data)
    {
        auto result = c2pa_load_settings(format.c_str(), data.c_str());
        if (result != 0)
        {
            throw c2pa::Exception();
        }
    }

    /// Reads a file and returns the manifest json as a C2pa::String.
    /// Note: Paths are UTF-8 encoded, use std.filename.u8string().c_str() if needed.
    /// @param source_path the path to the file to read.
    /// @param data_dir the directory to store binary resources (optional).
    /// @return a string containing the manifest json if a manifest was found.
    /// @throws a C2pa::Exception for errors encountered by the C2PA library.
    optional<string> read_file(const filesystem::path &source_path, const optional<path> data_dir)
    {
        const char *dir = data_dir.has_value() ? data_dir.value().c_str() : nullptr;
        char *result = c2pa_read_file(source_path.c_str(), dir);
        if (result == nullptr)
        {
            auto exception = c2pa::Exception();
            if (strstr(exception.what(), "ManifestNotFound") != NULL)
            {
                return std::nullopt;
            }
            throw c2pa::Exception();
        }
        string str = string(result);
        c2pa_release_string(result);
        return str;
    }

    /// Reads a file and returns an ingredient JSON as a C2pa::String.
    /// @param source_path the path to the file to read.
    /// @param data_dir the directory to store binary resources.
    /// @return a string containing the ingredient json.
    /// @throws a C2pa::Exception for errors encountered by the C2PA library.
    string read_ingredient_file(const path &source_path, const path &data_dir)
    {
        char *result = c2pa_read_ingredient_file(source_path.c_str(), data_dir.c_str());
        if (result == NULL)
        {
            throw c2pa::Exception();
        }
        string str = string(result);
        c2pa_release_string(result);
        return str;
    }

    /// Adds the manifest and signs a file.
    // source_path: path to the asset to be signed
    // dest_path: the path to write the signed file to
    // manifest: the manifest json to add to the file
    // signer_info: the signer info to use for signing
    // data_dir: the directory to store binary resources (optional)
    // Throws a C2pa::Exception for errors encountered by the C2PA library
    void sign_file(const path &source_path,
                   const path &dest_path,
                   const char *manifest,
                   c2pa::SignerInfo *signer_info,
                   const std::optional<path> data_dir)
    {
        const char *dir = data_dir.has_value() ? data_dir.value().c_str() : NULL;
        char *result = c2pa_sign_file(source_path.c_str(), dest_path.c_str(), manifest, signer_info, dir);
        if (result == NULL)
        {

            throw c2pa::Exception();
        }
        c2pa_release_string(result);
        return;
    }

    /// IStream Class wrapper for CStream.
    template <typename IStream>
    CppIStream::CppIStream(IStream &istream) : CStream()
    {
        // assert(std::is_base_of<std::istream, IStream>::value, "Stream must be derived from std::istream");

        c_stream = c2pa_create_stream(reinterpret_cast<StreamContext *>(&istream), (ReadCallback)reader, (SeekCallback)seeker, (WriteCallback)writer, (FlushCallback)flusher);
    }

    CppIStream::~CppIStream()
    {
        c2pa_release_stream(c_stream);
    }

    size_t CppIStream::reader(StreamContext *context, void *buffer, size_t size)
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

    long CppIStream::seeker(StreamContext *context, long int offset, int whence)
    {
        std::istream *istream = (std::istream *)context;

        std::ios_base::seekdir dir = std::ios_base::beg;
        switch (whence)
        {
        case SEEK_SET:
            dir = std::ios_base::beg;
            break;
        case SEEK_CUR:
            dir = std::ios_base::cur;
            break;
        case SEEK_END:
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
        int pos = istream->tellg();
        if (pos < 0)
        {
            errno = EIO;
            return -1;
        }
        // printf("seeker offset= %ld pos = %d whence = %d\n", offset, pos, dir);
        return pos;
    }

    int CppIStream::writer(StreamContext *context, const void *buffer, int size)
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

    int CppIStream::flusher(StreamContext *context)
    {
        std::iostream *iostream = (std::iostream *)context;
        iostream->flush();
        return 0;
    }

    /// Ostream Class wrapper for CStream implementation.

    template <typename OStream>
    CppOStream::CppOStream(OStream &ostream) : CStream()
    {
        static_assert(std::is_base_of<std::ostream, OStream>::value, "Stream must be derived from std::ostream");
        c_stream = c2pa_create_stream(reinterpret_cast<StreamContext *>(&ostream), (ReadCallback)reader, (SeekCallback)seeker, (WriteCallback)writer, (FlushCallback)flusher);
    }

    CppOStream::~CppOStream()
    {
        c2pa_release_stream(c_stream);
    }

    size_t CppOStream::reader(StreamContext *context, void *buffer, size_t size)
    {
        errno = EINVAL; // Invalid argument
        return -1;
    }

    long CppOStream::seeker(StreamContext *context, long int offset, int whence)
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
        case SEEK_SET:
            dir = ios_base::beg;
            break;
        case SEEK_CUR:
            dir = ios_base::cur;
            break;
        case SEEK_END:
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
        int pos = ostream->tellp();
        if (pos < 0)
        {
            errno = EIO; // Input/output error
            return -1;
        }
        return pos;
    }

    int CppOStream::writer(StreamContext *context, const void *buffer, int size)
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

    int CppOStream::flusher(StreamContext *context)
    {
        std::ofstream *ofstream = (std::ofstream *)context;
        ofstream->flush();
        return 0;
    }

    /// IOStream Class wrapper for CStream implementation.
    template <typename IOStream>
    CppIOStream::CppIOStream(IOStream &iostream)
    {
        static_assert(std::is_base_of<std::iostream, IOStream>::value, "Stream must be derived from std::iostream");
        c_stream = c2pa_create_stream(reinterpret_cast<StreamContext *>(&iostream), (ReadCallback)reader, (SeekCallback)seeker, (WriteCallback)writer, (FlushCallback)flusher);
    }
    CppIOStream::~CppIOStream()
    {
        c2pa_release_stream(c_stream);
    }

    size_t CppIOStream::reader(StreamContext *context, void *buffer, size_t size)
    {
        std::iostream *iostream = (std::iostream *)context;
        iostream->read((char *)buffer, size);
        if (iostream->fail())
        {
            if (!iostream->eof())
            {                   // do not report eof as an error
                errno = EINVAL; // Invalid argument
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

    long CppIOStream::seeker(StreamContext *context, long int offset, int whence)
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
        case SEEK_SET:
            dir = std::ios_base::beg;
            break;
        case SEEK_CUR:
            dir = std::ios_base::cur;
            break;
        case SEEK_END:
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
        int pos = iostream->tellg();
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
        pos = iostream->tellp();
        if (pos < 0)
        {
            errno = EIO; // Input/output error
            return -1;
        }
        return pos;
    }

    int CppIOStream::writer(StreamContext *context, const void *buffer, int size)
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

    int CppIOStream::flusher(StreamContext *context)
    {
        std::iostream *iostream = (std::iostream *)context;
        iostream->flush();
        return 0;
    }

    /// Reader class for reading a manifest implementation.
    Reader::Reader(const string &format, std::istream &stream)
    {
        cpp_stream = new CppIStream(stream); // keep this allocated for life of Reader
        c2pa_reader = c2pa_reader_from_stream(format.c_str(), cpp_stream->c_stream);
        if (c2pa_reader == NULL)
        {
            throw Exception();
        }
    }

    Reader::Reader(const std::filesystem::path &source_path)
    {
        std::ifstream file_stream(source_path, std::ios::binary);
        if (!file_stream.is_open())
        {
            throw Exception("Failed to open file: " + source_path.string() + " - " + std::strerror(errno));
        }
        string extension = source_path.extension().string();
        if (!extension.empty())
        {
            extension = extension.substr(1); // Skip the dot
        }

        cpp_stream = new CppIStream(file_stream); // keep this allocated for life of Reader
        c2pa_reader = c2pa_reader_from_stream(extension.c_str(), cpp_stream->c_stream);
        if (c2pa_reader == NULL)
        {
            throw Exception();
        }
    }

    Reader::~Reader()
    {
        c2pa_reader_free(c2pa_reader);
        if (cpp_stream != NULL)
        {
            delete cpp_stream;
        }
    }

    string Reader::json()
    {
        char *result = c2pa_reader_json(c2pa_reader);
        if (result == NULL)
        {
            throw Exception();
        }
        string str = string(result);
        c2pa_release_string(result);
        return str;
    }

    int Reader::get_resource(const string &uri, const std::filesystem::path &path)
    {
        std::ofstream file_stream(path, std::ios::binary);
        if (!file_stream.is_open())
        {
            throw Exception(); // Handle file open error appropriately
        }
        return get_resource(uri.c_str(), file_stream);
    }

    int Reader::get_resource(const string &uri, std::ostream &stream)
    {
        CppOStream cpp_stream(stream);
        int result = c2pa_reader_resource_to_stream(c2pa_reader, uri.c_str(), cpp_stream.c_stream);
        if (result < 0)
        {
            throw Exception();
        }
        return result;
    }

    intptr_t signer_passthrough(const void *context, const unsigned char *data, uintptr_t len, unsigned char *signature, uintptr_t sig_max_len)
    {
        try
        {
            // the context is a pointer to the C++ callback function
            SignerFunc *callback = (SignerFunc *)context;
            std::vector<uint8_t> data_vec(data, data + len);
            std::vector<uint8_t> signature_vec = (callback)(data_vec);
            if (signature_vec.size() > sig_max_len)
            {
                return -1;
            }
            std::copy(signature_vec.begin(), signature_vec.end(), signature);
            return signature_vec.size();
        }
        catch (std::exception const &e)
        {
            // todo pass exceptions to Rust error handling
            // printf("Error: signer_passthrough - %s\n", e.what());
            return -1;
        }
        catch (...)
        {
            // printf("Error: signer_passthrough - unknown exception\n");
            return -1;
        }
    }

    Signer::Signer(SignerFunc *callback, C2paSigningAlg alg, const string &sign_cert, const string &tsa_uri)
    {
        // Pass the C++ callback as a context to our static callback wrapper.
        signer = c2pa_signer_create((const void *)callback, &signer_passthrough, alg, sign_cert.c_str(), tsa_uri.c_str());
    }

    Signer::~Signer()
    {
        c2pa_signer_free(signer);
    }

    /// @brief  Get the C2paSigner
    C2paSigner *Signer::c2pa_signer()
    {
        return signer;
    }

    /// @brief  Builder class for creating a manifest implementation.
    Builder::Builder(const string &manifest_json)
    {
        builder = c2pa_builder_from_json(manifest_json.c_str());
        if (builder == NULL)
        {
            throw Exception();
        }
    }

    /// @brief Create a Builder from an archive.
    /// @param archive  The input stream to read the archive from.
    /// @throws C2pa::Exception for errors encountered by the C2PA library.
    Builder::Builder(istream &archive)
    {
        CppIStream c_archive = CppIStream(archive);
        builder = c2pa_builder_from_archive(c_archive.c_stream);
        if (builder == NULL)
        {
            throw Exception();
        }
    }

    Builder::~Builder()
    {
        c2pa_builder_free(builder);
    }

    void Builder::set_no_embed() {
        c2pa_builder_set_no_embed(builder);
    }

    void Builder::set_remote_url(const string &remote_url)
    {
        int result = c2pa_builder_set_remote_url(builder, remote_url.c_str());
        if (result < 0)
        {
            throw Exception();
        }
    }

    void Builder::add_resource(const string &uri, istream &source)
    {
        CppIStream c_source = CppIStream(source);
        int result = c2pa_builder_add_resource(builder, uri.c_str(), c_source.c_stream);
        if (result < 0)
        {
            throw Exception();
        }
    }

    void Builder::add_resource(const string &uri, const std::filesystem::path &source_path)
    {
        ifstream stream = ifstream(source_path);
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
            throw Exception();
        }
    }

    void Builder::add_ingredient(const string &ingredient_json, const std::filesystem::path &source_path)
    {
        ifstream stream = ifstream(source_path);
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

    std::unique_ptr<std::vector<unsigned char>> Builder::sign(const string &format, istream &source, ostream &dest, Signer &signer)
    {
        CppIStream c_source = CppIStream(source);
        CppOStream c_dest = CppOStream(dest);
        const unsigned char *c2pa_manifest_bytes = NULL; // TODO: Make returning manifest bytes optional.
        auto result = c2pa_builder_sign(builder, format.c_str(), c_source.c_stream, c_dest.c_stream, signer.c2pa_signer(), &c2pa_manifest_bytes);
        if (result < 0)
        {
            throw Exception();
        }
        if (c2pa_manifest_bytes != NULL)
        {
            // Allocate a new vector on the heap and fill it with the data.
            auto manifest_bytes = std::make_unique<std::vector<unsigned char>>(c2pa_manifest_bytes, c2pa_manifest_bytes + result);

            c2pa_manifest_bytes_free(c2pa_manifest_bytes);
            return manifest_bytes;
        }
        return nullptr;
    }

    /// @brief Sign a file and write the signed data to an output file.
    /// @param source_path The path to the file to sign.
    /// @param dest_path The path to write the signed file to.
    /// @param signer A signer object to use when signing.
    /// @return A vector containing the signed manifest bytes.
    /// @throws C2pa::Exception for errors encountered by the C2PA library.
    std::unique_ptr<std::vector<unsigned char>> Builder::sign(const path &source_path, const path &dest_path, Signer &signer)
    {
        std::ifstream source(source_path, std::ios::binary);
        if (!source.is_open())
        {
            throw std::runtime_error("Failed to open source file: " + source_path.string());
        }
        // Ensure the destination directory exists
        std::filesystem::path dest_dir = dest_path.parent_path();
        if (!std::filesystem::exists(dest_dir))
        {
            std::filesystem::create_directories(dest_dir);
        }
        std::ofstream dest(dest_path, std::ios::binary);
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
    /// @throws C2pa::Exception for errors encountered by the C2PA library.
    Builder Builder::from_archive(istream &archive)
    {
        return Builder(archive);
    }

    /// @brief Create a Builder from an archive file.
    /// @param archive The input path to read the archive from.
    /// @throws C2pa::Exception for errors encountered by the C2PA library.
    Builder Builder::from_archive(const path &archive_path)
    {
        std::ifstream path(archive_path, std::ios::binary);
        if (!path.is_open())
        {
            throw std::runtime_error("Failed to open archive file: " + archive_path.string());
        }
        return from_archive(path);
    }

    /// @brief Write the builder to an archive stream.
    /// @param dest The output stream to write the archive to.
    /// @throws C2pa::Exception for errors encountered by the C2PA library.
    void Builder::to_archive(ostream &dest)
    {
        CppOStream c_dest = CppOStream(dest);
        int result = c2pa_builder_to_archive(builder, c_dest.c_stream);
        if (result < 0)
        {
            throw Exception();
        }
    }

    /// @brief Write the builder to an archive file.
    /// @param dest_path The path to write the archive file to.
    /// @throws C2pa::Exception for errors encountered by the C2PA library.
    void Builder::to_archive(const path &dest_path)
    {
        std::ofstream dest(dest_path, std::ios::binary);
        if (!dest.is_open())
        {
            throw std::runtime_error("Failed to open destination file: " + dest_path.string());
        }
        to_archive(dest);
    }

}