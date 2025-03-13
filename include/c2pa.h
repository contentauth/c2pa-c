// Copyright 2024 Adobe. All rights reserved.
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

// This file is generated by cbindgen. Do not edit by hand.

#ifndef c2pa_bindings_h
#define c2pa_bindings_h

#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#if C2PA_DYNAMIC_LOADING
    #define C2PA_API
#else
    #if defined(_WIN32) || defined(_WIN64)
        #if C2PA_DLL
            #if __GNUC__
                #define C2PA_API __attribute__((dllexport))
            #else
                #define C2PA_API __declspec(dllexport)
            #endif
        #else
            #if __GNUC__
                #define C2PA_API __attribute__((dllimport))
            #else
                #define C2PA_API __declspec(dllimport)
            #endif
        #endif
    #else
        #if __GNUC__
            #define C2PA_API __attribute__((visibility("default")))
        #else
            #define C2PA_API
        #endif
    #endif
#endif




/**
 * An enum to define the seek mode for the seek callback
 * Start - seek from the start of the stream
 * Current - seek from the current position in the stream
 * End - seek from the end of the stream
 */
typedef enum C2paSeekMode {
  Start = 0,
  Current = 1,
  End = 2,
} C2paSeekMode;

/**
 * List of supported signing algorithms.
 */
typedef enum C2paSigningAlg {
  Es256,
  Es384,
  Es512,
  Ps256,
  Ps384,
  Ps512,
  Ed25519,
} C2paSigningAlg;

typedef struct C2paSigner C2paSigner;

/**
 * An Opaque struct to hold a context value for the stream callbacks
 */
typedef struct StreamContext {

} StreamContext;

/**
 * Defines a callback to read from a stream
 * The return value is the number of bytes read, or a negative number for an error
 */
typedef intptr_t (*ReadCallback)(struct StreamContext *context, uint8_t *data, intptr_t len);

/**
 * Defines a callback to seek to an offset in a stream
 * The return value is the new position in the stream, or a negative number for an error
 */
typedef intptr_t (*SeekCallback)(struct StreamContext *context,
                                 intptr_t offset,
                                 enum C2paSeekMode mode);

/**
 * Defines a callback to write to a stream
 * The return value is the number of bytes written, or a negative number for an error
 */
typedef intptr_t (*WriteCallback)(struct StreamContext *context, const uint8_t *data, intptr_t len);

/**
 * Defines a callback to flush a stream
 * The return value is 0 for success, or a negative number for an error
 */
typedef intptr_t (*FlushCallback)(struct StreamContext *context);

/**
 * A C2paStream is a Rust Read/Write/Seek stream that can be created in C
 */
typedef struct C2paStream {
  struct StreamContext *context;
  ReadCallback reader;
  SeekCallback seeker;
  WriteCallback writer;
  FlushCallback flusher;
} C2paStream;

/**
 * Defines the configuration for a Signer.
 *
 * The signer is created from the sign_cert and private_key fields.
 * an optional url to an RFC 3161 compliant time server will ensure the signature is timestamped.
 *
 */
typedef struct C2paSignerInfo {
  /**
   * The signing algorithm.
   */
  const char *alg;
  /**
   * The public certificate chain in PEM format.
   */
  const char *sign_cert;
  /**
   * The private key in PEM format.
   */
  const char *private_key;
  /**
   * The timestamp authority URL or NULL.
   */
  const char *ta_url;
} C2paSignerInfo;

typedef struct C2paReader {

} C2paReader;

typedef struct C2paBuilder {

} C2paBuilder;

/**
 * Defines a callback to read from a stream.
 *
 * # Parameters
 * * context: A generic context value to used by the C code, often a file or stream reference.
 *
 */
typedef intptr_t (*SignerCallback)(const void *context,
                                   const unsigned char *data,
                                   uintptr_t len,
                                   unsigned char *signed_bytes,
                                   uintptr_t signed_len);

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

/**
 * Creates a new C2paStream from context with callbacks
 *
 * This allows implementing streams in other languages
 *
 * # Arguments
 * * `context` - a pointer to a StreamContext
 * * `read` - a ReadCallback to read from the stream
 * * `seek` - a SeekCallback to seek in the stream
 * * `write` - a WriteCallback to write to the stream
 *
 * # Safety
 * The context must remain valid for the lifetime of the C2paStream
 * The resulting C2paStream must be released by calling c2pa_release_stream
 *
 */
C2PA_API extern
struct C2paStream *c2pa_create_stream(struct StreamContext *context,
                                      ReadCallback reader,
                                      SeekCallback seeker,
                                      WriteCallback writer,
                                      FlushCallback flusher);

/**
 * Releases a C2paStream allocated by Rust
 *
 * # Safety
 * can only be released once and is invalid after this call
 */
C2PA_API extern void c2pa_release_stream(struct C2paStream *stream);

/**
 * Returns a version string for logging.
 *
 * # Safety
 * The returned value MUST be released by calling release_string
 * and it is no longer valid after that call.
 */
C2PA_API extern char *c2pa_version(void);

/**
 * Returns the last error message.
 *
 * # Safety
 * The returned value MUST be released by calling release_string
 * and it is no longer valid after that call.
 */
C2PA_API extern char *c2pa_error(void);

/**
 * Load Settings from a string.
 *
 * # Errors
 * Returns -1 if there were errors, otherwise returns 0.
 * The error string can be retrieved by calling c2pa_error.
 *
 * # Safety
 * Reads from NULL-terminated C strings.
 */
C2PA_API extern int c2pa_load_settings(const char *settings, const char *format);

/**
 * Returns a ManifestStore JSON string from a file path.
 *
 * Any thumbnails or other binary resources will be written to data_dir if provided.
 *
 * # Errors
 * Returns NULL if there were errors, otherwise returns a JSON string.
 * The error string can be retrieved by calling c2pa_error.
 *
 * # Safety
 * Reads from NULL-terminated C strings.
 * The returned value MUST be released by calling release_string
 * and it is no longer valid after that call.
 */
C2PA_API extern char *c2pa_read_file(const char *path, const char *data_dir);

/**
 * Returns an Ingredient JSON string from a file path.
 *
 * Any thumbnail or C2PA data will be written to data_dir if provided.
 *
 * # Errors
 * Returns NULL if there were errors, otherwise returns a JSON string
 * containing the Ingredient.
 * The error string can be retrieved by calling c2pa_error.
 *
 * # Safety
 * Reads from NULL-terminated C strings.
 * The returned value MUST be released by calling release_string
 * and it is no longer valid after that call.
 */
C2PA_API extern char *c2pa_read_ingredient_file(const char *path, const char *data_dir);

/**
 * Add a signed manifest to the file at path with the given signer information.
 *
 * # Errors
 * Returns an error field if there were errors.
 *
 * # Safety
 * Reads from NULL-terminated C strings.
 * The returned value MUST be released by calling release_string
 * and it is no longer valid after that call.
 */
C2PA_API extern
char *c2pa_sign_file(const char *source_path,
                     const char *dest_path,
                     const char *manifest,
                     const struct C2paSignerInfo *signer_info,
                     const char *data_dir);

/**
 * Frees a string allocated by Rust.
 *
 * Deprecated: for backward api compatibility only.
 *
 * # Safety
 * Reads from NULL-terminated C strings.
 * The string must not have been modified in C.
 * The string can only be freed once and is invalid after this call.
 */
C2PA_API extern void c2pa_release_string(char *s);

/**
 * Frees a string allocated by Rust.
 *
 * # Safety
 * Reads from NULL-terminated C strings.
 * The string must not have been modified in C.
 * The string can only be freed once and is invalid after this call.
 */
C2PA_API extern void c2pa_string_free(char *s);

/**
 * Creates and verifies a C2paReader from an asset stream with the given format.
 *
 * Parameters
 * * format: pointer to a C string with the mime type or extension.
 * * stream: pointer to a C2paStream.
 *
 * # Errors
 * Returns NULL if there were errors, otherwise returns a pointer to a ManifestStore.
 * The error string can be retrieved by calling c2pa_error.
 *
 * # Safety
 * Reads from NULL-terminated C strings.
 * The returned value MUST be released by calling c2pa_reader_free
 * and it is no longer valid after that call.
 *
 * # Example
 * ```c
 * auto result = c2pa_reader_from_stream("image/jpeg", stream);
 * if (result == NULL) {
 *     let error = c2pa_error();
 *     printf("Error: %s\n", error);
 *     c2pa_string_free(error);
 * }
 * ```
 */
C2PA_API extern
struct C2paReader *c2pa_reader_from_stream(const char *format,
                                           struct C2paStream *stream);

/**
 * Frees a C2paReader allocated by Rust.
 *
 * # Safety
 * The C2paReader can only be freed once and is invalid after this call.
 */
C2PA_API extern void c2pa_reader_free(struct C2paReader *reader_ptr);

/**
 * Returns a JSON string generated from a C2paReader.
 *
 * # Safety
 * The returned value MUST be released by calling c2pa_string_free
 * and it is no longer valid after that call.
 */
C2PA_API extern char *c2pa_reader_json(struct C2paReader *reader_ptr);

/**
 * Writes a C2paReader resource to a stream given a URI.
 *
 * The resource uri should match an identifier in the the manifest store.
 *
 * # Parameters
 * * reader_ptr: pointer to a Reader.
 * * uri: pointer to a C string with the URI to identify the resource.
 * * stream: pointer to a writable C2paStream.
 *
 * # Errors
 * Returns -1 if there were errors, otherwise returns size of stream written.
 *
 * # Safety
 * Reads from NULL-terminated C strings.
 *
 * # Example
 * ```c
 * auto result = c2pa_reader_resource_to_stream(store, "uri", stream);
 * if (result < 0) {
 *     auto error = c2pa_error();
 *     printf("Error: %s\n", error);
 *     c2pa_string_free(error);
 * }
 * ```
 */
C2PA_API extern
int64_t c2pa_reader_resource_to_stream(struct C2paReader *reader_ptr,
                                       const char *uri,
                                       struct C2paStream *stream);

/**
 * Creates a C2paBuilder from a JSON manifest definition string.
 *
 * # Errors
 * Returns NULL if there were errors, otherwise returns a pointer to a Builder.
 * The error string can be retrieved by calling c2pa_error.
 *
 * # Safety
 * Reads from NULL-terminated C strings.
 * The returned value MUST be released by calling c2pa_builder_free
 * and it is no longer valid after that call.
 *
 * # Example
 * ```c
 * auto result = c2pa_builder_from_json(manifest_json);
 * if (result == NULL) {
 *     auto error = c2pa_error();
 *     printf("Error: %s\n", error);
 *     c2pa_string_free(error);
 * }
 * ```
 */
C2PA_API extern struct C2paBuilder *c2pa_builder_from_json(const char *manifest_json);

/**
 * Create a C2paBuilder from an archive stream.
 *
 * # Errors
 * Returns NULL if there were errors, otherwise returns a pointer to a Builder.
 * The error string can be retrieved by calling c2pa_error.
 *
 * # Safety
 * Reads from NULL-terminated C strings.
 * The returned value MUST be released by calling c2pa_builder_free
 * and it is no longer valid after that call.
 *
 * # Example
 * ```c
 * auto result = c2pa_builder_from_archive(stream);
 * if (result == NULL) {
 *     auto error = c2pa_error();
 *     printf("Error: %s\n", error);
 *     c2pa_string_free(error);
 * }
 * ```
 */
C2PA_API extern struct C2paBuilder *c2pa_builder_from_archive(struct C2paStream *stream);

/**
 * Frees a C2paBuilder allocated by Rust.
 *
 * # Safety
 * The C2paBuilder can only be freed once and is invalid after this call.
 */
C2PA_API extern void c2pa_builder_free(struct C2paBuilder *builder_ptr);

/**
 * Sets the no-embed flag on the Builder.
 * When set, the builder will not embed a C2PA manifest store into the asset when signing.
 * This is useful when creating cloud or sidecar manifests.
 * # Parameters
 * * builder_ptr: pointer to a Builder.
 * # Safety
 * builder_ptr must be a valid pointer to a Builder.
 */
C2PA_API extern void c2pa_builder_set_no_embed(struct C2paBuilder *builder_ptr);

/**
 * Sets the remote URL on the Builder.
 * When set, the builder will embed a remote URL into the asset when signing.
 * This is useful when creating cloud based Manifests.
 * # Parameters
 * * builder_ptr: pointer to a Builder.
 * * remote_url: pointer to a C string with the remote URL.
 * # Errors
 * Returns -1 if there were errors, otherwise returns 0.
 * The error string can be retrieved by calling c2pa_error.
 * # Safety
 * Reads from NULL-terminated C strings.
 */
C2PA_API extern
int c2pa_builder_set_remote_url(struct C2paBuilder *builder_ptr,
                                const char *remote_url);

/**
 * Adds a resource to the C2paBuilder.
 *
 * The resource uri should match an identifier in the manifest definition.
 *
 * # Parameters
 * * builder_ptr: pointer to a Builder.
 * * uri: pointer to a C string with the URI to identify the resource.
 * * stream: pointer to a C2paStream.
 * # Errors
 * Returns -1 if there were errors, otherwise returns 0.
 * The error string can be retrieved by calling c2pa_error.
 *
 * # Safety
 * Reads from NULL-terminated C strings
 */
C2PA_API extern
int c2pa_builder_add_resource(struct C2paBuilder *builder_ptr,
                              const char *uri,
                              struct C2paStream *stream);

/**
 * Adds an ingredient to the C2paBuilder.
 *
 * # Parameters
 * * builder_ptr: pointer to a Builder.
 * * ingredient_json: pointer to a C string with the JSON ingredient definition.
 * * format: pointer to a C string with the mime type or extension.
 * * source: pointer to a C2paStream.
 *
 * # Errors
 * Returns -1 if there were errors, otherwise returns 0.
 * The error string can be retrieved by calling c2pa_error.
 *
 * # Safety
 * Reads from NULL-terminated C strings.
 */
C2PA_API extern
int c2pa_builder_add_ingredient_from_stream(struct C2paBuilder *builder_ptr,
                                            const char *ingredient_json,
                                            const char *format,
                                            struct C2paStream *source);

/**
 * Writes an Archive of the Builder to the destination stream.
 *
 * # Parameters
 * * builder_ptr: pointer to a Builder.
 * * stream: pointer to a writable C2paStream.
 *
 * # Errors
 * Returns -1 if there were errors, otherwise returns 0.
 * The error string can be retrieved by calling c2pa_error.
 *
 * # Safety
 * Reads from NULL-terminated C strings.
 *
 * # Example
 * ```c
 * auto result = c2pa_builder_to_archive(builder, stream);
 * if (result < 0) {
 *     auto error = c2pa_error();
 *     printf("Error: %s\n", error);
 *     c2pa_string_free(error);
 * }
 * ```
 */
C2PA_API extern
int c2pa_builder_to_archive(struct C2paBuilder *builder_ptr,
                            struct C2paStream *stream);

/**
 * Creates and writes signed manifest from the C2paBuilder to the destination stream.
 *
 * # Parameters
 * * builder_ptr: pointer to a Builder.
 * * format: pointer to a C string with the mime type or extension.
 * * source: pointer to a C2paStream.
 * * dest: pointer to a writable C2paStream.
 * * signer: pointer to a C2paSigner.
 * * c2pa_bytes_ptr: pointer to a pointer to a c_uchar to return manifest_bytes (optional, can be NULL).
 *
 * # Errors
 * Returns -1 if there were errors, otherwise returns the size of the c2pa data.
 * The error string can be retrieved by calling c2pa_error.
 *
 * # Safety
 * Reads from NULL-terminated C strings
 * If manifest_bytes_ptr is not NULL, the returned value MUST be released by calling c2pa_manifest_bytes_free
 * and it is no longer valid after that call.
 */
C2PA_API extern
int64_t c2pa_builder_sign(struct C2paBuilder *builder_ptr,
                          const char *format,
                          struct C2paStream *source,
                          struct C2paStream *dest,
                          struct C2paSigner *signer,
                          const unsigned char **manifest_bytes_ptr);

/**
 * Frees a C2PA manifest returned by c2pa_builder_sign.
 *
 * # Safety
 * The bytes can only be freed once and are invalid after this call.
 */
C2PA_API extern void c2pa_manifest_bytes_free(const unsigned char *manifest_bytes_ptr);

/**
 * Creates a hashed placeholder from a Builder.
 * The placeholder is used to reserve size in an asset for later signing.
 *
 * # Parameters
 * * builder_ptr: pointer to a Builder.
 * * reserved_size: the size required for a signature from the intended signer.
 * * format: pointer to a C string with the mime type or extension.
 * * manifest_bytes_ptr: pointer to a pointer to a c_uchar to return manifest_bytes.
 *
 * # Errors
 * Returns -1 if there were errors, otherwise returns the size of the manifest_bytes.
 * The error string can be retrieved by calling c2pa_error.
 *
 * # Safety
 * Reads from NULL-terminated C strings.
 * If manifest_bytes_ptr is not NULL, the returned value MUST be released by calling c2pa_manifest_bytes_free
 * and it is no longer valid after that call.
 */
C2PA_API extern
int64_t c2pa_builder_data_hashed_placeholder(struct C2paBuilder *builder_ptr,
                                             uintptr_t reserved_size,
                                             const char *format,
                                             const unsigned char **manifest_bytes_ptr);

/**
 * Sign a Builder using the specified signer and data hash.
 * The data hash is a JSON string containing DataHash information for the asset.
 * This is a low-level method for advanced use cases where the caller handles embedding the manifest.
 *
 * # Parameters
 * * builder_ptr: pointer to a Builder.
 * * signer: pointer to a C2paSigner.
 * * data_hash: pointer to a C string with the JSON data hash.
 * * format: pointer to a C string with the mime type or extension.
 * * asset: pointer to a C2paStream (may be NULL to use pre calculated hashes).
 * * manifest_bytes_ptr: pointer to a pointer to a c_uchar to return manifest_bytes (optional, can be NULL).
 *
 * # Errors
 * Returns -1 if there were errors, otherwise returns the size of the manifest_bytes.
 * The error string can be retrieved by calling c2pa_error.
 *
 * # Safety
 * Reads from NULL-terminated C strings.
 * If manifest_bytes_ptr is not NULL, the returned value MUST be released by calling c2pa_manifest_bytes_free
 * and it is no longer valid after that call.
 */
C2PA_API extern
int64_t c2pa_builder_sign_data_hashed_embeddable(struct C2paBuilder *builder_ptr,
                                                 struct C2paSigner *signer,
                                                 const char *data_hash,
                                                 const char *format,
                                                 struct C2paStream *asset,
                                                 const unsigned char **manifest_bytes_ptr);

/**
 * Convert a binary c2pa manifest into an embeddable version for the given format.
 * A raw manifest (in application/c2pa format) can be uploaded to the cloud but
 * it cannot be embedded directly into an asset without extra processing.
 * This method converts the raw manifest into an embeddable version that can be
 * embedded into an asset.
 *
 * # Parameters
 * * format: pointer to a C string with the mime type or extension.
 * * manifest_bytes_ptr: pointer to a c_uchar with the raw manifest bytes.
 * * manifest_bytes_size: the size of the manifest_bytes.
 * * result_bytes_ptr: pointer to a pointer to a c_uchar to return the embeddable manifest bytes.
 *
 * # Errors
 * Returns -1 if there were errors, otherwise returns the size of the result_bytes.
 * The error string can be retrieved by calling c2pa_error.
 *
 * # Safety
 * Reads from NULL-terminated C strings.
 * The returned value MUST be released by calling c2pa_manifest_bytes_free
 * and it is no longer valid after that call.
 */
C2PA_API extern
int64_t c2pa_format_embeddable(const char *format,
                               const unsigned char *manifest_bytes_ptr,
                               uintptr_t manifest_bytes_size,
                               const unsigned char **result_bytes_ptr);

/**
 * Creates a C2paSigner from a callback and configuration.
 *
 * # Parameters
 * * callback: a callback function to sign data.
 * * alg: the signing algorithm.
 * * certs: a pointer to a NULL-terminated string containing the certificate chain in PEM format.
 * * tsa_url: a pointer to a NULL-terminated string containing the RFC 3161 compliant timestamp authority URL.
 *
 * # Errors
 * Returns NULL if there were errors, otherwise returns a pointer to a C2paSigner.
 * The error string can be retrieved by calling c2pa_error.
 *
 * # Safety
 * Reads from NULL-terminated C strings
 * The returned value MUST be released by calling c2pa_signer_free
 * and it is no longer valid after that call.
 *
 * # Example
 * ```c
 * auto result = c2pa_signer_create(callback, alg, certs, tsa_url);
 * if (result == NULL) {
 *     auto error = c2pa_error();
 *     printf("Error: %s\n", error);
 *     c2pa_string_free(error);
 * }
 * ```
 */
C2PA_API extern
struct C2paSigner *c2pa_signer_create(const void *context,
                                      SignerCallback callback,
                                      enum C2paSigningAlg alg,
                                      const char *certs,
                                      const char *tsa_url);

/**
 * Creates a C2paSigner from a SignerInfo.
 * The signer is created from the sign_cert and private_key fields.
 * an optional url to an RFC 3161 compliant time server will ensure the signature is timestamped.
 *
 * # Parameters
 * * signer_info: pointer to a C2paSignerInfo.
 * # Errors
 * Returns NULL if there were errors, otherwise returns a pointer to a C2paSigner.
 * The error string can be retrieved by calling c2pa_error.
 * # Safety
 * Reads from NULL-terminated C strings.
 * The returned value MUST be released by calling c2pa_signer_free
 * and it is no longer valid after that call.
 * # Example
 * ```c
 * auto result = c2pa_signer_from_info(signer_info);
 * if (result == NULL) {
 *     auto error = c2pa_error();
 *     printf("Error: %s\n", error);
 *     c2pa_string_free(error);
 * }
 * ```
 */
C2PA_API extern struct C2paSigner *c2pa_signer_from_info(const struct C2paSignerInfo *signer_info);

/**
 * Returns the size to reserve for the signature for this signer.
 *
 * # Parameters
 * * signer_ptr: pointer to a C2paSigner.
 *
 * # Errors
 * Returns -1 if there were errors, otherwise returns the size to reserve.
 * The error string can be retrieved by calling c2pa_error.
 *
 * # Safety
 * The signer_ptr must be a valid pointer to a C2paSigner.
 */
C2PA_API extern int64_t c2pa_signer_reserve_size(struct C2paSigner *signer_ptr);

/**
 * Frees a C2paSigner allocated by Rust.
 *
 * # Safety
 * The C2paSigner can only be freed once and is invalid after this call.
 */
C2PA_API extern void c2pa_signer_free(const struct C2paSigner *signer_ptr);

/**
 * Signs a byte array using the Ed25519 algorithm.
 * # Safety
 * The returned value MUST be freed by calling c2pa_signature_free
 * and it is no longer valid after that call.
 *
 */
C2PA_API extern
const unsigned char *c2pa_ed25519_sign(const unsigned char *bytes,
                                       uintptr_t len,
                                       const char *private_key);

/**
 * Frees a signature allocated by Rust.
 * # Safety
 * The signature can only be freed once and is invalid after this call.
 */
C2PA_API extern void c2pa_signature_free(const uint8_t *signature_ptr);

#ifdef __cplusplus
}  // extern "C"
#endif  // __cplusplus

#endif  /* c2pa_bindings_h */


