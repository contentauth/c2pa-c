# Using the C++ library

To use this library, include the header file in your code as follows:

```cpp
#include "c2pa.hpp"
```

## Read and validate an istream 

Use the `Reader` constructor to read C2PA data from a stream. This constructor examines the specified stream for C2PA data in the given format and its return value is a Reader that can be used to extract more information. Exceptions are thrown on errors.

```cpp
  auto reader = c2pa::Reader(<"FORMAT">, <"STREAM">);
```
The parameters are:

- `<FORMAT>`- A MIME string format for the stream; must be one of the [supported file formats](supported-formats.md).
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

## Creating a manifest JSON definition

The manifest JSON string defines the C2PA manifest to add to the file.

A sample JSON manifest is provided in [tests/fixtures/training.json](https://github.com/contentauth/c2pa-c/blob/main/tests/fixtures/training.json).

For example:
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

## Creating a Builder

Use the `Builder` constructor to create a `Builder` instance.

```cpp
  auto builder = Builder("<MANIFEST_JSON>");
```
The parameter is:
- `<MANIFEST_JSON>`- A string in JSON format as as described above, defining the manifest to be generated.

For example:

```cpp
  auto builder = Builder(manifest_json);
```

## Creating a Signer

For testing you can create a signer using any supported algorithm by with Signer constructor.
There are multiple forms of constructors. But in this example we show how to create a signer with
a public and private key.

```cpp
  Signer signer = Signer("<SIGNING_ALG>", "<PUBLIC_CERTS>",  "<PRIVATE_KEY>", "<TIMESTAMP_URL>");
```
The parameters are:
- `<SIGNING_ALG>`- The `C2paSigningAlg` from `c2pa.h` associated with the signing function.
- `<PUBLIC_CERTS>`- A buffer containing the public cert chain in PEM format.
- `<PRIVATE_KEY>`- A buffer containing the private_key in PEM format.
- `<TIMESTAMP_URL>`- An optional parameter containing a URL to a public Time Stamp Authority service.

For example:
```cpp
Signer signer = c2pa::Signer("Es256", certs, private_key, "http://timestamp.digicert.com");
```

## Signing and embedding a manifest

```cpp
  auto manifest_data = builder.sign(image_path, output_path, signer);
```

The parameters are:
- `<SOURCE_ASSET>`- A file path or an istream referencing the asset to sign.
- `<OUTPUT_ASSET>`- A file path or an iostream referencing the asset to generate.
- `<SIGNER>` - A `Signer` instance.

For example: 
```cpp
  auto manifest_data = builder.sign("source_asset.jpg", "output_asset.jpg", signer);
```

## More examples

The simple C++ example in [`examples/training.cpp`](https://github.com/contentauth/c2pa-c/blob/main/examples/training.cpp) uses the [JSON for Modern C++](https://json.nlohmann.me/) library class.

Build and run the example by entering this `make` command:

```
make examples
```

This example adds the manifest [`tests/fixtures/training.json`](https://github.com/contentauth/c2pa-c/blob/main/tests/fixtures/training.json) to the image file [`tests/fixtures/A.jpg`](https://github.com/contentauth/c2pa-c/blob/main/tests/fixtures/A.jpg) using the sample private key and certificate in the [`tests/fixtures`](https://github.com/contentauth/c2pa-c/tree/main/tests/fixtures) directory.

The example displays some text to standard out that summarizes whether AI training is allowed based on the specified manifest and then saves the resulting image file with attached manifest to `target/example/training.jpg`.
