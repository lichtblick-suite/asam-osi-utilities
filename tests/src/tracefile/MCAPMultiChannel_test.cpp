//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <mcap/reader.hpp>
#include <string>
#include <vector>

#include "../TestUtilities.h"
#include "osi-utilities/tracefile/reader/MCAPTraceFileReader.h"
#include "osi-utilities/tracefile/writer/MCAPTraceFileWriter.h"
#include "osi_groundtruth.pb.h"
#include "osi_sensordata.pb.h"
#include "osi_sensorview.pb.h"

class MCAPMultiChannelTest : public ::testing::Test {
   protected:
    osi3::MCAPTraceFileWriter writer_;
    osi3::MCAPTraceFileReader reader_;
    std::filesystem::path test_file_;

    void SetUp() override { test_file_ = osi3::testing::MakeTempPath("multi", osi3::testing::FileExtensions::kMcap); }

    void TearDown() override {
        reader_.Close();
        writer_.Close();
        osi3::testing::SafeRemoveTestFile(test_file_);
    }
};

TEST_F(MCAPMultiChannelTest, WriteAndReadTwoChannelsSameType) {
    ASSERT_TRUE(writer_.Open(test_file_));
    writer_.AddFileMetadata(osi3::MCAPTraceFileWriter::PrepareRequiredFileMetadata());
    writer_.AddChannel("gt_channel_1", osi3::GroundTruth::descriptor());
    writer_.AddChannel("gt_channel_2", osi3::GroundTruth::descriptor());

    osi3::GroundTruth gt1;
    gt1.mutable_timestamp()->set_seconds(1);
    ASSERT_TRUE(writer_.WriteMessage(gt1, "gt_channel_1"));

    osi3::GroundTruth gt2;
    gt2.mutable_timestamp()->set_seconds(2);
    ASSERT_TRUE(writer_.WriteMessage(gt2, "gt_channel_2"));

    writer_.Close();

    ASSERT_TRUE(reader_.Open(test_file_));
    reader_.SetSkipNonOSIMsgs(true);

    const auto result1 = reader_.ReadMessage();
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1->message_type, osi3::ReaderTopLevelMessage::kGroundTruth);
    EXPECT_EQ(result1->channel_name, "gt_channel_1");
    const auto result2 = reader_.ReadMessage();
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2->message_type, osi3::ReaderTopLevelMessage::kGroundTruth);
    EXPECT_EQ(result2->channel_name, "gt_channel_2");
    EXPECT_FALSE(reader_.HasNext());
}

TEST_F(MCAPMultiChannelTest, WriteAndReadMixedTypes) {
    ASSERT_TRUE(writer_.Open(test_file_));
    writer_.AddFileMetadata(osi3::MCAPTraceFileWriter::PrepareRequiredFileMetadata());
    writer_.AddChannel("ground_truth", osi3::GroundTruth::descriptor());
    writer_.AddChannel("sensor_view", osi3::SensorView::descriptor());
    writer_.AddChannel("sensor_data", osi3::SensorData::descriptor());

    osi3::GroundTruth gt;
    gt.mutable_timestamp()->set_seconds(1);
    ASSERT_TRUE(writer_.WriteMessage(gt, "ground_truth"));

    osi3::SensorView sv;
    sv.mutable_timestamp()->set_seconds(2);
    ASSERT_TRUE(writer_.WriteMessage(sv, "sensor_view"));

    osi3::SensorData sd;
    sd.mutable_timestamp()->set_seconds(3);
    ASSERT_TRUE(writer_.WriteMessage(sd, "sensor_data"));

    writer_.Close();

    ASSERT_TRUE(reader_.Open(test_file_));

    std::vector<osi3::ReaderTopLevelMessage> types_read;
    std::vector<std::string> channels_read;
    while (reader_.HasNext()) {
        const auto result = reader_.ReadMessage();
        if (result.has_value()) {
            types_read.push_back(result->message_type);
            channels_read.push_back(result->channel_name);
        }
    }

    ASSERT_EQ(types_read.size(), 3);
    EXPECT_EQ(types_read[0], osi3::ReaderTopLevelMessage::kGroundTruth);
    EXPECT_EQ(types_read[1], osi3::ReaderTopLevelMessage::kSensorView);
    EXPECT_EQ(types_read[2], osi3::ReaderTopLevelMessage::kSensorData);
    EXPECT_EQ(channels_read[0], "ground_truth");
    EXPECT_EQ(channels_read[1], "sensor_view");
    EXPECT_EQ(channels_read[2], "sensor_data");
}

TEST_F(MCAPMultiChannelTest, ReadWithTimeFilter) {
    ASSERT_TRUE(writer_.Open(test_file_));
    writer_.AddFileMetadata(osi3::MCAPTraceFileWriter::PrepareRequiredFileMetadata());
    writer_.AddChannel("gt", osi3::GroundTruth::descriptor());

    // Write 10 messages with timestamps 0-9 seconds
    for (int i = 0; i < 10; ++i) {
        osi3::GroundTruth gt;
        gt.mutable_timestamp()->set_seconds(i);
        gt.mutable_timestamp()->set_nanos(0);
        ASSERT_TRUE(writer_.WriteMessage(gt, "gt"));
    }
    writer_.Close();

    // Read with time filter: messages at timestamps 3-6 seconds (nanoseconds in MCAP)
    mcap::ReadMessageOptions options;
    options.startTime = 3'000'000'000;  // 3 seconds in nanoseconds
    options.endTime = 7'000'000'000;    // 7 seconds in nanoseconds (exclusive)
    ASSERT_TRUE(reader_.Open(test_file_, options));

    int count = 0;
    while (reader_.HasNext()) {
        const auto result = reader_.ReadMessage();
        if (result.has_value()) {
            ++count;
        }
    }
    EXPECT_GT(count, 0);
    EXPECT_LT(count, 10);
}

TEST_F(MCAPMultiChannelTest, SkipNonOSIInMultiChannel) {
    ASSERT_TRUE(writer_.Open(test_file_));
    writer_.AddFileMetadata(osi3::MCAPTraceFileWriter::PrepareRequiredFileMetadata());
    writer_.AddChannel("gt", osi3::GroundTruth::descriptor());

    osi3::GroundTruth gt;
    gt.mutable_timestamp()->set_seconds(1);
    ASSERT_TRUE(writer_.WriteMessage(gt, "gt"));

    // Add a non-OSI JSON message directly via MCAP writer
    auto* mcap_writer = writer_.GetMcapWriter();
    auto json_schema = mcap::Schema("my_json_schema", "jsonschema", "{}");
    mcap_writer->addSchema(json_schema);
    mcap::Channel json_channel("json_data", "json", json_schema.id);
    mcap_writer->addChannel(json_channel);
    std::string json_data = R"({"key": "value"})";
    mcap::Message msg;
    msg.channelId = json_channel.id;
    msg.data = reinterpret_cast<const std::byte*>(json_data.data());
    msg.dataSize = json_data.size();
    msg.logTime = 2'000'000'000;
    msg.publishTime = msg.logTime;
    ASSERT_EQ(mcap_writer->write(msg).code, mcap::StatusCode::Success);

    writer_.Close();

    ASSERT_TRUE(reader_.Open(test_file_));
    reader_.SetSkipNonOSIMsgs(true);

    const auto result1 = reader_.ReadMessage();
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1->message_type, osi3::ReaderTopLevelMessage::kGroundTruth);
    // JSON message should be skipped
    const auto result2 = reader_.ReadMessage();
    EXPECT_FALSE(result2.has_value());
}

TEST_F(MCAPMultiChannelTest, MetadataRoundTrip) {
    ASSERT_TRUE(writer_.Open(test_file_));
    writer_.AddFileMetadata(osi3::MCAPTraceFileWriter::PrepareRequiredFileMetadata());
    writer_.AddFileMetadata("custom_metadata", {{"key1", "value1"}, {"key2", "value2"}});
    writer_.AddChannel("gt", osi3::GroundTruth::descriptor());

    osi3::GroundTruth gt;
    gt.mutable_timestamp()->set_seconds(1);
    ASSERT_TRUE(writer_.WriteMessage(gt, "gt"));
    writer_.Close();

    // Verify metadata by reading the MCAP file directly
    std::ifstream file(test_file_, std::ios::binary);
    mcap::McapReader mcap_reader;
    ASSERT_TRUE(mcap_reader.open(file).ok());
    ASSERT_TRUE(mcap_reader.readSummary(mcap::ReadSummaryMethod::AllowFallbackScan).ok());

    bool found_custom_metadata = false;
    for (const auto& [name, metadata_index] : mcap_reader.metadataIndexes()) {
        if (name == "custom_metadata") {
            mcap::Record record{};
            const auto read_status = mcap::McapReader::ReadRecord(*mcap_reader.dataSource(), metadata_index.offset, &record);
            ASSERT_TRUE(read_status.ok());
            mcap::Metadata metadata;
            const auto parse_status = mcap::McapReader::ParseMetadata(record, &metadata);
            ASSERT_TRUE(parse_status.ok());
            found_custom_metadata = true;
            EXPECT_EQ(metadata.metadata.at("key1"), "value1");
            EXPECT_EQ(metadata.metadata.at("key2"), "value2");
        }
    }
    EXPECT_TRUE(found_custom_metadata);
    mcap_reader.close();
}
