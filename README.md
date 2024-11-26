# C2PA C++ library

[The c2pa-c repository](https://github.com/contentauth/c2pa-c) implements C++ APIs that:
- Read and validate C2PA data from media files in supported formats.
- Add signed manifests to media files in supported formats.

**WARNING**: This is a prerelease version of this library.  There may be bugs and unimplemented features, and the API is subject to change.

<div style={{display: 'none'}}>

For the best experience, read the docs on the [CAI Open Source SDK documentation website](https://opensource.contentauthenticity.org/docs/c2pa-c).  If you want to view the documentation in GitHub, see:
- [Using the C++ library](docs/usage.md)
- [Supported formats](https://github.com/contentauth/c2pa-rs/blob/crandmck/reorg-docs/docs/supported-formats.md)

</div>

## Installation

### CMake setup
Build the project using CMake and FetchContent like this:

```c
FetchContent_Declare(
    c2pa_cpp
    GIT_REPOSITORY https://github.com/contentauth/c2pa-c.git
    GIT_TAG gpeacock/cmake_work`
)
FetchContent_MakeAvailable(c2pa_cpp)
```

And then add `"${c2pa_cpp_SOURCE_DIR}/include"` to your include path.

## Development

This project has been tested on macOS and should also work on common Linux distributions.

### Prerequisites

If you haven't already done so, install [Rust](https://www.rust-lang.org/tools/install).

Install [cbindgen](https://github.com/mozilla/cbindgen/blob/master/docs.md):

```sh
cargo install --force cbindgen
```

You must install the [Ninja](https://github.com/ninja-build/ninja/wiki/Pre-built-Ninja-packages) build system to run the unit tests. 

### Building 

Building the library requires [GNU make](https://www.gnu.org/software/make/), which is installed on most macOS systems.

Enter this command to build the C library: 

```
make release
```

The Makefile has a number of other targets; for example:
- `unit-tests` to run C++ unit tests
- `examples` to build and run the C++ examples.
- `all` to run everything.

Results are saved in the `target` directory.

### Testing

Build the [unit tests](https://github.com/contentauth/c2pa-c/tree/main/tests) by entering this `make` command:

```
make unit-test
```

## License

This package is distributed under the terms of both the [MIT license](https://github.com/contentauth/c2pa-c/blob/main/LICENSE-MIT) and the [Apache License (Version 2.0)](https://github.com/contentauth/c2pa-c/blob/main/LICENSE-APACHE).

Note that some components and dependent crates are licensed under different terms; please check the license terms for each crate and component for details.

### Contributions and feedback

We welcome contributions to this project.  For information on contributing, providing feedback, and about ongoing work, see [Contributing](https://github.com/contentauth/c2pa-c/blob/main/CONTRIBUTING.md).


