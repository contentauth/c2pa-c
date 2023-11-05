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
#include <cstring>
#include "../include/c2pa.hpp"

// assert that c2pa_str contains substr or exit
void assert_contains(const char *what, C2pa::String *c2pa_str, const char *substr)
{
    if (strstr(c2pa_str->c_str(), substr) == NULL)
    {
        fprintf(stderr, "FAILED %s: %s not found in %s\n", what, c2pa_str->c_str(), substr);
        exit(1);
    }
    printf("PASSED: %s\n", what);
}

using namespace std;

int main()
{
    auto version = C2pa::version();
    assert_contains("C2pa::version", &version, "c2pa-c/0.");

    try
    {
        // read a file with a valid manifest
        auto manifest_json = C2pa::read_file("tests/fixtures/C.jpg", "target/tmp");
        assert_contains("C2pa::read_file", &manifest_json, "C.jpg");
    }
    catch (C2pa::Exception e)
    {
        cout << "Failed: C2pa::read_file_: " << e.what() << endl;
        return (1);
    };
}
