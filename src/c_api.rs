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

use std::{
    ffi::{CStr, CString},
    os::raw::c_char,
};

use crate::{
    error::Error,
    json_api::{read_file, read_ingredient_file, sign_file},
    signer_info::SignerInfo,
};

// Internal routine to convert a *const c_char to a rust String
unsafe fn from_c_str(s: *const c_char) -> String {
    CStr::from_ptr(s).to_string_lossy().into_owned()
}

// Internal routine to convert a *const c_char to a rust String or return a null error
macro_rules! from_cstr_null_check {
    ($ptr : expr) => {
        if $ptr.is_null() {
            Error::set_last(Error::NullParameter(stringify!($ptr).to_string()));
            return std::ptr::null_mut();
        } else {
            from_c_str($ptr)
        }
    };
}

// Internal routine to convert a *const c_char to Option<String>
macro_rules! from_cstr_option {
    ($ptr : expr) => {
        if $ptr.is_null() {
            None
        } else {
            Some(from_c_str($ptr))
        }
    };
}

// Internal routine to return a rust String reference to C as *mut c_char
// The returned value MUST be released by calling release_string
// and it is no longer valid after that call.
unsafe fn to_c_string(s: String) -> *mut c_char {
    match CString::new(s) {
        Ok(c_str) => c_str.into_raw(),
        Err(_) => std::ptr::null_mut(),
    }
}

/// Returns a version string for logging
///
/// # Safety
/// The returned value MUST be released by calling release_string
/// and it is no longer valid after that call.
#[no_mangle]
pub unsafe extern "C" fn c2pa_version() -> *mut c_char {
    let version = format!(
        "{}/{} {}/{}",
        env!("CARGO_PKG_NAME"),
        env!("CARGO_PKG_VERSION"),
        c2pa::NAME,
        c2pa::VERSION
    );
    to_c_string(version)
}

/// Returns the last error message
///
/// # Safety
/// The returned value MUST be released by calling release_string
/// and it is no longer valid after that call.
#[no_mangle]
pub unsafe extern "C" fn c2pa_error() -> *mut c_char {
    to_c_string(Error::last_message().unwrap_or_default())
}

/// Returns a ManifestStore JSON string from a file path.
/// Any thumbnails or other binary resources will be written to data_dir if provided
///
/// # Errors
/// Returns NULL if there were errors, otherwise returns a JSON string
/// The error string can be retrieved by calling c2pa_error
///
/// # Safety
/// Reads from null terminated C strings
/// The returned value MUST be released by calling release_string
/// and it is no longer valid after that call.
#[no_mangle]
pub unsafe extern "C" fn c2pa_read_file(
    path: *const c_char,
    data_dir: *const c_char,
) -> *mut c_char {
    let path = from_cstr_null_check!(path);
    let data_dir = from_cstr_option!(data_dir);

    let result = read_file(&path, data_dir);

    match result {
        Ok(json) => to_c_string(json),
        Err(e) => {
            e.set_last();
            std::ptr::null_mut()
        }
    }
}

/// Returns an Ingredient JSON string from a file path.
/// Any thumbnail or c2pa data will be written to data_dir if provided
///
/// # Errors
/// Returns NULL if there were errors, otherwise returns a JSON string
/// containing the Ingredient
/// The error string can be retrieved by calling c2pa_error
///
/// # Safety
/// Reads from null terminated C strings
/// The returned value MUST be released by calling release_string
/// and it is no longer valid after that call.
#[no_mangle]
pub unsafe extern "C" fn c2pa_read_ingredient_file(
    path: *const c_char,
    data_dir: *const c_char,
) -> *mut c_char {
    let path = from_cstr_null_check!(path);
    let data_dir = from_cstr_null_check!(data_dir);

    let result = read_ingredient_file(&path, &data_dir);

    match result {
        Ok(json) => to_c_string(json),
        Err(e) => {
            e.set_last();
            std::ptr::null_mut()
        }
    }
}

#[repr(C)]
/// Defines the configuration for a Signer
///
/// The signer is created from the signcert and pkey fields.
/// an optional url to an RFC 3161 compliant time server will ensure the signature is timestamped
///
pub struct C2paSignerInfo {
    /// The signing algorithm
    pub alg: *const c_char,
    /// The public certificate chain in PEM format
    pub sign_cert: *const c_char,
    /// The private key in PEM format
    pub private_key: *const c_char,
    /// The timestamp authority URL or NULL
    pub ta_url: *const c_char,
}

/// Add a signed manifest to the file at path using auth_token
/// If cloud is true, upload the manifest to the cloud
///
/// # Errors
/// Returns an error field if there were errors
///
/// # Safety
/// Reads from null terminated C strings
/// The returned value MUST be released by calling release_string
/// and it is no longer valid after that call.
#[no_mangle]
pub unsafe extern "C" fn c2pa_sign_file(
    source_path: *const c_char,
    dest_path: *const c_char,
    manifest: *const c_char,
    signer_info: &C2paSignerInfo,
    data_dir: *const c_char,
) -> *mut c_char {
    // convert C pointers into Rust
    let source_path = from_cstr_null_check!(source_path);
    let dest_path = from_cstr_null_check!(dest_path);
    let manifest = from_cstr_null_check!(manifest);
    let data_dir = from_cstr_option!(data_dir);

    let signer_info = SignerInfo {
        alg: from_cstr_null_check!(signer_info.alg),
        sign_cert: from_cstr_null_check!(signer_info.sign_cert).into_bytes(),
        private_key: from_cstr_null_check!(signer_info.private_key).into_bytes(),
        ta_url: from_cstr_option!(signer_info.ta_url),
    };
    // Read manifest from JSON and then sign and write it
    let result = sign_file(&source_path, &dest_path, &manifest, &signer_info, data_dir);

    match result {
        Ok(_c2pa_data) => to_c_string("".to_string()),
        Err(e) => {
            e.set_last();
            std::ptr::null_mut()
        }
    }
}

/// Releases a string allocated by Rust
///
/// # Safety
/// Reads from null terminated C strings
/// The string must not have been modified in C
/// can only be released once and is invalid after this call
#[no_mangle]
pub unsafe extern "C" fn c2pa_release_string(s: *mut c_char) {
    if s.is_null() {
        return;
    }
    let _release = CString::from_raw(s);
}
