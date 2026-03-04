// Copyright 2026 Adobe. All rights reserved.
// This file is licensed to you under the Apache License,
// Version 2.0 (http://www.apache.org/licenses/LICENSE-2.0)
// or the MIT license (http://opensource.org/licenses/MIT),
// at your option.
//
// This file is used only for the build to generate an amalgamated c2pa.cpp.
// It is not part of the public API. Do not include it directly.

/// @file   c2pa_amalgam.cpp
/// @brief  Amalgam source:
///         Includes all C++ implementation parts for the build.
///         This is so downstream builds still only deal with one file,
///         the "merged" c2pa.cpp file.

#include "c2pa_core.cpp"
#include "c2pa_context.cpp"
#include "c2pa_builder.cpp"
#include "c2pa_reader.cpp"
#include "c2pa_settings.cpp"
#include "c2pa_signer.cpp"
#include "c2pa_streams.cpp"
