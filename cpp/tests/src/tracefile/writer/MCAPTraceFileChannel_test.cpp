//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/writer/MCAPTraceFileChannel.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <mcap/reader.hpp>
#include <mcap/writer.hpp>
#include <string>

#include "../../TestUtilities.h"
#include "osi-utilities/tracefile/reader/MCAPTraceFileReader.h"
#include "osi_groundtruth.pb.h"
#include "osi_sensorview.pb.h"

class MCAPTraceFileChannelTest : public ::testing::Test {
   protected:
    std::filesystem::path test_file_;
    std::ofstream file_stream_;
    mcap::McapWriter mcap_writer_;

    void SetUp() override { test_file_ = osi3::testing::MakeTempPath("channel_test", osi3::testing::FileExtensions::kMcap); }

    void TearDown() override {
        if (file_stream_.is_open()) {
            mcap_writer_.close();
            file_stream_.close();
        }
        osi3::testing::SafeRemoveTestFile(test_file_);
    }

    void OpenWriter() {
        file_stream_.open(test_file_, std::ios::binary);
        ASSERT_TRUE(file_stream_.is_open());
        mcap_writer_.open(file_stream_, mcap::McapWriterOptions("protobuf"));
    }

    void CloseWriter() {
        mcap_writer_.close();
        file_stream_.close();
    }
};

TEST_F(MCAPTraceFileChannelTest, WriteOSIMessagesWithExternalWriter) {
    OpenWriter();

    // Create channel helper attached to external writer
    osi3::MCAPTraceFileChannel osi_channel(mcap_writer_);

    // Add required OSI metadata
    const auto status = mcap_writer_.write(osi3::MCAPTraceFileChannel::PrepareRequiredFileMetadata());
    ASSERT_EQ(status.code, mcap::StatusCode::Success);

    // Add OSI channel
    const auto channel_id = osi_channel.AddChannel("ground_truth", osi3::GroundTruth::descriptor());
    EXPECT_GT(channel_id, 0);

    // Write OSI message
    osi3::GroundTruth gt;
    gt.mutable_timestamp()->set_seconds(1);
    gt.mutable_timestamp()->set_nanos(500000000);
    ASSERT_TRUE(osi_channel.WriteMessage(gt, "ground_truth"));

    CloseWriter();

    // Verify with reader
    osi3::MCAPTraceFileReader reader;
    ASSERT_TRUE(reader.Open(test_file_));

    const auto result = reader.ReadMessage();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->message_type, osi3::ReaderTopLevelMessage::kGroundTruth);
    EXPECT_EQ(result->channel_name, "ground_truth");

    reader.Close();
}

TEST_F(MCAPTraceFileChannelTest, MixedOSIAndNonOSIChannels) {
    OpenWriter();

    // Add a non-OSI channel first (user's own channel)
    mcap::Schema custom_schema("my.custom.Message", "protobuf", "");
    mcap_writer_.addSchema(custom_schema);
    mcap::Channel custom_channel("custom_topic", "protobuf", custom_schema.id);
    mcap_writer_.addChannel(custom_channel);

    // Now add OSI channels via helper
    osi3::MCAPTraceFileChannel osi_channel(mcap_writer_);
    ASSERT_EQ(mcap_writer_.write(osi3::MCAPTraceFileChannel::PrepareRequiredFileMetadata()).code, mcap::StatusCode::Success);
    osi_channel.AddChannel("osi/ground_truth", osi3::GroundTruth::descriptor());
    osi_channel.AddChannel("osi/sensor_view", osi3::SensorView::descriptor());

    // Write a custom message
    std::string custom_data = "custom payload";
    mcap::Message custom_msg;
    custom_msg.channelId = custom_channel.id;
    custom_msg.data = reinterpret_cast<const std::byte*>(custom_data.data());
    custom_msg.dataSize = custom_data.size();
    custom_msg.logTime = 1'000'000'000;
    custom_msg.publishTime = custom_msg.logTime;
    ASSERT_EQ(mcap_writer_.write(custom_msg).code, mcap::StatusCode::Success);

    // Write OSI messages
    osi3::GroundTruth gt;
    gt.mutable_timestamp()->set_seconds(2);
    ASSERT_TRUE(osi_channel.WriteMessage(gt, "osi/ground_truth"));

    osi3::SensorView sv;
    sv.mutable_timestamp()->set_seconds(3);
    ASSERT_TRUE(osi_channel.WriteMessage(sv, "osi/sensor_view"));

    CloseWriter();

    // Verify OSI messages can be read
    osi3::MCAPTraceFileReader reader;
    ASSERT_TRUE(reader.Open(test_file_));
    reader.SetSkipNonOSIMsgs(true);

    std::vector<std::string> osi_channels_read;
    while (reader.HasNext()) {
        const auto result = reader.ReadMessage();
        if (result.has_value()) {
            osi_channels_read.push_back(result->channel_name);
        }
    }

    ASSERT_EQ(osi_channels_read.size(), 2);
    EXPECT_EQ(osi_channels_read[0], "osi/ground_truth");
    EXPECT_EQ(osi_channels_read[1], "osi/sensor_view");

    reader.Close();
}

TEST_F(MCAPTraceFileChannelTest, SchemaDeduplication) {
    OpenWriter();

    osi3::MCAPTraceFileChannel osi_channel(mcap_writer_);
    ASSERT_EQ(mcap_writer_.write(osi3::MCAPTraceFileChannel::PrepareRequiredFileMetadata()).code, mcap::StatusCode::Success);

    // Add multiple channels with the same message type
    const auto id1 = osi_channel.AddChannel("gt_1", osi3::GroundTruth::descriptor());
    const auto id2 = osi_channel.AddChannel("gt_2", osi3::GroundTruth::descriptor());
    const auto id3 = osi_channel.AddChannel("gt_3", osi3::GroundTruth::descriptor());

    // Each channel should have a unique ID
    EXPECT_NE(id1, id2);
    EXPECT_NE(id2, id3);
    EXPECT_NE(id1, id3);

    // Write to all channels
    osi3::GroundTruth gt;
    gt.mutable_timestamp()->set_seconds(1);
    ASSERT_TRUE(osi_channel.WriteMessage(gt, "gt_1"));
    gt.mutable_timestamp()->set_seconds(2);
    ASSERT_TRUE(osi_channel.WriteMessage(gt, "gt_2"));
    gt.mutable_timestamp()->set_seconds(3);
    ASSERT_TRUE(osi_channel.WriteMessage(gt, "gt_3"));

    CloseWriter();

    // Verify file has only one schema but three channels
    std::ifstream file(test_file_, std::ios::binary);
    mcap::McapReader mcap_reader;
    ASSERT_TRUE(mcap_reader.open(file).ok());
    ASSERT_TRUE(mcap_reader.readSummary(mcap::ReadSummaryMethod::AllowFallbackScan).ok());

    // Count schemas with GroundTruth name
    int gt_schema_count = 0;
    for (const auto& [id, schema] : mcap_reader.schemas()) {
        if (schema->name == "osi3.GroundTruth") {
            ++gt_schema_count;
        }
    }
    EXPECT_EQ(gt_schema_count, 1);

    // Count channels
    EXPECT_EQ(mcap_reader.channels().size(), 3);

    mcap_reader.close();
}

TEST_F(MCAPTraceFileChannelTest, DuplicateTopicReturnsExistingId) {
    OpenWriter();

    osi3::MCAPTraceFileChannel osi_channel(mcap_writer_);

    const auto id1 = osi_channel.AddChannel("duplicate_topic", osi3::GroundTruth::descriptor());
    const auto id2 = osi_channel.AddChannel("duplicate_topic", osi3::GroundTruth::descriptor());

    // Same topic with same type should return the same channel ID
    EXPECT_EQ(id1, id2);

    CloseWriter();
}

TEST_F(MCAPTraceFileChannelTest, WriteToUnregisteredTopicFails) {
    OpenWriter();

    osi3::MCAPTraceFileChannel osi_channel(mcap_writer_);

    osi3::GroundTruth gt;
    gt.mutable_timestamp()->set_seconds(1);

    // Writing to a topic that was never added should fail
    EXPECT_FALSE(osi_channel.WriteMessage(gt, "nonexistent_topic"));

    CloseWriter();
}

TEST_F(MCAPTraceFileChannelTest, AddChannelTopicExistsWithDifferentType) {
    OpenWriter();

    osi3::MCAPTraceFileChannel osi_channel(mcap_writer_);
    osi_channel.AddChannel("topic", osi3::GroundTruth::descriptor());

    EXPECT_THROW(osi_channel.AddChannel("topic", osi3::SensorView::descriptor()), std::runtime_error);

    CloseWriter();
}

TEST_F(MCAPTraceFileChannelTest, WriteWithEmptyTopicFails) {
    OpenWriter();

    osi3::MCAPTraceFileChannel osi_channel(mcap_writer_);
    osi_channel.AddChannel("valid_topic", osi3::GroundTruth::descriptor());

    osi3::GroundTruth gt;
    gt.mutable_timestamp()->set_seconds(1);

    // Empty topic should fail
    EXPECT_FALSE(osi_channel.WriteMessage(gt, ""));

    CloseWriter();
}

TEST_F(MCAPTraceFileChannelTest, PrepareRequiredFileMetadataContent) {
    const auto metadata = osi3::MCAPTraceFileChannel::PrepareRequiredFileMetadata();

    EXPECT_EQ(metadata.name, "net.asam.osi.trace");
    EXPECT_TRUE(metadata.metadata.find("version") != metadata.metadata.end());
    EXPECT_TRUE(metadata.metadata.find("min_osi_version") != metadata.metadata.end());
    EXPECT_TRUE(metadata.metadata.find("max_osi_version") != metadata.metadata.end());
    EXPECT_TRUE(metadata.metadata.find("min_protobuf_version") != metadata.metadata.end());
    EXPECT_TRUE(metadata.metadata.find("max_protobuf_version") != metadata.metadata.end());
}

TEST_F(MCAPTraceFileChannelTest, GetCurrentTimeAsStringFormat) {
    const auto time_str = osi3::MCAPTraceFileChannel::GetCurrentTimeAsString();

    // Should be in ISO 8601 format with Z suffix
    EXPECT_FALSE(time_str.empty());
    EXPECT_EQ(time_str.back(), 'Z');
    EXPECT_NE(time_str.find('T'), std::string::npos);
}

TEST_F(MCAPTraceFileChannelTest, ChannelMetadataContainsOSIVersion) {
    OpenWriter();

    osi3::MCAPTraceFileChannel osi_channel(mcap_writer_);
    osi_channel.AddChannel("gt", osi3::GroundTruth::descriptor());

    // Write at least one message so the channel is persisted in a chunk
    osi3::GroundTruth gt;
    gt.mutable_timestamp()->set_seconds(1);
    ASSERT_TRUE(osi_channel.WriteMessage(gt, "gt"));

    CloseWriter();

    // Verify channel metadata
    std::ifstream file(test_file_, std::ios::binary);
    mcap::McapReader mcap_reader;
    ASSERT_TRUE(mcap_reader.open(file).ok());
    ASSERT_TRUE(mcap_reader.readSummary(mcap::ReadSummaryMethod::AllowFallbackScan).ok());

    bool found_channel = false;
    for (const auto& [id, channel] : mcap_reader.channels()) {
        if (channel->topic == "gt") {
            found_channel = true;
            EXPECT_TRUE(channel->metadata.find("net.asam.osi.trace.channel.osi_version") != channel->metadata.end());
            EXPECT_TRUE(channel->metadata.find("net.asam.osi.trace.channel.protobuf_version") != channel->metadata.end());
        }
    }
    EXPECT_TRUE(found_channel);

    mcap_reader.close();
}
