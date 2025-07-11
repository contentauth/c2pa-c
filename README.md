# C2PA C++ library

The [c2pa-c repository](https://github.com/contentauth/c2pa-c) implements C++ APIs that:
- Read and validate C2PA data from media files in supported formats.
- Add signed manifests to media files in supported formats.

Although this library works for plain C applications, the documentation assumes you're using C++, since that's most common for modern applications.

<div style={{display: 'none'}}>

For the best experience, read the docs on the [CAI Open Source SDK documentation website](https://opensource.contentauthenticity.org/docs/c2pa-c).  If you want to view the documentation in GitHub, see:
- [Using the C++ library](docs/usage.md)
- [Supported formats](https://github.com/contentauth/c2pa-rs/blob/crandmck/reorg-docs/docs/supported-formats.md)

</div>

## Using c2pa_cpp in Your Application

The recommended way to use this library in your own CMake project is with [FetchContent](https://cmake.org/cmake/help/latest/module/FetchContent.html):

```cmake
include(FetchContent)

FetchContent_Declare(
    c2pa_cpp
    GIT_REPOSITORY https://github.com/contentauth/c2pa-c.git
    GIT_TAG main  # Or use a specific release tag
)
FetchContent_MakeAvailable(c2pa_cpp)

add_executable(myapp main.cpp)
target_link_libraries(myapp PRIVATE c2pa_cpp)
```

This will automatically fetch, build, and link the `c2pa_cpp` library and its dependencies.  

> **Note:**  
> This project uses pre-built dynamic libraries from the [c2pa-rs](https://github.com/contentauth/c2pa-rs) repository. It should select the correct library for your platform. If your platform is not supported, you can build your own library using the c2pa_rs repo.

### Example Usage

See the [`examples/`](examples/) directory for sample applications that demonstrate how to use the library in practice.  

---

## Development

This project has been tested on macOS and should also work on common Linux distributions.


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

Results are saved in the `build` directory.

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


