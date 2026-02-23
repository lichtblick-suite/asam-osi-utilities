//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/writer/MCAPTraceFileChannel.h"

#include "MCAPWriterUtils.h"
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

namespace osi3 {

MCAPTraceFileChannel::MCAPTraceFileChannel(mcap::McapWriter& writer) : mcap_writer_(writer) {}

template <typename T>
auto MCAPTraceFileChannel::WriteMessage(const T& top_level_message, const std::string& topic) -> bool {
    if (topic.empty()) {
        std::cerr << "ERROR: cannot write message, topic is empty\n";
        return false;
    }

    // get channel id from topic using topic_to_channel_id_
    const auto topic_channel_id = topic_to_channel_id_.find(topic);
    if (topic_channel_id == topic_to_channel_id_.end()) {
        std::cerr << "ERROR: cannot write message, topic " << topic << " not found\n";
        return false;
    }

    if (!top_level_message.SerializeToString(&serialize_buffer_)) {
        std::cerr << "ERROR: Failed to serialize protobuf message\n";
        return false;
    }
    mcap::Message msg;
    msg.channelId = topic_channel_id->second;

    msg.logTime =
        static_cast<uint64_t>(top_level_message.timestamp().seconds()) * tracefile::config::kNanosecondsPerSecond + static_cast<uint64_t>(top_level_message.timestamp().nanos());
    msg.publishTime = msg.logTime;
    msg.data = reinterpret_cast<const std::byte*>(serialize_buffer_.data());
    msg.dataSize = serialize_buffer_.size();
    if (const auto status = mcap_writer_.write(msg); status.code != mcap::StatusCode::Success) {
        std::cerr << "ERROR: Failed to write message " << status.message;
        return false;
    }
    return true;
}

auto MCAPTraceFileChannel::AddChannel(const std::string& topic, const google::protobuf::Descriptor* descriptor,
                                      std::unordered_map<std::string, std::string> channel_metadata) -> uint16_t {
    // Check if the schema for this descriptor's full name already exists
    const auto& schema_name = descriptor->full_name();
    auto it_schema = schemas_.find(schema_name);

    // Check if topic already exists
    if (auto it_topic = topic_to_channel_id_.find(topic); it_topic != topic_to_channel_id_.end()) {
        // verify the existing topic uses the same schema
        if (topic_to_schema_name_[topic] == schema_name) {
            std::cout << "WARNING: Topic already exists with the same message type, returning original channel id\n";
            return it_topic->second;
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

    // add the channel to the writer/mcap file
    mcap::Channel channel(topic, "protobuf", path_schema.id, channel_metadata);
    mcap_writer_.addChannel(channel);

    // store channel id and schema name for writing use and duplicate detection
    topic_to_channel_id_[topic] = channel.id;
    topic_to_schema_name_[topic] = schema_name;

    return channel.id;
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
