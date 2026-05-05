//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#ifndef OSIUTILITIES_EXAMPLES_MCAP_CLI_UTILS_H_
#define OSIUTILITIES_EXAMPLES_MCAP_CLI_UTILS_H_

/**
 * \file
 * \brief Shared compression string/enum maps for MCAP CLI tools.
 *
 * These maps provide bidirectional conversion between human-readable string
 * names (used in command-line arguments and output) and MCAP library enums
 * for compression algorithms and levels.
 */

#include <map>
#include <mcap/types.hpp>
#include <string>

namespace osi3::examples {

// clang-format off

/** \brief Map compression string names to enum values. */
inline const std::map<std::string, mcap::Compression> kCompressionStringMap = {
    {"none", mcap::Compression::None},
    {"lz4", mcap::Compression::Lz4},
    {"zstd", mcap::Compression::Zstd},
};

/** \brief Map compression enum values to string names. */
inline const std::map<mcap::Compression, std::string> kCompressionNameMap = {
    {mcap::Compression::None, "none"},
    {mcap::Compression::Lz4, "lz4"},
    {mcap::Compression::Zstd, "zstd"},
};

/** \brief Map compression level string names to enum values. */
inline const std::map<std::string, mcap::CompressionLevel> kLevelStringMap = {
    {"fastest", mcap::CompressionLevel::Fastest},
    {"fast", mcap::CompressionLevel::Fast},
    {"default", mcap::CompressionLevel::Default},
    {"slow", mcap::CompressionLevel::Slow},
    {"slowest", mcap::CompressionLevel::Slowest},
};

/** \brief Map compression level enum values to string names. */
inline const std::map<mcap::CompressionLevel, std::string> kLevelNameMap = {
    {mcap::CompressionLevel::Fastest, "fastest"},
    {mcap::CompressionLevel::Fast, "fast"},
    {mcap::CompressionLevel::Default, "default"},
    {mcap::CompressionLevel::Slow, "slow"},
    {mcap::CompressionLevel::Slowest, "slowest"},
};

// clang-format on

}  // namespace osi3::examples

#endif  // OSIUTILITIES_EXAMPLES_MCAP_CLI_UTILS_H_
