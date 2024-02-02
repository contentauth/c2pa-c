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
    ffi::CString,
    os::raw::{c_char, c_int, c_uchar},
};

use c2pa::{ManifestStore, ManifestStoreBuilder};

use crate::{
    c_stream::CStream,
    error::Error,
    json_api::{read_file, read_ingredient_file, sign_file},
    signer_info::SignerInfo,
};

// Internal routine to convert a *const c_char to a rust String or return a null error
#[macro_export]
macro_rules! from_cstr_null_check {
    ($ptr : expr) => {
        if $ptr.is_null() {
            Error::set_last(Error::NullParameter(stringify!($ptr).to_string()));
            return std::ptr::null_mut();
        } else {
            std::ffi::CStr::from_ptr($ptr)
                .to_string_lossy()
                .into_owned()
        }
    };
}
#[macro_export]
macro_rules! from_cstr_null_check_int {
    ($ptr : expr) => {
        if $ptr.is_null() {
            Error::set_last(Error::NullParameter(stringify!($ptr).to_string()));
            return -1;
        } else {
            std::ffi::CStr::from_ptr($ptr)
                .to_string_lossy()
                .into_owned()
        }
    };
}

// Internal routine to convert a *const c_char to Option<String>
#[macro_export]
macro_rules! from_cstr_option {
    ($ptr : expr) => {
        if $ptr.is_null() {
            None
        } else {
            Some(
                std::ffi::CStr::from_ptr($ptr)
                    .to_string_lossy()
                    .into_owned(),
            )
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

/// Releases a ManifestStore allocated by Rust
///
/// # Safety
/// can only be released once and is invalid after this call
#[no_mangle]
pub unsafe extern "C" fn c2pa_release_manifest_store(store: *mut c2pa::ManifestStore) {
    if !store.is_null() {
        drop(Box::from_raw(store));
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
    if !s.is_null() {
        drop(CString::from_raw(s));
    }
}

/// Reads a ManifestStore from a stream with the given format
/// # Errors
/// Returns NULL if there were errors, otherwise returns a pointer to a ManifestStore
/// The error string can be retrieved by calling c2pa_error
/// # Safety
/// Reads from null terminated C strings
/// The returned value MUST be released by calling c2pa_release_manifest_store
/// and it is no longer valid after that call.
/// # Example
/// ```c
/// auto result = c2pa_manifest_store_read("image/jpeg", stream);
/// if (result == NULL) {
///   printf("Error: %s\n", c2pa_error());
/// }
/// ```
#[no_mangle]
pub unsafe extern "C" fn c2pa_manifest_store_read(
    format: *const c_char,
    stream: *mut CStream,
) -> *mut ManifestStore {
    let format = from_cstr_null_check!(format);
    let result = ManifestStore::from_stream(&format, &mut (*stream), true);
    match result {
        Ok(manifest_store) => Box::into_raw(Box::new(manifest_store)),
        Err(err) => {
            Error::from_c2pa_error(err).set_last();
            std::ptr::null_mut()
        }
    }
}

/// Releases a ManifestStore allocated by Rust
/// # Safety
/// can only be released once and is invalid after this call
#[no_mangle]
pub unsafe extern "C" fn c2pa_manifest_store_release(store_ptr: *mut ManifestStore) {
    if !store_ptr.is_null() {
        drop(Box::from_raw(store_ptr));
    }
}

/// Returns a JSON string generated from a ManifestStore
#[no_mangle]
pub unsafe extern "C" fn c2pa_manifest_store_json(store_ptr: *mut ManifestStore) -> *mut c_char {
    let store: Box<ManifestStore> = Box::from_raw(store_ptr);
    let json = store.to_string();
    Box::into_raw(store);
    to_c_string(json)
}

/// writes a ManifestStore resource stream given a manifest id and resource id
/// # Errors
/// Returns -1 if there were errors, otherwise returns size of stream written
///
/// # Safety
/// Reads from null terminated C strings
///
/// # Example
/// ```c
/// result c2pa_manifest_store_get_resource(store, "uri", stream);
/// if (result < 0) {
///    printf("Error: %s\n", c2pa_error());
/// }
/// ```
#[no_mangle]
pub unsafe extern "C" fn c2pa_manifest_store_get_resource(
    store_ptr: *mut ManifestStore,
    uri: *const c_char,
    stream: *mut CStream,
) -> c_int {
    let manifest_store: Box<ManifestStore> = Box::from_raw(store_ptr);
    let uri = from_cstr_null_check_int!(uri);
    let result = manifest_store.get_resource(&uri, &mut (*stream));
    Box::into_raw(manifest_store);
    match result {
        Ok(len) => len as c_int,
        Err(err) => {
            Error::from_c2pa_error(err).set_last();
            -1
        }
    }
}

/// Returns a ManifestStoreBuilder from a JSON string
/// # Errors
/// Returns NULL if there were errors, otherwise returns a pointer to a ManifestStoreBuilder
/// The error string can be retrieved by calling c2pa_error
/// # Safety
/// Reads from null terminated C strings
/// The returned value MUST be released by calling c2pa_manifest_store_builder_release
/// and it is no longer valid after that call.
/// # Example
/// ```c
/// auto result = c2pa_manifest_store_builder_from_json(manifest_json);
/// if (result == NULL) {
///  printf("Error: %s\n", c2pa_error());
/// }
/// ```
///
#[no_mangle]
pub unsafe extern "C" fn c2pa_manifest_store_builder_from_json(
    manifest_json: *const c_char,
) -> *mut ManifestStoreBuilder {
    let manifest_json = from_cstr_null_check!(manifest_json);
    let result = ManifestStoreBuilder::from_json(&manifest_json);
    match result {
        Ok(builder) => Box::into_raw(Box::new(builder)),
        Err(err) => {
            Error::from_c2pa_error(err).set_last();
            std::ptr::null_mut()
        }
    }
}

/// Release a ManifestStoreBuilder allocated by Rust
/// # Safety
/// can only be released once and is invalid after this call
#[no_mangle]
pub unsafe extern "C" fn c2pa_manifest_store_builder_release(
    builder_ptr: *mut ManifestStoreBuilder,
) {
    if !builder_ptr.is_null() {
        drop(Box::from_raw(builder_ptr));
    }
}

/// Adds a resource to the ManifestStoreBuilder
/// # Errors
/// Returns -1 if there were errors, otherwise returns 0
/// The error string can be retrieved by calling c2pa_error
/// # Safety
/// Reads from null terminated C strings
///
#[no_mangle]
pub unsafe extern "C" fn c2pa_manifest_builder_add_resource(
    builder_ptr: *mut ManifestStoreBuilder,
    uri: *const c_char,
    stream: *mut CStream,
) -> c_int {
    let mut builder: Box<ManifestStoreBuilder> = Box::from_raw(builder_ptr);
    let uri = from_cstr_null_check_int!(uri);
    let result = builder.add_resource(&uri, &mut (*stream));
    match result {
        Ok(builder) => {
            Box::into_raw(Box::new(builder));
            0 as c_int
        }
        Err(err) => {
            Error::from_c2pa_error(err).set_last();
            -1
        }
    }
}

/// Creates and writes signed manifest from the ManifestStoreBuilder to the destination stream
/// # Parameters
/// * builder_ptr: pointer to a ManifestStoreBuilder
/// * format: pointer to a C string with the mime type or extension
/// * source: pointer to a CStream
/// * dest: pointer to a writable CStream
/// * signer_info: pointer to a C2paSignerInfo
/// * c2pa_data_ptr: pointer to a pointer to a c_uchar (optional, can be NULL)
/// # Errors
/// Returns -1 if there were errors, otherwise returns the size of the c2pa data
/// The error string can be retrieved by calling c2pa_error
/// # Safety
/// Reads from null terminated C strings
/// If c2pa_data_ptr is not NULL, the returned value MUST be released by calling c2pa_release_string
/// and it is no longer valid after that call.
#[no_mangle]
pub unsafe extern "C" fn c2pa_manifest_store_builder_sign(
    builder_ptr: *mut ManifestStoreBuilder,
    format: *const c_char,
    source: *mut CStream,
    dest: *mut CStream,
    signer_info: &C2paSignerInfo,
    c2pa_data_ptr: *mut *const c_uchar,
) -> c_int {
    let mut builder: Box<ManifestStoreBuilder> = Box::from_raw(builder_ptr);
    builder.format = from_cstr_null_check_int!(format);

    let signer_info = SignerInfo {
        alg: from_cstr_null_check_int!(signer_info.alg),
        sign_cert: from_cstr_null_check_int!(signer_info.sign_cert).into_bytes(),
        private_key: from_cstr_null_check_int!(signer_info.private_key).into_bytes(),
        ta_url: from_cstr_option!(signer_info.ta_url),
    };
    let signer = match signer_info.signer() {
        Ok(signer) => signer,
        Err(err) => {
            err.set_last();
            return -1;
        }
    };
    let format = &builder.format.to_owned();
    let result = builder.sign(format, &mut *source, &mut *dest, signer.as_ref());
    Box::into_raw(Box::new(builder));
    match result {
        Ok(c2pa_data) => {
            if !c2pa_data_ptr.is_null() {
                *c2pa_data_ptr = c2pa_data.as_ptr();
            };
            c2pa_data.len() as c_int
        }
        Err(err) => {
            Error::from_c2pa_error(err).set_last();
            -1
        }
    }
}
