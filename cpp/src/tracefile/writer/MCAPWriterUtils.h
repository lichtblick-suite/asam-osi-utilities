//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#ifndef OSIUTILITIES_TRACEFILE_WRITER_MCAPWRITERUTILS_H_
#define OSIUTILITIES_TRACEFILE_WRITER_MCAPWRITERUTILS_H_

#include <google/protobuf/descriptor.h>
#include <google/protobuf/descriptor.pb.h>
#include <google/protobuf/message.h>
#include <google/protobuf/stubs/common.h>

#include <array>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <mcap/mcap.hpp>
#include <optional>
#include <regex>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_set>

#include "osi_version.pb.h"

namespace osi3::mcap_utils {

/**
 * @brief Recursively adds all file descriptor dependencies to a FileDescriptorSet
 * @param fd_set The FileDescriptorSet to populate
 * @param files Set of already-added file names (for deduplication)
 * @param file_descriptor The file descriptor to process
 */
inline void AddFileDescriptorDependencies(google::protobuf::FileDescriptorSet& fd_set, std::unordered_set<std::string>& files,
                                          const google::protobuf::FileDescriptor* file_descriptor) {
    for (int i = 0; i < file_descriptor->dependency_count(); ++i) {
        const auto* dep = file_descriptor->dependency(i);
        if (auto [_, inserted] = files.insert(std::string(dep->name())); !inserted) {
            continue;
        }
        AddFileDescriptorDependencies(fd_set, files, file_descriptor->dependency(i));
    }
    file_descriptor->CopyTo(fd_set.add_file());
}

/**
 * @brief Creates a serialized FileDescriptorSet for MCAP schema registration
 *
 * Returns a serialized google::protobuf::FileDescriptorSet containing
 * the necessary FileDescriptor's to describe the given message descriptor.
 *
 * @param descriptor The protobuf message descriptor
 * @return Serialized FileDescriptorSet as string
 * @throws std::runtime_error if serialization fails
 */
inline auto CreateSerializedFileDescriptorSet(const google::protobuf::Descriptor* descriptor) -> std::string {
    std::unordered_set<std::string> files;
    google::protobuf::FileDescriptorSet fd_set;
    AddFileDescriptorDependencies(fd_set, files, descriptor->file());
    std::string result;
    if (!fd_set.SerializeToString(&result)) {
        throw std::runtime_error("ERROR: Failed to serialize FileDescriptorSet");
    }
    return result;
}

/**
 * @brief Returns the current OSI version as a string (e.g., "3.8.0")
 * @return OSI version string
 */
inline auto GetOsiVersionString() -> std::string {
    const auto osi_version = osi3::InterfaceVersion::descriptor()->file()->options().GetExtension(osi3::current_interface_version);
    return std::to_string(osi_version.version_major()) + "." + std::to_string(osi_version.version_minor()) + "." + std::to_string(osi_version.version_patch());
}

/**
 * @brief Returns the current protobuf version as a string
 * @return Protobuf version string
 */
inline auto GetProtobufVersionString() -> std::string { return google::protobuf::internal::VersionString(GOOGLE_PROTOBUF_VERSION); }

/**
 * @brief Normalizes a version string to a strict ``major.minor.patch`` core.
 *
 * The OSI trace metadata version fields must be ``major.minor.patch``; any
 * pre-release/build suffix (e.g. ``"3.8.0-rc1"`` -> ``"3.8.0"``) is stripped so the emitted
 * value stays spec-conformant across release-candidate cycles. The input is returned
 * unchanged if it does not start with a numeric ``major.minor.patch``.
 *
 * @param version The version string to normalize
 * @return The ``major.minor.patch`` core, or the original string if it has no numeric core
 */
inline auto NormalizeOsiVersionString(const std::string& version) -> std::string {
    static const std::regex kVersionCoreRegex(R"(^\s*(\d+)\.(\d+)\.(\d+))");
    std::smatch match;
    if (std::regex_search(version, match, kVersionCoreRegex)) {
        return match[1].str() + "." + match[2].str() + "." + match[3].str();
    }
    return version;
}

/**
 * @brief Formats an OSI version triple as a ``major.minor.patch`` string.
 */
inline auto OsiVersionToString(const std::array<uint32_t, 3>& version) -> std::string {
    return std::to_string(version[0]) + "." + std::to_string(version[1]) + "." + std::to_string(version[2]);
}

/**
 * @brief Extracts the embedded OSI ``InterfaceVersion`` from a top-level OSI message.
 *
 * Uses protobuf reflection to read the singular ``version`` message field that all OSI
 * top-level messages carry. Returns ``std::nullopt`` when the message has no such field,
 * it is unset, or it is the meaningless all-zero default (which OSI itself documents as an
 * unset/legacy version).
 *
 * @param message The top-level OSI protobuf message
 * @return ``{major, minor, patch}`` if a usable version is present, otherwise ``std::nullopt``
 */
inline auto GetMessageOsiVersion(const google::protobuf::Message& message) -> std::optional<std::array<uint32_t, 3>> {
    const auto* descriptor = message.GetDescriptor();
    const auto* version_field = descriptor->FindFieldByName("version");
    if (version_field == nullptr || version_field->is_repeated() || version_field->cpp_type() != google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE) {
        return std::nullopt;
    }
    const auto* reflection = message.GetReflection();
    if (!reflection->HasField(message, version_field)) {
        return std::nullopt;
    }
    const auto& version_message = reflection->GetMessage(message, version_field);
    const auto* version_descriptor = version_message.GetDescriptor();
    const auto* version_reflection = version_message.GetReflection();
    const auto read_component = [&](const char* name) -> uint32_t {
        const auto* component_field = version_descriptor->FindFieldByName(name);
        if (component_field == nullptr || component_field->cpp_type() != google::protobuf::FieldDescriptor::CPPTYPE_UINT32) {
            return 0U;
        }
        return version_reflection->GetUInt32(version_message, component_field);
    };
    const std::array<uint32_t, 3> triple = {read_component("version_major"), read_component("version_minor"), read_component("version_patch")};
    if (triple[0] == 0U && triple[1] == 0U && triple[2] == 0U) {
        return std::nullopt;
    }
    return triple;
}

/**
 * @brief Helper function that returns the current time as a formatted string
 * @return Current timestamp as string in ISO 8601 format with Z suffix
 *
 * This helper function is intended to be used when creating metadata entries
 * that require timestamps, particularly for the OSI-specification mandatory metadata.
 */
inline auto GetCurrentTimeAsString() -> std::string {
    const auto now = std::chrono::system_clock::now();
    const auto now_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    const auto timer = std::chrono::system_clock::to_time_t(now);
    std::tm utc_time_structure{};
#ifdef _WIN32
    gmtime_s(&utc_time_structure, &timer);
#else
    gmtime_r(&timer, &utc_time_structure);
#endif
    std::ostringstream oss;
    oss << std::put_time(&utc_time_structure, "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(1) << (now_in_ms.count() / 100);
    oss << "Z";
    return oss.str();
}

/**
 * @brief Prepares the required (by OSI spec) metadata for MCAP trace files
 *
 * This function initializes and populates an mcap::Metadata object with information
 * specific to the OSI trace format. The metadata includes details about the OSI version
 * and protobuf version.
 *
 * @param trace_file_spec_version The OSI trace file specification version string
 * @return mcap::Metadata Object containing the necessary metadata for OSI compliance
 */
inline auto PrepareRequiredFileMetadata(const std::string& trace_file_spec_version) -> mcap::Metadata {
    mcap::Metadata metadata;
    metadata.name = "net.asam.osi.trace";

    // "version" is the OSI trace-file *format* version (normalized to major.minor.patch).
    // min/max_osi_version are intentionally left empty here: MCAPTraceFileWriter fills them
    // at Close() from the embedded InterfaceVersion of the messages actually written
    // (falling back to the linked OSI library version). Callers using an external writer
    // may populate them themselves.
    metadata.metadata["version"] = NormalizeOsiVersionString(trace_file_spec_version);
    metadata.metadata["min_osi_version"] = "";
    metadata.metadata["max_osi_version"] = "";
    metadata.metadata["min_protobuf_version"] = GetProtobufVersionString();
    metadata.metadata["max_protobuf_version"] = GetProtobufVersionString();

    return metadata;
}

/**
 * @brief Adds OSI-required metadata to channel metadata map
 *
 * Adds net.asam.osi.trace.channel.osi_version and protobuf_version
 * if not already present.
 *
 * @param channel_metadata The metadata map to populate (modified in place)
 */
inline void AddOsiChannelMetadata(std::unordered_map<std::string, std::string>& channel_metadata) {
    if (channel_metadata.find("net.asam.osi.trace.channel.osi_version") == channel_metadata.end()) {
        channel_metadata["net.asam.osi.trace.channel.osi_version"] = GetOsiVersionString();
    }
    if (channel_metadata.find("net.asam.osi.trace.channel.protobuf_version") == channel_metadata.end()) {
        channel_metadata["net.asam.osi.trace.channel.protobuf_version"] = GetProtobufVersionString();
    }
}

}  // namespace osi3::mcap_utils

#endif  // OSIUTILITIES_TRACEFILE_WRITER_MCAPWRITERUTILS_H_
