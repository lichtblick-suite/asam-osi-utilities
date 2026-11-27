//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/writer/MCAPTraceFileChannel.h"

#include <stdexcept>
#include <utility>

#include "MCAPWriterUtils.h"
#include "osi-utilities/tracefile/TimestampUtils.h"
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

MCAPTraceFileChannel::MCAPTraceFileChannel(mcap::McapWriter& writer) : mcap_writer_(writer) {}

auto MCAPTraceFileChannel::WriteMessage(const google::protobuf::Message& message, const std::string& topic) -> bool {
    if (topic.empty()) {
        std::cerr << "ERROR: cannot write message, topic is empty\n";
        return false;
    }

    auto channel = channels_.find(topic);
    if (channel == channels_.end()) {
        // Auto-register: label the channel with the OSI version the message was created with
        std::unordered_map<std::string, std::string> channel_metadata;
        if (const auto message_version = mcap_utils::GetMessageOsiVersion(message); message_version.has_value()) {
            channel_metadata[mcap_utils::kChannelOsiVersionKey] = mcap_utils::OsiVersionToString(*message_version);
        }
        AddChannel(topic, message.GetDescriptor(), std::move(channel_metadata));
        channel = channels_.find(topic);
    }

    uint64_t log_time = 0;
    try {
        log_time = tracefile::TimestampToNanoseconds(message);
    } catch (const std::out_of_range& error) {
        std::cerr << "ERROR: invalid message timestamp: " << error.what() << '\n';
        return false;
    }
    return WriteToChannel(channel->second, message, topic, log_time);
}

template <typename T>
auto MCAPTraceFileChannel::WriteMessage(const T& top_level_message, const std::string& topic) -> bool {
    if (topic.empty()) {
        std::cerr << "ERROR: cannot write message, topic is empty\n";
        return false;
    }

    const auto channel = channels_.find(topic);
    if (channel == channels_.end()) {
        std::cerr << "ERROR: cannot write message, topic " << topic << " not found\n";
        return false;
    }

    uint64_t log_time = 0;
    try {
        log_time = tracefile::TimestampToNanoseconds(top_level_message);
    } catch (const std::out_of_range& error) {
        std::cerr << "ERROR: invalid message timestamp: " << error.what() << '\n';
        return false;
    }
    return WriteToChannel(channel->second, top_level_message, topic, log_time);
}

auto MCAPTraceFileChannel::WriteToChannel(ChannelInfo& channel, const google::protobuf::Message& message, const std::string& topic, uint64_t log_time) -> bool {
    if (!message.SerializeToString(&serialize_buffer_)) {
        std::cerr << "ERROR: Failed to serialize protobuf message\n";
        return false;
    }
    CheckMessageOsiVersion(channel, message, topic);

    mcap::Message msg;
    msg.channelId = channel.id;
    msg.logTime = log_time;
    msg.publishTime = msg.logTime;
    msg.data = reinterpret_cast<const std::byte*>(serialize_buffer_.data());
    msg.dataSize = serialize_buffer_.size();
    if (const auto status = mcap_writer_.write(msg); status.code != mcap::StatusCode::Success) {
        std::cerr << "ERROR: Failed to write message " << status.message;
        return false;
    }
    return true;
}

void MCAPTraceFileChannel::CheckMessageOsiVersion(ChannelInfo& channel, const google::protobuf::Message& message, const std::string& topic) {
    if (channel.osi_version_mismatch_reported || !channel.osi_version.has_value()) {
        return;
    }
    const auto message_version = mcap_utils::GetMessageOsiVersion(message);
    if (message_version.has_value() && *message_version != *channel.osi_version) {
        std::cerr << "WARNING: message on topic '" << topic << "' has OSI version " << mcap_utils::OsiVersionToString(*message_version) << " but the channel is labelled "
                  << mcap_utils::OsiVersionToString(*channel.osi_version) << '\n';
        channel.osi_version_mismatch_reported = true;
    }
}

auto MCAPTraceFileChannel::AddChannel(const std::string& topic, const google::protobuf::Descriptor* descriptor, std::unordered_map<std::string, std::string> channel_metadata)
    -> uint16_t {
    // Check if the schema for this descriptor's full name already exists
    const auto& schema_name = descriptor->full_name();
    auto it_schema = schemas_.find(schema_name);

    // Check if topic already exists
    if (auto it_topic = channels_.find(topic); it_topic != channels_.end()) {
        // verify the existing topic uses the same schema
        if (it_topic->second.schema_name == schema_name) {
            std::cout << "WARNING: Topic already exists with the same message type, returning original channel id\n";
            return it_topic->second.id;
        }
        throw std::runtime_error("Topic already exists with a different message type");
    }

    // for a new topic, check if the schema exists or must be added first
    mcap::Schema path_schema;
    if (it_schema == schemas_.end()) {
        // Schema doesn't exist, create a new one
        path_schema = mcap::Schema(schema_name, "protobuf", mcap_utils::CreateSerializedFileDescriptorSet(descriptor));
        mcap_writer_.addSchema(path_schema);
        schemas_.emplace(schema_name, path_schema);
    } else {
        // Schema already exists, use the existing one
        path_schema = it_schema->second;
    }

    // add OSI-required channel metadata
    mcap_utils::AddOsiChannelMetadata(channel_metadata);
    const auto osi_version = mcap_utils::ParseOsiVersion(channel_metadata[mcap_utils::kChannelOsiVersionKey]);

    // add the channel to the writer/mcap file
    mcap::Channel channel(topic, "protobuf", path_schema.id, channel_metadata);
    mcap_writer_.addChannel(channel);

    channels_[topic] = ChannelInfo{channel.id, std::string(schema_name), osi_version};

    return channel.id;
}

void MCAPTraceFileChannel::FillOsiVersionRange(mcap::Metadata& metadata) const {
    std::optional<mcap_utils::OsiVersion> min_version;
    std::optional<mcap_utils::OsiVersion> max_version;
    for (const auto& [topic, channel] : channels_) {
        if (!channel.osi_version.has_value()) {
            continue;
        }
        if (!min_version.has_value() || *channel.osi_version < *min_version) {
            min_version = channel.osi_version;
        }
        if (!max_version.has_value() || *channel.osi_version > *max_version) {
            max_version = channel.osi_version;
        }
    }
    if (min_version.has_value()) {
        metadata.metadata["min_osi_version"] = mcap_utils::OsiVersionToString(*min_version);
        metadata.metadata["max_osi_version"] = mcap_utils::OsiVersionToString(*max_version);
    }
}

void MCAPTraceFileChannel::Reset() {
    schemas_.clear();
    channels_.clear();
}

auto MCAPTraceFileChannel::PrepareRequiredFileMetadata() -> mcap::Metadata { return mcap_utils::PrepareRequiredFileMetadata(OSI_TRACE_FILE_SPEC_VERSION); }

auto MCAPTraceFileChannel::GetCurrentTimeAsString() -> std::string { return mcap_utils::GetCurrentTimeAsString(); }

// template instantiations of allowed OSI top-level messages
template bool MCAPTraceFileChannel::WriteMessage<osi3::GroundTruth>(const osi3::GroundTruth&, const std::string&);
template bool MCAPTraceFileChannel::WriteMessage<osi3::SensorData>(const osi3::SensorData&, const std::string&);
template bool MCAPTraceFileChannel::WriteMessage<osi3::SensorView>(const osi3::SensorView&, const std::string&);
template bool MCAPTraceFileChannel::WriteMessage<osi3::HostVehicleData>(const osi3::HostVehicleData&, const std::string&);
template bool MCAPTraceFileChannel::WriteMessage<osi3::TrafficCommand>(const osi3::TrafficCommand&, const std::string&);
template bool MCAPTraceFileChannel::WriteMessage<osi3::TrafficCommandUpdate>(const osi3::TrafficCommandUpdate&, const std::string&);
template bool MCAPTraceFileChannel::WriteMessage<osi3::TrafficUpdate>(const osi3::TrafficUpdate&, const std::string&);
template bool MCAPTraceFileChannel::WriteMessage<osi3::MotionRequest>(const osi3::MotionRequest&, const std::string&);
template bool MCAPTraceFileChannel::WriteMessage<osi3::StreamingUpdate>(const osi3::StreamingUpdate&, const std::string&);

}  // namespace osi3
