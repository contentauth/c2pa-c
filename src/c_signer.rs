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

use std::ffi::c_char;

use crate::{
    from_cstr_null_check, from_cstr_option,
    signer::{C2paSigner, SignerCallback, SignerConfig},
    Error,
};

#[repr(C)]
/// Defines the configuration for a Signer
///
/// # Example
/// ```
/// use c2pa::SignerConfig;
/// let config = SignerConfig {
///    alg: "Rs256".to_string(),
///    certs: vec![vec![0; 10]],
///    time_authority_url: Some("http://example.com".to_string()),
///    use_ocsp: true,
/// };
pub struct CSignerConfig {
    /// Returns the algorithm of the Signer.
    pub alg: *const c_char,

    /// Returns the certificates as a Vec containing a Vec of DER bytes for each certificate.
    pub certs: *const c_char,

    /// URL for time authority to time stamp the signature
    pub time_authority_url: *const c_char,

    /// Try to fetch OCSP response for the signing cert if available
    pub use_ocsp: bool,
}

#[repr(C)]
/// A C2paSignerCallback defines a signer in C to be called from Rust
#[derive(Debug)]
struct CSigner {
    signer: CSignerCallback,
}

impl SignerCallback for CSigner {
    fn sign(&self, data: Vec<u8>) -> c2pa::Result<Vec<u8>> {
        //println!("SignerCallback signing {:p} {}",self, data.len());
        // We must preallocate the signature buffer to the maximum size
        // so that it can be filled by the callback
        let sig_max_size = 100000;
        let mut signature = vec![0; sig_max_size];

        // This callback returns the size of the signature, if negative it means there was an error
        let sig: *mut u8 = signature.as_ptr() as *mut u8;
        let result = unsafe {
            (self.signer)(
                data.as_ptr() as *mut u8,
                data.len(),
                sig,
                sig_max_size as isize,
            )
        };
        if result < 0 {
            // todo: return errors from callback
            return Err(c2pa::Error::CoseSignature);
        }
        signature.truncate(result as usize);

        Ok(signature)
    }
}

/// Defines a callback to sign data
type CSignerCallback = unsafe extern "C" fn(
    data: *mut u8,
    len: usize,
    signature: *mut u8,
    sig_max_size: isize,
) -> isize;

#[no_mangle]
pub unsafe extern "C" fn c2pa_create_signer(
    signer: CSignerCallback,
    config: &CSignerConfig,
) -> *mut C2paSigner {
    let config = SignerConfig {
        alg: from_cstr_null_check!(config.alg).to_lowercase(),
        certs: from_cstr_null_check!(config.certs).into_bytes(),
        time_authority_url: from_cstr_option!(config.time_authority_url),
        use_ocsp: config.use_ocsp,
    };
    let callback = Box::new(CSigner { signer });
    let signer = C2paSigner::new(callback);
    match signer.configure(&config) {
        Ok(_) => Box::into_raw(Box::new(signer)),
        Err(e) => {
            Error::from_c2pa_error(e).set_last();
            std::ptr::null_mut()
        }
    }
}
