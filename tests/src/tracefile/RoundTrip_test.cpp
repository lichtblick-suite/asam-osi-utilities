//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include <gtest/gtest.h>

#include <filesystem>
#include <memory>
#include <string>

#include "../TestUtilities.h"
#include "osi-utilities/tracefile/Reader.h"
#include "osi-utilities/tracefile/reader/MCAPTraceFileReader.h"
#include "osi-utilities/tracefile/reader/SingleChannelBinaryTraceFileReader.h"
#include "osi-utilities/tracefile/reader/TXTHTraceFileReader.h"
#include "osi-utilities/tracefile/writer/MCAPTraceFileWriter.h"
#include "osi-utilities/tracefile/writer/SingleChannelBinaryTraceFileWriter.h"
#include "osi-utilities/tracefile/writer/TXTHTraceFileWriter.h"
#include "osi_groundtruth.pb.h"
#include "osi_hostvehicledata.pb.h"
#include "osi_motionrequest.pb.h"
#include "osi_sensordata.pb.h"
#include "osi_sensorview.pb.h"
#include "osi_streamingupdate.pb.h"
#include "osi_trafficcommand.pb.h"
#include "osi_trafficcommandupdate.pb.h"
#include "osi_trafficupdate.pb.h"

namespace {

constexpr int64_t kTestTimestampSeconds = 42;
constexpr int32_t kTestTimestampNanos = 123456;

std::string GetTypeShortCode(osi3::ReaderTopLevelMessage type) {
    switch (type) {
        case osi3::ReaderTopLevelMessage::kGroundTruth:
            return "gt";
        case osi3::ReaderTopLevelMessage::kSensorData:
            return "sd";
        case osi3::ReaderTopLevelMessage::kSensorView:
            return "sv";
        case osi3::ReaderTopLevelMessage::kSensorViewConfiguration:
            return "svc";
        case osi3::ReaderTopLevelMessage::kHostVehicleData:
            return "hvd";
        case osi3::ReaderTopLevelMessage::kTrafficCommand:
            return "tc";
        case osi3::ReaderTopLevelMessage::kTrafficCommandUpdate:
            return "tcu";
        case osi3::ReaderTopLevelMessage::kTrafficUpdate:
            return "tu";
        case osi3::ReaderTopLevelMessage::kMotionRequest:
            return "mr";
        case osi3::ReaderTopLevelMessage::kStreamingUpdate:
            return "su";
        default:
            return "unknown";
    }
}

std::unique_ptr<google::protobuf::Message> CreateMinimalMessage(osi3::ReaderTopLevelMessage type) {
    switch (type) {
        case osi3::ReaderTopLevelMessage::kGroundTruth: {
            auto msg = std::make_unique<osi3::GroundTruth>();
            msg->mutable_timestamp()->set_seconds(kTestTimestampSeconds);
            msg->mutable_timestamp()->set_nanos(kTestTimestampNanos);
            return msg;
        }
        case osi3::ReaderTopLevelMessage::kSensorData: {
            auto msg = std::make_unique<osi3::SensorData>();
            msg->mutable_timestamp()->set_seconds(kTestTimestampSeconds);
            msg->mutable_timestamp()->set_nanos(kTestTimestampNanos);
            return msg;
        }
        case osi3::ReaderTopLevelMessage::kSensorView: {
            auto msg = std::make_unique<osi3::SensorView>();
            msg->mutable_timestamp()->set_seconds(kTestTimestampSeconds);
            msg->mutable_timestamp()->set_nanos(kTestTimestampNanos);
            return msg;
        }
        case osi3::ReaderTopLevelMessage::kHostVehicleData: {
            auto msg = std::make_unique<osi3::HostVehicleData>();
            msg->mutable_timestamp()->set_seconds(kTestTimestampSeconds);
            msg->mutable_timestamp()->set_nanos(kTestTimestampNanos);
            return msg;
        }
        case osi3::ReaderTopLevelMessage::kTrafficCommand: {
            auto msg = std::make_unique<osi3::TrafficCommand>();
            msg->mutable_timestamp()->set_seconds(kTestTimestampSeconds);
            msg->mutable_timestamp()->set_nanos(kTestTimestampNanos);
            return msg;
        }
        case osi3::ReaderTopLevelMessage::kTrafficCommandUpdate: {
            auto msg = std::make_unique<osi3::TrafficCommandUpdate>();
            msg->mutable_timestamp()->set_seconds(kTestTimestampSeconds);
            msg->mutable_timestamp()->set_nanos(kTestTimestampNanos);
            return msg;
        }
        case osi3::ReaderTopLevelMessage::kTrafficUpdate: {
            auto msg = std::make_unique<osi3::TrafficUpdate>();
            msg->mutable_timestamp()->set_seconds(kTestTimestampSeconds);
            msg->mutable_timestamp()->set_nanos(kTestTimestampNanos);
            return msg;
        }
        case osi3::ReaderTopLevelMessage::kMotionRequest: {
            auto msg = std::make_unique<osi3::MotionRequest>();
            msg->mutable_timestamp()->set_seconds(kTestTimestampSeconds);
            msg->mutable_timestamp()->set_nanos(kTestTimestampNanos);
            return msg;
        }
        case osi3::ReaderTopLevelMessage::kStreamingUpdate: {
            auto msg = std::make_unique<osi3::StreamingUpdate>();
            msg->mutable_timestamp()->set_seconds(kTestTimestampSeconds);
            msg->mutable_timestamp()->set_nanos(kTestTimestampNanos);
            return msg;
        }
        default:
            return nullptr;
    }
}

bool WriteBinaryMessage(osi3::SingleChannelBinaryTraceFileWriter& writer, osi3::ReaderTopLevelMessage type, const google::protobuf::Message& msg) {
    switch (type) {
        case osi3::ReaderTopLevelMessage::kGroundTruth:
            return writer.WriteMessage(static_cast<const osi3::GroundTruth&>(msg));
        case osi3::ReaderTopLevelMessage::kSensorData:
            return writer.WriteMessage(static_cast<const osi3::SensorData&>(msg));
        case osi3::ReaderTopLevelMessage::kSensorView:
            return writer.WriteMessage(static_cast<const osi3::SensorView&>(msg));
        case osi3::ReaderTopLevelMessage::kHostVehicleData:
            return writer.WriteMessage(static_cast<const osi3::HostVehicleData&>(msg));
        case osi3::ReaderTopLevelMessage::kTrafficCommand:
            return writer.WriteMessage(static_cast<const osi3::TrafficCommand&>(msg));
        case osi3::ReaderTopLevelMessage::kTrafficCommandUpdate:
            return writer.WriteMessage(static_cast<const osi3::TrafficCommandUpdate&>(msg));
        case osi3::ReaderTopLevelMessage::kTrafficUpdate:
            return writer.WriteMessage(static_cast<const osi3::TrafficUpdate&>(msg));
        case osi3::ReaderTopLevelMessage::kMotionRequest:
            return writer.WriteMessage(static_cast<const osi3::MotionRequest&>(msg));
        case osi3::ReaderTopLevelMessage::kStreamingUpdate:
            return writer.WriteMessage(static_cast<const osi3::StreamingUpdate&>(msg));
        default:
            return false;
    }
}

bool WriteTxthMessage(osi3::TXTHTraceFileWriter& writer, osi3::ReaderTopLevelMessage type, const google::protobuf::Message& msg) {
    switch (type) {
        case osi3::ReaderTopLevelMessage::kGroundTruth:
            return writer.WriteMessage(static_cast<const osi3::GroundTruth&>(msg));
        case osi3::ReaderTopLevelMessage::kSensorData:
            return writer.WriteMessage(static_cast<const osi3::SensorData&>(msg));
        case osi3::ReaderTopLevelMessage::kSensorView:
            return writer.WriteMessage(static_cast<const osi3::SensorView&>(msg));
        case osi3::ReaderTopLevelMessage::kHostVehicleData:
            return writer.WriteMessage(static_cast<const osi3::HostVehicleData&>(msg));
        case osi3::ReaderTopLevelMessage::kTrafficCommand:
            return writer.WriteMessage(static_cast<const osi3::TrafficCommand&>(msg));
        case osi3::ReaderTopLevelMessage::kTrafficCommandUpdate:
            return writer.WriteMessage(static_cast<const osi3::TrafficCommandUpdate&>(msg));
        case osi3::ReaderTopLevelMessage::kTrafficUpdate:
            return writer.WriteMessage(static_cast<const osi3::TrafficUpdate&>(msg));
        case osi3::ReaderTopLevelMessage::kMotionRequest:
            return writer.WriteMessage(static_cast<const osi3::MotionRequest&>(msg));
        case osi3::ReaderTopLevelMessage::kStreamingUpdate:
            return writer.WriteMessage(static_cast<const osi3::StreamingUpdate&>(msg));
        default:
            return false;
    }
}

bool WriteMcapMessage(osi3::MCAPTraceFileWriter& writer, osi3::ReaderTopLevelMessage type, const google::protobuf::Message& msg, const std::string& topic) {
    switch (type) {
        case osi3::ReaderTopLevelMessage::kGroundTruth:
            return writer.WriteMessage(static_cast<const osi3::GroundTruth&>(msg), topic);
        case osi3::ReaderTopLevelMessage::kSensorData:
            return writer.WriteMessage(static_cast<const osi3::SensorData&>(msg), topic);
        case osi3::ReaderTopLevelMessage::kSensorView:
            return writer.WriteMessage(static_cast<const osi3::SensorView&>(msg), topic);
        case osi3::ReaderTopLevelMessage::kHostVehicleData:
            return writer.WriteMessage(static_cast<const osi3::HostVehicleData&>(msg), topic);
        case osi3::ReaderTopLevelMessage::kTrafficCommand:
            return writer.WriteMessage(static_cast<const osi3::TrafficCommand&>(msg), topic);
        case osi3::ReaderTopLevelMessage::kTrafficCommandUpdate:
            return writer.WriteMessage(static_cast<const osi3::TrafficCommandUpdate&>(msg), topic);
        case osi3::ReaderTopLevelMessage::kTrafficUpdate:
            return writer.WriteMessage(static_cast<const osi3::TrafficUpdate&>(msg), topic);
        case osi3::ReaderTopLevelMessage::kMotionRequest:
            return writer.WriteMessage(static_cast<const osi3::MotionRequest&>(msg), topic);
        case osi3::ReaderTopLevelMessage::kStreamingUpdate:
            return writer.WriteMessage(static_cast<const osi3::StreamingUpdate&>(msg), topic);
        default:
            return false;
    }
}

const google::protobuf::Descriptor* GetDescriptor(osi3::ReaderTopLevelMessage type) {
    switch (type) {
        case osi3::ReaderTopLevelMessage::kGroundTruth:
            return osi3::GroundTruth::descriptor();
        case osi3::ReaderTopLevelMessage::kSensorData:
            return osi3::SensorData::descriptor();
        case osi3::ReaderTopLevelMessage::kSensorView:
            return osi3::SensorView::descriptor();
        case osi3::ReaderTopLevelMessage::kHostVehicleData:
            return osi3::HostVehicleData::descriptor();
        case osi3::ReaderTopLevelMessage::kTrafficCommand:
            return osi3::TrafficCommand::descriptor();
        case osi3::ReaderTopLevelMessage::kTrafficCommandUpdate:
            return osi3::TrafficCommandUpdate::descriptor();
        case osi3::ReaderTopLevelMessage::kTrafficUpdate:
            return osi3::TrafficUpdate::descriptor();
        case osi3::ReaderTopLevelMessage::kMotionRequest:
            return osi3::MotionRequest::descriptor();
        case osi3::ReaderTopLevelMessage::kStreamingUpdate:
            return osi3::StreamingUpdate::descriptor();
        default:
            return nullptr;
    }
}

void VerifyMessage(const osi3::ReadResult& result) {
    ASSERT_NE(result.message, nullptr);
    const auto* reflection = result.message->GetReflection();
    const auto* descriptor = result.message->GetDescriptor();
    const auto* timestamp_field = descriptor->FindFieldByName("timestamp");
    ASSERT_NE(timestamp_field, nullptr);
    const auto& timestamp_msg = reflection->GetMessage(*result.message, timestamp_field);
    const auto* ts_descriptor = timestamp_msg.GetDescriptor();
    const auto* ts_reflection = timestamp_msg.GetReflection();
    EXPECT_EQ(ts_reflection->GetInt64(timestamp_msg, ts_descriptor->FindFieldByName("seconds")), kTestTimestampSeconds);
    EXPECT_EQ(ts_reflection->GetUInt32(timestamp_msg, ts_descriptor->FindFieldByName("nanos")), static_cast<uint32_t>(kTestTimestampNanos));
}

}  // namespace

class RoundTripTest : public ::testing::TestWithParam<osi3::ReaderTopLevelMessage> {};

TEST_P(RoundTripTest, BinaryRoundTrip) {
    const auto msg_type = GetParam();
    const auto short_code = GetTypeShortCode(msg_type);
    const auto file_path = osi3::testing::MakeTempPath("rt_" + short_code, osi3::testing::FileExtensions::kOsi);

    // Write
    auto message = CreateMinimalMessage(msg_type);
    ASSERT_NE(message, nullptr);

    osi3::SingleChannelBinaryTraceFileWriter writer;
    ASSERT_TRUE(writer.Open(file_path));
    ASSERT_TRUE(WriteBinaryMessage(writer, msg_type, *message));
    writer.Close();

    // Read back
    osi3::SingleChannelBinaryTraceFileReader reader;
    ASSERT_TRUE(reader.Open(file_path, msg_type));
    ASSERT_TRUE(reader.HasNext());

    const auto result = reader.ReadMessage();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->message_type, msg_type);
    VerifyMessage(*result);

    EXPECT_FALSE(reader.HasNext());
    reader.Close();
    osi3::testing::SafeRemoveTestFile(file_path);
}

TEST_P(RoundTripTest, TXTHRoundTrip) {
    const auto msg_type = GetParam();
    const auto short_code = GetTypeShortCode(msg_type);
    const auto file_path = osi3::testing::MakeTempPath("rt_" + short_code, osi3::testing::FileExtensions::kTxth);

    // Write
    auto message = CreateMinimalMessage(msg_type);
    ASSERT_NE(message, nullptr);

    osi3::TXTHTraceFileWriter writer;
    ASSERT_TRUE(writer.Open(file_path));
    ASSERT_TRUE(WriteTxthMessage(writer, msg_type, *message));
    writer.Close();

    // Read back
    osi3::TXTHTraceFileReader reader;
    ASSERT_TRUE(reader.Open(file_path, msg_type));
    ASSERT_TRUE(reader.HasNext());

    const auto result = reader.ReadMessage();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->message_type, msg_type);
    VerifyMessage(*result);

    reader.Close();
    osi3::testing::SafeRemoveTestFile(file_path);
}

TEST_P(RoundTripTest, MCAPRoundTrip) {
    const auto msg_type = GetParam();
    const auto short_code = GetTypeShortCode(msg_type);
    const auto file_path = osi3::testing::MakeTempPath("rt_" + short_code, osi3::testing::FileExtensions::kMcap);
    const std::string topic = "test_" + short_code;

    // Write
    auto message = CreateMinimalMessage(msg_type);
    ASSERT_NE(message, nullptr);

    osi3::MCAPTraceFileWriter writer;
    ASSERT_TRUE(writer.Open(file_path));
    writer.AddFileMetadata(osi3::MCAPTraceFileWriter::PrepareRequiredFileMetadata());
    writer.AddChannel(topic, GetDescriptor(msg_type));
    ASSERT_TRUE(WriteMcapMessage(writer, msg_type, *message, topic));
    writer.Close();

    // Read back
    osi3::MCAPTraceFileReader reader;
    ASSERT_TRUE(reader.Open(file_path));
    ASSERT_TRUE(reader.HasNext());

    const auto result = reader.ReadMessage();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->message_type, msg_type);
    EXPECT_EQ(result->channel_name, topic);
    VerifyMessage(*result);

    EXPECT_FALSE(reader.HasNext());
    reader.Close();
    osi3::testing::SafeRemoveTestFile(file_path);
}

// SensorViewConfiguration excluded: it has no timestamp field, so writers cannot
// instantiate WriteMessage<SensorViewConfiguration> (MCAP writer requires timestamp()).
INSTANTIATE_TEST_SUITE_P(AllMessageTypes, RoundTripTest,
                         ::testing::Values(osi3::ReaderTopLevelMessage::kGroundTruth, osi3::ReaderTopLevelMessage::kSensorData, osi3::ReaderTopLevelMessage::kSensorView,
                                           osi3::ReaderTopLevelMessage::kHostVehicleData, osi3::ReaderTopLevelMessage::kTrafficCommand,
                                           osi3::ReaderTopLevelMessage::kTrafficCommandUpdate, osi3::ReaderTopLevelMessage::kTrafficUpdate,
                                           osi3::ReaderTopLevelMessage::kMotionRequest, osi3::ReaderTopLevelMessage::kStreamingUpdate),
                         [](const ::testing::TestParamInfo<osi3::ReaderTopLevelMessage>& info) { return GetTypeShortCode(info.param); });
