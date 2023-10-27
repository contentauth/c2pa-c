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
    json_api::{add_manifest_to_file_json, ingredient_from_file_json, read_from_file_json},
    signer_info::SignerInfo,
};

//use serde::Serialize;

// Internal routine to convert a *const c_char to a rust String
unsafe fn from_c_str(s: *const c_char) -> String {
    CStr::from_ptr(s).to_string_lossy().into_owned()
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

// convert a Result into JSON result string as *mut c_char
// The returned value MUST be released by calling release_string
// and it is no longer valid after that call.
// unsafe fn result_to_c_string<T: Serialize>(result: Result<T>) -> *mut c_char {
//     to_c_string(Response::from_result(result).to_string())
// }

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

/// Returns a JSON array of supported file format extensions
///
/// # Safety
/// The returned value MUST be released by calling release_string
/// and it is no longer valid after that call.
#[no_mangle]
pub unsafe extern "C" fn c2pa_supported_formats() -> *mut c_char {
    let formats = "[\"jpeg\"]".to_string();
    to_c_string(formats)
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
    let path = if path.is_null() {
        Error::set_last(Error::NullParameter("path".to_string()));
        return std::ptr::null_mut();
    } else {
        from_c_str(path)
    };
    let data_dir = if data_dir.is_null() {
        None
    } else {
        Some(from_c_str(data_dir))
    };
    let result = read_from_file_json(&path, data_dir);
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
pub unsafe extern "C" fn c2pa_ingredient_from_file(
    path: *const c_char,
    data_dir: *const c_char,
) -> *mut c_char {
    // convert C pointers into Rust
    let path = if path.is_null() {
        Error::set_last(Error::NullParameter("path".to_string()));
        return std::ptr::null_mut();
    } else {
        from_c_str(path)
    };
    let data_dir = if data_dir.is_null() {
        Error::set_last(Error::NullParameter("data_dir is NULL".to_string()));
        return std::ptr::null_mut();
    } else {
        from_c_str(data_dir)
    };

    let result = ingredient_from_file_json(&path, &data_dir);

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
    /// The public certificate chain in PEM format
    pub signcert: *const c_char,
    /// The private key in PEM format
    pub pkey: *const c_char,
    /// The signing algorithm
    pub alg: *const c_char,
    /// The timestamp authority URL or NULL
    pub tsa_url: *const c_char,
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
pub unsafe extern "C" fn c2pa_add_manifest_to_file(
    source_path: *const c_char,
    dest_path: *const c_char,
    manifest: *const c_char,
    signer_info: C2paSignerInfo,
    data_dir: *const c_char,
) -> *mut c_char {
    // convert C pointers into Rust
    let source_path = from_c_str(source_path);
    let dest_path = from_c_str(dest_path);
    let manifest = from_c_str(manifest);
    let data_dir = if data_dir.is_null() {
        None
    } else {
        Some(from_c_str(data_dir))
    };
    let signer_info = SignerInfo {
        signcert: from_c_str(signer_info.signcert).into_bytes(),
        pkey: from_c_str(signer_info.pkey).into_bytes(),
        alg: from_c_str(signer_info.alg),
        tsa_url: if signer_info.tsa_url.is_null() {
            None
        } else {
            Some(from_c_str(signer_info.tsa_url))
        },
    };
    // Read manifest from JSON and then sign and write it
    let result =
        add_manifest_to_file_json(&source_path, &dest_path, &manifest, signer_info, data_dir);

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
