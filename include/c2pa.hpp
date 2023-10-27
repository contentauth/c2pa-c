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

using namespace std;

namespace C2pa
{
    // C++ wrapper for Rust strings with destructor to release memory
    class C2paString {
    private:
        char* str;

    public:
        C2paString(char* s) {
            str = s;
        }
        ~C2paString() {
            c2pa_release_string(str);
        }
        char *c_str() const {
            return str;
        }
        std::string toString() const {
            return std::string((*this).str);
        }
        friend ostream& operator<<(ostream& os, const C2paString& s) {
            os  << s.str;
            return os;
        }
    };

    // Exception class for C2pa errors
    class C2paException : public std::exception {
    private:
        char * message;

    public:
        C2paException() : message(c2pa_error()) {}

        virtual const char *what() const throw() {
            return message;
        }
    };

    // Return the version of the C2pa library
    C2paString version() {
        return C2paString(c2pa_version());
    }

    // Read a file and return the manifest json as a C2paString
    // filename: the name of the file to read
    // data_dir: the directory to store binary resources (can be NULL)
    // Returns a C2paString containing the manifest json
    // Throws a C2paException for errors encountered by the C2pa library
    C2paString read_file(const char* filename, const char* data_dir) {
        char * result = c2pa_read_file(filename, data_dir);
        if (result == NULL) {
            throw C2pa::C2paException();
        }
        return C2paString(result);
    }
}
