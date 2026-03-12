//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/FilenameUtils.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <iomanip>
#include <regex>
#include <sstream>
#include <unordered_set>

namespace osi3 {
namespace tracefile {

namespace {

auto ToLower(std::string value) -> std::string {
    std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char character) { return static_cast<char>(std::tolower(character)); });
    return value;
}

auto IsKnownMessageTypeCode(const std::string& message_type_code) -> bool {
    static const std::unordered_set<std::string> kKnownMessageTypeCodes = {"gt", "sd", "sv", "svc", "hvd", "tc", "tcu", "tu", "mr", "su"};
    return kKnownMessageTypeCodes.find(message_type_code) != kKnownMessageTypeCodes.end();
}

auto IsValidUtcTimestamp(const std::string& timestamp) -> bool {
    std::tm parsed_timestamp = {};
    std::istringstream timestamp_stream(timestamp);
    timestamp_stream >> std::get_time(&parsed_timestamp, "%Y%m%dT%H%M%SZ");
    return !timestamp_stream.fail();
}

}  // namespace

ReaderTopLevelMessage InferMessageTypeFromFilename(const std::filesystem::path& file_path) {
    const auto normalized_file_name = ToLower(file_path.filename().string());
    const auto stem = ToLower(file_path.stem().string());

    for (const auto& [pattern, msg_type] : kFileNameMessageTypeMap) {
        if (normalized_file_name.find(pattern) != std::string::npos) {
            return msg_type;
        }

        const auto type_code = pattern.substr(1, pattern.size() - 2);
        const auto suffix = "_" + type_code;
        if (stem == type_code || (stem.size() > suffix.size() && stem.compare(stem.size() - suffix.size(), suffix.size(), suffix) == 0)) {
            return msg_type;
        }
    }

    return ReaderTopLevelMessage::kUnknown;
}

std::optional<OsiTraceFilenameComponents> ParseOsiTraceFilename(const std::filesystem::path& file_path) {
    static const std::regex kFilenamePattern(R"(^(\d{8}T\d{6}Z)_([A-Za-z0-9]+)_([^_]+)_([^_]+)_(\d+)_([^_.]+)(?:_(.+))?\.[^.]+$)");

    std::smatch match;
    const auto file_name = file_path.filename().string();
    if (!std::regex_match(file_name, match, kFilenamePattern)) {
        return std::nullopt;
    }

    const auto timestamp = match[1].str();
    const auto message_type = match[2].str();
    if (!IsValidUtcTimestamp(timestamp) || !IsKnownMessageTypeCode(message_type)) {
        return std::nullopt;
    }

    OsiTraceFilenameComponents result;
    result.timestamp = timestamp;
    result.message_type = message_type;
    result.osi_version = match[3].str();
    result.protobuf_version = match[4].str();
    result.frame_count = match[5].str();
    result.identifier = match[6].str();
    result.description = match[7].matched ? match[7].str() : "";

    return result;
}

}  // namespace tracefile
}  // namespace osi3
