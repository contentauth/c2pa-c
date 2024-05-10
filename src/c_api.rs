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
    os::raw::{c_char, c_int, c_uchar, c_void},
};

// C has no namespace so we prefix things with C2pa to make them unique
use c2pa::{Builder as C2paBuilder, CallbackSigner, Reader as C2paReader, SigningAlg};

use crate::{
    c_stream::CStream,
    error::Error,
    json_api::{read_file, read_ingredient_file, sign_file},
    signer_info::SignerInfo,
};

// work around limitations in cbindgen
mod cbindgen_fix {
    #[repr(C)]
    #[allow(dead_code)]
    pub struct C2paBuilder;

    #[repr(C)]
    #[allow(dead_code)]
    pub struct C2paReader;
}

#[repr(C)]
pub enum C2paSigningAlg {
    Es256,
    Es384,
    Es512,
    Ps256,
    Ps384,
    Ps512,
    Ed25519,
}

impl From<C2paSigningAlg> for SigningAlg {
    fn from(alg: C2paSigningAlg) -> Self {
        match alg {
            C2paSigningAlg::Es256 => SigningAlg::Es256,
            C2paSigningAlg::Es384 => SigningAlg::Es384,
            C2paSigningAlg::Es512 => SigningAlg::Es512,
            C2paSigningAlg::Ps256 => SigningAlg::Ps256,
            C2paSigningAlg::Ps384 => SigningAlg::Ps384,
            C2paSigningAlg::Ps512 => SigningAlg::Ps512,
            C2paSigningAlg::Ed25519 => SigningAlg::Ed25519,
        }
    }
}

#[repr(C)]
pub struct C2paSigner {
    signer: Box<dyn c2pa::Signer>,
}

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

// Internal routine to convert a *const c_char to a rust String or return a -1 int error
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

/// Defines a callback to read from a stream
pub type SignerCallback = unsafe extern "C" fn(
    context: *const (),
    data: *const c_uchar,
    len: usize,
    signed_bytes: *mut c_uchar,
    signed_len: usize,
) -> isize;

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

/// Frees a string allocated by Rust
///
/// # Safety
/// Reads from null terminated C strings
/// The string must not have been modified in C
/// can only be freed once and is invalid after this call
#[no_mangle]
pub unsafe extern "C" fn c2pa_string_free(s: *mut c_char) {
    if !s.is_null() {
        drop(CString::from_raw(s));
    }
}

/// Frees a string allocated by Rust
///
/// Provided for backward api compatibility
/// # Safety
/// Reads from null terminated C strings
/// The string must not have been modified in C
/// can only be freed once and is invalid after this call
#[no_mangle]
pub unsafe extern "C" fn c2pa_release_string(s: *mut c_char) {
    c2pa_string_free(s)
}

/// Creates a C2paReader from an asset stream with the given format
/// # Errors
/// Returns NULL if there were errors, otherwise returns a pointer to a ManifestStore
/// The error string can be retrieved by calling c2pa_error
/// # Safety
/// Reads from null terminated C strings
/// The returned value MUST be released by calling c2pa_reader_free
/// and it is no longer valid after that call.
/// # Example
/// ```c
/// auto result = c2pa_reader_from_stream("image/jpeg", stream);
/// if (result == NULL) {
///   printf("Error: %s\n", c2pa_error());
/// }
/// ```
#[no_mangle]
pub unsafe extern "C" fn c2pa_reader_from_stream(
    format: *const c_char,
    stream: *mut CStream,
) -> *mut C2paReader {
    let format = from_cstr_null_check!(format);

    let result = C2paReader::from_stream(&format, &mut (*stream));
    match result {
        Ok(reader) => Box::into_raw(Box::new(reader)),
        Err(err) => {
            Error::from_c2pa_error(err).set_last();
            std::ptr::null_mut()
        }
    }
}

/// Frees a C2paReader allocated by Rust
/// # Safety
/// can only be freed once and is invalid after this call
#[no_mangle]
pub unsafe extern "C" fn c2pa_reader_free(reader_ptr: *mut C2paReader) {
    if !reader_ptr.is_null() {
        drop(Box::from_raw(reader_ptr));
    }
}

/// Returns a JSON string generated from a C2paReader
///
/// # Safety
/// The returned value MUST be released by calling c2pa_string_free
/// and it is no longer valid after that call.
#[no_mangle]
pub unsafe extern "C" fn c2pa_reader_json(reader_ptr: *mut C2paReader) -> *mut c_char {
    let reader: Box<C2paReader> = Box::from_raw(reader_ptr);
    let json = reader.json();
    Box::into_raw(reader);
    to_c_string(json)
}

/// writes a C2paReader resource to a stream given a uri
/// # Errors
/// Returns -1 if there were errors, otherwise returns size of stream written
///
/// # Safety
/// Reads from null terminated C strings
///
/// # Example
/// ```c
/// result c2pa_reader_resource_to_stream(store, "uri", stream);
/// if (result < 0) {
///    printf("Error: %s\n", c2pa_error());
/// }
/// ```
#[no_mangle]
pub unsafe extern "C" fn c2pa_reader_resource_to_stream(
    reader_ptr: *mut C2paReader,
    uri: *const c_char,
    stream: *mut CStream,
) -> c_int {
    let reader: Box<C2paReader> = Box::from_raw(reader_ptr);
    let uri = from_cstr_null_check_int!(uri);
    let result = reader.resource_to_stream(&uri, &mut (*stream));
    Box::into_raw(reader);
    match result {
        Ok(len) => len as c_int,
        Err(err) => {
            Error::from_c2pa_error(err).set_last();
            -1
        }
    }
}

/// Creates a C2paBuilder from a JSON manifest definition string
/// # Errors
/// Returns NULL if there were errors, otherwise returns a pointer to a Builder
/// The error string can be retrieved by calling c2pa_error
/// # Safety
/// Reads from null terminated C strings
/// The returned value MUST be released by calling c2pa_builder_free
/// and it is no longer valid after that call.
/// # Example
/// ```c
/// auto result = c2pa_builder_from_json(manifest_json);
/// if (result == NULL) {
///  printf("Error: %s\n", c2pa_error());
/// }
/// ```
///
#[no_mangle]
pub unsafe extern "C" fn c2pa_builder_from_json(manifest_json: *const c_char) -> *mut C2paBuilder {
    let manifest_json = from_cstr_null_check!(manifest_json);
    let result = C2paBuilder::from_json(&manifest_json);
    match result {
        Ok(builder) => Box::into_raw(Box::new(builder)),
        Err(err) => {
            Error::from_c2pa_error(err).set_last();
            std::ptr::null_mut()
        }
    }
}

/// Create a C2paBuilder from an archive stream
/// # Errors
/// Returns NULL if there were errors, otherwise returns a pointer to a Builder
/// The error string can be retrieved by calling c2pa_error
/// # Safety
/// Reads from null terminated C strings
/// The returned value MUST be released by calling c2pa_builder_free
/// and it is no longer valid after that call.
/// # Example
/// ```c
/// auto result = c2pa_builder_from_archive(stream);
/// if (result == NULL) {
/// printf("Error: %s\n", c2pa_error());
/// }
/// ```
#[no_mangle]
pub unsafe extern "C" fn c2pa_builder_from_archive(stream: *mut CStream) -> *mut C2paBuilder {
    let result = C2paBuilder::from_archive(&mut (*stream));
    match result {
        Ok(builder) => Box::into_raw(Box::new(builder)),
        Err(err) => {
            Error::from_c2pa_error(err).set_last();
            std::ptr::null_mut()
        }
    }
}

/// Frees a C2paBuilder allocated by Rust
/// # Safety
/// can only be freed once and is invalid after this call
#[no_mangle]
pub unsafe extern "C" fn c2pa_builder_free(builder_ptr: *mut C2paBuilder) {
    if !builder_ptr.is_null() {
        drop(Box::from_raw(builder_ptr));
    }
}

/// Adds a resource to the C2paBuilder
/// # Errors
/// Returns -1 if there were errors, otherwise returns 0
/// The error string can be retrieved by calling c2pa_error
/// # Safety
/// Reads from null terminated C strings
///
#[no_mangle]
pub unsafe extern "C" fn c2pa_builder_add_resource(
    builder_ptr: *mut C2paBuilder,
    uri: *const c_char,
    stream: *mut CStream,
) -> c_int {
    let mut builder: Box<C2paBuilder> = Box::from_raw(builder_ptr);
    let uri = from_cstr_null_check_int!(uri);
    let result = builder.add_resource(&uri, &mut (*stream));
    match result {
        Ok(_builder) => {
            Box::into_raw(builder);
            0 as c_int
        }
        Err(err) => {
            Error::from_c2pa_error(err).set_last();
            -1
        }
    }
}

/// Adds an ingredient to the C2paBuilder
///
/// # Parameters
/// * builder_ptr: pointer to a Builder
/// * ingredient_json: pointer to a C string with the JSON ingredient definition
/// * format: pointer to a C string with the mime type or extension
/// * source: pointer to a CStream
/// # Errors
/// Returns -1 if there were errors, otherwise returns 0
/// The error string can be retrieved by calling c2pa_error
/// # Safety
/// Reads from null terminated C strings
///
#[no_mangle]
pub unsafe extern "C" fn c2pa_builder_add_ingredient(
    builder_ptr: *mut C2paBuilder,
    ingredient_json: *const c_char,
    format: *const c_char,
    source: *mut CStream,
) -> c_int {
    let mut builder: Box<C2paBuilder> = Box::from_raw(builder_ptr);
    let ingredient_json = from_cstr_null_check_int!(ingredient_json);
    let format = from_cstr_null_check_int!(format);
    let result = builder.add_ingredient(&ingredient_json, &format, &mut (*source));
    match result {
        Ok(_builder) => {
            Box::into_raw(builder);
            0 as c_int
        }
        Err(err) => {
            Error::from_c2pa_error(err).set_last();
            -1
        }
    }
}

/// Writes an Archive of the Builder to the destination stream
/// # Parameters
/// * builder_ptr: pointer to a Builder
/// * stream: pointer to a writable CStream
/// # Errors
/// Returns -1 if there were errors, otherwise returns 0
/// The error string can be retrieved by calling c2pa_error
/// # Safety
/// Reads from null terminated C strings
/// # Example
/// ```c
/// auto result = c2pa_builder_to_archive(builder, stream);
/// if (result < 0) {
///   printf("Error: %s\n", c2pa_error());
/// }
/// ```
#[no_mangle]
pub unsafe extern "C" fn c2pa_builder_to_archive(
    builder_ptr: *mut C2paBuilder,
    stream: *mut CStream,
) -> c_int {
    let mut builder: Box<C2paBuilder> = Box::from_raw(builder_ptr);
    let result = builder.to_archive(&mut (*stream));
    match result {
        Ok(_builder) => {
            Box::into_raw(builder);
            0 as c_int
        }
        Err(err) => {
            Error::from_c2pa_error(err).set_last();
            -1
        }
    }
}

/// Creates and writes signed manifest from the C2paBuilder to the destination stream
/// # Parameters
/// * builder_ptr: pointer to a Builder
/// * format: pointer to a C string with the mime type or extension
/// * source: pointer to a CStream
/// * dest: pointer to a writable CStream
/// * signer: pointer to a C2paSigner
/// * c2pa_data_ptr: pointer to a pointer to a c_uchar (optional, can be NULL)
/// # Errors
/// Returns -1 if there were errors, otherwise returns the size of the c2pa data
/// The error string can be retrieved by calling c2pa_error
/// # Safety
/// Reads from null terminated C strings
/// If c2pa_data_ptr is not NULL, the returned value MUST be released by calling c2pa_release_string
/// and it is no longer valid after that call.
#[no_mangle]
pub unsafe extern "C" fn c2pa_builder_sign(
    builder_ptr: *mut C2paBuilder,
    format: *const c_char,
    source: *mut CStream,
    dest: *mut CStream,
    signer: *mut C2paSigner,
    c2pa_data_ptr: *mut *const c_uchar,
) -> c_int {
    let mut builder: Box<C2paBuilder> = Box::from_raw(builder_ptr);
    let format = from_cstr_null_check_int!(format);

    let c2pa_signer = Box::from_raw(signer);

    let result = builder.sign(
        c2pa_signer.signer.as_ref(),
        &format,
        &mut *source,
        &mut *dest,
    );
    Box::into_raw(c2pa_signer);
    Box::into_raw(builder);
    match result {
        Ok(c2pa_data) => {
            let len = c2pa_data.len() as c_int;
            if !c2pa_data_ptr.is_null() {
                *c2pa_data_ptr = c2pa_data.as_ptr() as *mut c_uchar;
                std::mem::forget(c2pa_data);
            };
            len
        }
        Err(err) => {
            Error::from_c2pa_error(err).set_last();
            -1
        }
    }
}

/// Frees a the c2pa manifest optionally returned by c2pa_builder_sign
/// # Safety
/// can only be freed once and is invalid after this call
#[no_mangle]
pub unsafe extern "C" fn c2pa_manifest_free(manifest_data_ptr: *const c_uchar) {
    if !manifest_data_ptr.is_null() {
        drop(CString::from_raw(manifest_data_ptr as *mut c_char));
    }
}

/// Creates a C2paSigner from a callback and configuration
/// # Parameters
/// * callback: a callback function to sign data
/// * alg: the signing algorithm
/// * certs: a pointer to a null terminated string containing the certificate chain in PEM format
/// * tsa_url: a pointer to a null terminated string containing the RFC 3161 compliant timestamp authority URL
/// # Errors
/// Returns NULL if there were errors, otherwise returns a pointer to a C2paSigner
/// The error string can be retrieved by calling c2pa_error
/// # Safety
/// Reads from null terminated C strings
/// The returned value MUST be released by calling c2pa_signer_free
/// and it is no longer valid after that call.
/// # Example
/// ```c
/// auto result = c2pa_signer_create(callback, alg, certs, tsa_url);
/// if (result == NULL) {
///  printf("Error: %s\n", c2pa_error());
/// }
/// ```
#[no_mangle]
pub unsafe extern "C" fn c2pa_signer_create(
    context: *const c_void,
    callback: SignerCallback,
    alg: C2paSigningAlg,
    certs: *const c_char,
    tsa_url: *const c_char,
) -> *mut C2paSigner {
    let certs = from_cstr_null_check!(certs);
    let tsa_url = from_cstr_option!(tsa_url);
    let context = context as *const ();

    let c_callback = move |context: *const (), data: &[u8]| {
        // we need to guess at a max signed size, the callback must verify this is big enough or fail.
        let signed_len_max = data.len() * 2;
        let mut signed_bytes: Vec<u8> = vec![0; signed_len_max];
        let signed_size = unsafe {
            (callback)(
                context,
                data.as_ptr(),
                data.len(),
                signed_bytes.as_mut_ptr(),
                signed_len_max,
            )
        };
        //println!("signed_size: {}", signed_size);
        if signed_size < 0 {
            return Err(c2pa::Error::CoseSignature); // todo:: return errors from callback
        }
        signed_bytes.set_len(signed_size as usize);
        Ok(signed_bytes)
    };

    let mut signer = CallbackSigner::new(c_callback, alg.into(), certs).set_context(context);
    if let Some(tsa_url) = tsa_url.as_ref() {
        signer = signer.set_tsa_url(tsa_url);
    }
    Box::into_raw(Box::new(C2paSigner {
        signer: Box::new(signer),
    }))
}

/// Frees a C2paSigner allocated by Rust
/// # Safety
/// can only be freed once and is invalid after this call
#[no_mangle]
pub unsafe extern "C" fn c2pa_signer_free(signer_ptr: *mut C2paSigner) {
    if !signer_ptr.is_null() {
        drop(Box::from_raw(signer_ptr));
    }
}
