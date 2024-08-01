// Copyright 2023 Adobe. All rights reserved.
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
#pragma warning(disable: 4505)
#endif

#include <fstream>
#include <iostream>
#include <optional>  // C++17
#include <filesystem> // C++17

#include "c2pa.h"

using path = std::filesystem::path;

namespace c2pa
{
    using namespace std;

    typedef C2paSignerInfo SignerInfo;

    // Exception class for C2pa errors
    class Exception : public exception
    {
    private:
        string message;

    public:
        Exception() : message(c2pa_error()) {
            auto result = c2pa_error();
            message = string(result);
            c2pa_release_string(result);
        }

        virtual const char *what() const throw()
        {
            return message.c_str();
        }
    };

    // Return the version of the C2pa library
    static string version()
    {
        auto result = c2pa_version();
        string str = string(result);
        c2pa_release_string(result);
        return str;
    }

    /// Load c2pa settings from a string in a given format
    /// @param format the mime format of the string
    /// @param data the string to load
    static void load_settings(const string format, const string data)
    {
        c2pa_load_settings(format.c_str(), data.c_str());
    }   

    // Read a file and return the manifest json as a C2pa::String
    // Note: Paths are UTF-8 encoded, use std.filename.u8string().c_str() if needed
    // source_path: path to the file to read
    // data_dir: the directory to store binary resources (optional)
    // Returns a string containing the manifest json if a manifest was found
    // Throws a C2pa::Exception for errors encountered by the C2pa library
    static std::optional<string> read_file(const std::filesystem::path& source_path, const std::optional<path> data_dir = std::nullopt)
    {
        const char* dir = data_dir.has_value() ? data_dir.value().c_str() : NULL;
        char *result = c2pa_read_file(source_path.c_str(), dir);
        if (result == NULL)
        {   
            auto exception = Exception();
            if (strstr(exception.what(), "ManifestNotFound") != NULL)
            {
                return std::nullopt;
            }
            throw Exception();
        }
        string str = string(result);
        c2pa_release_string(result);
        return str;
    }

    // Read a file and return an ingredient json as a C2pa::String
    // source_path: path to the file to read
    // data_dir: the directory to store binary resources
    // Returns a string containing the manifest json
    // Throws a C2pa::Exception for errors encountered by the C2pa library
    static string read_ingredient_file(const path& source_path, const path& data_dir)
    {
        char *result = c2pa_read_ingredient_file(source_path.c_str(), data_dir.c_str());
        if (result == NULL)
        {
            throw Exception();
        }
        string str = string(result);
        c2pa_release_string(result);
        return str;
    }

    // Add the manifest and sign a file
    // source_path: path to the asset to be signed
    // dest_path: the path to write the signed file to
    // manifest: the manifest json to add to the file
    // signer_info: the signer info to use for signing
    // data_dir: the directory to store binary resources (optional)
    // Throws a C2pa::Exception for errors encountered by the C2pa library
    static void sign_file(const path& source_path,
                     const path& dest_path,
                     const char *manifest,
                     SignerInfo *signer_info,
                     const std::optional<path> data_dir = std::nullopt)
    {
        const char* dir = data_dir.has_value() ? data_dir.value().c_str() : NULL;
        char *result = c2pa_sign_file(source_path.c_str(), dest_path.c_str(), manifest, signer_info, dir);
        if (result == NULL)
        {

            throw Exception();
        }
        c2pa_release_string(result);
        return;
    }

    // IStream Class wrapper for CStream
    class CppIStream : public CStream
    {
    public:
        CStream *c_stream;

        template <typename IStream>
        CppIStream(IStream& istream) {
            static_assert(std::is_base_of<std::istream, IStream>::value, "Stream must be derived from std::istream");
            c_stream = c2pa_create_stream(reinterpret_cast<StreamContext*>(&istream), (ReadCallback)reader, (SeekCallback) seeker, (WriteCallback)writer, (FlushCallback)flusher);
        }

        ~CppIStream() {
            c2pa_release_stream(c_stream);
        }
    private:
        static size_t reader(StreamContext *context, void *buffer, size_t size) {
            std::istream *istream = (std::istream *)context;
            istream->read((char *)buffer, size);
            if (istream->fail()) {
                if (!istream->eof()) {  // do not report eof as an error
                    errno = EINVAL; // Invalid argument
                    //std::perror("Error: Invalid argument");
                    return -1;
                }
            } 
            if (istream->bad()) {
                errno = EIO; // Input/output error
                //std::perror("Error: Input/output error");
                return -1;
            } 
            size_t gcount = istream->gcount();
            return gcount;
        }

        static long seeker(StreamContext *context, long int offset, int whence) { 
            std::istream *istream = (std::istream *)context;
            
            std::ios_base::seekdir dir = std::ios_base::beg;
            switch (whence) {   
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
            if (istream->fail()) {
                errno = EINVAL;
                return -1;
            } else if (istream->bad()) {
                errno = EIO;
                return -1;
            } 
            int pos = istream->tellg();
            if (pos < 0) {
                errno = EIO;
                return -1;
            }
            // printf("seeker offset= %ld pos = %d whence = %d\n", offset, pos, dir);
            return pos;
        }

        static int writer(StreamContext *context, const void *buffer, int size) {
            std::iostream *iostream = (std::iostream *)context;
            iostream->write((const char *)buffer, size);
            if (iostream->fail()) {
                errno = EINVAL;  // Invalid argument
                return -1;
            } else if (iostream->bad()) {
                errno = EIO;  // I/O error
                return -1;
            }
            return size;
        }

        static int flusher(StreamContext *context) {
            std::iostream *iostream = (std::iostream *)context;
            iostream->flush();
            return 0;
        }
    };

    // Ostream Class wrapper for CStream
    class CppOStream : public CStream
    {
    public:
        CStream *c_stream;

        template <typename OStream>
        CppOStream(OStream& ostream) {
            static_assert(std::is_base_of<std::ostream, OStream>::value, "Stream must be derived from std::ostream");
            c_stream = c2pa_create_stream(reinterpret_cast<StreamContext*>(&ostream), (ReadCallback)reader, (SeekCallback) seeker, (WriteCallback)writer, (FlushCallback)flusher);
        }

        ~CppOStream() {
            c2pa_release_stream(c_stream);
        }
    private:
        static size_t reader(StreamContext *context, void *buffer, size_t size) {
            errno = EINVAL; // Invalid argument
            return -1;
        }

        static long seeker(StreamContext *context, long int offset, int whence) { 
            std::ostream *ostream = (std::ostream *)context;
            //printf("seeker ofstream = %p\n", ostream);

            if (!ostream) {
                errno = EINVAL; // Invalid argument
                return -1;
            }

            std::ios_base::seekdir dir = ios_base::beg;
            switch (whence) {   
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
            if (ostream->fail()) {
                errno = EINVAL; // Invalid argument
                return -1;
            } else if (ostream->bad()) {
                errno = EIO; // Input/output error
                return -1;
            }
            int pos = ostream->tellp();
            if (pos < 0) {
                errno = EIO; // Input/output error
                return -1;
            }
            return pos; 
        }

        static int writer(StreamContext *context, const void *buffer, int size) {
            std::ostream *ofstream = (std::ostream *)context;
            //printf("writer ofstream = %p\n", ofstream);
            ofstream->write((const char *)buffer, size);
            if (ofstream->fail()) {
                errno = EINVAL;  // Invalid argument
                return -1;
            } else if (ofstream->bad()) {
                errno = EIO;  // I/O error
                return -1;
            }
            return size;
        }

        static int flusher(StreamContext *context) {
            std::ofstream *ofstream = (std::ofstream *)context;
            ofstream->flush();
            return 0;
        }
    };


    // IOStream Class wrapper for CStream
    class CppIOStream : public CStream
    {
    public:
        CStream *c_stream;

        template <typename IOStream>
        CppIOStream(IOStream& iostream) {
            static_assert(std::is_base_of<std::iostream, IOStream>::value, "Stream must be derived from std::iostream");
            c_stream = c2pa_create_stream(reinterpret_cast<StreamContext*>(&iostream), (ReadCallback)reader, (SeekCallback) seeker, (WriteCallback)writer, (FlushCallback)flusher);
        }
        ~CppIOStream() {
            c2pa_release_stream(c_stream);
        }
    private:
        static size_t reader(StreamContext *context, void *buffer, size_t size) {
            std::iostream *iostream = (std::iostream *)context;
            iostream->read((char *)buffer, size);
            if (iostream->fail()) {
                if (!iostream->eof()) {  // do not report eof as an error
                    errno = EINVAL; // Invalid argument
                    //std::perror("Error: Invalid argument");
                    return -1;
                }
            } 
            if (iostream->bad()) {
                errno = EIO; // Input/output error
                //std::perror("Error: Input/output error");
                return -1;
            } 
            size_t gcount = iostream->gcount();
            return gcount;
        }

        static long seeker(StreamContext *context, long int offset, int whence) { 
            iostream *iostream = (std::iostream *)context;

            if (!iostream) {
                errno = EINVAL; // Invalid argument
                return -1;
            }

            std::ios_base::seekdir dir = std::ios_base::beg;
            switch (whence) {   
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
            if (iostream->fail()) {
                errno = EINVAL; // Invalid argument
                return -1;
            } else if (iostream->bad()) {
                errno = EIO; // Input/output error
                return -1;
            }
            int pos = iostream->tellg();
            if (pos < 0) {
                errno = EIO; // Input/output error
                return -1;
            }

            iostream->seekp(offset, dir);
            if (iostream->fail()) {
                errno = EINVAL; // Invalid argument
                return -1;
            } else if (iostream->bad()) {
                errno = EIO; // Input/output error
                return -1;
            }
            pos = iostream->tellp();
            if (pos < 0) {
                errno = EIO; // Input/output error
                return -1;
            }
            return pos; 
        }

        static int writer(StreamContext *context, const void *buffer, int size) {
            std::iostream *iostream = (std::iostream *)context;
            iostream->write((const char *)buffer, size);
            if (iostream->fail()) {
                errno = EINVAL;  // Invalid argument
                return -1;
            } else if (iostream->bad()) {
                errno = EIO;  // I/O error
                return -1;
            }
            return size;
        }

        static int flusher(StreamContext *context) {
            std::iostream *iostream = (std::iostream *)context;
            iostream->flush();
            return 0;
        }
    };

    /// @brief Reader class for reading a manifest
    /// @details This class is used to read and validate a manifest from a stream or file
    class Reader
    {
  
    public:
        C2paReader *c2pa_reader;
        CppIStream *cpp_stream = NULL;
 
        /// @brief Create a Reader from a stream
        /// @details The validation_status field in the json contains validation results
        /// @param format the mime format of the stream
        /// @param stream the input stream to read from
        /// @throws C2pa::Exception for errors encountered by the C2pa library
        Reader(const string format, std::istream& stream)
        {
            cpp_stream = new CppIStream(stream); // keep this allocated for life of Reader
            c2pa_reader = c2pa_reader_from_stream(format.c_str(), cpp_stream->c_stream);
            if (c2pa_reader == NULL)
            {
                throw Exception();
            }
        }

        /// @brief Create a Reader from a file path
        /// @param source_path  the path to the file to read
        /// @throws C2pa::Exception for errors encountered by the C2pa library
        Reader(const std::filesystem::path& source_path) {
            std::ifstream file_stream(source_path, std::ios::binary);
            if (!file_stream.is_open()) {
                throw Exception(); // Handle file open error appropriately
            }
            string extension = source_path.extension().string();
            if (!extension.empty()) {
                extension = extension.substr(1);  // Skip the dot
            }
            *this = Reader(extension, file_stream);
        }

        ~Reader()
        {
            c2pa_reader_free(c2pa_reader);
            if (cpp_stream != NULL) {
                delete cpp_stream;
            }   
        }

        /// @brief Get the manifest as a json string
        /// @return the manifest as a json string
        /// @throws C2pa::Exception for errors encountered by the C2pa library
        string json()
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

        /// @brief  Get a resource from the reader and write it to a file
        /// @param uri the uri of the resource
        /// @param path the path to write the resource to
        /// @return the number of bytes written
        /// @throws C2pa::Exception for errors encountered by the C2pa library
        int get_resource(const string uri, const std::filesystem::path& path) {
            std::ofstream file_stream(path, std::ios::binary);
            if (!file_stream.is_open()) {
                throw Exception(); // Handle file open error appropriately
            }
            return get_resource(uri.c_str(), file_stream);
        }
        

        /// @brief  Get a resource from the reader  and write it to an output stream
        /// @param uri the uri of the resource
        /// @param stream the output stream to write the resource to  
        /// @return the number of bytes written
        /// @throws C2pa::Exception for errors encountered by the C2pa library
        int get_resource(const string uri, std::ostream& stream) {
            CppOStream cpp_stream(stream);
            int result = c2pa_reader_resource_to_stream(c2pa_reader, uri.c_str(), cpp_stream.c_stream);
            if (result < 0)
            {
                throw Exception();
            }
            return result;
        }
    };  

    /// @brief  Signer Callback function type
    /// @param  data  the data to sign
    /// @return the signature as a vector of bytes
    /// @details This function type is used to create a callback function for signing
    using SignerFunc = std::vector<unsigned char> (const std::vector<unsigned char>&);


    // this static callback interfaces with the C level library allowing C++ to use vectors
    static intptr_t signer_passthrough(const void *context, const unsigned char *data, uintptr_t len, unsigned char *signature, uintptr_t sig_max_len) {
        // the context is a pointer to the C++ callback function
        SignerFunc *callback = (SignerFunc *)context;
        std::vector<uint8_t> data_vec(data, data + len);
        std::vector<uint8_t> signature_vec = (callback)(data_vec);
        if (signature_vec.size() > sig_max_len) {
            return -1;
        }
        std::copy(signature_vec.begin(), signature_vec.end(), signature);
        return signature_vec.size();
    }

    /// @brief  Signer class for creating a signer
    /// @details This class is used to create a signer from a signing algorithm, certificate, and TSA URI
    class Signer
    {
    private:
       C2paSigner *signer;

    public:

        /// @brief Create a Signer from a callback function, signing algorithm, certificate, and TSA URI
        /// @param callback  the callback function to use for signing
        /// @param alg  the signing algorithm to use
        /// @param sign_cert the certificate to use for signing
        /// @param tsa_uri  the TSA URI to use for timestamping
        Signer(SignerFunc *callback, C2paSigningAlg alg, const string sign_cert, const string tsa_uri)
        {
            // pass the C++ callback as a context to our static callback wrapper
            signer = c2pa_signer_create((const void *)callback, &signer_passthrough, alg, sign_cert.c_str(), tsa_uri.c_str());
        }

        ~Signer()
        {
            c2pa_signer_free(signer);
        }

        /// @brief  Get the C2paSigner
        C2paSigner *c2pa_signer() {
            return signer;
        }  
    };

    /// @brief Builder class for creating a manifest
    /// @details This class is used to create a manifest from a json string and add resources and ingredients to the manifest
    class Builder
    {
    private:
       C2paBuilder *builder;

    public:

        /// @brief  Create a Builder from a manifest json string
        /// @param manifest_json  the manifest json string
        /// @throws C2pa::Exception for errors encountered by the C2pa library
        Builder(const string manifest_json)
        {
            builder = c2pa_builder_from_json(manifest_json.c_str());
            if (builder == NULL)
            {
                throw Exception();
            }
        }

    
        /// @brief Create a Builder from an archive
        /// @param archive  the input stream to read the archive from
        /// @throws C2pa::Exception for errors encountered by the C2pa library
        Builder(istream& archive) {
            CppIStream c_archive = CppIStream(archive);
            builder = c2pa_builder_from_archive(c_archive.c_stream);
            if (builder == NULL)
            {
                throw Exception();
            }
        }

        // /// @brief Create a Builder from an archive
        // /// @param archive_path  the path to the archive file
        // /// @throws C2pa::Exception for errors encountered by the C2pa library
        // Builder(const std::filesystem::path& archive_path) {
        //     ifstream stream = ifstream(archive_path);
        //     if (!stream.is_open()) {
        //         throw Exception(); // Handle file open error appropriately
        //     }
        //     *this = Builder(stream);
        // }

        ~Builder()
        {
            c2pa_builder_free(builder);
        }

        /// @brief  Add a resource to the builder
        /// @param uri  the uri of the resource
        /// @param source  the input stream to read the resource from
        /// @throws C2pa::Exception for errors encountered by the C2pa library
        void add_resource(const string uri, istream& source) {
            CppIStream c_source = CppIStream(source);
            int result = c2pa_builder_add_resource(builder, uri.c_str(), c_source.c_stream);
            if (result < 0)
            {
                throw Exception();
            }
        }

        /// @brief  Add a resource to the builder
        /// @param uri  the uri of the resource
        /// @param source_path  the path to the resource file
        /// @throws C2pa::Exception for errors encountered by the C2pa library
        void add_resource(const string uri, const std::filesystem::path& source_path) {
            ifstream stream = ifstream(source_path);
            if (!stream.is_open()) {
                throw Exception(); // Handle file open error appropriately
            }
            add_resource(uri, stream);
        }    

        /// @brief Add an ingredient to the builder
        /// @param ingredient_json  Any fields of the ingredient you want to define
        /// @param format  the format of the ingredient file
        /// @param source  the input stream to read the ingredient from
        /// @throws C2pa::Exception for errors encountered by the C2pa library
        void add_ingredient(const string ingredient_json, const string format, istream& source) {
            CppIStream c_source = CppIStream(source);
            int result = c2pa_builder_add_ingredient(builder, ingredient_json.c_str(), format.c_str(), c_source.c_stream);
            if (result < 0)
            {
                throw Exception();
            }
        }

        /// @brief Add an ingredient to the builder
        /// @param ingredient_json  Any fields of the ingredient you want to define   
        /// @param source_path  the path to the ingredient file
        /// @throws C2pa::Exception for errors encountered by the C2pa library
        void add_ingredient(const string ingredient_json, const std::filesystem::path& source_path) {
            ifstream stream = ifstream(source_path);
            if (!stream.is_open()) {
                throw Exception(); // Handle file open error appropriately
            }
            auto format = source_path.extension().string();
            if (!format.empty()) {
                format = format.substr(1);  // Skip the dot
            }
            add_ingredient(ingredient_json, format.c_str(), stream);
        }    

        /// @brief Sign an input stream and write the signed data to an output stream
        /// @param format the format of the output stream
        /// @param source the input stream to sign
        /// @param dest the output stream to write the signed data to
        /// @param signer  
        /// @return a vector containing the signed manifest bytes
        /// @throws C2pa::Exception for errors encountered by the C2pa library
        std::vector<const unsigned char>* sign(const string format, istream& source, ostream& dest , Signer& signer) {
            CppIStream c_source = CppIStream(source);
            CppOStream c_dest = CppOStream(dest);
            const unsigned char *c2pa_manifest_bytes = NULL; // todo: make returning manifest bytes optional
            auto result = c2pa_builder_sign(builder, format.c_str(), c_source.c_stream, c_dest.c_stream, signer.c2pa_signer(), &c2pa_manifest_bytes);
            if (result < 0)
            {
                throw Exception();
            }
            if (c2pa_manifest_bytes != NULL) {
                // Allocate a new vector on the heap and fill it with the data
                std::vector<const unsigned char>* manifest_bytes = new std::vector<const unsigned char>(c2pa_manifest_bytes, c2pa_manifest_bytes + result);
                c2pa_manifest_bytes_free(c2pa_manifest_bytes);
                return manifest_bytes;
            }
            return nullptr;
        }

        /// @brief Sign a file and write the signed data to an output file
        /// @param source_path the path to the file to sign
        /// @param dest_path the path to write the signed file to
        /// @param signer A signer object to use when signing
        /// @return a vector containing the signed manifest bytes
        /// @throws C2pa::Exception for errors encountered by the C2pa library
        std::vector<const unsigned char>* sign(const path& source_path, const path& dest_path, Signer& signer) {
            std::ifstream source(source_path, std::ios::binary);
            std::ofstream dest(dest_path, std::ios::binary);
            if (!source.is_open() || !dest.is_open()) {
                throw Exception(); // Handle file open error appropriately
            }
            auto format = dest_path.extension().string();
            if (!format.empty()) {
                format = format.substr(1);  // Skip the dot
            }
            auto result = sign(format.c_str(), source, dest, signer);
            return result;
        }

        // @brief write the builder to an archive stream
        // @param dest the output stream to write the archive to
        // @throws C2pa::Exception for errors encountered by the C2pa library
        void to_archive(ostream& dest) {
            CppOStream c_dest = CppOStream(dest);
            int result = c2pa_builder_to_archive(builder, c_dest.c_stream);
            if (result < 0)
            {
                throw Exception();
            }
        }

        /// @brief write the builder to an archive file
        /// @param dest_path the path to write the archive file to
        /// @throws C2pa::Exception for errors encountered by the C2pa library
        void to_archive(const path& dest_path) {
            std::ofstream dest(dest_path, std::ios::binary);
            if (!dest.is_open()) {
                throw Exception(); // Handle file open error appropriately
            }
            to_archive(dest);
        }
        


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