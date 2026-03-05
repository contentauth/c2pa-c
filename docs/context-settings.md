# Configuring the SDK with Context and Settings

This guide explains how to configure the C2PA SDK using `Context` and `Settings`. The configuration controls SDK behavior including verification, trust anchors, thumbnails, signing, and more.

## Quick start

The simplest way to configure the SDK is to create a `Context` with inline JSON and pass it to `Reader` or `Builder`:

```cpp
#include "c2pa.hpp"

// Create a Context with settings
c2pa::Context context(R"({
  "version": 1,
  "builder": {
    "claim_generator_info": {"name": "My App", "version": "1.0"},
    "thumbnail": {"enabled": false}
  },
  "verify": {
    "remote_manifest_fetch": false
  }
})");

// Use with Reader or Builder
c2pa::Reader reader(context, "image.jpg");
c2pa::Builder builder(context, manifest_json);
```

For default SDK configuration, just create an empty `Context`:

```cpp
c2pa::Context context;  // Uses SDK defaults
c2pa::Reader reader(context, "image.jpg");
```

## Understanding Context and Settings

### What is Context?

`Context` encapsulates SDK configuration that controls how `Reader`, `Builder`, and other components operate:

- **Settings**: Verification options, trust configuration, builder behavior, thumbnails, and more
- **Signer configuration**: Optional signing credentials that can be stored for reuse
- **State isolation**: Each `Context` is independent, allowing multiple configurations to coexist

### Why use Context?

`Context` provides explicit, isolated configuration:

- **Makes dependencies explicit**: Configuration is passed directly to `Reader` and `Builder`, not hidden in global state
- **Enables multiple configurations**: Run different configurations simultaneously (e.g., development with test certificates, production with strict validation)
- **Eliminates thread-local state**: No subtle bugs from shared state
- **Simplifies testing**: Isolated configurations without cleanup or interference
- **Improves code clarity**: `Builder(context, manifest)` shows configuration is being used

> [!NOTE]
> The deprecated `c2pa::load_settings(data, format)` still works but you should migrate to `Context`. See [Migrating from thread-local Settings](#migrating-from-thread-local-settings).

### Context lifecycle

- **Non-copyable, moveable**: `Context` can be moved but not copied. After moving, `is_valid()` returns `false` on the source
- **Used at construction**: `Reader` and `Builder` copy configuration at construction time. The `Context` doesn't need to outlive them
- **Reusable**: Use the same `Context` to create multiple readers and builders

```cpp
c2pa::Context context(settings);

// All three use the same configuration
c2pa::Builder builder1(context, manifest1);
c2pa::Builder builder2(context, manifest2);
c2pa::Reader reader(context, "image.jpg");

// Context can go out of scope, readers/builders still work
```

### Settings format

Settings use JSON or TOML format. The schema is shared across all SDK languages (Rust, C/C++, Python). JSON is preferred for C++.

```cpp
// JSON (preferred)
c2pa::Context context(R"({"verify": {"verify_after_sign": true}})");

// TOML
c2pa::Settings settings(R"([verify]
verify_after_sign = true)", "toml");
c2pa::Context context(settings);
```

## Creating a Context

There are several ways to create a `Context`:

### 1. SDK defaults

```cpp
c2pa::Context context;  // Uses SDK defaults
```

**When to use:** Quick prototyping or when defaults are sufficient.

### 2. From inline JSON

```cpp
c2pa::Context context(R"({
  "version": 1,
  "verify": {"verify_after_sign": true},
  "builder": {"claim_generator_info": {"name": "My App"}}
})");
```

**When to use:** Simple configuration that doesn't need to be shared across the codebase.

### 3. From a Settings object

```cpp
c2pa::Settings settings;
settings.set("builder.thumbnail.enabled", "false");
settings.set("verify.verify_after_sign", "true");
settings.update(R"({"builder": {"claim_generator_info": {"name": "My App"}}})");

c2pa::Context context(settings);
```

**When to use:** Configuration that needs runtime logic or incremental construction.

### 4. Using ContextBuilder

```cpp
c2pa::Settings base_settings;
base_settings.set("builder.thumbnail.enabled", "true");

auto context = c2pa::Context::ContextBuilder()
    .with_settings(base_settings)
    .with_json(R"({"verify": {"verify_after_sign": true}})")
    .with_json_settings_file("config/overrides.json")
    .create_context();
```

**When to use:** Loading from files or combining multiple configuration sources. Don't use for single sources—direct construction is simpler.

> [!IMPORTANT]
> Later configuration overrides earlier configuration. In the example above, `overrides.json` will override values from `base_settings` and the inline JSON.

**ContextBuilder methods:**

| Method | Description |
|--------|-------------|
| `with_settings(settings)` | Apply a `Settings` object |
| `with_json(json_string)` | Apply settings from a JSON string |
| `with_json_settings_file(path)` | Load and apply settings from a JSON file |
| `create_context()` | Build and return the `Context` (consumes the builder) |

## Settings API

The `Settings` class provides methods for creating and manipulating configuration:

| Method | Description |
|--------|-------------|
| `Settings()` | Create default settings |
| `Settings(data, format)` | Parse settings from a string. Format is `"json"` or `"toml"` |
| `set(path, json_value)` | Set a value by dot-separated path (e.g., `"verify.verify_after_sign"`). Value must be JSON-encoded. Returns `*this` for chaining |
| `update(data)` | Merge JSON configuration (same as `update(data, "json")`) |
| `update(data, format)` | Merge configuration from a string with specified format |
| `is_valid()` | Returns `true` if the object is valid (not moved-from) |

**Important notes:**

- Settings are **moveable, not copyable**. After moving, `is_valid()` returns `false` on the source
- `set()` and `update()` can be chained for sequential configuration
- Later calls override earlier ones (last wins)

## Common configuration patterns

### Development with test certificates

Trust self-signed or custom CA certificates during development:

```cpp
std::string test_ca = read_file("test-ca.pem");

c2pa::Context dev_context(R"({
  "version": 1,
  "trust": {
    "user_anchors": ")" + test_ca + R"("
  },
  "verify": {
    "remote_manifest_fetch": false,
    "ocsp_fetch": false
  },
  "builder": {
    "claim_generator_info": {"name": "Dev Build", "version": "dev"},
    "thumbnail": {"enabled": false}
  }
})");
```

### Configuration from environment variables

Adapt configuration based on runtime environment:

```cpp
std::string env = std::getenv("ENVIRONMENT") ? std::getenv("ENVIRONMENT") : "dev";

c2pa::Settings settings;
if (env == "production") {
    settings.update(read_file("config/production.json"), "json");
    settings.set("verify.strict_v1_validation", "true");
} else {
    settings.update(read_file("config/development.json"), "json");
    settings.set("verify.remote_manifest_fetch", "false");
}

c2pa::Context context(settings);
```

### Layered configuration

Load base configuration and apply environment-specific overrides:

```cpp
auto context = c2pa::Context::ContextBuilder()
    .with_json_settings_file("config/base.json")
    .with_json_settings_file("config/" + environment + ".json")
    .with_json(R"({
      "builder": {
        "claim_generator_info": {"version": ")" + app_version + R"("}
      }
    })")
    .create_context();
```

### Multiple contexts

Use different `Context` objects for different purposes:

```cpp
c2pa::Context dev_context(dev_settings);
c2pa::Context prod_context(prod_settings);

c2pa::Builder dev_builder(dev_context, manifest);
c2pa::Builder prod_builder(prod_context, manifest);
```

### Temporary contexts

Since configuration is copied at construction, you can use temporary contexts:

```cpp
c2pa::Reader reader(
    c2pa::Context(R"({"verify": {"remote_manifest_fetch": false}})"),
    "image.jpg"
);
```

## Settings reference

### Settings structure

Settings JSON has this top-level structure:

```json
{
  "version": 1,
  "trust": { ... },
  "cawg_trust": { ... },
  "core": { ... },
  "verify": { ... },
  "builder": { ... },
  "signer": { ... },
  "cawg_x509_signer": { ... }
}
```

| Property | Description |
|----------|-------------|
| `version` | Settings format version (must be 1) |
| [`trust`](#trust-configuration) | Certificate trust configuration for C2PA validation |
| [`cawg_trust`](#cawg-trust-configuration) | Certificate trust configuration for CAWG identity assertions |
| [`core`](#core-settings) | Core SDK behavior and performance tuning |
| [`verify`](#verify-settings) | Validation and verification behavior |
| [`builder`](#builder-settings) | Manifest creation and embedding behavior |
| [`signer`](#signer-configuration) | C2PA signer configuration |
| [`cawg_x509_signer`](#cawg-x509-signer-configuration) | CAWG identity assertion signer configuration |

The `version` property must be `1`. All other properties are optional.

> [!IMPORTANT]
> If you don't specify a property, the SDK uses the default value. If you specify `null`, the property is explicitly set to null (not the default). This distinction matters when overriding default behavior.

For Boolean values, use JSON `true` and `false`, not the strings `"true"` and `"false"`.

### Trust configuration

The [`trust` properties](https://opensource.contentauthenticity.org/docs/manifest/json-ref/settings-schema/#trust) control which certificates are trusted when validating C2PA manifests.

| Property | Type | Description | Default |
|----------|------|-------------|---------|
| `trust.user_anchors` | string | Additional root certificates (PEM format). Adds custom CAs without replacing built-in trust anchors. Recommended for development. | — |
| `trust.allowed_list` | string | Explicitly allowed certificates (PEM format). Trusted regardless of chain validation. Use for development/testing to bypass validation. | — |
| `trust.trust_anchors` | string | Default trust anchor root certificates (PEM format). **Replaces** the SDK's built-in trust anchors entirely. | — |
| `trust.trust_config` | string | Allowed Extended Key Usage (EKU) OIDs for certificate purposes (e.g., document signing: `1.3.6.1.4.1.311.76.59.1.9`). | — |

#### Using `user_anchors` (recommended for development)

Add your test root CA without replacing the SDK's default trust store:

```cpp
std::string test_root_ca = R"(-----BEGIN CERTIFICATE-----
MIICEzCCAcWgAwIBAgIUW4fUnS38162x10PCnB8qFsrQuZgwBQYDK2VwMHcxCzAJ
...
-----END CERTIFICATE-----)";

c2pa::Context context(R"({
  "version": 1,
  "trust": {
    "user_anchors": ")" + test_root_ca + R"("
  }
})");

c2pa::Reader reader(context, "signed_asset.jpg");
```

#### Using `allowed_list` (quick testing)

Bypass chain validation by explicitly allowing a specific certificate:

```cpp
std::string test_cert = read_file("test_cert.pem");

c2pa::Settings settings;
settings.update(R"({
  "version": 1,
  "trust": {
    "allowed_list": ")" + test_cert + R"("
  }
})");

c2pa::Context context(settings);
c2pa::Reader reader(context, "signed_asset.jpg");
```

#### Loading trust from a file

```json
{
  "version": 1,
  "trust": {
    "user_anchors": "-----BEGIN CERTIFICATE-----\nMIICEzCCA...\n-----END CERTIFICATE-----",
    "trust_config": "1.3.6.1.4.1.311.76.59.1.9\n1.3.6.1.4.1.62558.2.1"
  }
}
```

**PEM format requirements:**

- Use literal `\n` (as two-character strings) in JSON for line breaks
- Include the full certificate chain if needed
- Concatenate multiple certificates into a single string

Load in your application:

```cpp
auto context = c2pa::Context::ContextBuilder()
    .with_json_settings_file("dev_trust_config.json")
    .create_context();

c2pa::Reader reader(context, "signed_asset.jpg");
```

### CAWG trust configuration

The `cawg_trust` properties configure CAWG (Creator Assertions Working Group) validation of identity assertions in C2PA manifests. The structure is identical to [`trust`](#trust-configuration).

> [!NOTE]
> CAWG trust settings only apply when processing identity assertions with X.509 certificates. If your workflow doesn't use CAWG identity assertions, these settings have no effect.

### Verify settings

The [`verify` properties](https://opensource.contentauthenticity.org/docs/manifest/json-ref/settings-schema/#verify) control how the SDK validates C2PA manifests.

**Key properties (all default to `true`):**

| Property | Description | Default |
|----------|-------------|---------|
| `verify_after_reading` | Automatically verify manifests when reading assets | `true` |
| `verify_after_sign` | Automatically verify manifests after signing (recommended) | `true` |
| `verify_trust` | Verify signing certificates against trust anchors | `true` |
| `verify_timestamp_trust` | Verify timestamp authority (TSA) certificates | `true` |
| `remote_manifest_fetch` | Fetch remote manifests referenced in the asset | `true` |
| `ocsp_fetch` | Fetch OCSP responses for certificate validation | `false` |
| `skip_ingredient_conflict_resolution` | Skip ingredient conflict resolution | `false` |
| `strict_v1_validation` | Enable strict C2PA v1 validation | `false` |

> [!WARNING]
> Disabling `verify_trust` or `verify_timestamp_trust` makes verification non-compliant with the C2PA specification. Only modify in controlled environments or with specific requirements.

#### Offline or air-gapped environments

Disable network-dependent features:

```cpp
c2pa::Context context(R"({
  "version": 1,
  "verify": {
    "remote_manifest_fetch": false,
    "ocsp_fetch": false
  }
})");

c2pa::Reader reader(context, "signed_asset.jpg");
```

#### Fast development iteration

Disable verification for faster iteration:

```cpp
// WARNING: Only use during development, not in production!
c2pa::Settings dev_settings;
dev_settings.set("verify.verify_after_reading", "false");
dev_settings.set("verify.verify_after_sign", "false");

c2pa::Context dev_context(dev_settings);
```

#### Strict validation

Enable all validation features for certification or compliance testing:

```cpp
c2pa::Context context(R"({
  "version": 1,
  "verify": {
    "strict_v1_validation": true,
    "ocsp_fetch": true,
    "verify_trust": true,
    "verify_timestamp_trust": true
  }
})");

c2pa::Reader reader(context, "asset_to_validate.jpg");
auto validation_result = reader.json();
```

### Builder settings

The [`builder` properties](https://opensource.contentauthenticity.org/docs/manifest/json-ref/settings-schema/#buildersettings) control how the SDK creates and embeds C2PA manifests.

#### Claim generator information

Identifies your application in the C2PA manifest:

| Property | Description |
|----------|-------------|
| `claim_generator_info.name` | Application name (required, e.g., `"My Photo Editor"`) |
| `claim_generator_info.version` | Application version (recommended, e.g., `"2.1.0"`) |
| `claim_generator_info.icon` | Icon in C2PA format (optional) |
| `claim_generator_info.operating_system` | OS identifier or `"auto"` to auto-detect (optional) |

**Example:**

```cpp
c2pa::Context context(R"({
  "version": 1,
  "builder": {
    "claim_generator_info": {
      "name": "My Photo Editor",
      "version": "2.1.0",
      "operating_system": "auto"
    }
  }
})");

c2pa::Builder builder(context, manifest_json);
```

#### Thumbnail settings

The [`builder.thumbnail`](https://opensource.contentauthenticity.org/docs/manifest/json-ref/settings-schema/#thumbnailsettings) properties control automatic thumbnail generation:

| Property | Description | Default |
|----------|-------------|---------|
| `enabled` | Enable/disable thumbnail generation | `true` |
| `long_edge` | Maximum size in pixels for the long edge | `1024` |
| `quality` | Quality level: `"low"`, `"medium"`, or `"high"` | `"medium"` |
| `format` | Output format (null for auto-detect) | `null` |
| `prefer_smallest_format` | Prefer smallest format by file size | `true` |
| `ignore_errors` | Continue if thumbnail generation fails | `true` |

**Examples:**

```cpp
// Disable thumbnails for batch processing
c2pa::Context no_thumbnails(R"({
  "builder": {
    "claim_generator_info": {"name": "Batch Processor"},
    "thumbnail": {"enabled": false}
  }
})");

// Customize for mobile (smaller size, lower quality)
c2pa::Context mobile_thumbnails(R"({
  "builder": {
    "thumbnail": {
      "long_edge": 512,
      "quality": "low",
      "prefer_smallest_format": true
    }
  }
})");
```

#### Action tracking settings

Control automatic action generation:

| Property | Description | Default |
|----------|-------------|---------|
| `actions.auto_created_action.enabled` | Add `c2pa.created` action for new content | `true` |
| `actions.auto_created_action.source_type` | Source type for created action | `"empty"` |
| `actions.auto_opened_action.enabled` | Add `c2pa.opened` action when opening content | `true` |
| `actions.auto_placed_action.enabled` | Add `c2pa.placed` action when placing content | `true` |

#### Builder intent

Set the purpose of the claim:

| Property | Description | Default |
|----------|-------------|---------|
| `builder.intent` | Claim intent: `{"Create": "digitalCapture"}`, `{"Edit": null}`, or `{"Update": null}` | `null` |

**Example: Original digital capture (photos from camera)**

```cpp
c2pa::Context camera_context(R"({
  "version": 1,
  "builder": {
    "intent": {"Create": "digitalCapture"},
    "claim_generator_info": {"name": "Camera App", "version": "1.0"}
  }
})");
```

**Example: Editing existing content**

```cpp
c2pa::Context editor_context(R"({
  "version": 1,
  "builder": {
    "intent": {"Edit": null},
    "claim_generator_info": {"name": "Photo Editor", "version": "2.0"}
  }
})");
```

#### Other builder settings

| Property | Description | Default |
|----------|-------------|---------|
| `builder.generate_c2pa_archive` | Generate content in C2PA archive format | `true` |
| `builder.certificate_status_fetch` | Certificate status fetch behavior | `null` |
| `builder.certificate_status_should_override` | Override certificate status | `null` |
| `builder.created_assertion_labels` | Custom assertion labels | `null` |

### Core settings

The [`core` properties](https://opensource.contentauthenticity.org/docs/manifest/json-ref/settings-schema/#core) control SDK behavior and performance tuning:

| Property | Description | Default |
|----------|-------------|---------|
| `merkle_tree_chunk_size_in_kb` | Merkle tree chunk size | `null` |
| `merkle_tree_max_proofs` | Maximum merkle tree proofs | `5` |
| `backing_store_memory_threshold_in_mb` | Memory threshold for backing store | `512` |
| `decode_identity_assertions` | Decode identity assertions | `true` |
| `allowed_network_hosts` | Allowed network hosts for SDK requests | `null` |

**Use cases:**

- **Performance tuning for large files**: Set `backing_store_memory_threshold_in_mb` to `2048` or higher for large video files with sufficient RAM
- **Restricted network environments**: Set `allowed_network_hosts` to limit which domains the SDK can contact

### Signer configuration

The [`signer` properties](https://opensource.contentauthenticity.org/docs/manifest/json-ref/settings-schema/#signersettings) configure the C2PA signer. Set to `null` if you provide the signer at runtime, or configure as **local** or **remote** in settings.

> [!NOTE]
> In C++, you typically create a `c2pa::Signer` explicitly and pass it to `Builder::sign()`. Settings-based signing is useful when you need the same configuration across multiple operations or when loading from files.

#### Local signer

Use when you have direct access to the private key and certificate. See [signer.local](https://opensource.contentauthenticity.org/docs/manifest/json-ref/settings-schema/#signerlocal) for all properties.

**Example: Local signer with ES256**

```cpp
std::string config = R"({
  "version": 1,
  "signer": {
    "local": {
      "alg": "es256",
      "sign_cert": "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----",
      "private_key": "-----BEGIN PRIVATE KEY-----\n...\n-----END PRIVATE KEY-----",
      "tsa_url": "http://timestamp.digicert.com"
    }
  }
})";

c2pa::Context context(config);
c2pa::Builder builder(context, manifest_json);
builder.sign(source_path, dest_path);  // Uses signer from context
```

#### Remote signer

Use when the private key is on a secure signing service (HSM, cloud KMS). See [signer.remote](https://opensource.contentauthenticity.org/docs/manifest/json-ref/settings-schema/#signerremote) for all properties.

The signing service receives a POST request with data to sign and must return the signature:

```cpp
c2pa::Context context(R"({
  "version": 1,
  "signer": {
    "remote": {
      "url": "https://signing-service.example.com/sign",
      "alg": "ps256",
      "sign_cert": "-----BEGIN CERTIFICATE-----\n...\n-----END CERTIFICATE-----",
      "tsa_url": "http://timestamp.digicert.com"
    }
  }
})");
```

#### Explicit signer (typical C++ approach)

For full programmatic control, create a `Signer` and pass it to `Builder::sign()`:

```cpp
c2pa::Signer signer("es256", certs_pem, private_key_pem, "http://timestamp.digicert.com");
c2pa::Builder builder(context, manifest_json);
builder.sign(source_path, dest_path, signer);
```

The `Context` controls verification and builder options. The signer is used only for the cryptographic signature.

### CAWG X.509 signer configuration

The `cawg_x509_signer` property configures signing for identity assertions. It has the same structure as `signer` (local or remote).

**When to use:** To sign identity assertions separately from the main C2PA claim. When both `signer` and `cawg_x509_signer` are configured, the SDK uses a dual signer:

- Main claim signature from `signer`
- Identity assertions signed with `cawg_x509_signer`

**Example: Dual signer configuration**

```cpp
c2pa::Context context(R"({
  "version": 1,
  "signer": {
    "local": {
      "alg": "es256",
      "sign_cert": "...",
      "private_key": "..."
    }
  },
  "cawg_x509_signer": {
    "local": {
      "alg": "ps256",
      "sign_cert": "...",
      "private_key": "..."
    }
  }
})");
```

## Default configuration

The complete default configuration with all properties and default values:

```json
{
  "version": 1,
  "builder": {
    "claim_generator_info": null,
    "created_assertion_labels": null,
    "certificate_status_fetch": null,
    "certificate_status_should_override": null,
    "generate_c2pa_archive": true,
    "intent": null,
    "actions": {
      "all_actions_included": null,
      "templates": null,
      "actions": null,
      "auto_created_action": {
        "enabled": true,
        "source_type": "empty"
      },
      "auto_opened_action": {
        "enabled": true,
        "source_type": null
      },
      "auto_placed_action": {
        "enabled": true,
        "source_type": null
      }
    },
    "thumbnail": {
      "enabled": true,
      "ignore_errors": true,
      "long_edge": 1024,
      "format": null,
      "prefer_smallest_format": true,
      "quality": "medium"
    }
  },
  "cawg_trust": {
    "verify_trust_list": true,
    "user_anchors": null,
    "trust_anchors": null,
    "trust_config": null,
    "allowed_list": null
  },
  "cawg_x509_signer": null,
  "core": {
    "merkle_tree_chunk_size_in_kb": null,
    "merkle_tree_max_proofs": 5,
    "backing_store_memory_threshold_in_mb": 512,
    "decode_identity_assertions": true,
    "allowed_network_hosts": null
  },
  "signer": null,
  "trust": {
    "user_anchors": null,
    "trust_anchors": null,
    "trust_config": null,
    "allowed_list": null
  },
  "verify": {
    "verify_after_reading": true,
    "verify_after_sign": true,
    "verify_trust": true,
    "verify_timestamp_trust": true,
    "ocsp_fetch": false,
    "remote_manifest_fetch": true,
    "skip_ingredient_conflict_resolution": false,
    "strict_v1_validation": false
  }
}
```

## Using Context with Reader

`Reader` uses `Context` to control validation, trust configuration, network access, and performance.

> [!IMPORTANT]
> `Context` is used only at construction. `Reader` copies the configuration it needs internally, so the `Context` doesn't need to outlive the `Reader`.

### Reading from a file

```cpp
c2pa::Context context(R"({
  "version": 1,
  "verify": {
    "remote_manifest_fetch": false,
    "ocsp_fetch": false
  }
})");

c2pa::Reader reader(context, "image.jpg");
std::cout << reader.json() << std::endl;
```

### Reading from a stream

```cpp
std::ifstream stream("image.jpg", std::ios::binary);
c2pa::Reader reader(context, "image/jpeg", stream);
std::cout << reader.json() << std::endl;
```

## Using Context with Builder

`Builder` uses `Context` to control manifest creation, signing, thumbnails, and verification.

> [!IMPORTANT]
> The `Context` is used only when constructing the `Builder`. It copies the configuration internally, so the `Context` doesn't need to outlive the `Builder`.

### Basic usage

```cpp
c2pa::Context context(R"({
  "version": 1,
  "builder": {
    "claim_generator_info": {"name": "My App", "version": "1.0"},
    "intent": {"Create": "digitalCapture"}
  }
})");

c2pa::Builder builder(context, manifest_json);

// Pass signer explicitly at signing time
c2pa::Signer signer("es256", certs, private_key);
builder.sign(source_path, output_path, signer);
```

## Migrating from thread-local Settings

The legacy function `c2pa::load_settings(data, format)` sets thread-local settings. This function is deprecated; use `Context` instead.

| Aspect | load_settings (legacy) | Context |
|--------|------------------------|---------|
| Scope | Global / thread-local | Per Reader/Builder, passed explicitly |
| Multiple configs | Awkward (per-thread) | One context per configuration |
| Testing | Shared global state | Isolated contexts per test |

**Deprecated approach:**

```cpp
// Thread-local settings
std::ifstream config_file("settings.json");
std::string config((std::istreambuf_iterator<char>(config_file)), std::istreambuf_iterator<char>());
c2pa::load_settings(config, "json");
c2pa::Reader reader("image/jpeg", stream);  // uses thread-local settings
```

**Current approach:**

```cpp
c2pa::Context context(settings_json_string);
c2pa::Reader reader(context, "image/jpeg", stream);
```

If you still use `load_settings`, construct `Reader` or `Builder` without a context to use the thread-local settings. Prefer passing a context for new code.

## See also

- [Usage](usage.md): Reading and signing with Reader and Builder
- [Rust SDK settings](https://github.com/contentauth/c2pa-rs/blob/main/docs/settings.md): Shared settings schema and additional JSON examples
- [CAI settings schema reference](https://opensource.contentauthenticity.org/docs/manifest/json-ref/settings-schema/): Complete schema reference
