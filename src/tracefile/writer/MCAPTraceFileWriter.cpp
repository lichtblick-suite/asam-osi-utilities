//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/writer/MCAPTraceFileWriter.h"

#include <google/protobuf/stubs/common.h>

#include "google/protobuf/descriptor.pb.h"
#include "osi-utilities/tracefile/TraceFileConfig.h"
#include "osi_groundtruth.pb.h"
#include "osi_hostvehicledata.pb.h"
#include "osi_motionrequest.pb.h"
#include "osi_sensordata.pb.h"
#include "osi_sensorview.pb.h"
#include "osi_streamingupdate.pb.h"
#include "osi_trafficcommand.pb.h"
#include "osi_trafficcommandupdate.pb.h"
#include "osi_trafficupdate.pb.h"
#include "osi_version.pb.h"

namespace {
// helper functions from https://github.com/foxglove/mcap/blob/4ec37c5a5d0115bceaca428b1e8a0e3e5aae20cf/website/docs/guides/cpp/protobuf.md?plain=1#L198

// Recursively adds all `fd` dependencies to `fd_set`.
void fdSetInternal(google::protobuf::FileDescriptorSet& fd_set, std::unordered_set<std::string>& files, const google::protobuf::FileDescriptor* file_descriptor) {
    for (int i = 0; i < file_descriptor->dependency_count(); ++i) {
        const auto* dep = file_descriptor->dependency(i);
        if (auto [_, inserted] = files.insert(std::string(dep->name())); !inserted) {
            continue;
        }
        fdSetInternal(fd_set, files, file_descriptor->dependency(i));
    }
    file_descriptor->CopyTo(fd_set.add_file());
}

// Returns a serialized google::protobuf::FileDescriptorSet containing
// the necessary google::protobuf::FileDescriptor's to describe d.
auto fdSet(const google::protobuf::Descriptor* descriptor) -> std::string {
    std::unordered_set<std::string> files;
    google::protobuf::FileDescriptorSet fd_set;
    fdSetInternal(fd_set, files, descriptor->file());
    return fd_set.SerializeAsString();
}

}  // namespace

namespace osi3 {

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
    if (topic.empty()) {
        std::cerr << "ERROR: cannot write message, topic is empty\n";
        return false;
    }
    if (!(trace_file_ && trace_file_.is_open())) {
        std::cerr << "ERROR: cannot write message, file is not open\n";
        return false;
    }
    if (!required_metadata_added_) {
        std::cerr << "ERROR: cannot write message, required metadata (according to the OSI specification) was not set in advance\n";
        return false;
    }

    // get channel id from topic using topic_to_channel_id_
    const auto topic_channel_id = topic_to_channel_id_.find(topic);
    if (topic_channel_id == topic_to_channel_id_.end()) {
        std::cerr << "ERROR: cannot write message, topic " << topic << " not found\n";
        return false;
    }

    const std::string data = top_level_message.SerializeAsString();
    mcap::Message msg;
    msg.channelId = topic_channel_id->second;

    // msg.logTime should be now in nanoseconds
    msg.logTime = top_level_message.timestamp().seconds() * tracefile::config::kNanosecondsPerSecond + top_level_message.timestamp().nanos();
    msg.publishTime = msg.logTime;
    msg.data = reinterpret_cast<const std::byte*>(data.data());
    msg.dataSize = data.size();
    if (const auto status = mcap_writer_.write(msg); status.code != mcap::StatusCode::Success) {
        std::cerr << "ERROR: Failed to write message " << status.message;
        return false;
    }
    return true;
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
                std::cerr << "ERROR: cannot add net.asam.osi.trac meta-data record without a " << field << " field.\n";
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

auto MCAPTraceFileWriter::PrepareRequiredFileMetadata() -> mcap::Metadata {
    mcap::Metadata metadata;
    metadata.name = "net.asam.osi.trace";

    const auto osi_version = osi3::InterfaceVersion::descriptor()->file()->options().GetExtension(osi3::current_interface_version);
    const auto osi_version_string =
        std::to_string(osi_version.version_major()) + "." + std::to_string(osi_version.version_minor()) + "." + std::to_string(osi_version.version_patch());
    metadata.metadata["version"] = OSI_TRACE_FILE_SPEC_VERSION;
    metadata.metadata["min_osi_version"] = osi_version_string;
    metadata.metadata["max_osi_version"] = osi_version_string;
    metadata.metadata["min_protobuf_version"] = google::protobuf::internal::VersionString(GOOGLE_PROTOBUF_VERSION);
    metadata.metadata["max_protobuf_version"] = google::protobuf::internal::VersionString(GOOGLE_PROTOBUF_VERSION);

    return metadata;
}

auto MCAPTraceFileWriter::AddChannel(const std::string& topic, const google::protobuf::Descriptor* descriptor,
                                     std::unordered_map<std::string, std::string> channel_metadata) -> uint16_t {
    // Check if the schema for this descriptor's full name already exists
    mcap::Schema path_schema;
    const auto it_schema = std::find_if(schemas_.begin(), schemas_.end(), [&](const mcap::Schema& schema) { return schema.name == descriptor->full_name(); });

    // Check if topic already exists
    if (topic_to_channel_id_.find(topic) != topic_to_channel_id_.end()) {
        // if it has the same schema, return the channel id
        if (it_schema != schemas_.end()) {
            std::cout << "WARNING: Topic already exists with the same message type, returning original channel id\n";
            return topic_to_channel_id_[topic];
        }
        throw std::runtime_error("Topic already exists with a different message type");
    }

    // for a new topic, check if the schema exists or must be added first
    if (it_schema == schemas_.end()) {
        // Schema doesn't exist, create a new one
        path_schema = mcap::Schema(descriptor->full_name(), "protobuf", fdSet(descriptor));
        mcap_writer_.addSchema(path_schema);
        schemas_.push_back(path_schema);
    } else {
        // Schema already exists, use the existing one
        path_schema = *it_schema;
    }

    // add osi version (if not present) to channel metadata as required by spec.
    if (channel_metadata.find("net.asam.osi.trace.channel.osi_version") == channel_metadata.end()) {
        const auto osi_version = osi3::InterfaceVersion::descriptor()->file()->options().GetExtension(osi3::current_interface_version);
        channel_metadata["net.asam.osi.trace.channel.osi_version"] =
            std::to_string(osi_version.version_major()) + "." + std::to_string(osi_version.version_minor()) + "." + std::to_string(osi_version.version_patch());
    }

    // add protobuf version (if not present) to channel metadata as required by spec.
    if (channel_metadata.find("net.asam.osi.trace.channel.protobuf_version") == channel_metadata.end()) {
        channel_metadata["net.asam.osi.trace.channel.protobuf_version"] = google::protobuf::internal::VersionString(GOOGLE_PROTOBUF_VERSION);
    }

    // add the channel to the writer/mcap file
    mcap::Channel channel(topic, "protobuf", path_schema.id, channel_metadata);
    mcap_writer_.addChannel(channel);

    // store channel id for writing use
    topic_to_channel_id_[topic] = channel.id;

    return channel.id;
}

auto MCAPTraceFileWriter::GetCurrentTimeAsString() -> std::string {
    const auto now = std::chrono::system_clock::now();
    const auto now_in_ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
    const auto timer = std::chrono::system_clock::to_time_t(now);
    const std::tm utc_time_structure = *std::gmtime(&timer);
    // Greenwich Mean Time (GMT) is in Coordinated Universal Time (UTC) zone
    std::ostringstream oss;
    oss << std::put_time(&utc_time_structure, "%Y-%m-%dT%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(1) << (now_in_ms.count() / 100);
    oss << "Z";  // As GMT is used as a reference time zone, add Z to indicate UTC (+00:00)
    return oss.str();
}

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
