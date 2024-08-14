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

// This shows that the c2pa_c library can export the Rust API
// could reexport the c_api and add other rust specific features.

#[test]
fn test_reader() {
    let mut stream = std::fs::File::open("tests/fixtures/C.jpg").unwrap();
    let reader = c2pa_c::Reader::from_stream("image/jpeg", &mut stream).unwrap();
    println!("{}", reader.json());
    assert!(reader.validation_status().is_none())
}
