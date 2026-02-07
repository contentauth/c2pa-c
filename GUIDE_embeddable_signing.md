# Data-Hashed Embeddable Signing Guide

A comprehensive guide to the `data_hashed_placeholder`, `sign_data_hashed_embeddable`,
and `format_embeddable` APIs in the C2PA C/C++ SDK.

---

## Table of Contents

1. [C2PA Background](#1-c2pa-background)
2. [The Normal sign() Flow](#2-the-normal-sign-flow)
3. [Why Data-Hashed Embeddable Exists](#3-why-data-hashed-embeddable-exists)
4. [The Three APIs in Detail](#4-the-three-apis-in-detail)
5. [Complete Flow Diagrams](#5-complete-flow-diagrams)
6. [The DataHash JSON Structure](#6-the-datahash-json-structure)
7. [Anatomy of a Signed Asset](#7-anatomy-of-a-signed-asset)
8. [Context, Settings, and How They Propagate](#8-context-settings-and-how-they-propagate)
9. [Limitations](#9-limitations)
10. [Pseudocode Flow Overview](#10-pseudocode-flow-overview)
11. [Complete C++ Example](#11-complete-c-example)
12. [Suggested Test Plan](#12-suggested-test-plan)

---

## 1. C2PA Background

**C2PA** (Coalition for Content Provenance and Authenticity) is an open standard that
lets you attach tamper-evident metadata to digital assets (images, videos, audio, etc.).
Think of it as a "nutrition label" for media: it records who created or edited a file,
what tools were used, and whether the content has been modified.

### Key Concepts

**Manifest**: A bundle of metadata attached to an asset. It contains a *claim*
(the statement about the asset), *assertions* (individual facts like "this image was
created with Photoshop"), and a *signature* (cryptographic proof that the claim hasn't
been tampered with). A file can have multiple manifests in a *manifest store*; the most
recent one is the *active manifest*.

**Claim**: The core data structure inside a manifest. It lists all assertions by
reference (hash pointers), identifies the signer, and is itself signed. The signature
covers the CBOR-serialized claim data using COSE Sign1 format.

**Assertions**: Individual pieces of metadata inside a manifest. Examples include
`c2pa.actions` (what was done to the asset), `cawg.training-mining` (AI training
permissions), thumbnails, and ingredient references. Assertions are stored in the
*assertion store* inside the manifest.

**JUMBF** (JPEG Universal Metadata Box Format): The binary container format used to
serialize C2PA manifests. JUMBF organizes data into nested "boxes", each with a type
label. The entire manifest store is a JUMBF superbox. JUMBF is format-agnostic — it
gets *wrapped* differently depending on whether it goes into a JPEG, PNG, PDF, etc.

**Hash Binding**: The mechanism that ties a manifest to a specific asset. The manifest
contains a hash of the asset's bytes (with the manifest region excluded). If the asset
changes, the hash won't match, and the manifest is invalidated.

### The Chicken-and-Egg Problem

Here's the fundamental challenge with C2PA signing:

```
The manifest must be EMBEDDED in the asset file.
But the manifest contains a HASH of the asset file.
But the hash changes if you embed something in the file.
```

The standard `sign()` flow solves this with a two-pass approach internally:
1. Insert a placeholder manifest (with dummy hash) into the asset
2. Hash the asset (skipping the placeholder region)
3. Replace the dummy hash with the real hash, sign, and patch the signature in

The data-hashed embeddable APIs give YOU control over this process.

---

## 2. The Normal sign() Flow

The standard `Builder::sign()` method handles everything in a single call:

```
+------------------------------------------------------------------+
|                    Builder::sign()                                |
|                                                                  |
|   source.jpg ──> [SDK reads source]                              |
|                       |                                          |
|                       v                                          |
|              [Create manifest from                               |
|               Builder's JSON definition]                         |
|                       |                                          |
|                       v                                          |
|              [Auto-generate thumbnail]   <-- if settings allow   |
|                       |                                          |
|                       v                                          |
|              [Insert placeholder manifest                        |
|               into copy of source asset]                         |
|                       |                                          |
|                       v                                          |
|              [Hash asset bytes, excluding                        |
|               the placeholder region]                            |
|                       |                                          |
|                       v                                          |
|              [Sign the claim (COSE Sign1)]                       |
|                       |                                          |
|                       v                                          |
|              [Patch signature into                                |
|               the placeholder]                                   |
|                       |                                          |
|                       v                                          |
|              [Write final asset to dest] ──> output.jpg          |
|                                                                  |
+------------------------------------------------------------------+
```

```cpp
// C++ usage — one call does everything
auto manifest_data = builder.sign("source.jpg", "output.jpg", signer);
// output.jpg now contains the original image + embedded signed manifest
```

This is the simplest path. The SDK reads the source, creates the output, and handles
all hashing, signing, and embedding internally. The caller never touches raw manifest
bytes.

---

## 3. Why Data-Hashed Embeddable Exists

The normal `sign()` flow requires the SDK to have simultaneous read/write access to
both the source and destination. This doesn't work for several real-world scenarios:

- **Cloud workflows**: The asset lives on a server. You want to generate the manifest
  locally (or on a different server) and upload it separately.
- **Remote/HSM signing**: The signing key lives in a hardware security module. You need
  to send the hash to the HSM, get back a signature, and embed it yourself.
- **Streaming pipelines**: You're generating an asset on-the-fly and need to reserve
  space for the manifest before the asset is complete.
- **Custom embedding**: You need to control exactly where in the file the manifest goes,
  or you're working with a format the SDK doesn't natively support for embedding.
- **Separate hash calculation**: You want to calculate the asset hash yourself (perhaps
  on a different machine or at a different time than signing).

The data-hashed embeddable APIs break the monolithic `sign()` into discrete steps that
you control.

---

## 4. The Three APIs in Detail

### 4a. `data_hashed_placeholder(reserve_size, format)`

```cpp
// C++ signature (include/c2pa.hpp:807)
std::vector<unsigned char> Builder::data_hashed_placeholder(
    uintptr_t reserved_size,
    const std::string &format
);
```

**Purpose**: Creates an exact-sized placeholder of the final manifest. You embed this
into your asset so you know exactly which byte range to exclude when hashing.

**What happens internally** (builder.rs:1918-1936):

1. If the Builder doesn't already have a DataHash assertion, one is created with
   10 pre-allocated exclusion entries (dummy `HashRange(0, 2)` values) to reserve space
2. The Builder is converted to a `Store` (which uses Context settings for thumbnail
   generation, claim_generator_info, actions, intent, etc.)
3. The Store serializes the manifest to JUMBF binary, using `reserve_size` to allocate
   space for the signature box
4. The JUMBF is then "composed" (wrapped) into the target format

**Parameters**:
- `reserved_size`: The number of bytes needed for a signature from your signer.
  Get this from `signer.reserve_size()`. This ensures the placeholder is exactly the
  same size as the final signed manifest.
- `format`: MIME type or extension of the target asset (e.g., `"image/jpeg"`,
  `"application/c2pa"` for raw JUMBF)

**Returns**: Binary bytes of the placeholder, already formatted for the target format.

**Critical invariant**: `placeholder.size() == final_signed_manifest.size()`. The signing
step produces output that is byte-for-byte the same size as the placeholder. This is what
makes the "patch in place" approach work.

### 4b. `sign_data_hashed_embeddable(signer, data_hash, format, asset?)`

```cpp
// C++ signature (include/c2pa.hpp:816)
std::vector<unsigned char> Builder::sign_data_hashed_embeddable(
    Signer &signer,
    const std::string &data_hash,
    const std::string &format,
    std::istream *asset = nullptr
);
```

**Purpose**: Produces the final signed manifest bytes. This is the core signing step.

**What happens internally**:

The C FFI layer (c_api.rs:1627-1660) does two things:

1. **If an asset stream is provided**: Calls `data_hash.gen_hash_from_stream(&mut *asset)`
   to calculate the hash of the asset, respecting the exclusion ranges in the DataHash JSON.
   This fills in the `hash` field automatically.
2. **Delegates to Rust SDK**: Calls `builder.sign_data_hashed_embeddable(signer, &data_hash, format)`

The Rust SDK (builder.rs:1959-1985) then:

1. Converts the Builder to a Store (using Context settings)
2. Updates the DataHash assertion with the provided (or calculated) hash and exclusions
3. Serializes to JUMBF
4. Signs the CBOR-serialized claim → produces COSE Sign1 signature
5. Locates the signature placeholder bytes in the JUMBF and patches them with the real signature
6. Wraps the result in the target format's container (if format != "application/c2pa")

**Two operating modes**:

```
MODE A: Pre-calculated hash (no asset stream)
+-------------------------------------------+
|  Caller has already:                      |
|  1. Embedded placeholder in asset         |
|  2. Hashed asset (excluding placeholder)  |
|  3. Put hash value in data_hash JSON      |
|                                           |
|  data_hash JSON has:                      |
|    "hash": "gWZNEOM...base64..."          |
|                                           |
|  Call: sign_data_hashed_embeddable(       |
|    signer, data_hash, "image/jpeg")       |
|    // no asset stream                     |
|                                           |
|  Returns: format-specific manifest bytes  |
+-------------------------------------------+

MODE B: Auto-calculated hash (with asset stream)
+-------------------------------------------+
|  Caller provides:                         |
|  1. Asset stream (with placeholder in it) |
|  2. data_hash JSON with hash=""           |
|                                           |
|  data_hash JSON has:                      |
|    "hash": ""    <-- empty = auto-calc    |
|                                           |
|  Call: sign_data_hashed_embeddable(       |
|    signer, data_hash,                     |
|    "application/c2pa", &asset)            |
|                                           |
|  SDK reads asset, skips exclusion ranges, |
|  calculates SHA-256 hash automatically    |
|                                           |
|  Returns: raw JUMBF manifest bytes        |
|  (needs format_embeddable() to embed)     |
+-------------------------------------------+
```

**Parameters**:
- `signer`: Your Signer object (ES256, PS256, etc.)
- `data_hash`: JSON string describing hash parameters (see Section 6)
- `format`: `"image/jpeg"` for ready-to-embed output, or `"application/c2pa"` for raw JUMBF
  (which then needs `format_embeddable()`)
- `asset`: Optional stream. If provided, the SDK calculates the hash from the stream
  (respecting exclusions). If nullptr, the hash in `data_hash` JSON must already be filled.

### 4c. `format_embeddable(format, data)` — static method

```cpp
// C++ signature (include/c2pa.hpp:822)
static std::vector<unsigned char> Builder::format_embeddable(
    const std::string &format,
    std::vector<unsigned char> &data
);
```

**Purpose**: Converts raw JUMBF manifest bytes into format-specific embeddable bytes.

**When you need it**: Only when `sign_data_hashed_embeddable` was called with
`format="application/c2pa"`. If you already signed with a specific format like
`"image/jpeg"`, the output is already embeddable and you skip this step.

**What happens internally** (c_api.rs:1683-1706):

Calls `c2pa::Builder::composed_manifest(bytes, format)` which delegates to
`Store::get_composed_manifest()`. This looks up the format handler and calls
`compose_manifest()` to wrap the JUMBF bytes in the appropriate container:

- **JPEG**: Splits JUMBF into APP11 marker segments (max 65000 bytes each), adds
  JPEG XT headers (CI=0x4A50, En=0x0211, sequence numbers)
- **PNG**: Wraps in PNG ancillary chunks with CRC
- **PDF**: Wraps in PDF content streams
- **Raw c2pa**: Returns bytes unchanged

**Parameters**:
- `format`: Target MIME type (e.g., `"image/jpeg"`, `"image/png"`)
- `data`: Raw JUMBF bytes from `sign_data_hashed_embeddable` using `"application/c2pa"`

**Returns**: Format-wrapped bytes, always larger than the input due to container overhead.

**No context needed**: This is a pure stateless byte transformation. It's a static method
on the Builder class.

---

## 5. Complete Flow Diagrams

### Flow A: Pre-Calculated Hash (Simplest)

You already know the hash. Three API calls total.

```
  Caller                              SDK
  ------                              ---
    |                                  |
    |  1. data_hashed_placeholder()    |
    |--------------------------------->|
    |  <--- placeholder bytes ---------|
    |                                  |
    |  [Caller embeds placeholder      |
    |   in asset at known offset]      |
    |                                  |
    |  [Caller hashes asset,           |
    |   excluding placeholder region]  |
    |                                  |
    |  [Caller builds data_hash JSON   |
    |   with real hash + exclusions]   |
    |                                  |
    |  2. sign_data_hashed_embeddable()|
    |     format="image/jpeg"          |
    |     asset=nullptr                |
    |--------------------------------->|
    |  <--- signed manifest bytes -----|
    |                                  |
    |  [Caller patches manifest into   |
    |   asset, replacing placeholder]  |
    |                                  |
    |  DONE: Asset is signed           |
```

### Flow B: Auto-Calculated Hash + Format Conversion

SDK calculates the hash from the asset stream. Four API calls total.

```
  Caller                              SDK
  ------                              ---
    |                                  |
    |  1. data_hashed_placeholder()    |
    |     format="image/jpeg"          |
    |--------------------------------->|
    |  <--- placeholder bytes ---------|
    |                                  |
    |  [Caller embeds placeholder      |
    |   in asset at known offset]      |
    |                                  |
    |  [Caller builds data_hash JSON   |
    |   with hash="" (empty)           |
    |   and exclusion ranges]          |
    |                                  |
    |  2. sign_data_hashed_embeddable()|
    |     format="application/c2pa"    |
    |     asset=&asset_stream          |
    |--------------------------------->|
    |     [SDK reads stream, skips     |
    |      exclusions, calcs hash]     |
    |  <--- raw JUMBF bytes -----------|
    |                                  |
    |  3. format_embeddable()          |
    |     format="image/jpeg"          |
    |--------------------------------->|
    |  <--- JPEG-embeddable bytes -----|
    |                                  |
    |  [Caller patches manifest into   |
    |   asset, replacing placeholder]  |
    |                                  |
    |  DONE: Asset is signed           |
```

### Flow C: Full Lifecycle (Byte-Level Detail)

Shows exactly what happens to the bytes at each step.

```
STEP 1: Create placeholder
==============================

  Builder ──> data_hashed_placeholder(reserve_size=10000, "image/jpeg")
                    |
                    v
  Internally: Builder -> to_store() -> JUMBF binary -> compose for JPEG
                    |
                    v
  Returns:  [APP11][APP11][APP11]...  (JPEG marker segments)
            |<-- placeholder_size -->|
            e.g., 45884 bytes

STEP 2: Embed placeholder in asset
==============================

  Original JPEG:  [SOI][APP0/EXIF][Image Data...][EOI]

  With placeholder:
  [SOI][APP0/EXIF][APP11][APP11][APP11]...[Image Data...][EOI]
  ^                ^                     ^
  byte 0           byte 20              byte 20 + 45884

  Exclusion = { start: 20, length: 45884 }
  (This is where the manifest sits — must be excluded from hash)

STEP 3: Hash the asset (excluding manifest region)
==============================

  Hash input:  [SOI][APP0/EXIF]  +  [Image Data...][EOI]
               bytes 0..19          bytes 45904..end

               (bytes 20..45903 are SKIPPED — that's the placeholder)

  Result: SHA-256 hash = "gWZNEOMHQNiULfA/tO5HD2awOwYMA3tnfUPApIr9csk="

STEP 4: Sign and get final manifest
==============================

  data_hash JSON = {
    "exclusions": [{"start": 20, "length": 45884}],
    "alg": "sha256",
    "hash": "gWZNEOMHQNiULfA/tO5HD2awOwYMA3tnfUPApIr9csk=",
    ...
  }

  sign_data_hashed_embeddable(signer, data_hash, "image/jpeg")
                    |
                    v
  Internally:
    1. Update DataHash assertion with real hash + exclusions
    2. Serialize claim to CBOR
    3. Sign CBOR with signer -> COSE Sign1 signature
    4. Find signature placeholder in JUMBF -> patch with real signature
    5. Compose for JPEG format
                    |
                    v
  Returns: [APP11][APP11][APP11]...  (same size as placeholder!)

STEP 5: Patch manifest into asset
==============================

  Write signed manifest bytes at offset 20 (replacing placeholder):

  [SOI][APP0/EXIF][SIGNED_APP11][SIGNED_APP11]...[Image Data...][EOI]
                   ^-- real signed manifest replaces placeholder --^
```

### Comparison: sign() vs Data-Hashed

```
+------------------------------+--------------------------------------+
|      Builder::sign()         |   Data-Hashed Embeddable Flow        |
+------------------------------+--------------------------------------+
|                              |                                      |
|  1 API call                  |  2-4 API calls                       |
|                              |                                      |
|  SDK reads source            |  Caller manages asset I/O            |
|  SDK writes dest             |  Caller embeds manifest              |
|  SDK calculates hash         |  Caller or SDK calculates hash       |
|  SDK handles embedding       |  Caller patches bytes                |
|                              |                                      |
|  Returns: manifest bytes     |  Returns: manifest bytes             |
|  + writes complete output    |  (caller must embed them)            |
|                              |                                      |
|  Simple, opaque              |  Flexible, transparent               |
|  One-shot                    |  Multi-step                          |
|                              |                                      |
+------------------------------+--------------------------------------+
```

---

## 6. The DataHash JSON Structure

The `data_hash` parameter to `sign_data_hashed_embeddable` is a JSON string:

```json
{
  "exclusions": [
    {
      "start": 20,
      "length": 45884
    }
  ],
  "name": "jumbf manifest",
  "alg": "sha256",
  "hash": "gWZNEOMHQNiULfA/tO5HD2awOwYMA3tnfUPApIr9csk=",
  "pad": " "
}
```

### Field-by-field explanation

| Field | Type | Description |
|-------|------|-------------|
| `exclusions` | Array of `{start, length}` | Byte ranges in the asset to **skip** when calculating the hash. Typically one entry: the region where the manifest placeholder was embedded. |
| `exclusions[].start` | Integer | Starting byte offset of the excluded region. |
| `exclusions[].length` | Integer | Length in bytes of the excluded region. Must match `placeholder.size()`. |
| `name` | String | Human-readable label for this hash binding. Typically `"jumbf manifest"`. |
| `alg` | String | Hash algorithm. Usually `"sha256"`. Also supports `"sha384"`, `"sha512"`. |
| `hash` | String (base64) | The actual hash value, base64-encoded. Set to `""` (empty string) if providing an asset stream for the SDK to auto-calculate. |
| `pad` | String | Padding character used for JUMBF alignment. Typically `" "` (space). |

### How exclusions work

When hashing, the algorithm processes the asset bytes sequentially but **skips** any
byte ranges listed in `exclusions`:

```
Asset bytes:   [AAAA][BBBBBBBB][CCCC]
                     ^--------^
                     excluded range
                     start=4, length=8

Hash input:    [AAAA]          [CCCC]
               bytes 0-3       bytes 12-15
               (bytes 4-11 are skipped)
```

The exclusion ranges MUST exactly match where the placeholder was embedded. If they
don't, the hash won't match when a Reader validates the asset, and the manifest will
be reported as invalid.

---

## 7. Anatomy of a Signed Asset

### Byte layout of a signed JPEG

```
+------+----------+-------------------------------------------+-----------------+-----+
| SOI  | APP0/    |        C2PA Manifest (APP11 segments)     |   Image Data    | EOI |
| 0xFFD8| EXIF    |                                           |   (scan data)   |0xFFD9|
|      | markers  |                                           |                 |     |
+------+----------+-------------------------------------------+-----------------+-----+
^      ^          ^                                           ^
|      |          |                                           |
byte 0 byte 2    byte 20 (example)                           byte 20+manifest_size
                  |<---- exclusion range -------------------->|
```

### Inside the APP11 segments (JPEG manifest embedding)

```
[0xFF 0xEB]           APP11 marker
[0xXX 0xXX]           Segment length (big-endian)
[0x4A 0x50]           CI: JPEG extensions identifier
[0x02 0x11]           En: C2PA instance number
[0x00 0x00 0x00 0x01] Z: Sequence number (1, 2, 3, ...)
[JUMBF chunk data]    Up to ~65000 bytes of JUMBF

... repeated for each 65000-byte chunk of the JUMBF ...
```

### JUMBF box hierarchy inside the manifest

```
CAI Superbox (UUID: 12A0E9C8-5AEB-11E1-BF21-0002A5D5C51B)
|
+-- Manifest Store Box (label: "c2pa")
    |
    +-- Claim Box
    |   +-- CBOR Content Box
    |       (Serialized claim referencing all assertions by hash)
    |
    +-- Assertion Store Box
    |   +-- DataHash Assertion Box (CBOR)
    |   |   (exclusions, algorithm, hash value, padding)
    |   +-- c2pa.actions Assertion Box (CBOR)
    |   +-- cawg.training-mining Assertion Box (CBOR)
    |   +-- Thumbnail Assertion Box (binary)
    |   +-- ... other assertions ...
    |
    +-- Signature Box
        +-- CBOR Content Box
            (COSE Sign1 signature over the claim)
```

The signature in the Signature Box covers the CBOR-serialized Claim. The Claim
references each assertion by its hash. The DataHash assertion contains the hash
of the asset (with exclusions). This creates a chain of trust:

```
Signature --> Claim --> DataHash assertion --> Asset bytes
              (signs)   (references)          (hashes)
```

If any link in this chain is broken (asset modified, assertion changed, etc.),
validation fails.

---

## 8. Context, Settings, and How They Propagate

### 8a. Two Ways to Create a Builder

**Recommended: With Context**

```cpp
// Create a context with custom settings
auto context = c2pa::Context::from_json(R"({
    "builder": {
        "thumbnail": { "enabled": false }
    }
})");

// Create builder using context
auto builder = c2pa::Builder(context, manifest_json);
```

Internally:
- C++ calls `c2pa_builder_from_context(context->c_context())`
- Rust creates `Builder::from_shared_context()` with the context's `Arc<Context>`
- Context is stored inside the Rust Builder and used throughout its lifetime

**Deprecated: Without Context**

```cpp
// Legacy pattern — uses thread-local settings
auto builder = c2pa::Builder(manifest_json);  // [[deprecated]]
```

Internally:
- C++ calls `c2pa_builder_from_json(manifest_json)`
- Rust `Builder::from_json()` calls `crate::settings::get_thread_local_settings()`
- Creates a new Context from whatever thread-local settings exist (possibly none)
- Not thread-safe for multi-threaded applications

### 8b. Settings That Affect Data-Hashed Signing

These settings from the Context are used during the data-hashed flow:

| Setting Path | Used During | Effect |
|---|---|---|
| `builder.thumbnail.enabled` | `data_hashed_placeholder()` and `sign_data_hashed_embeddable()` (via `to_store()` → `to_claim()`) | Controls auto-thumbnail generation. Affects manifest size. |
| `builder.intent` | `to_store()` | Create/Edit/Update intent. Affects how the store commits the claim. |
| `builder.claim_generator_info` | `to_claim()` | Overrides the claim generator info in the manifest. |
| `builder.actions.*` | `to_claim()` | Action templates, auto-created actions, auto-placed actions. |
| `builder.generate_c2pa_archive` | `to_archive()` only | Whether to use the new archive format. The new archive format is default and is preferred to the old deprecated format. |
| *(signing settings)* | `sign_data_hashed_embeddable()` (via `sign_claim()`) | Timestamp handling, verification after signing. |

### 8c. Context Propagation Through the Data-Hashed Flow

```
Context::from_json(settings_json)
    |
    v
Builder(context, manifest_json)
    |
    |  C++ stores context as builder_context (shared_ptr)
    |  Rust Builder stores context as Arc<Context>
    |
    |--- data_hashed_placeholder(reserve_size, format)
    |       |
    |       +---> self.to_store()
    |       |       |
    |       |       +---> self.to_claim()
    |       |       |       Uses context for:
    |       |       |         - builder.thumbnail.enabled
    |       |       |         - builder.claim_generator_info
    |       |       |         - builder.actions.templates
    |       |       |         - builder.actions.auto_created_action
    |       |       |         - builder.intent
    |       |       |
    |       |       +---> Store::from_context(&self.context)
    |       |               Store inherits context settings
    |       |
    |       +---> store.get_data_hashed_manifest_placeholder()
    |               Serializes to JUMBF + composes for format
    |
    |--- sign_data_hashed_embeddable(signer, data_hash, format)
    |       |
    |       +---> self.to_store()        <-- same context usage
    |       |
    |       +---> store.get_data_hashed_embeddable_manifest(
    |       |         dh, signer, format, None, &self.context)
    |       |       |
    |       |       +---> prep_embeddable_store()
    |       |       |       Updates DataHash assertion
    |       |       |
    |       |       +---> sign_claim(pc, signer, reserve_size,
    |       |       |         context.settings())
    |       |       |       Settings affect signing behavior
    |       |       |
    |       |       +---> finish_embeddable_store()
    |       |               Patches signature, composes for format
    |       |
    |       +---> Returns signed manifest bytes
    |
    |--- format_embeddable(format, data)    <-- STATIC, no context
            Pure byte transformation, no settings involved
```

### 8d. Archive Context Preservation

This is a critical distinction that affects settings propagation:

**`Builder::from_archive(stream)` — LOSES context**

```cpp
// Static factory method — creates a brand-new Builder with DEFAULT settings
auto builder = c2pa::Builder::from_archive(archive_stream);
// builder has NO custom settings — uses defaults
```

Internally: Creates a new Rust Builder with `Context::new()` (default settings).

**`Builder(context).load_archive(stream)` — PRESERVES context**

```cpp
// Create builder with your context FIRST, then load archive INTO it
auto context = c2pa::Context::from_json(settings_json);
auto builder = c2pa::Builder(context);
builder.load_archive(archive_stream);
// builder retains your custom settings from context
```

Internally: Calls `c2pa_builder_with_archive()`, which calls Rust's `with_archive()`.
This method (builder.rs:1164-1194):
1. Clones your context's settings
2. Creates a temporary context with `verify_after_reading=false` (archives have
   placeholder signatures that would fail verification)
3. Loads the archive using the temporary context
4. Returns a Builder that uses YOUR original context (not the temporary one)

### 8e. Thread-Local Settings (Deprecated)

```cpp
// Old pattern — sets settings globally for the current thread
c2pa::load_settings(R"({"builder": {"thumbnail": {"enabled": false}}})", "json");

// Builder picks up thread-local settings
auto builder = c2pa::Builder(manifest_json);  // Uses whatever load_settings set
```

This is the deprecated path. The Rust SDK's `Builder::from_json()` internally calls
`crate::settings::get_thread_local_settings()` and creates a Context from them.

Problems with this approach:
- **Not thread-safe**: Settings are per-thread, not per-builder. Two threads calling
  `load_settings` with different values will interfere.
- **Implicit state**: No clear ownership of settings. Hard to reason about which
  settings apply to which builder.
- **No sharing**: Can't share the same settings across multiple builders efficiently.

The Context pattern replaces this entirely.

### 8f. When to Use Context with Data-Hashed APIs

| Scenario | Use Context? | Why |
|----------|-------------|-----|
| Disable auto-thumbnails | Yes | `builder.thumbnail.enabled` is a Context setting |
| Custom action templates | Yes | `builder.actions.templates` is a Context setting |
| Multi-threaded application | Yes | Thread-local settings are not thread-safe |
| Archive round-trips | Yes | `from_archive()` loses settings; `load_archive()` preserves them |
| Sharing settings across builders | Yes | `Arc<Context>` enables efficient sharing |
| Just want defaults, single-threaded | Optional | Deprecated `Builder(manifest)` still works |

---

## 9. Limitations

### BMFF Formats Not Supported

**MP4, MOV, and other BMFF-based formats cannot use the data-hashed flow.**

The code in `store.rs:2488-2492` explicitly rejects them:

```rust
if !pc.bmff_hash_assertions().is_empty() {
    return Err(Error::BadParam(
        "BMFF assertions not supported in embeddable manifests".to_string(),
    ));
}
```

BMFF (Box-based Media File Format) encodes box positions and offsets in metadata.
The simple "exclude a byte range" approach doesn't work because moving boxes changes
offsets throughout the file. For BMFF formats, use `sign_box_hashed_embeddable` instead,
which handles box-level hashing.

### CAWG Identity Assertions

CAWG (Creator Assertions Working Group) identity assertions
(`cawg.identity`) require special handling that the data-hashed path doesn't fully
support:

- Identity assertions require async validation (see `reader.rs:226`)
- The identity signing flow requires a `cawg_x509_signer` in the Context settings
  (see `context.rs:504`)
- The C++ test for CAWG identity is disabled (`read_file.test.cpp:75`):
  `/* remove this until we resolve CAWG Identity testing */`

Note: CAWG *data* assertions like `cawg.training-mining` work fine with the
data-hashed flow. It's specifically the `cawg.identity` assertion (which involves
signing a credential) that has limitations.

### Placeholder Must Be Called First

`sign_data_hashed_embeddable` expects that `data_hashed_placeholder` was called first
to set up the DataHash assertion space in the manifest. The placeholder step creates
the assertion with pre-allocated exclusion entries. Skipping it will result in an error:
"Claim must have hash binding assertion."

### Size Mismatch Causes Failure

If the final signed manifest is a different size than the placeholder, the signature
patching will fail with `CoseSigboxTooSmall`. This can happen if:
- You change the Builder's manifest definition between placeholder and sign
- You use a different signer with a different `reserve_size`
- You add/remove assertions between the two calls

### Builder Is Consumed After Signing

`sign_data_hashed_embeddable` internally converts the Builder to a Store. After calling
it, the Builder should be considered consumed. Creating a new Builder is required for
additional signing operations.

### Context Lost on from_archive

The static `Builder::from_archive(stream)` creates a default context. If you need
custom settings after loading an archive, you must use `Builder(context).load_archive(stream)`.

---

## 10. Pseudocode Flow Overview

### Recommended Pattern (with Context)

```
// 1. CREATE CONTEXT with your settings
context = Context::from_json({
    "builder": { "thumbnail": { "enabled": false } }
})

// 2. CREATE BUILDER with context and manifest definition
builder = Builder(context, manifest_json)

// 3. CREATE SIGNER
signer = Signer("Es256", certs, private_key, timestamp_url)

// 4. GET PLACEHOLDER — tells you the exact size of the final manifest
placeholder = builder.data_hashed_placeholder(signer.reserve_size(), "image/jpeg")

// 5. EMBED PLACEHOLDER IN ASSET
//    Copy the source asset, inserting placeholder at a known offset
//    For JPEG: insert after SOI marker (typically at byte offset 2 or 20)
asset_copy = insert_bytes(source_asset, placeholder, offset=20)

// 6. BUILD DATA HASH JSON
data_hash = {
    "exclusions": [{ "start": 20, "length": placeholder.size() }],
    "name": "jumbf manifest",
    "alg": "sha256",
    "hash": "",           // empty = let SDK calculate from asset stream
    "pad": " "
}

// 7. SIGN
asset_stream = open(asset_copy)
manifest = builder.sign_data_hashed_embeddable(
    signer, data_hash, "application/c2pa", &asset_stream
)

// 8. FORMAT FOR EMBEDDING (only needed because we used "application/c2pa")
embeddable = Builder::format_embeddable("image/jpeg", manifest)

// 9. PATCH MANIFEST INTO ASSET
//    Write embeddable bytes at the same offset where placeholder was
write_bytes(asset_copy, embeddable, offset=20)

// DONE: asset_copy is now a fully signed C2PA asset
```

### Deprecated Pattern (without Context)

```
// Same flow, but using deprecated constructors
builder = Builder(manifest_json)   // Uses thread-local settings
signer = Signer(...)
placeholder = builder.data_hashed_placeholder(signer.reserve_size(), "image/jpeg")
// ... rest is identical ...
```

---

## 11. Complete C++ Example

```cpp
#include "c2pa.hpp"
#include <fstream>
#include <sstream>
#include <vector>
#include <cstring>

void sign_with_data_hashed_embeddable(
    const std::string& manifest_json,
    const std::string& certs,
    const std::string& private_key,
    const std::string& source_path)
{
    // 1. Create context (disable thumbnails for this example)
    auto context = c2pa::Context::from_json(R"({
        "builder": { "thumbnail": { "enabled": false } }
    })");

    // 2. Create builder with context
    auto builder = c2pa::Builder(context, manifest_json);

    // 3. Create signer
    c2pa::Signer signer("Es256", certs, private_key,
                         "http://timestamp.digicert.com");

    // 4. Get placeholder
    auto placeholder = builder.data_hashed_placeholder(
        signer.reserve_size(), "image/jpeg");

    // 5. Build data hash JSON with empty hash (SDK will calculate)
    //    The exclusion start/length must match where you embed the placeholder
    std::string data_hash = R"({
        "exclusions": [
            {
                "start": 20,
                "length": )" + std::to_string(placeholder.size()) + R"(
            }
        ],
        "name": "jumbf manifest",
        "alg": "sha256",
        "hash": "",
        "pad": " "
    })";

    // 6. Open source asset
    std::ifstream asset(source_path, std::ios::binary);
    if (!asset) {
        throw std::runtime_error("Failed to open: " + source_path);
    }

    // 7. Sign — SDK calculates hash from asset stream
    auto manifest_data = builder.sign_data_hashed_embeddable(
        signer, data_hash, "application/c2pa", &asset);

    // 8. Convert from raw c2pa to JPEG-embeddable format
    auto embeddable = c2pa::Builder::format_embeddable(
        "image/jpeg", manifest_data);

    // embeddable now contains the signed manifest formatted as JPEG APP11
    // segments, ready to be written into a JPEG file at the exclusion offset.
    //
    // In a real application, you would:
    //   a. Read the source JPEG
    //   b. Insert 'embeddable' at the correct offset
    //   c. Write the result to a new file
}
```

---

## 12. Suggested Test Plan

The following tests should be added to `tests/embeddable_workflows.test.cpp` (new test file). Each test is described
with its purpose, approach, and pseudocode.

### Test 1: TestDataHashedFullEmbedAndRead

Two variations for this test: 
1) Sign A.jpg (does have existing C2PA metada)
2) Sign C.jpg (has existing C2PA, check it gets preserved in the end manifest)

**Purpose**: End-to-end test. Embed placeholder, hash, sign, patch, then read back.

```
Setup:  Load manifest, certs, key. Create signer, builder.
Step 1: Get placeholder for "image/jpeg"
Step 2: Read source JPEG into memory
Step 3: Insert placeholder at known offset in the JPEG copy
Step 4: Build data_hash JSON with exclusion = {offset, placeholder.size()}
Step 5: Open the modified JPEG as a stream
Step 6: Call sign_data_hashed_embeddable with asset stream, format="image/jpeg"
Step 7: Patch signed manifest into the JPEG copy at the same offset
Step 8: Create a Reader from the patched JPEG
Step 9: Verify: reader.json() contains expected assertions
Verify: No C2paException thrown
Verify: Reader validation status is "Valid" (or not "Invalid")
```

### Test 2: TestDataHashedPreCalculatedHash

**Purpose**: Verify signing with a pre-calculated hash (no asset stream).

```
Setup:  Load manifest, certs, key. Create signer, builder.
Step 1: Get placeholder
Step 2: Pre-calculate a hash (you may need to look how c2pa-rs does). Build data_hash JSON with a known hash value (pre-calculated)
Step 3: Call sign_data_hashed_embeddable(signer, data_hash, "image/jpeg")
        — no asset stream
Verify: Returns non-empty byte vector
Verify: No exception thrown
```

### Test 3: TestDataHashedAutoHashWithAsset

**Purpose**: Verify the SDK correctly calculates hash from an asset stream.

```
Setup:  Load manifest, certs, key. Create signer, builder.
Step 1: Get placeholder
Step 2: Build data_hash JSON with hash="" (empty)
Step 3: Open source image as ifstream
Step 4: Call sign_data_hashed_embeddable(signer, data_hash,
        "application/c2pa", &asset)
Verify: Returns non-empty byte vector
Verify: No exception thrown
```

### Test 4: TestFormatEmbeddableRoundTrip

**Purpose**: Verify format conversion from raw c2pa to JPEG-embeddable.

```
Setup:  Same as Test 3 — sign with format="application/c2pa"
Step 1: Get raw manifest bytes from sign_data_hashed_embeddable
Step 2: Call format_embeddable("image/jpeg", raw_bytes)
Verify: embeddable.size() > raw_bytes.size()
        (JPEG APP11 container adds overhead)
Verify: No exception thrown
```

### Test 5: TestPlaceholderSizeMatchesFinal

**Purpose**: Verify the critical invariant that placeholder size equals final size.

Two variations for this test: 
1) With A.jpg (does have existing C2PA metada)
2) With C.jpg (has existing C2PA, check it gets preserved in the end manifest)


```
Setup:  Same as Test 1 — full embed flow with format="image/jpeg"
Step 1: Get placeholder. Record size.
Step 2: Complete the signing flow with same format
Step 3: Get signed manifest bytes
Verify: signed_manifest.size() == placeholder.size()
```

### Test 6: TestDataHashedWithContext

**Purpose**: Verify context settings propagate through data-hashed flow.

```
Setup:  Create context with thumbnails DISABLED
Step 1: Build using context: Builder(context, manifest_json)
Step 2: Get placeholder, sign, format, embed (full flow)
Step 3: Read the signed asset with a Reader
Verify: Reader JSON does NOT contain a thumbnail assertion
```

### Test 7: TestDataHashedWithoutContext

**Purpose**: Verify deprecated path still works.

```
Setup:  Do NOT create a context
Step 1: Build using: Builder(manifest_json)   [deprecated]
Step 2: Get placeholder, sign (basic flow)
Verify: Returns valid manifest bytes
Verify: No exception thrown
```

### Test 8: TestDataHashedArchiveRoundTripWithContext

Multiple variations for this test: 
1) With A.jpg (does have existing C2PA metada)
2) With C.jpg (has existing C2PA, check it gets preserved in the end manifest)
3) With A.jpg on a Builder as ingredient too
4) With C.jpg on a builder as ingredient too

**Purpose**: Verify settings survive archive round-trip with proper pattern.

```
Setup:  Create context with thumbnails DISABLED
Step 1: Builder(context, manifest_json)
Step 2: builder.to_archive(archive_stream)
Step 3: auto builder2 = Builder(context)    <-- same context
Step 4: builder2.load_archive(archive_stream)
Step 5: Perform data-hashed signing flow with builder2
Step 6: Read result with Reader
Verify: Settings preserved (no thumbnail in output)
```

### Test 9: TestDataHashedMultipleFormats

**Purpose**: Verify data-hashed flow works for JPEG and PNG.

```
For each format in ["image/jpeg", "image/png"]:
    Step 1: Get placeholder for format
    Step 2: Sign with matching format
    Verify: Returns non-empty bytes
    Verify: No exception
```

### Test 10: TestDataHashedMissingPlaceholder

**Purpose**: Verify error when skipping the placeholder step.

```
Setup:  Create builder with manifest (no DataHash assertion)
Step 1: Skip data_hashed_placeholder entirely
Step 2: Try sign_data_hashed_embeddable directly
Verify: Throws C2paException
        ("Claim must have hash binding assertion")
```
