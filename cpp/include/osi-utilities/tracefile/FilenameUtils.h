//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#ifndef OSIUTILITIES_TRACEFILE_FILENAMEUTILS_H_
#define OSIUTILITIES_TRACEFILE_FILENAMEUTILS_H_

#include <filesystem>
#include <optional>
#include <string>

#include "osi-utilities/tracefile/Reader.h"

namespace osi3 {
namespace tracefile {

/**
 * @brief Parsed components of an OSI trace filename.
 *
 * OSI naming convention: YYYYMMDDThhmmssZ_type_osiVer_pbVer_frameCount_PID_description.ext
 */
struct OsiTraceFilenameComponents {
    std::string timestamp;        /**< ISO 8601 UTC timestamp (e.g. "20240101T120000Z") */
    std::string message_type;     /**< Short type abbreviation (e.g. "gt", "sv") */
    std::string osi_version;      /**< OSI version string (e.g. "3.7.0") */
    std::string protobuf_version; /**< Protobuf version string (e.g. "4.25.0") */
    std::string frame_count;      /**< Number of frames as string */
    std::string identifier;       /**< PID or other identifier */
    std::string description;      /**< Optional description suffix */
};

/**
 * @brief Infer the OSI message type from a filename.
 *
 * Searches the filename (stem) for known type abbreviations like "_gt_", "_sv_", etc.
 * Uses the existing kFileNameMessageTypeMap for pattern matching.
 *
 * @param file_path Path to the trace file
 * @return The inferred message type, or kUnknown if no pattern matches
 */
ReaderTopLevelMessage InferMessageTypeFromFilename(const std::filesystem::path& file_path);

/**
 * @brief Parse an OSI trace filename into its components.
 *
 * Expects the OSI naming convention:
 *   YYYYMMDDThhmmssZ_type_osiVer_pbVer_frameCount_PID_description.ext
 *
 * @param file_path Path to the trace file
 * @return Parsed components if the filename matches the convention, std::nullopt otherwise
 */
std::optional<OsiTraceFilenameComponents> ParseOsiTraceFilename(const std::filesystem::path& file_path);

}  // namespace tracefile
}  // namespace osi3

#endif  // OSIUTILITIES_TRACEFILE_FILENAMEUTILS_H_
