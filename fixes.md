# Security Fixes Plan

Six issues to fix across two files: `src/c2pa.cpp` and `include/c2pa.hpp`.

---

## Fix 1: LOW-3 — Null check on `manifest` in `sign_file`

**File:** `src/c2pa.cpp`, line 595-601

**Steps:**
1. Add a null check for `manifest` immediately after the function opening brace (before line 599), throwing if null:
   ```cpp
   if (manifest == nullptr) {
       throw c2pa::C2paException("manifest must not be null");
   }
   ```

---

## Fix 2: LOW-4 — Bounded file read in `with_json_settings_file`

**File:** `src/c2pa.cpp`, lines 493-501

**Current code:**
```cpp
auto file = detail::open_file_binary<std::ifstream>(settings_path);
std::string json_content((std::istreambuf_iterator<char>(*file)),
                          std::istreambuf_iterator<char>());
```

**Steps:**
1. After `open_file_binary` succeeds (line 498), seek to end, get the file size, and seek back to beginning.
2. Check the file size against a reasonable limit (1 MB). If it exceeds the limit, throw `C2paException` with a descriptive message.
3. Then proceed with the existing read into `json_content`.

Result:
```cpp
auto file = detail::open_file_binary<std::ifstream>(settings_path);

file->seekg(0, std::ios::end);
auto file_size = file->tellg();
file->seekg(0, std::ios::beg);
constexpr std::streamoff max_settings_size = 2 * 1024 * 1024; // 1 MB
if (file_size < 0 {
    throw C2paException("Settings file is not readable");
}

if (file_size > max_settings_size) {
    throw C2paException("Settings file is too large (>2MB)");
}

std::string json_content((std::istreambuf_iterator<char>(*file)),
                          std::istreambuf_iterator<char>());
```

---

## Fix 3: MED-5 — Null check in `Signer(C2paSigner*)` constructor

**File:** `include/c2pa.hpp`, line 741

**Current code:**
```cpp
Signer(C2paSigner *c_signer) : signer(c_signer) {}
```

**Steps:**
1. Replace the inline constructor body with a null check that throws:
   ```cpp
   Signer(C2paSigner *c_signer) : signer(c_signer) {
       if (!c_signer) {
           throw C2paException("Signer pointer must not be null");
       }
   }
   ```

---

## Fix 4: HIGH-1 — Signed/unsigned mismatch in `Signer::reserve_size()`

**File:** `src/c2pa.cpp`, lines 863-866

**Current code:**
```cpp
uintptr_t Signer::reserve_size()
{
    return c2pa_signer_reserve_size(signer);
}
```

**Steps:**
1. Capture the return value of `c2pa_signer_reserve_size` as `int64_t`.
2. Check if the result is negative. If so, throw `C2paException`.
3. Then cast to `uintptr_t` and return.

Result:
```cpp
uintptr_t Signer::reserve_size()
{
    int64_t result = c2pa_signer_reserve_size(signer);
    if (result < 0) {
        throw C2paException();
    }
    return static_cast<uintptr_t>(result);
}
```

---

## Fix 5: MED-3 — `stream_writer` returns requested size instead of actual bytes written

**File:** `src/c2pa.cpp`, lines 210-216

**Current code:**
```cpp
template<typename Stream>
intptr_t stream_writer(StreamContext* context, const uint8_t* buffer, intptr_t size) {
    return stream_op<Stream>(context, [buffer, size](Stream* s) {
        s->write(reinterpret_cast<const char*>(buffer), size);
        return size;
    });
}
```

**Steps:**
1. Before calling `s->write()`, capture the current stream position via `s->tellp()`.
2. Call `s->write()` as before.
3. After writing, capture the new position via `s->tellp()`.
4. If either `tellp()` returns -1 (stream doesn't support positioning), fall back to returning `size` (preserving current behavior for non-seekable streams, since `stream_op` already checks `fail()`/`bad()` bits).
5. Otherwise, return the difference (actual bytes written).

Result:
```cpp
template<typename Stream>
intptr_t stream_writer(StreamContext* context, const uint8_t* buffer, intptr_t size) {
    return stream_op<Stream>(context, [buffer, size](Stream* s) {
        auto before = s->tellp();
        s->write(reinterpret_cast<const char*>(buffer), size);
        auto after = s->tellp();
        if (before < 0 || after < 0) {
            return size; // non-seekable stream, fall back to requested size
        }
        return static_cast<intptr_t>(after - before);
    });
}
```

---
