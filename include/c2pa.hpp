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
#include "c2pa.h"

namespace C2pa
{
    using namespace std;

    // C++ wrapper for Rust strings with destructor to release memory
    class String
    {
    private:
        char *str;

        // This should only be called by the C2pa library with Rust Strings
        String(char *rust_str)
        {
            str = rust_str;
        }

        // These functions are friends of the String class so they can access the private constructor
        friend String version();
        friend String read_file(const char *filename, const char *data_dir);
        friend String read_ingredient_from_file(const char *filename, const char *data_dir);
        friend String sign_file(const char *source_path,
                                const char *dest_path,
                                const char *manifest,
                                struct C2paSignerInfo signer_info,
                                const char *data_dir);

    public:
        ~String()
        {
            c2pa_release_string(str);
        }

        char *c_str() const
        {
            return str;
        }
        string toString() const
        {
            return string((*this).str);
        }
        friend ostream &operator<<(ostream &os, const String &s)
        {
            os << s.str;
            return os;
        }
    };

    // Exception class for C2pa errors
    class Exception : public exception
    {
    private:
        char *message;

    public:
        Exception() : message(c2pa_error()) {}

        virtual const char *what() const throw()
        {
            return message;
        }
    };

    // Return the version of the C2pa library
    String version()
    {
        return String(c2pa_version());
    }

    // Read a file and return the manifest json as a C2pa::String
    // filename: the name of the file to read
    // data_dir: the directory to store binary resources (can be NULL)
    // Returns a C2pa::String containing the manifest json
    // Throws a C2pa::Exception for errors encountered by the C2pa library
    String read_file(const char *filename, const char *data_dir)
    {
        char *result = c2pa_read_file(filename, data_dir);
        if (result == NULL)
        {
            throw Exception();
        }
        return String(result);
    }

    // Read a file and return an ingredient json as a C2pa::String
    // filename: the name of the file to read
    // data_dir: the directory to store binary resources
    // Returns a C2pa::String containing the manifest json
    // Throws a C2pa::Exception for errors encountered by the C2pa library
    String read_ingredient_from_file(const char *filename, const char *data_dir)
    {
        char *result = c2pa_ingredient_from_file(filename, data_dir);
        if (result == NULL)
        {
            throw Exception();
        }
        return String(result);
    }

    // Add the manifest and sign a file
    // filename: the name of the source file to sign
    // dest_path: the path to write the signed file
    // manifest: the manifest json to add to the file
    // signer_info: the signer info to use for signing
    // data_dir: the directory to store binary resources (can be NULL)
    // Returns a C2pa::String containing the manifest binary
    // Throws a C2pa::Exception for errors encountered by the C2pa library
    String sign_file(const char *source_path,
                     const char *dest_path,
                     const char *manifest,
                     struct C2paSignerInfo signer_info,
                     const char *data_dir)
    {
        char *result = c2pa_sign_file(source_path, dest_path, manifest, signer_info, data_dir);
        if (result == NULL)
        {
            throw Exception();
        }
        return String(result);
    }

}
