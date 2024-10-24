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

mod c_api;
/// This module exports a C2PA library
mod c_stream;
mod error;
mod json_api;
mod signer_info;

pub use c2pa::{
    AsyncSigner, Builder, Error as C2paError, Reader, Result as C2paResult, Signer, SigningAlg,
};
pub use c_api::*;
pub use c_stream::*;
pub use error::{Error, Result};
pub use json_api::{read_file, read_ingredient_file, sdk_version, sign_file};
pub use signer_info::SignerInfo;

use uniffi;
uniffi::include_scaffolding!("c2pa_c");

pub fn version() -> String {
  format!("{}/{}", env!("CARGO_PKG_NAME"), env!("CARGO_PKG_VERSION"),)
}
