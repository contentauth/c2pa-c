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

namespace C2pa
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
}
