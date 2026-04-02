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

/// @file   c2pa_core.cpp
/// @brief  Core C2PA exception class and free functions.

#include <string.h>
#include <utility>

#include "c2pa.hpp"
#include "c2pa_internal.hpp"

namespace c2pa
{
    /// C2paException class for C2PA errors.
    /// This class is used to throw exceptions for errors encountered by the C2PA library via c2pa_error().

    C2paException::C2paException()
        : message_([]{
            auto result = c2pa_error();
            std::string msg = result ? std::string(result) : std::string();
            c2pa_free(result);
            return msg;
        }())
    {
    }

    C2paException::C2paException(std::string message) : message_(std::move(message))
    {
    }

    const char* C2paException::what() const noexcept
    {
        return message_.c_str();
    }

    C2paUnsupportedOperationException::C2paUnsupportedOperationException(std::string message)
        : C2paException(std::move(message))
    {
    }

    /// Returns the version of the C2PA library.
    std::string version()
    {
        return detail::c_string_to_string(c2pa_version());
    }

    /// Loads C2PA settings from a std::string in a given format.
    /// @param data the std::string to load.
    /// @param format the mime format of the string.
    /// @throws a C2pa::C2paException for errors encountered by the C2PA library.
    [[deprecated("Use Settings on Builder and Reader instead")]]
    void load_settings(const std::string &data, const std::string &format)
    {
        auto result = c2pa_load_settings(data.c_str(), format.c_str());
        if (result != 0)
        {
            throw c2pa::C2paException();
        }
    }

    /// Reads a file and returns the manifest json as a C2pa::String.
    /// @param source_path the path to the file to read.
    /// @param data_dir the directory to store binary resources (optional).
    /// @return a std::string containing the manifest json if a manifest was found.
    /// @throws a C2pa::C2paException for errors encountered by the C2PA library.
    [[deprecated("Use stream APIs instead: Reader to read data, combined with Builder to manage ingredients")]]
    std::optional<std::string> read_file(const std::filesystem::path &source_path, const std::optional<std::filesystem::path> data_dir)
    {
        const char* dir_ptr = nullptr;
        std::string dir_str;
        if (data_dir.has_value()) {
            auto u = data_dir.value().u8string();
            dir_str = std::string(u.begin(), u.end());
            dir_ptr = dir_str.c_str();
        }

        auto src_u8 = source_path.u8string();
        std::string src_str(src_u8.begin(), src_u8.end());
        char *result = c2pa_read_file(src_str.c_str(), dir_ptr);

        if (result == nullptr)
        {
            auto C2paException = c2pa::C2paException();
            if (strstr(C2paException.what(), "ManifestNotFound") != nullptr)
            {
                return std::nullopt;
            }
            throw c2pa::C2paException();
        }
        std::string str(result);
        c2pa_free(result);
        return str;
    }

    /// Reads a file and returns an ingredient JSON as a C2pa::String.
    /// @param source_path the path to the file to read.
    /// @param data_dir the directory to store binary resources.
    /// @return a std::string containing the ingredient json.
    /// @throws a C2pa::C2paException for errors encountered by the C2PA library.
    [[deprecated("Use stream APIs instead: add_ingredient on the Builder")]]
    std::string read_ingredient_file(const std::filesystem::path &source_path, const std::filesystem::path &data_dir)
    {
        auto src_u8 = source_path.u8string();
        auto dir_u8 = data_dir.u8string();
        return detail::c_string_to_string(
            c2pa_read_ingredient_file(std::string(src_u8.begin(), src_u8.end()).c_str(),
                                     std::string(dir_u8.begin(), dir_u8.end()).c_str()));
    }

    /// Adds the manifest and signs a file.
    // source_path: path to the asset to be signed
    // dest_path: the path to write the signed file to
    // manifest: the manifest json to add to the file
    // signer_info: the signer info to use for signing
    // data_dir: the directory to store binary resources (optional)
    // Throws a C2pa::C2paException for errors encountered by the C2PA library
    [[deprecated("Use stream APIs instead: sign on Builder")]]
    void sign_file(const std::filesystem::path &source_path,
                   const std::filesystem::path &dest_path,
                   const char *manifest,
                   c2pa::SignerInfo *signer_info,
                   const std::optional<std::filesystem::path> data_dir)
    {
        auto src_u8 = source_path.u8string();
        auto dst_u8 = dest_path.u8string();
        std::string src_str(src_u8.begin(), src_u8.end());
        std::string dst_str(dst_u8.begin(), dst_u8.end());
        std::string dir_str;
        if (data_dir.has_value()) {
            auto u = data_dir.value().u8string();
            dir_str = std::string(u.begin(), u.end());
        }

        char *result = c2pa_sign_file(src_str.c_str(), dst_str.c_str(), manifest, signer_info, dir_str.c_str());
        if (result == nullptr)
        {
            throw c2pa::C2paException();
        }
        c2pa_free(result);
    }
} // namespace c2pa
