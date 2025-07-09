# C2PA C API

This describes a C API using an older model. This API will be deprecated in favor of a newer Builder/Reader model API.
Most applications should use the C++ API

### C
Add the prebuilt library and header file `include/c2pa.h` to your project and add the dynamic library to your library path.

For instructions on how to build the library and run the tests and examples, see [Development](#development).

### Read and validate C2PA data in a file

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
C2paSignerInfo sign_info = {"es256", 
                            certs.c_str(), 
                            private_key.c_str(), 
                            "http://timestamp.digicert.com"};
```

For the list of supported signing algorithms, see [Creating and using an X.509 certificate](https://opensource.contentauthenticity.org/docs/c2patool/x_509).

**WARNING**: Do not access a private key and certificate directly like this in production  because it's not secure. Instead use a hardware security module (HSM) and optionally a Key Management Service (KMS) to access the key; for example as show in the [C2PA Python Example](https://github.com/contentauth/c2pa-python-example).


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