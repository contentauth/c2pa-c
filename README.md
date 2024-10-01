# c2pa-c

This library implements C and C++ APIs that:
- Read and validate C2PA data from media files in [supported formats](#supported-file-formats).
- Add signed manifests to media files in [supported formats](#supported-file-formats).

This documentation assumes you are using this library with C++.

**WARNING**: This is a prerelease version of this library.  There may be bugs and unimplemented features, and the API is subject to change.

## Installation

### C++
We build with CMake and you can use FetchContent like this:
```FetchContent_Declare(
    c2pa_cpp
    GIT_REPOSITORY https://github.com/contentauth/c2pa-c.git
    GIT_TAG gpeacock/cmake_work`
)
FetchContent_MakeAvailable(c2pa_cpp)
```
And then add:
```"${c2pa_cpp_SOURCE_DIR}/include"```
to your include path.

### C
Add the prebuilt library and header file `include/c2pa.h` to your project and add the dynamic library to your library path.

For instructions on how to build the library and run the tests and examples, see [Development](#development) below.

## Usage

To use this library, include the header file in your code as follows:

```cpp
#include "c2pa.hpp"
```

### Read and validate an iostream 

Use the `Reader` constructor to read C2PA data from a stream. This constructor examines the specified stream for C2PA data in the given format and its return value is a Reader that can be used to extract more information. Exceptions are thrown on errors.

```cpp
  auto reader = c2pa::Reader(<"FORMAT">, <"STREAM">);
```
Where:

- `<FORMAT>`- A MIME string format for the stream [supported file formats](#supported-file-formats).
- `<STREAM>` - An open readable iostream.
For example:

```cpp
std::ifstream ifs("tests/fixtures/C.jpg", std::ios::binary);

// the Reader supports streams or file paths
auto reader = c2pa::Reader("image/jpeg", ifs);

// print out the Manifest Store information
printf("Manifest Store = %s", reader.json())

// write the thumbnail into a file
std::ofstream ofs("test_thumbail.jpg", std::ios::binary);
reader.get_resource("self#jumbf=c2pa.assertions/c2pa.thumbnail.claim.jpeg", ofs);
ifs.close();
```

### Read and validate C2PA data in a file using the 

Use the `read_file` function to read C2PA data from the specified file. This function examines the specified asset file for C2PA data and its return value is a JSON report if it finds C2PA data. If there are validation errors, the report includes a `validation_status` field. Exceptions are thrown on errors.

```cpp
auto json_store = C2pa::read_file("<ASSET_FILE>", "<DATA_DIR>")
```

Where:

- `<ASSET_FILE>`- The asset file to read; The file must be one of the [supported file formats](#supported-file-formats).
- `<DATA_DIR>` - Optional path to data output directory; If provided, the function extracts any binary resources, such as thumbnails, icons, and C2PA data into that directory. These files are referenced by the identifier fields in the manifest store report.

For example:

```cpp
auto json_store = C2pa::read_file("work/media_file.jpg", "output/data_dir")
```

A media file may contain many manifests in a manifest store. The `active_manifest` property in the manifest store identifies the most recently-added manifest.  For a comprehensive reference to the JSON manifest structure, see the [CAI manifest store reference](https://opensource.contentauthenticity.org/docs/manifest/manifest-ref).

### Create a SignerInfo instance

A `SignerInfo` object contains information about a signature.  To create an instance of `SignerInfo`, first set up the signer information from the public and private key files. For example, using the simple `read_text_file` function defined in the [`training.cpp` example](https://github.com/contentauth/c2pa-c/blob/main/examples/training.cpp): 

```cpp
string certs = read_text_file("path/to/certs.pem").data();
string private_key = read_text_file("path/to/private.key").data();
```

Then create a new `SignerInfo` instance using the keys, specifying the signing algorithm in the `.alg` field and optionally a time stamp authority URL in the `.ta_url` field:

```cpp
C2paSignerInfo sign_info = {.alg = "es256", 
                            .sign_cert = certs.c_str(), 
                            .private_key = private_key.c_str(), 
                            .ta_url = "http://timestamp.digicert.com"};
```

For the list of supported signing algorithms, see [Creating and using an X.509 certificate](https://opensource.contentauthenticity.org/docs/c2patool/x_509).

**WARNING**: Do not access a private key and certificate directly like this in production  because it's not secure. Instead use a hardware security module (HSM) and optionally a Key Management Service (KMS) to access the key; for example as show in the [C2PA Python Example](https://github.com/contentauth/c2pa-python-example).

### Creating a manifest JSON definition

The manifest JSON string defines the C2PA manifest to add to the file.

A sample JSON manifest is provided in [tests/fixtures/training.json](https://github.com/contentauth/c2pa-c/blob/main/tests/fixtures/training.json).

```cpp
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


```cpp
C2pa::sign_file("<ASSET_FILE>", 
                         "<SIGNED_ASSET_FILE>",
                         manifest_json.c_str(),
                         &sign_info,
                         "<DATA_DIR>");
```

The parameters are:
- `<ASSET_FILE>`- Path to the source (original) asset file to read; The file must be one of the [supported file formats](#supported-file-formats).
- `<SIGNED_ASSET_FILE>` - Path to the destination asset file that will contain a copy of the source asset with the manifest data added.
- `manifest_json` - An string containing a JSON-formatted manifest data to add to the asset; see [Creating a manifest JSON definition file](#creating-a-manifest-json-definition-file).
- `sign_info` - A valid `SignerInfo` object instance; see [Generating SignerInfo](#generating-signerinfo).
- `<DATA_DIR>` - Optional path to data directory from which to load resource files referenced in the manifest JSON; for example, thumbnails, icons, and manifest data for ingredients.  

Exceptions will be thrown on errors.

For example:

```cpp
C2pa::sign_file("path/to/source.jpg", 
                         "path/to/dest.jpg",
                         manifest_json.c_str(),
                         sign_info,
                         "path/to/data_dir");
```

See [training.cpp](https://github.com/contentauth/c2pa-c/blob/main/examples/training.cpp) for an example.

## Development

This project has been tested on macOS and should also work on common Linux distributions.

### Prerequisites

If you haven't already done so, install [Rust](https://www.rust-lang.org/tools/install).

Install [cbindgen](https://github.com/mozilla/cbindgen/blob/master/docs.md):

```sh
cargo install --force cbindgen
```

The c unit tests require Ninja. Installation instructions are here:

https://github.com/ninja-build/ninja/wiki/Pre-built-Ninja-packages

### Building 

Building the library requires [GNU make](https://www.gnu.org/software/make/), which is installed on most macOS systems.

Enter this command to build the C library: 

```
make release
```

The Makefile also has numerous other targets:
- `test-cpp` to build and run the C++ tests.
- `test-c` to build and run the C tests.
- `check-format`, `clippy`, and `test-rust` for Rust linting and testing.
- `test` to run all of the above targets.
- `example` to build and run the C++ example.
- `all` to run everything.

Results are saved in the `target` directory.

### Testing

Build the [unit tests](https://github.com/contentauth/c2pa-c/tree/main/tests) by entering this `make` command:

```
make test
```

### Example

The simple C++ example in [`examples/training.cpp`](https://github.com/contentauth/c2pa-c/blob/main/examples/training.cpp) uses the [JSON for Modern C++](https://json.nlohmann.me/) library class.

Build and run the example by entering this `make` command:

```
make example
```

This example adds the manifest [`tests/fixtures/training.json`](https://github.com/contentauth/c2pa-c/blob/main/tests/fixtures/training.json) to the image file [`tests/fixtures/A.jpg`](https://github.com/contentauth/c2pa-c/blob/main/tests/fixtures/A.jpg) using the sample private key and certificate in the [`tests/fixtures`](https://github.com/contentauth/c2pa-c/tree/main/tests/fixtures) directory.

The example displays some text to standard out that summarizes whether AI training is allowed based on the specified manifest and then saves the resulting image file with attached manifest to `target/example/training.jpg`.

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

This package is distributed under the terms of both the [MIT license](https://github.com/contentauth/c2pa-c/blob/main/LICENSE-MIT) and the [Apache License (Version 2.0)](https://github.com/contentauth/c2pa-c/blob/main/LICENSE-APACHE).

Note that some components and dependent crates are licensed under different terms; please check the license terms for each crate and component for details.

### Contributions and feedback

We welcome contributions to this project.  For information on contributing, providing feedback, and about ongoing work, see [Contributing](https://github.com/contentauth/c2pa-c/blob/main/CONTRIBUTING.md).


