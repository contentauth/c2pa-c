# c2pa-c
C and C++ binding for C2PA Content Authenticity Initiative (CAI) library.

This library enables you to read and validate C2PA data in supported media files and add signed manifests to supported media files.

## Installation

Add prebuilt library and header files to your project


## Usage

### Include

Include the C2PA header as follows:

```cpp
#include "c2pa.hpp"
```

### Read and validate C2PA data in a file

Use the `read_file` function to read C2PA data from the specified file:

```c++
auto json_store = C2pa::read_file("path/to/media_file.jpg", "path/to/data_dir")
```

This function examines the specified media file for C2PA data and generates a JSON report of any data it finds. If there are validation errors, the report includes a `validation_status` field.  For a summary of supported media types, see [Supported file formats](#supported-file-formats).

A media file may contain many manifests in a manifest store. The most recent manifest is identified by the value of the `active_manifest` field in the manifests map.

If the optional `data_dir` is provided, the function extracts any binary resources, such as thumbnails, icons, and C2PA data into that directory. These files are referenced by the identifier fields in the manifest store report.

NOTE: For a comprehensive reference to the JSON manifest structure, see the [CAI manifest store reference](https://contentauth.github.io/json-manifest-reference/manifest-reference).

### Create a SignerInfo Instance

A `SignerInfo` object contains information about a signature.  To create an instance of `SignerInfo`, first set up the signer information from the public and private key `.pem` files as follows:

```cpp
string certs = read_text_file("path/to/certs.pem").data();
string private_key = read_text_file("path/to/private.key").data();
```

Then create a new `SignerInfo` instance using the keys as follows, specifying the signing algorithm used and optionally a time stamp authority URL:

```c++
C2paSignerInfo sign_info = {.alg = "es256",  .sign_cert = certs.c_str(), .private_key = private_key.c_str(), .ta_url = "http://timestamp.digicert.com"};
```

For the list of supported signing algorithms, see [Creating and using an X.509 certificate](https://opensource.contentauthenticity.org/docs/c2patool/x_509).

### Creating a manifest JSON definition

The manifest JSON string defines the C2PA manifest to add to the file.

A sample JSON manifest is provided in tests/fixtures/training.json.

```c++
const std::string manifest_json = R"{
    "claim_generator": "c2pa_c_test/0.1",
    "claim_generator_info": [
      {
        "name": "c2pa-c test",
        "version": "0.1"
      }
    ],
    "assertions": [
    {
      "label": "c2pa.training-mining",
      "data": {
        "entries": {
          "c2pa.ai_generative_training": { "use": "notAllowed" },
          "c2pa.ai_inference": { "use": "notAllowed" },
          "c2pa.ai_training": { "use": "notAllowed" },
          "c2pa.data_mining": { "use": "notAllowed" }
        }
      }
    }
  ]
 };
```

### Add a signed manifest to a media file

Use the `sign_file` function to add a signed manifest to a media file.

```c++
result = C2pa::sign_file("path/to/source.jpg", 
                         "path/to/dest.jpg",
                         manifest_json.c_str(),
                         sign_info,
                         "path/to/data_dir);
                          "target/example/training.jpg", manifest_json.c_str(), sign_info, NULL);

```

The parameters (in order) are:
- The source (original) media file.
- The destination file that will contain a copy of the source file with the manifest data added.
- `manifest_json`, a JSON-formatted string containing the manifest data you want to add; see [Creating a manifest JSON definition file](#creating-a-manifest-json-definition-file) below.
- `sign_info`, a `SignerInfo` object instance; see [Generating SignerInfo](#generating-signerinfo) below.
- `data_dir` optionally specifies a directory path from which to load resource files referenced in the manifest JSON identifier fields; for example, thumbnails, icons, and manifest data for ingredients.

See [training.cpp](https://github.com/contentauth/c2pa-c/blob/main/examples/training.py) for an example.

## Development

Requires Rust tools installed
Also depends on Gnu make

```
make release
```

### Testing

The repo includes a custom set of unit tests that can be invoked via:

```
make test
```

## Supported file formats

 | Extensions    | MIME type                                           |
 |---------------| --------------------------------------------------- |
 | `avi`         | `video/msvideo`, `video/avi`, `application-msvideo` |
 | `avif`        | `image/avif`                                        |
 | `c2pa`        | `application/x-c2pa-manifest-store`                 |
 | `dng`         | `image/x-adobe-dng`                                 |
 | `heic`        | `image/heic`                                        |
 | `heif`        | `image/heif`                                        |
 | `jpg`, `jpeg` | `image/jpeg`                                        |
 | `m4a`         | `audio/mp4`                                         |
 | `mp3`         | `"audio/mpeg"`                                      |
 | `mp4`         | `video/mp4`, `application/mp4` <sup>*</sup>         |
 | `mov`         | `video/quicktime`                                   |
 | `pdf`         | `application/pdf`  <sup>**</sup>                    |
 | `png`         | `image/png`                                         |
 | `svg`         | `image/svg+xml`                                     |
 | `tif`,`tiff`  | `image/tiff`                                        |
 | `wav`         | `audio/x-wav`                                       |
 | `webp`        | `image/webp`                                        |

<sup>*</sup> Fragmented mp4 is not yet supported.
<sup>**</sup> Read only

## License

This package is distributed under the terms of both the [MIT license](https://github.com/contentauth/c2pa-rs/blob/main/LICENSE-MIT) and the [Apache License (Version 2.0)](https://github.com/contentauth/c2pa-rs/blob/main/LICENSE-APACHE).

Note that some components and dependent crates are licensed under different terms; please check the license terms for each crate and component for details.

### Contributions and feedback

We welcome contributions to this project.  For information on contributing, providing feedback, and about ongoing work, see [Contributing](https://github.com/contentauth/c2pa-js/blob/main/CONTRIBUTING.md).


