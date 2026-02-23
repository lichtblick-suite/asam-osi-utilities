//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/writer/MCAPTraceFileWriter.h"

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

template <typename T>
auto MCAPTraceFileWriter::WriteMessage(const T& top_level_message, const std::string& topic) -> bool {
    if (!(trace_file_ && trace_file_.is_open())) {
        std::cerr << "ERROR: cannot write message, file is not open\n";
        return false;
    }
    if (!required_metadata_added_) {
        std::cerr << "ERROR: cannot write message, required metadata (according to the OSI specification) was not set in advance\n";
        return false;
    }
    return channel_.WriteMessage(top_level_message, topic);
}

auto MCAPTraceFileWriter::AddFileMetadata(const mcap::Metadata& metadata) -> bool {
    // check if the provided metadata contains the required metadata by the OSI specification
    // to allow writing messages to the trace file
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
        required_metadata_added_ = true;
    }

    // add metadata to file
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
    mcap_writer_.close();
    trace_file_.close();
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
