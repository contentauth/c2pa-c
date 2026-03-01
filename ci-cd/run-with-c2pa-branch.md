# Running workflows using a c2pa-rs branch

The `run-with-c2pa-branch.yml` workflow builds and tests c2pa-c against a specific c2pa-rs branch. This is useful during development when a c2pa-rs branch introduces new C FFI functions or changes that c2pa-c depends on.

This can be used as pre-flight checks on a branch in c2pa-rs before releasing a c2pa-rs version, as the main CI/CD workflow on branches expects to use released version of c2pa-rs.

## What does it do?

1. Checks out c2pa-c (this repo) and c2pa-rs (at the configured branch) side by side.
2. Installs the Rust toolchain (needed to compile c2pa-rs from source).
3. Builds c2pa-c with `C2PA_BUILD_FROM_SOURCE=ON`, pointing CMake at the checked-out c2pa-rs directory.
4. Runs `make test` (debug) and `make test-san` (sanitizers, non-Windows) on the full OS matrix (8 platforms).

## How do I use it?

1. Open `.github/workflows/run-with-c2pa-branch.yml`.
2. Change the `C2PA_RS_REF` value at the top of the file to the branch, tag, or commit SHA you want to target.
3. Trigger the workflow manually once the branch to use is updated.
4. Optional, but good practice: Set `C2PA_RS_REF` back to `main` when done.
