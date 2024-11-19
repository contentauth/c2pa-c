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

use core::panic;
use std::io::Cursor;

use c2pa::{CallbackSigner, SigningAlg};

const CERTS: &[u8] = include_bytes!("../tests/fixtures/ed25519.pub");
const PRIVATE_KEY: &[u8] = include_bytes!("../tests/fixtures/ed25519.pem");

#[test]
fn test_reader() {
    let mut stream = std::fs::File::open("tests/fixtures/C.jpg").unwrap();
    let reader = c2pa_c::Reader::from_stream("image/jpeg", &mut stream).unwrap();
    println!("{}", reader.json());
    assert!(reader.validation_status().is_none())
}

#[test]
fn test_builder_remote_url() {
    let ed_signer =
        |_context: *const (), data: &[u8]| CallbackSigner::ed25519_sign(data, PRIVATE_KEY);
    let signer = CallbackSigner::new(ed_signer, SigningAlg::Ed25519, CERTS);
    let mut source = std::fs::File::open("tests/fixtures/A.jpg").unwrap();
    let manifest_json = std::fs::read_to_string("tests/fixtures/training.json").unwrap();
    let mut builder = c2pa::Builder::from_json(&manifest_json).unwrap();
    // very important to use a URL that does not exist, otherwise we may get a JumbfParseError or JumbfNotFound
    builder.set_remote_url("http://this_does_not_exist/foo.jpg");
    builder.set_no_embed(true);
    let mut output = Cursor::new(Vec::new());
    let _c2pa_data = builder
        .sign(&signer, "image/jpeg", &mut source, &mut output)
        .unwrap();
    output.set_position(0);
    let result = c2pa_c::Reader::from_stream("image/jpeg", &mut output);
    if let Err(c2pa::Error::RemoteManifestFetch(url)) = result {
        assert_eq!(url, "http://this_does_not_exist/foo.jpg");
    } else {
        panic!("Expected RemoteManifestFetch error");
    }
}
