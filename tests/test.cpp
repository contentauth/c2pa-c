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
#include <stdlib.h>
#include <string.h>
#include "../include/c2pa.hpp"

// assert that c2pa_str contains substr or exit
void assert_contains(const char *what, std::string str, const char *substr)
{
    if (strstr(str.c_str(), substr) == NULL)
    {
        fprintf(stderr, "FAILED %s: %s not found in %s\n", what, str.c_str(), substr);
        exit(1);
    }
    printf("PASSED: %s\n", what);
}

using namespace std;

int main()
{
    auto version = C2pa::version();
    assert_contains("C2pa::version", version, "c2pa-c/0.");

    try
    {
        // read a file with a valid manifest
        auto manifest_json = C2pa::read_file("tests/fixtures/C.jpg", "target/tmp");
        if (manifest_json.has_value())
        {
            assert_contains("C2pa::read_file", manifest_json.value(), "C.jpg");
        }
        else
        {
            cout << "Failed: C2pa::read_file_: manifest_json is empty" << endl;
            return (1);
        }
    }
    catch (C2pa::Exception e)
    {
        cout << "Failed: C2pa::read_file_: " << e.what() << endl;
        return (1);
    };

    try
    {
        // read a file with with no manifest and no data_dir
        auto manifest_json2 = C2pa::read_file("tests/fixtures/A.jpg");
        if (manifest_json2.has_value())
        {
            cout << "Failed: C2pa::read_file_no_manifest: manifest_json2 is not empty" << endl;
            return (1);
        }
    }
    catch (C2pa::Exception e)
    {
        cout << "Failed: C2pa::read_file_no_manifest: " << e.what() << endl;
    };

     try
    {
        // read a file with with no manifest and no data_dir
        auto manifest_json2 = C2pa::read_file("tests/fixtures/Z.jpg");
        cout << "Failed: C2pa::read_file_not_found";
        return (1);
    }
    catch (C2pa::Exception e)
    {
        assert_contains("C2pa::read_file_not_found", e.what(), "No such file or directory");
    };
}
