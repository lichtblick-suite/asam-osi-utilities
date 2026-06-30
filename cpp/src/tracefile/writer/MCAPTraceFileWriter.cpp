//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/writer/MCAPTraceFileWriter.h"

#include "MCAPWriterUtils.h"
#include "osi_groundtruth.pb.h"
#include "osi_hostvehicledata.pb.h"
#include "osi_motionrequest.pb.h"
#include "osi_sensordata.pb.h"
#include "osi_sensorview.pb.h"
#include "osi_streamingupdate.pb.h"
#include "osi_trafficcommand.pb.h"
#include "osi_trafficcommandupdate.pb.h"
#include "osi_trafficupdate.pb.h"

namespace osi3 {

MCAPTraceFileWriter::~MCAPTraceFileWriter() {
    if (trace_file_.is_open()) {
        Close();
    }
}

auto MCAPTraceFileWriter::Open(const std::filesystem::path& file_path) -> bool {
    // prevent opening again if already opened
    if (trace_file_.is_open()) {
        std::cerr << "ERROR: Opening file " << file_path << ", writer has already a file opened" << std::endl;
        return false;
    }

    trace_file_.open(file_path, std::ios::binary);
    if (!trace_file_) {
        std::cerr << "ERROR: Opening file " << file_path << std::endl;
        return false;
    }
    mcap_writer_.open(trace_file_, mcap_options_);
    return true;
}

auto MCAPTraceFileWriter::Open(const std::filesystem::path& file_path, const mcap::McapWriterOptions& options) -> bool {
    mcap_options_ = options;
    return this->Open(file_path);
}

auto MCAPTraceFileWriter::WriteMessage(const google::protobuf::Message& message, const std::string& topic) -> bool {
    if (!(trace_file_ && trace_file_.is_open())) {
        std::cerr << "ERROR: cannot write message, file is not open\n";
        return false;
    }
    // The net.asam.osi.trace record is buffered now and written at Close() with
    // data-accurate min/max_osi_version derived from the messages actually written.
    EnsurePendingMetadata();
    if (!channel_.WriteMessage(message, topic)) {
        return false;
    }
    TrackOsiVersion(message);
    return true;
}

template <typename T>
auto MCAPTraceFileWriter::WriteMessage(const T& top_level_message, const std::string& topic) -> bool {
    if (!(trace_file_ && trace_file_.is_open())) {
        std::cerr << "ERROR: cannot write message, file is not open\n";
        return false;
    }
    EnsurePendingMetadata();
    if (!channel_.WriteMessage(top_level_message, topic)) {
        return false;
    }
    TrackOsiVersion(top_level_message);
    return true;
}

auto MCAPTraceFileWriter::AddFileMetadata(const mcap::Metadata& metadata) -> bool {
    // The net.asam.osi.trace record is buffered and written at Close(), so that
    // min/max_osi_version can be filled from the OSI version of the messages written.
    if (metadata.name == "net.asam.osi.trace") {
        if (required_metadata_added_) {
            std::cerr << "ERROR: cannot add net.asam.osi.trace metadata record, it was already added.\n";
            return false;
        }

        constexpr std::array<const char*, 5> kRequiredFields = {"version", "min_osi_version", "max_osi_version", "min_protobuf_version", "max_protobuf_version"};
        for (const auto& field : kRequiredFields) {
            if (metadata.metadata.find(field) == metadata.metadata.end()) {
                std::cerr << "ERROR: cannot add net.asam.osi.trace metadata record without a " << field << " field.\n";
                return false;
            }
        }
        pending_osi_metadata_ = metadata;
        required_metadata_added_ = true;
        return true;
    }

    // All other metadata records are written immediately.
    if (const auto status = mcap_writer_.write(metadata); status.code != mcap::StatusCode::Success) {
        std::cerr << "ERROR: Failed to write metadata with name " << metadata.name << "\n" << status.message;
        return false;
    }
    return true;
}

auto MCAPTraceFileWriter::AddFileMetadata(const std::string& name, const std::unordered_map<std::string, std::string>& metadata_entries) -> bool {
    mcap::Metadata metadata;
    metadata.name = name;
    metadata.metadata = metadata_entries;
    return this->AddFileMetadata(metadata);
}

void MCAPTraceFileWriter::Close() {
    FinalizeFileMetadata();
    mcap_writer_.close();
    trace_file_.close();
}

void MCAPTraceFileWriter::EnsurePendingMetadata() {
    if (!pending_osi_metadata_.has_value()) {
        pending_osi_metadata_ = PrepareRequiredFileMetadata();
        required_metadata_added_ = true;
    }
}

void MCAPTraceFileWriter::TrackOsiVersion(const google::protobuf::Message& message) {
    const auto version = mcap_utils::GetMessageOsiVersion(message);
    if (!version.has_value()) {
        return;
    }
    if (!osi_version_min_.has_value() || *version < *osi_version_min_) {
        osi_version_min_ = version;
    }
    if (!osi_version_max_.has_value() || *version > *osi_version_max_) {
        osi_version_max_ = version;
    }
}

void MCAPTraceFileWriter::FinalizeFileMetadata() {
    if (!pending_osi_metadata_.has_value()) {
        return;  // nothing written and no metadata added: no record to finalize
    }
    auto& metadata = *pending_osi_metadata_;

    std::string min_version = osi_version_min_.has_value() ? mcap_utils::OsiVersionToString(*osi_version_min_) : std::string{};
    std::string max_version = osi_version_max_.has_value() ? mcap_utils::OsiVersionToString(*osi_version_max_) : std::string{};
    // Fall back to the linked OSI library version when no written message carried one.
    if (min_version.empty() || max_version.empty()) {
        const auto fallback_version = mcap_utils::GetOsiVersionString();
        if (min_version.empty()) {
            min_version = fallback_version;
        }
        if (max_version.empty()) {
            max_version = fallback_version;
        }
    }
    // Only fill fields the caller left empty; respect explicit user-provided values.
    if (const auto it = metadata.metadata.find("min_osi_version"); it == metadata.metadata.end() || it->second.empty()) {
        metadata.metadata["min_osi_version"] = min_version;
    }
    if (const auto it = metadata.metadata.find("max_osi_version"); it == metadata.metadata.end() || it->second.empty()) {
        metadata.metadata["max_osi_version"] = max_version;
    }

    if (const auto status = mcap_writer_.write(metadata); status.code != mcap::StatusCode::Success) {
        std::cerr << "ERROR: Failed to write net.asam.osi.trace metadata\n" << status.message;
    }
    pending_osi_metadata_.reset();
    osi_version_min_.reset();
    osi_version_max_.reset();
    required_metadata_added_ = false;
}

auto MCAPTraceFileWriter::PrepareRequiredFileMetadata() -> mcap::Metadata { return MCAPTraceFileChannel::PrepareRequiredFileMetadata(); }

auto MCAPTraceFileWriter::AddChannel(const std::string& topic, const google::protobuf::Descriptor* descriptor,
                                     std::unordered_map<std::string, std::string> channel_metadata) -> uint16_t {
    return channel_.AddChannel(topic, descriptor, std::move(channel_metadata));
}

auto MCAPTraceFileWriter::GetCurrentTimeAsString() -> std::string { return MCAPTraceFileChannel::GetCurrentTimeAsString(); }

// template instantiations of allowed OSI top-level messages
template bool MCAPTraceFileWriter::WriteMessage<osi3::GroundTruth>(const osi3::GroundTruth&, const std::string&);
template bool MCAPTraceFileWriter::WriteMessage<osi3::SensorData>(const osi3::SensorData&, const std::string&);
template bool MCAPTraceFileWriter::WriteMessage<osi3::SensorView>(const osi3::SensorView&, const std::string&);
template bool MCAPTraceFileWriter::WriteMessage<osi3::HostVehicleData>(const osi3::HostVehicleData&, const std::string&);
template bool MCAPTraceFileWriter::WriteMessage<osi3::TrafficCommand>(const osi3::TrafficCommand&, const std::string&);
template bool MCAPTraceFileWriter::WriteMessage<osi3::TrafficCommandUpdate>(const osi3::TrafficCommandUpdate&, const std::string&);
template bool MCAPTraceFileWriter::WriteMessage<osi3::TrafficUpdate>(const osi3::TrafficUpdate&, const std::string&);
template bool MCAPTraceFileWriter::WriteMessage<osi3::MotionRequest>(const osi3::MotionRequest&, const std::string&);
template bool MCAPTraceFileWriter::WriteMessage<osi3::StreamingUpdate>(const osi3::StreamingUpdate&, const std::string&);
}  // namespace osi3
