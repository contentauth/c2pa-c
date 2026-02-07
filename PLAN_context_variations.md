# Context Variation Tests Plan

This document outlines the tests that should have "UsingContext" variations to verify API consistency between the old (no context) and new (with context) APIs.

## Overview

Based on analysis of existing tests, **35 tests** were identified that create Builder or Reader directly without Context and would benefit from having UsingContext variations. These variations will:
- Verify behavioral consistency between old and new APIs
- Ensure Context doesn't introduce regressions
- Validate that features work identically with explicit Context management

## Test Naming Convention

All context variation tests should be named with the suffix `UsingContext`:
- Original: `TEST(Builder, AddAnActionAndSign)`
- Variation: `TEST(Builder, AddAnActionAndSignUsingContext)`

## Implementation Guidelines

1. **Structure**: Each UsingContext variation should:
   - Create a Context using `auto context = c2pa::Context::create()`
   - Pass the context to Builder/Reader constructors
   - Otherwise follow the exact same logic as the original test
   - Verify the same assertions/expectations

2. **Code Reuse**: Consider extracting test logic into helper functions to avoid duplication

3. **Placement**: Add UsingContext variations immediately after their corresponding original tests

## Builder Tests to Implement

### Actions & Basic Signing
1. **AddAnActionAndSignUsingContext**
   - Original: line 155 in builder.test.cpp
   - Tests: Adding a single action to builder and signing
   - Validates: Action appears in manifest assertions

2. **AddMultipleActionsAndSignUsingContext**
   - Original: line 248 in builder.test.cpp
   - Tests: Adding multiple different actions before signing
   - Validates: Action parameters and types are preserved

### Resources & Assets
3. **SignImageFileWithResourceUsingContext**
   - Original: line 748 in builder.test.cpp
   - Tests: Adding a single resource (thumbnail) before signing
   - Validates: Resource is accessible in signed manifest

### Ingredients
4. **SignVideoFileWithMultipleIngredientsUsingContext**
   - Original: line 896 in builder.test.cpp
   - Tests: Adding multiple ingredients with different relationships
   - Validates: Ingredient relationships (parentOf, componentOf) are preserved

5. **LinkIngredientsAndSignUsingContext**
    - Original: line 1669 in builder.test.cpp
    - Tests: Ingredient linking via ingredientIds in actions
    - Validates: Ingredient-action linking is preserved

### Data Hashing
6. **SignDataHashedEmbeddedUsingContext**
    - Original: line 1336 in builder.test.cpp
    - Tests: Data hashing placeholder functionality
    - Validates: Cryptographic hashing behavior

7. **SignDataHashedEmbeddedWithAssetUsingContext**
    - Original: line 1372 in builder.test.cpp
    - Tests: Data hashing with actual asset data
    - Validates: Hashing workflow with real assets

## Reader Tests to Implement

### Basic Reader Operations
1. **MultipleReadersSameFileUsingContext**
   - Original: line 62 in reader.test.cpp
   - Tests: Multiple Reader instances on same file work independently
   - Validates: Reader independence with Context

2. **VideoStreamWithManifestUsingExtensionUsingContext**
   - Original: line 97 in reader.test.cpp
   - Tests: Mime-type detection from extension with streams
   - Validates: Stream handling with Context

### Advanced Reader Operations
3. **HasManifestUtf8PathUsingContext**
   - Original: line 194 in reader.test.cpp
   - Tests: UTF-8 path handling with streams
   - Validates: Internationalization support

## Notes

- **CRITICAL: Temporary test file paths**
  - All temporary output files created during tests MUST use the build directory
  - Pattern: `fs::path output_path = current_dir / "../build/examples/test_output.jpg";`
  - Do NOT create temp files in the tests directory (e.g., `.test_*.jpg`)
  - This prevents cluttering the source tree and follows existing test conventions
  - Steps 1-6 need to be updated to follow this pattern

- Some tests (like those in ErrorHandling suites) already test both APIs and don't need variations
- Tests already named "WithoutContext" likely have Context counterparts and don't need variations
- Focus on substantial tests; skip trivial tests like `supported_mime_types()`
- Consider extracting common test logic into helper functions to reduce duplication
