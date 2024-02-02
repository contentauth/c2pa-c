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
mod c_signer;
mod c_stream;
mod error;
mod json_api;
mod signer;
mod signer_info;

pub use c2pa::{ManifestStore, ManifestStoreBuilder};
pub use c_stream::*;
pub use error::{Error, Result};
pub use json_api::*;
pub use signer_info::SignerInfo;
