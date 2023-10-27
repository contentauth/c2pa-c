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
#include "../include/c2pa.hpp"

int main() {
    cout << "The C2pa library version is " << C2pa::version() << endl;

    try {
        // read a file with a valid manifest
        cout << "Manifest is " << C2pa::read_file("tests/fixtures/C.jpg", "target/tmp") << endl;

        // read a file without a manifest to throw an exception
        C2pa::C2paString manifest_json = C2pa::read_file("tests/fixtures/A.jpg", "target/tmp");
    }
    catch (C2pa::C2paException e) {
        cout << "Error reading file: " << e.what() << endl;
    }
}
