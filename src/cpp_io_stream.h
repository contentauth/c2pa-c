// Copyright 2024 Adobe. All rights reserved.
// This file is licensed to you under the Apache License,
// Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0)
// or the MIT license (http://opensource.org/licenses/MIT),
// at your option.
// Unless required by applicable law or agreed to in writing,
// this software is distributed on an "AS IS" BASIS, WITHOUT
// WARRANTIES OR REPRESENTATIONS OF ANY KIND, either express or
// implied. See the LICENSE-MIT and LICENSE-APACHE files for the
// specific language governing permissions and limitations under
// each license.

/// @file   c2pa.hpp
/// @brief  C++ wrapper for the C2PA C library.
/// @details This is used for creating and verifying C2PA manifests.
///          This is an early version, and has not been fully tested.
///          Thread safety is not guaranteed due to the use of errno and etc.

#ifndef CPP_IO_STREAM_H
#define CPP_IO_STREAM_H

// Suppress unused function warning for GCC/Clang
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

// Suppress unused function warning for MSVC
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4505)
#endif

#include <fstream>
#include <iostream>
#include <string>
#include <optional> // C++17

#include "c2pa.h"

using path = std::filesystem::path;

namespace c2pa
{
    using namespace std; /// IStream Class wrapper for CStream.
    class CppIStream : public CStream
    {
    public:
        CStream *c_stream;
        template <typename IStream>
        explicit CppIStream(IStream &istream);

        CppIStream(const CppIStream &) = delete;
        CppIStream &operator=(const CppIStream &) = delete;
        CppIStream(CppIStream &&) = delete;
        CppIStream &operator=(CppIStream &&) = delete;

        ~CppIStream();

    private:
        static size_t reader(StreamContext *context, void *buffer, size_t size);
        static int writer(StreamContext *context, const void *buffer, int size);
        static long seeker(StreamContext *context, long int offset, int whence);
        static int flusher(StreamContext *context);

        friend class Reader;
    };

    /// Ostream Class wrapper for CStream.
    class CppOStream : public CStream
    {
    public:
        CStream *c_stream;
        template <typename OStream>
        explicit CppOStream(OStream &ostream);

        ~CppOStream();

    private:
        static size_t reader(StreamContext *context, void *buffer, size_t size);
        static int writer(StreamContext *context, const void *buffer, int size);
        static long seeker(StreamContext *context, long int offset, int whence);
        static int flusher(StreamContext *context);
    };

    /// IOStream Class wrapper for CStream.
    class CppIOStream : public CStream
    {
    public:
        CStream *c_stream;
        template <typename IOStream>
        CppIOStream(IOStream &iostream);
        ~CppIOStream();

    private:
        static size_t reader(StreamContext *context, void *buffer, size_t size);
        static int writer(StreamContext *context, const void *buffer, int size);
        static long seeker(StreamContext *context, long int offset, int whence);
        static int flusher(StreamContext *context);
    };
}

#endif
