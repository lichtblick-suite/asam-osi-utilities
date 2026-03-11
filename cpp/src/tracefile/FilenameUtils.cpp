//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/FilenameUtils.h"

#include <sstream>

namespace osi3 {
namespace tracefile {

ReaderTopLevelMessage InferMessageTypeFromFilename(const std::filesystem::path& file_path) {
    const auto stem = file_path.stem().string();
    for (const auto& [pattern, msg_type] : kFileNameMessageTypeMap) {
        if (stem.find(pattern) != std::string::npos) {
            return msg_type;
        }
    }
    return ReaderTopLevelMessage::kUnknown;
}

std::optional<OsiTraceFilenameComponents> ParseOsiTraceFilename(const std::filesystem::path& file_path) {
    const auto stem = file_path.stem().string();

    // Split by '_' — expecting at least 6 parts:
    // timestamp, type, osiVer, pbVer, frameCount, identifier[_description...]
    std::vector<std::string> parts;
    std::istringstream stream(stem);
    std::string part;
    while (std::getline(stream, part, '_')) {
        parts.push_back(part);
    }

    if (parts.size() < 6) {
        return std::nullopt;
    }

    OsiTraceFilenameComponents result;
    result.timestamp = parts[0];
    result.message_type = parts[1];
    result.osi_version = parts[2];
    result.protobuf_version = parts[3];
    result.frame_count = parts[4];
    result.identifier = parts[5];

    // Everything after index 5 is the description (re-join with '_')
    if (parts.size() > 6) {
        std::ostringstream desc;
        for (size_t i = 6; i < parts.size(); ++i) {
            if (i > 6) desc << '_';
            desc << parts[i];
        }
        result.description = desc.str();
    }

    return result;
}

}  // namespace tracefile
}  // namespace osi3
