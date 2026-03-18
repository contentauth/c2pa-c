# Plan: Add "Identifying ingredients" section with vendor parameter tests

## Context

The ingredients catalog pattern in `selective-manifests.md` shows how to pick ingredients from archives, but lacks guidance on how to tag ingredients with unique, application-specific identifiers that survive archiving and signing. The C2PA spec allows vendor-namespaced parameters (e.g., `com.mycompany.asset_id`) in action `parameters`. The existing `CustomParamsInActions` test proves these survive signing, but there's no test covering the archive round-trip (to_archive → from_archive → sign → read back).

The new "Identifying ingredients" section should be added after the ingredients catalog pattern, showing how to use vendor parameters on actions to tag ingredients with unique IDs.

## Step 1: Write tests

Add to `BuilderTest` in [tests/builder.test.cpp](tests/builder.test.cpp), after the existing `CustomParamsInActions` test (~line 4719).

### Test 1: `VendorParamsSurviveArchiveRoundTrip`

Verifies vendor parameters on actions survive: build → archive → restore → sign → read back.

1. Create a manifest with `c2pa.placed` action containing vendor params like `com.example.asset_id: "asset-42"` and `ingredientIds`
2. Add an ingredient with a matching label
3. Archive with `to_archive()`
4. Restore with `Builder::from_archive()`
5. Sign
6. Read back, verify:
   - `com.example.asset_id` is `"asset-42"` in the placed action's parameters
   - `ingredients` array is resolved (ingredientIds linked)

### Test 2: `VendorParamsSurviveArchiveWithArchiveRoundTrip`

Same but uses `with_archive()` instead of `from_archive()` to verify Context preservation path.

### Test 3: `VendorParamsIdentifyIngredientsInCatalog`

End-to-end catalog workflow: create an archive with two ingredients, each tagged with a different `com.example.asset_id` via their linked actions. Archive, restore, sign, read back, verify each action still has its unique vendor ID.

## Step 2: Run tests

```bash
cd build/debug && cmake --build . --target c2pa_c_tests && ./tests/c2pa_c_tests --gtest_filter="BuilderTest.VendorParams*"
```

## Step 3: Add documentation section

In [docs/selective-manifests.md](docs/selective-manifests.md), add `### Identifying ingredients` after the ingredients catalog pattern section (after line 517, before `### Overriding ingredient properties`).

Content:

- Explain that vendor-namespaced parameters on actions can tag ingredients with unique, application-specific IDs
- These IDs survive archiving and signing
- Useful for tracking which external asset an ingredient came from, or for filtering ingredients after signing
- Code example showing the pattern: set `com.mycompany.asset_id` in the action's parameters alongside `ingredientIds`, then read it back after signing
- Note about naming convention (reverse domain notation)

## Files to modify

| File | Change |
| --- | --- |
| [tests/builder.test.cpp](tests/builder.test.cpp) | Add 3 tests after `CustomParamsInActions` |
| [docs/selective-manifests.md](docs/selective-manifests.md) | Add "Identifying ingredients" subsection after ingredients catalog pattern |

## Verification

1. All 3 new tests pass
2. Existing tests still pass
3. Documentation is consistent with test results
