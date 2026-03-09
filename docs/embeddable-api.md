# Embeddable Signing API

> [!WARNING]
> The embeddable signing API is for advanced use cases that require fine-grained, low-level control over manifest embedding. The standard `Builder::sign()` method handles the full signing and embedding pipeline automatically and is the recommended approach for most use cases. The embeddable API should only be used when the application needs to manage the embedding process directly. With this level of control comes additional responsibility: callers must ensure that each step is performed correctly and in the right order.

> [!IMPORTANT]
> The embeddable APIs require the **Signer to be attached to the Context** via `Context::ContextBuilder::with_signer()` or through a signer configuration in the JSON settings. Calling `placeholder()` or `sign_embeddable()` without a signer on the Context will throw a `C2paException`.

The embeddable signing API provides direct, fine-grained control over how a C2PA manifest is embedded into an asset. Instead of letting the SDK manage everything by providing both the source and destination to `Builder::sign()`, the caller performs each step explicitly, in the following order:

1. Create a placeholder.
2. Embed the placeholder into the asset.
3. Hash the asset.
4. Sign the claim.
5. Patch the manifest in place.

## Why use the embeddable API

The standard `Builder::sign()` handles the full pipeline internally:

```cpp
// Standard approach: the SDK controls all I/O
auto manifest_bytes = builder.sign(source_path, output_path, signer);
```

The standard approach works for most use cases. The embeddable API exists for situations where the application requires explicit control over embedding, for example:

- The application controls its own I/O pipeline. Video transcoders, streaming ingestion services, and other tools have their own asset-writing code. Transferring stream ownership to the SDK conflicts with that architecture.
- The asset is too large to buffer. The SDK's `sign()` may re-read large files. With the embeddable API, the application can hash chunks as it writes them and pass the results directly to the builder.
- The application needs in-place patching. When using the placeholder workflow, `sign_embeddable()` returns a signed manifest that is byte-for-byte the same size as the placeholder. The caller can then overwrite the placeholder region in the file without changing the overall file size or shifting any surrounding data.

## Concepts

### Hard-binding modes

The embeddable API supports three hard-binding strategies, selected automatically based on format and settings:

| Mode | Assertion | Formats | Requires placeholder |
|------|-----------|---------|----------------------|
| [DataHash](#using-the-datahash-placeholder) | `DataHash` | JPEG, PNG, GIF, WebP, and others | Yes |
| [BmffHash](#using-the-bmffhash-placeholder) | `BmffHash` | MP4, video (BMFF containers), AVIF, HEIF/HEIC | Yes |
| [BoxHash](#using-boxhash-directly) | `BoxHash` | JPEG, PNG, GIF, WebP, and others | No |

Call `needs_placeholder()` on the `Builder` to decide which workflow to follow. It returns `true` when the format requires a placeholder step (DataHash or BmffHash) and `false` when the format supports BoxHash and `prefer_box_hash` is enabled in the settings.

Use the following decision tree to select the correct workflow:

```mermaid
flowchart TD
    Start["Builder.needs_placeholder(format)"] --> Check{Returns true?}
    Check -->|Yes| IsBmff{BMFF format?<br/>e.g. video/mp4, image/avif}
    IsBmff -->|Yes| BmffWorkflow["Use BmffHash placeholder workflow"]
    IsBmff -->|No| DataHashWorkflow["Use DataHash placeholder workflow"]
    Check -->|No| BoxHashWorkflow["Use BoxHash direct workflow<br/>No placeholder needed"]
```

To use `BoxHash` mode, enable `prefer_box_hash` in Builder settings. These formats support chunk-based hashing. `BoxHash` mode inserts the manifest as an independent chunk so byte offsets of existing data are never disturbed, removing the need for a pre-sized placeholder.

The `prefer_box_hash` setting can be provided in a JSON settings file:

```json
{
    "builder": {
        "prefer_box_hash": true
    }
}
```

Or set programmatically when building the Context:

```cpp
auto context = c2pa::Context::ContextBuilder()
    .with_signer(std::move(signer))
    .with_json(R"({
        "builder": {
            "prefer_box_hash": true
        }
    })")
    .create_context();
```

### Placeholder sizing

When a placeholder is required, the SDK pre-sizes the JUMBF manifest based on its current state and records the target length internally. After signing, `sign_embeddable()` pads the compressed manifest to exactly that length so the caller can overwrite the placeholder bytes without shifting any other data in the file.

### Signer on Context

Unlike `Builder::sign()` where a `Signer` is passed explicitly, the embeddable APIs obtain the signer (and its reserve size) from the Builder's Context. The signer must be attached when building the Context.

There are two ways to attach a signer to the Context:
- [Programmatically via ContextBuilder](#attaching-a-signer-programmatically-via-contextbuilder)
- [Via JSON settings](#attaching-signer-via-json-settings)

#### Attaching a signer programmatically via ContextBuilder:

```cpp
// Create a Signer
c2pa::Signer signer("Es256", certs, private_key, "http://timestamp.digicert.com");

// Attach it to the Context via ContextBuilder
auto context = c2pa::Context::ContextBuilder()
    .with_signer(std::move(signer))  // Signer is moved into the Context
    .create_context();

// The Builder now has access to the signer through its Context
auto builder = c2pa::Builder(context, manifest_json);
```

> [!NOTE]
> `with_signer()` consumes the `Signer` via move semantics. The `Signer` object is no longer valid after this call and must not be used after it has been moved.

#### Attaching signer via JSON settings:

The signer can also be configured in a JSON settings file or string. The following skeleton shows the structure; replace the placeholder values with actual PEM-encoded certificates and keys:

```json
{
    "signer": {
        "local": {
            "alg": "es256",
            "sign_cert": "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----\n",
            "private_key": "-----BEGIN PRIVATE KEY-----\n...\n-----END PRIVATE KEY-----\n"
        }
    }
}
```

Then load the settings into a Context:

```cpp
// From a JSON settings file
auto context = c2pa::Context::ContextBuilder()
    .with_json_settings_file("config/signer_settings.json")
    .create_context();

// Or from a Settings object loaded programmatically
auto settings = c2pa::Settings(settings_json_string, "json");
c2pa::Context context(settings);
```

## API summary

All methods listed below are called on a `Builder` instance.

### Workflow selection

These methods determine which workflow to follow based on the asset format and settings.

| Method | Description |
|--------|-------------|
| `Builder::needs_placeholder(format)` | Returns `true` when the format requires a pre-embedded placeholder before hashing. Always `true` for BMFF formats. Returns `false` when `prefer_box_hash` is enabled and the format supports `BoxHash`, or when a `BoxHash` assertion has already been added. Use this to choose between the placeholder and direct workflows. |

### Signing and embedding

These methods perform the signing workflow: placeholder creation, hashing, and signing.

| Method | Description |
|--------|-------------|
| `Builder::placeholder(format)` | Composes a placeholder manifest and returns it as format-specific bytes ready to embed (e.g., JPEG APP11 segments). Automatically adds the appropriate hash assertion (`BmffHash` for BMFF formats, `DataHash` for others). Stores the JUMBF length internally so `sign_embeddable()` can pad to the same size. |
| `Builder::set_data_hash_exclusions(exclusions)` | Replaces the dummy exclusion ranges in the `DataHash` assertion with the actual byte offset and length of the embedded placeholder. Call after embedding placeholder bytes and before `update_hash_from_stream()`. Takes a `std::vector<std::pair<uint64_t, uint64_t>>` of (start, length) pairs. |
| `Builder::update_hash_from_stream(format, stream)` | Reads the asset stream and computes the hard-binding hash. Automatically selects the appropriate path based on format: `BmffHash` for BMFF (skips manifest box), `BoxHash` for chunk-based formats (creates assertion if needed), or `DataHash` (skips exclusion ranges). Takes a `std::istream&`. |
| `Builder::set_bmff_mdat_hashes(leaf_hashes)` | Provides pre-computed Merkle leaf hashes for `mdat` segments in BMFF assets. Use when the application already hashes `mdat` chunks during writing/transcoding to avoid re-reading large files. Call before `sign_embeddable()`. |
| `Builder::sign_embeddable(format)` | Signs the manifest and returns bytes ready to embed. For placeholder workflows, the output is padded to match the placeholder size for in-place patching. For BoxHash workflows, the output is the actual signed manifest size (not padded), suitable for appending as a new chunk. |

## Workflows

### Using the DataHash placeholder

Use this workflow for JPEG, PNG, and other non-BMFF formats.

For this workflow, `prefer_box_hash` must not be enabled — this is the default. If it was previously set to `true`, disable it explicitly:

```json
{
    "builder": {
        "prefer_box_hash": false
    }
}
```

Or programmatically when building the Context:

```cpp
auto context = c2pa::Context::ContextBuilder()
    .with_signer(std::move(signer))
    .with_json(R"({
        "builder": {
            "prefer_box_hash": false
        }
    })")
    .create_context();
```

#### DataHash flow

```mermaid
flowchart TD
    A[Create Context with Signer] --> B[Create Builder]
    B --> C{"Builder.needs_placeholder(format)?"}
    C -->|Yes| D["Builder.placeholder(format)"]
    D --> E[Embed placeholder bytes into asset at chosen offset]
    E --> F["Builder.set_data_hash_exclusions(offset, size)"]
    F --> G["Builder.update_hash_from_stream(format, stream)"]
    G --> H["Builder.sign_embeddable(format)"]
    H --> I[Overwrite placeholder with signed manifest]

    style A fill:#e1f5fe
    style H fill:#c8e6c9
    style I fill:#c8e6c9
```

#### DataHash builder state transitions

```mermaid
stateDiagram-v2
    [*] --> Initialized : Builder(context, manifest_json)
    Initialized --> PlaceholderCreated : placeholder(format)
    PlaceholderCreated --> ExclusionsSet : set_data_hash_exclusions(ranges)
    ExclusionsSet --> Hashed : update_hash_from_stream(format, stream)
    Hashed --> Signed : sign_embeddable(format)
    Signed --> [*]

    note right of PlaceholderCreated
        placeholder size is stored internally
    end note

    note right of Signed
        signed manifest size == placeholder size
    end note
```

#### DataHash example

```cpp
#include <fstream>
#include "c2pa.hpp"

// Set up signing infrastructure. The signer must be on the Context.
auto context = c2pa::Context::ContextBuilder()
    .with_signer(c2pa::Signer("Es256", certs, private_key, "http://timestamp.digicert.com"))
    .create_context();

auto builder = c2pa::Builder(context, manifest_json);

// 1. Check if a placeholder is required for this format.
if (builder.needs_placeholder("image/jpeg")) {

    // 2. Get the placeholder bytes. The size is committed internally.
    auto placeholder_bytes = builder.placeholder("image/jpeg");

    // 3. Construct the output, inserting the placeholder after the JPEG SOI marker.
    //    The embedding code controls where the placeholder goes.
    auto source_bytes = read_file("input.jpg");  // application file-reading utility
    uint64_t insert_offset = 2;  // after SOI marker
    std::vector<uint8_t> output;
    output.insert(output.end(), source_bytes.begin(), source_bytes.begin() + insert_offset);
    output.insert(output.end(), placeholder_bytes.begin(), placeholder_bytes.end());
    output.insert(output.end(), source_bytes.begin() + insert_offset, source_bytes.end());

    // Write to a temporary file for stream-based hashing.
    std::ofstream tmp("output.jpg", std::ios::binary);
    tmp.write(reinterpret_cast<const char*>(output.data()), output.size());
    tmp.close();

    // 4. Tell the builder where the placeholder lives.
    builder.set_data_hash_exclusions({{insert_offset, placeholder_bytes.size()}});

    // 5. Hash the asset. The placeholder bytes are excluded from the hash.
    std::ifstream asset_stream("output.jpg", std::ios::binary);
    builder.update_hash_from_stream("image/jpeg", asset_stream);
    asset_stream.close();

    // 6. Sign. The returned bytes are the same size as placeholder_bytes.
    auto final_manifest = builder.sign_embeddable("image/jpeg");

    // 7. Overwrite the placeholder with the signed manifest.
    std::fstream patched("output.jpg", std::ios::binary | std::ios::in | std::ios::out);
    patched.seekp(insert_offset);
    patched.write(reinterpret_cast<const char*>(final_manifest.data()), final_manifest.size());
    patched.close();
}
```

### Using the BmffHash placeholder

Use this workflow with MP4 and other BMFF formats, which always require a placeholder. The `prefer_box_hash` setting has no effect on BMFF formats: they always use `BmffHash` regardless of the setting. No special Builder settings are required as the SDK selects `BmffHash` automatically based on the format.

BMFF containers (ISO Base Media File Format) store media data in `mdat` (media data) boxes, which hold the raw audio, video, and other media samples. These `mdat` boxes can be very large, making it expensive to re-read the entire file to compute a hash after signing. The SDK addresses this with a Merkle tree structure in the `BmffHash` assertion: it divides `mdat` content into fixed-size chunks and computes a hash for each chunk (a "leaf" in the tree). These leaf hashes are combined into a Merkle root hash that covers the entire `mdat` content. The `placeholder()` method pre-allocates slots in the `BmffHash` assertion for these Merkle leaf hashes, sized according to the `core.merkle_tree_chunk_size_in_kb` setting. This pre-allocation is necessary because the final manifest must be exactly the same size as the placeholder for in-place patching to work.

#### BmffHash flow

```mermaid
flowchart TD
    A[Create Context with Signer] --> B[Create Builder]
    B --> C["Builder.placeholder(video/mp4)"]
    C --> D[Insert placeholder uuid box into MP4 container]
    D --> E["Builder.update_hash_from_stream(video/mp4, stream)"]
    E --> F["Builder.sign_embeddable(video/mp4)"]
    F --> G[Overwrite placeholder with signed manifest]

    style A fill:#e1f5fe
    style F fill:#c8e6c9
    style G fill:#c8e6c9
```

#### BmffHash builder state transitions

```mermaid
stateDiagram-v2
    [*] --> Initialized : Builder(context, manifest_json)
    Initialized --> PlaceholderCreated : placeholder(format)
    PlaceholderCreated --> Hashed : update_hash_from_stream(format, stream)
    Hashed --> Signed : sign_embeddable(format)
    Signed --> [*]

    note right of PlaceholderCreated
        BmffHash handles exclusion of
        manifest box automatically
    end note
```

#### BmffHash example

```cpp
// Set up context with signer.
auto context = c2pa::Context::ContextBuilder()
    .with_signer(c2pa::Signer("Es256", certs, private_key, "http://timestamp.digicert.com"))
    .create_context();

auto builder = c2pa::Builder(context, manifest_json);

// 1. Compose the placeholder. Returns a BMFF uuid box suitable for insertion.
auto placeholder_bytes = builder.placeholder("video/mp4");

// 2. Insert the placeholder box into the MP4 container at an appropriate location
//    (for example, before the mdat box). The muxer/container writer controls this step.
uint64_t insert_offset = your_muxer.insert_manifest_box(placeholder_bytes);

// 3. Hash the asset. BmffHash handles exclusion of the manifest box automatically.
std::ifstream asset_stream("output.mp4", std::ios::binary);
builder.update_hash_from_stream("video/mp4", asset_stream);
asset_stream.close();

// 4. Sign and patch in place.
auto final_manifest = builder.sign_embeddable("video/mp4");
std::fstream patched("output.mp4", std::ios::binary | std::ios::in | std::ios::out);
patched.seekp(insert_offset);
patched.write(reinterpret_cast<const char*>(final_manifest.data()), final_manifest.size());
patched.close();
```

If the application already hashes `mdat` chunks during writing or transcoding, it can pass those pre-computed leaf hashes directly to the builder via `set_bmff_mdat_hashes()` to avoid re-reading the file:

```cpp
// leaf_hashes: outer = tracks, middle = chunks, inner = hash bytes
builder.set_bmff_mdat_hashes(leaf_hashes);
auto final_manifest = builder.sign_embeddable("video/mp4");
```

### Using BoxHash directly

Use this workflow when no placeholder is needed. No placeholder is written: the manifest is appended as a new independent chunk after signing.

For this workflow, `prefer_box_hash` must be enabled in Builder settings. This can be set in a JSON settings file:

```json
{
    "builder": {
        "prefer_box_hash": true
    }
}
```

Or programmatically when building the Context:

```cpp
auto context = c2pa::Context::ContextBuilder()
    .with_signer(std::move(signer))
    .with_json(R"({
        "builder": {
            "prefer_box_hash": true
        }
    })")
    .create_context();
```

#### BoxHash flow

```mermaid
flowchart TD
    A[Create Context with Signer + prefer_box_hash] --> B[Create Builder]
    B --> C{"Builder.needs_placeholder(format)?"}
    C -->|No| D["Builder.update_hash_from_stream(format, source_stream)"]
    D --> E["Builder.sign_embeddable(format)"]
    E --> F[Append manifest bytes as new chunk in asset]

    style A fill:#e1f5fe
    style E fill:#c8e6c9
    style F fill:#c8e6c9
```

#### BoxHash builder state transitions

```mermaid
stateDiagram-v2
    [*] --> Initialized : Builder(context, manifest_json)
    Initialized --> Hashed : update_hash_from_stream(format, stream)
    Hashed --> Signed : sign_embeddable(format)
    Signed --> [*]

    note right of Initialized
        No placeholder step needed.
        BoxHash hashes each chunk individually.
    end note
```

#### BoxHash example

```cpp
// Enable BoxHash via context settings.
auto context = c2pa::Context::ContextBuilder()
    .with_signer(c2pa::Signer("Es256", certs, private_key, "http://timestamp.digicert.com"))
    .with_json(R"({
        "builder": {
            "prefer_box_hash": true
        }
    })")
    .create_context();

auto builder = c2pa::Builder(context, manifest_json);

// needs_placeholder returns false for BoxHash-capable formats.
assert(!builder.needs_placeholder("image/jpeg"));

// No placeholder step. Hash the original asset directly.
std::ifstream source("input.jpg", std::ios::binary);
builder.update_hash_from_stream("image/jpeg", source);
source.close();

// Sign. Because there is no placeholder to match, the output is the actual
// signed manifest size without any padding.
auto manifest_bytes = builder.sign_embeddable("image/jpeg");

// Append manifest_bytes as a new independent chunk in the asset.
// The exact mechanism depends on the format handler used by the embedding code.
```

## Class relationships

This is a partial class diagram showing only the classes and methods relevant to the embeddable APIs. For the full API reference, see the [c2pa.hpp](../include/c2pa.hpp) header file.

```mermaid
classDiagram
    class Signer {
        +Signer(alg, sign_cert, private_key, tsa_uri)
        +reserve_size() size_t
    }

    class ContextBuilder {
        +with_signer(Signer&&) ContextBuilder&
        +with_json(string) ContextBuilder&
        +with_settings(Settings) ContextBuilder&
        +with_json_settings_file(path) ContextBuilder&
        +create_context() Context
    }

    class Context {
        +ContextBuilder : nested class
    }
    note for Context "When using embeddable APIs, the context MUST have a Signer <br> (set either explicitly using with_signer, or coming from settings)."

    class Builder {
        +Builder(Context&, manifest_json)
        +needs_placeholder(format) bool
        +placeholder(format) vector~uint8~
        +set_data_hash_exclusions(exclusions) void
        +set_bmff_mdat_hashes(leaf_hashes) void
        +update_hash_from_stream(format, stream) void
        +sign_embeddable(format) vector~uint8~
    }

    Signer --o ContextBuilder : moved into via with_signer()
    ContextBuilder --> Context : creates via create_context()
    Context --> Builder : passed to constructor
```
