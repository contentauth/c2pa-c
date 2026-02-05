# C2PA C API

This describes a C API using an older model. This API will be deprecated in favor of a newer Builder/Reader model API.
Most applications should use the C++ API

### C

TBD: This is totally obsolete now with prebuilt libraries and the C FFI moving to c2pa-rs. But we can salvage sentences from this readme to put them in other places.

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