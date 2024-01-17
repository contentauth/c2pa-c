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
    ffi::{c_int, c_long},
    io::{Read, Seek, Write},
};

use c2pa::{CAIRead, CAIReadWrite};

#[repr(C)]
#[derive(Debug)]
/// An Opaque struct to hold a context value for the stream callbacks
pub struct StreamContext {
    _priv: (),
}

/// Defines a callback to read from a stream
type ReadCallback =
    unsafe extern "C" fn(context: *const StreamContext, data: *mut u8, len: usize) -> isize;

/// Defines a callback to seek to an offset in a stream
type SeekCallback =
    unsafe extern "C" fn(context: *const StreamContext, offset: c_long, mode: c_int) -> c_int;

/// Defines a callback to write to a stream
type WriteCallback =
    unsafe extern "C" fn(context: *const StreamContext, data: *const u8, len: usize) -> isize;

type FlushCallback = unsafe extern "C" fn(context: *const StreamContext) -> isize;

#[repr(C)]
/// A CStream is a Rust Read/Write/Seek stream that can be created in C
#[derive(Debug)]
pub struct CStream {
    context: Box<StreamContext>,
    reader: ReadCallback,
    seeker: SeekCallback,
    writer: WriteCallback,
    flusher: FlushCallback,
}

impl CStream {
    /// Creates a new CStream from context with callbacks
    /// # Arguments
    /// * `context` - a pointer to a StreamContext
    /// * `read` - a ReadCallback to read from the stream
    /// * `seek` - a SeekCallback to seek in the stream
    /// * `write` - a WriteCallback to write to the stream
    /// * `flush` - a FlushCallback to flush the stream
    /// # Safety
    ///     The context must remain valid for the lifetime of the C2paStream
    ///     The read, seek, and write callbacks must be valid for the lifetime of the C2paStream
    ///     The resulting C2paStream must be released by calling c2pa_release_stream
    pub unsafe fn new(
        context: *mut StreamContext,
        reader: ReadCallback,
        seeker: SeekCallback,
        writer: WriteCallback,
        flusher: FlushCallback,
    ) -> Self {
        Self {
            context: unsafe { Box::from_raw(context) },
            reader,
            seeker,
            writer,
            flusher,
        }
    }
}

impl CAIRead for CStream {}
impl CAIReadWrite for CStream {}

impl Read for CStream {
    fn read(&mut self, buf: &mut [u8]) -> std::io::Result<usize> {
        let bytes_read = unsafe { (self.reader)(&(*self.context), buf.as_mut_ptr(), buf.len()) };
        // returns a negative number for errors
        if bytes_read < 0 {
            return Err(std::io::Error::last_os_error());
        }
        Ok(bytes_read as usize)
    }
}

impl Seek for CStream {
    fn seek(&mut self, from: std::io::SeekFrom) -> std::io::Result<u64> {
        let (pos, mode) = match from {
            std::io::SeekFrom::Current(pos) => (pos, 1),
            std::io::SeekFrom::Start(pos) => (pos as i64, 0),
            std::io::SeekFrom::End(pos) => (pos, 2),
        };
        let new_pos = unsafe { (self.seeker)(&(*self.context), pos as c_long, mode) };
        Ok(new_pos as u64)
    }
}

impl Write for CStream {
    fn write(&mut self, buf: &[u8]) -> std::io::Result<usize> {
        let bytes_written = unsafe { (self.writer)(&(*self.context), buf.as_ptr(), buf.len()) };
        if bytes_written < 0 {
            return Err(std::io::Error::last_os_error());
        }
        Ok(bytes_written as usize)
    }
    fn flush(&mut self) -> std::io::Result<()> {
        let err = unsafe { (self.flusher)(&(*self.context)) };
        if err < 0 {
            return Err(std::io::Error::last_os_error());
        }
        Ok(())
    }
}

/// Creates a new C2paStream from context with callbacks
///
/// This allows implementing streams in other languages
///
/// # Arguments
/// * `context` - a pointer to a StreamContext
/// * `read` - a ReadCallback to read from the stream
/// * `seek` - a SeekCallback to seek in the stream
/// * `write` - a WriteCallback to write to the stream
///     
/// # Safety
/// The context must remain valid for the lifetime of the C2paStream
/// The resulting C2paStream must be released by calling c2pa_release_stream
///
#[no_mangle]
pub unsafe extern "C" fn c2pa_create_stream(
    context: *mut StreamContext,
    reader: ReadCallback,
    seeker: SeekCallback,
    writer: WriteCallback,
    flusher: FlushCallback,
) -> *mut CStream {
    Box::into_raw(Box::new(CStream::new(
        context, reader, seeker, writer, flusher,
    )))
}

/// Releases a CStream allocated by Rust
///
/// # Safety
/// can only be released once and is invalid after this call
#[no_mangle]
pub unsafe extern "C" fn c2pa_release_stream(stream: *mut CStream) {
    if !stream.is_null() {
        drop(Box::from_raw(stream));
    }
}
