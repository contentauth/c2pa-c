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

#include <iostream>
#include <string.h>
#include "c2pa.h"
#include <optional>  // C++17
#include <filesystem> // C++17
using path = std::filesystem::path;

#include "file_stream.h"

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
    string version()
    {
        auto result = c2pa_version();
        string str = string(result);
        c2pa_release_string(result);
        return str;
    }

    // Read a file and return the manifest json as a C2pa::String
    // Note: Paths are UTF-8 encoded, use std.filename.u8string().c_str() if needed
    // source_path: path to the file to read
    // data_dir: the directory to store binary resources (optional)
    // Returns a string containing the manifest json if a manifest was found
    // Throws a C2pa::Exception for errors encountered by the C2pa library
    std::optional<string> read_file(const std::filesystem::path& source_path, const std::optional<path> data_dir = std::nullopt)
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
    string read_ingredient_file(const path& source_path, const path& data_dir)
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
    void sign_file(const path& source_path,
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


    // Class for ManifestStoreReader
    class ManifestStoreReader
    {
    private:
        ManifestStore *manifest_store;

    public:

        // Create a ManifestStoreReader from a CStream
        ManifestStoreReader(const char *format, CStream *stream)
        {
            manifest_store = c2pa_manifest_store_read(format, stream);
        }

        // Create a ManifestStoreReader from a file
        ManifestStoreReader(const std::filesystem::path& source_path)
        {
            CStream *stream = open_file_stream(source_path.c_str(), "rb");
            if (stream == NULL)
            {
                throw Exception();
            }
            // get the extension from the source_path to use as the format
            string extension = source_path.extension().string();
            if (!extension.empty()) {
                extension = extension.substr(1);  // Skip the dot
            }
            manifest_store = c2pa_manifest_store_read(extension.c_str(), stream);
            if (manifest_store == NULL)
            {
                throw Exception();
            }
            close_file_stream(stream);
        }

        ~ManifestStoreReader()
        {
            c2pa_release_manifest_store(manifest_store);
        }

        // Return ManifestStore as Json
        // Returns a string containing the manifest store json
        // Throws a C2pa::Exception for errors encountered by the C2pa library
        string json()
        {
            char *result = c2pa_manifest_store_json(manifest_store);
            if (result == NULL)
            {
                throw Exception();
            }
            string str = string(result);
            c2pa_release_string(result);
            return str;
        }

        int get_resource(const char *uri, CStream *stream) {
            int result = c2pa_manifest_store_get_resource(manifest_store, uri, stream);
            if (result < 0)
            {
                throw Exception();
            }
            return result;
        }

        int get_resource(const char *uri, const std::filesystem::path& source_path) {
            CStream *stream = open_file_stream(source_path.c_str(), "wb");
            int result = get_resource(uri, stream);
            close_file_stream(stream);
            return result;
        }
    };  


     // Class for ManifestStoreReader
    class ManifestBuilder
    {
    private:
        ManifestStoreBuilder *builder;

    public:

        // Create a ManifestStoreReader from a CStream
        ManifestBuilder(const char *manifest_json)
        {
            builder = c2pa_manifest_store_builder_from_json(manifest_json);
        }

        ~ManifestBuilder()
        {
            c2pa_manifest_store_builder_release(builder);
        }

        std::vector<unsigned char>* sign(const char* format, CStream *source, CStream *dest, SignerInfo *signer_info) {
            const unsigned char *c2pa_data_bytes = NULL;
            auto result = c2pa_manifest_store_builder_sign(builder, format, source, dest, signer_info, &c2pa_data_bytes);
            if (result < 0)
            {
                throw Exception();
            }
            if (c2pa_data_bytes != NULL) {
                // Allocate a new vector on the heap and fill it with the data
                std::vector<unsigned char>* c2pa_data = new std::vector<unsigned char>(c2pa_data_bytes, c2pa_data_bytes + result);  
                return c2pa_data;
            }
            return nullptr;
        }

        std::vector<unsigned char>* sign(const path& source_path, const path& dest_path, SignerInfo *signer_info) {
            CStream *source = open_file_stream(source_path.c_str(), "rb");
            CStream *dest = open_file_stream(dest_path.c_str(), "wb");
            auto format = dest_path.extension().string();
            if (!format.empty()) {
                format = format.substr(1);  // Skip the dot
            }
            auto result = sign(format.c_str(), source, dest, signer_info);
            close_file_stream(source);
            close_file_stream(dest);
            return result;
        }
    };

}
