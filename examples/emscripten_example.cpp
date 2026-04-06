// Example: Using c2pa C++ library in an Emscripten project
// Build: make emscripten-example
//
// HTTP resolver note: emscripten_fetch in SYNCHRONOUS mode is only available
// from a Web Worker, not the browser main thread. Under Node.js there is no
// such restriction.

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "c2pa.hpp"

#include <emscripten/fetch.h>

using namespace c2pa;

// ---------------------------------------------------------------------------
// Example 1: read from file path
// ---------------------------------------------------------------------------

static void read_from_file(const std::string& path) {
    std::cout << "\n[1] Reading manifest from file: " << path << std::endl;
    try {
        Context ctx;
        Reader reader(ctx, path);
        std::cout << reader.json() << std::endl;
    } catch (const C2paException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

// ---------------------------------------------------------------------------
// Example 2: read from an in-memory stream
// ---------------------------------------------------------------------------

static void read_from_stream(const std::vector<uint8_t>& data) {
    std::cout << "\n[2] Reading manifest from memory stream" << std::endl;
    try {
        // Wrap the data in a std::istringstream so CppIStream can use it.
        std::string str_data(reinterpret_cast<const char*>(data.data()), data.size());
        std::istringstream iss(str_data, std::ios::binary);

        Context ctx;
        Reader reader(ctx, "image/jpeg", iss);
        std::cout << reader.json() << std::endl;
        std::cout << "Embedded: " << (reader.is_embedded() ? "yes" : "no") << std::endl;
    } catch (const C2paException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

// ---------------------------------------------------------------------------
// Example 3: read with a custom HTTP resolver (emscripten_fetch)
//   - Exercises remote manifest fetching, OCSP, timestamps, etc.
//   - Must run from a Web Worker in the browser; no restriction under Node.js.
// ---------------------------------------------------------------------------

// Parse "Name: Value\n..." into a NULL-terminated char* array for
// emscripten_fetch_attr_t::requestHeaders.
struct ParsedHeaders {
    std::vector<std::string> storage;
    std::vector<const char*> ptrs;
};

static ParsedHeaders parse_headers(const std::string& raw) {
    ParsedHeaders h;
    std::istringstream ss(raw);
    std::string line;
    while (std::getline(ss, line)) {
        if (line.empty()) continue;
        auto colon = line.find(':');
        if (colon == std::string::npos) continue;
        h.storage.push_back(line.substr(0, colon));
        h.storage.push_back(line.substr(colon + 2));
    }
    for (auto& s : h.storage) h.ptrs.push_back(s.c_str());
    h.ptrs.push_back(nullptr);
    return h;
}

static int emscripten_http_handler(
    void* /*ctx*/, const C2paHttpRequest* req, C2paHttpResponse* resp)
{
    emscripten_fetch_attr_t attr;
    emscripten_fetch_attr_init(&attr);
    attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_SYNCHRONOUS;
    strncpy(attr.requestMethod, req->method, sizeof(attr.requestMethod) - 1);
    attr.requestMethod[sizeof(attr.requestMethod) - 1] = '\0';
    if (req->body && req->body_len > 0) {
        attr.requestData     = reinterpret_cast<const char*>(req->body);
        attr.requestDataSize = req->body_len;
    }
    auto headers = parse_headers(req->headers ? req->headers : "");
    if (headers.ptrs.size() > 1) attr.requestHeaders = headers.ptrs.data();

    emscripten_fetch_t* fetch = emscripten_fetch(&attr, req->url);
    if (!fetch) { c2pa_error_set_last("emscripten_fetch returned null"); return -1; }

    resp->status   = static_cast<int32_t>(fetch->status);
    resp->body_len = static_cast<size_t>(fetch->numBytes);
    if (resp->body_len > 0 && fetch->data) {
        resp->body = static_cast<uint8_t*>(malloc(resp->body_len));
        memcpy(resp->body, fetch->data, resp->body_len);
    } else {
        resp->body     = nullptr;
        resp->body_len = 0;
    }
    emscripten_fetch_close(fetch);
    return 0;
}

static void read_with_http_resolver(const std::vector<uint8_t>& data) {
    std::cout << "\n[3] Reading remote manifest with custom HTTP resolver" << std::endl;
    try {
        auto ctx = Context::ContextBuilder()
            .with_http_resolver(nullptr, emscripten_http_handler)
            .create_context();

        std::string str_data(reinterpret_cast<const char*>(data.data()), data.size());
        std::istringstream iss(str_data, std::ios::binary);

        Reader reader(ctx, "image/jpeg", iss);
        std::cout << reader.json() << std::endl;
    } catch (const C2paException& e) {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

// ---------------------------------------------------------------------------

int main(int argc, char* argv[]) {
    std::cout << "=== C2PA Emscripten Example ===" << std::endl;
    std::cout << "Version: " << version() << std::endl;

    if (argc < 2) {
        std::cout << "Usage: " << argv[0] << " <image.jpg>" << std::endl;
        return 0;
    }

    const char* path = argv[1];

    // Read file into memory once; reuse for stream-based examples.
    std::vector<uint8_t> image_data;
    {
        std::ifstream f(path, std::ios::binary | std::ios::ate);
        if (f) {
            auto sz = f.tellg();
            f.seekg(0);
            image_data.resize(static_cast<size_t>(sz));
            f.read(reinterpret_cast<char*>(image_data.data()), sz);
        }
    }

    read_from_file(path);

    if (!image_data.empty()) {
        read_from_stream(image_data);
        read_with_http_resolver(image_data);
    }

    std::cout << "\n=== Done ===" << std::endl;
    return 0;
}
