//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/writer/MCAPTraceFileWriter.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <mcap/reader.hpp>
#include <regex>
#include <string>

#include "../../TestUtilities.h"
#include "osi_groundtruth.pb.h"
#include "osi_sensordata.pb.h"

class MCAPTraceFileWriterTest : public ::testing::Test {
   protected:
    osi3::MCAPTraceFileWriter writer_;
    std::filesystem::path test_file_;

    void SetUp() override { test_file_ = osi3::testing::MakeTempPath("mcap", osi3::testing::FileExtensions::kMcap); }

    void TearDown() override {
        writer_.Close();
        osi3::testing::SafeRemoveTestFile(test_file_);
    }

    void AddRequiredMetadata() {
        auto required_metadata = osi3::MCAPTraceFileWriter::PrepareRequiredFileMetadata();
        required_metadata.metadata["description"] = "Example mcap trace file created with the ASAM OSI utilities library.";  // optional description
        if (!writer_.AddFileMetadata(required_metadata)) {
            throw std::runtime_error("Failed to add required metadata.");
        }
    }
};

TEST_F(MCAPTraceFileWriterTest, OpenCloseFile) {
    EXPECT_TRUE(writer_.Open(test_file_));
    EXPECT_TRUE(std::filesystem::exists(test_file_));
    writer_.Close();
}

TEST_F(MCAPTraceFileWriterTest, OpenWithCustomOptions) {
    mcap::McapWriterOptions mcap_options{"protobuf"};
    mcap_options.compression = mcap::Compression::None;
    mcap_options.chunkSize = 1024;

    EXPECT_TRUE(writer_.Open(test_file_, mcap_options));
    AddRequiredMetadata();

    // Write a test message to verify the options were applied
    osi3::GroundTruth ground_truth;
    ground_truth.mutable_timestamp()->set_seconds(123);
    ground_truth.mutable_timestamp()->set_nanos(456);

    // Add channel for ground truth
    const std::string topic = "/ground_truth";
    writer_.AddChannel(topic, osi3::GroundTruth::descriptor(), {});

    EXPECT_TRUE(writer_.WriteMessage(ground_truth, topic));

    writer_.Close();

    // Verify file exists and has non-zero size
    EXPECT_TRUE(std::filesystem::exists(test_file_));
    EXPECT_GT(std::filesystem::file_size(test_file_), 0);
}

TEST_F(MCAPTraceFileWriterTest, WriteMessage) {
    ASSERT_TRUE(writer_.Open(test_file_));
    AddRequiredMetadata();
    // Create test message
    osi3::GroundTruth ground_truth;
    ground_truth.mutable_timestamp()->set_seconds(123);
    ground_truth.mutable_timestamp()->set_nanos(456);

    // Add channel for ground truth
    const std::string topic = "/ground_truth";
    writer_.AddChannel(topic, osi3::GroundTruth::descriptor(), {});

    // Write message
    EXPECT_TRUE(writer_.WriteMessage(ground_truth, topic));
}

TEST_F(MCAPTraceFileWriterTest, TryWriteWithoutReqMetaData) {
    ASSERT_TRUE(writer_.Open(test_file_));
    // Create test message
    osi3::GroundTruth ground_truth;
    ground_truth.mutable_timestamp()->set_seconds(123);
    ground_truth.mutable_timestamp()->set_nanos(456);

    // Add channel for ground truth
    const std::string topic = "/ground_truth";
    writer_.AddChannel(topic, osi3::GroundTruth::descriptor(), {});

    // Try to write message but fail
    EXPECT_FALSE(writer_.WriteMessage(ground_truth, topic));
}

TEST_F(MCAPTraceFileWriterTest, SetMetadata) {
    ASSERT_TRUE(writer_.Open(test_file_));

    const std::unordered_map<std::string, std::string> metadata{{"key1", "value1"}, {"key2", "value2"}};

    EXPECT_TRUE(writer_.AddFileMetadata("test_metadata", metadata));
}

TEST_F(MCAPTraceFileWriterTest, AddChannel) {
    ASSERT_TRUE(writer_.Open(test_file_));

    const std::string topic = "/ground_truth";
    const std::unordered_map<std::string, std::string> channel_metadata{{"description", "Test channel"}};

    const uint16_t channel_id = writer_.AddChannel(topic, osi3::GroundTruth::descriptor(), channel_metadata);
    EXPECT_GT(channel_id, 0);

    // Test adding same topic with same schema
    const uint16_t second_id = writer_.AddChannel(topic, osi3::GroundTruth::descriptor(), channel_metadata);
    EXPECT_EQ(channel_id, second_id);
}

TEST_F(MCAPTraceFileWriterTest, WriteMessageWithoutChannel) {
    ASSERT_TRUE(writer_.Open(test_file_));

    osi3::GroundTruth ground_truth;
    ground_truth.mutable_timestamp()->set_seconds(123);

    // Try writing without adding channel first
    EXPECT_FALSE(writer_.WriteMessage(ground_truth, "/ground_truth"));
}

TEST_F(MCAPTraceFileWriterTest, WriteMessageWithEmptyTopic) {
    ASSERT_TRUE(writer_.Open(test_file_));

    const osi3::GroundTruth ground_truth;
    EXPECT_FALSE(writer_.WriteMessage(ground_truth, ""));
}

TEST_F(MCAPTraceFileWriterTest, GetCurrentTimeAsString) {
    const std::string timestamp = osi3::MCAPTraceFileWriter::GetCurrentTimeAsString();
    const std::regex iso8601_pattern(
        "-?([1-9][0-9]{3,}|0[0-9]{3})-(0[1-9]|1[0-2])-(0[1-9]|[12][0-9]|3[01])"
        "T(([01][0-9]|2[0-3]):[0-5][0-9]:[0-5][0-9](\\.[0-9]+)?|(24:00:00(\\.0+)?))"
        "(Z|(\\+|-)((0[0-9]|1[0-3]):[0-5][0-9]|14:00))");
    EXPECT_TRUE(std::regex_match(timestamp, iso8601_pattern));
}

TEST_F(MCAPTraceFileWriterTest, PrepareRequiredFileMetadata) {
    const auto metadata = osi3::MCAPTraceFileWriter::PrepareRequiredFileMetadata();
    EXPECT_EQ(metadata.name, "net.asam.osi.trace");
    EXPECT_NE(metadata.metadata.find("version"), metadata.metadata.end());
    EXPECT_NE(metadata.metadata.find("min_osi_version"), metadata.metadata.end());
    EXPECT_NE(metadata.metadata.find("max_osi_version"), metadata.metadata.end());
    EXPECT_NE(metadata.metadata.find("min_protobuf_version"), metadata.metadata.end());
    EXPECT_NE(metadata.metadata.find("max_protobuf_version"), metadata.metadata.end());
}

TEST_F(MCAPTraceFileWriterTest, AddFileMetadataDuplicate) {
    ASSERT_TRUE(writer_.Open(test_file_));
    auto metadata = osi3::MCAPTraceFileWriter::PrepareRequiredFileMetadata();
    EXPECT_TRUE(writer_.AddFileMetadata(metadata));
    // Try adding the same required metadata again
    EXPECT_FALSE(writer_.AddFileMetadata(metadata));
}

TEST_F(MCAPTraceFileWriterTest, OpenFileAlreadyOpened) {
    ASSERT_TRUE(writer_.Open(test_file_));
    // Try opening again while already open
    EXPECT_FALSE(writer_.Open(test_file_));
}

TEST_F(MCAPTraceFileWriterTest, OpenInvalidPath) {
    // Try opening a file in a non-existent directory
    EXPECT_FALSE(writer_.Open("/nonexistent/directory/test.mcap"));
}

TEST_F(MCAPTraceFileWriterTest, WriteMessageToClosedFile) {
    ASSERT_TRUE(writer_.Open(test_file_));
    writer_.Close();

    osi3::GroundTruth ground_truth;
    ground_truth.mutable_timestamp()->set_seconds(123);
    const std::string topic = "/ground_truth";

    EXPECT_FALSE(writer_.WriteMessage(ground_truth, topic));
}

TEST_F(MCAPTraceFileWriterTest, AddFileMetadataMissingRequiredFields) {
    ASSERT_TRUE(writer_.Open(test_file_));
    mcap::Metadata metadata;
    metadata.name = "net.asam.osi.trace";
    metadata.metadata["version"] = "1.0.0";  // Missing other required fields
    EXPECT_FALSE(writer_.AddFileMetadata(metadata));
}

TEST_F(MCAPTraceFileWriterTest, WriteMessageTopicNotFound) {
    ASSERT_TRUE(writer_.Open(test_file_));
    AddRequiredMetadata();

    osi3::GroundTruth ground_truth;
    ground_truth.mutable_timestamp()->set_seconds(123);

    // Try to write to a non-existent topic
    const std::string nonexistent_topic = "/nonexistent_topic";
    EXPECT_FALSE(writer_.WriteMessage(ground_truth, nonexistent_topic));
}

TEST_F(MCAPTraceFileWriterTest, WriteMessageFailedWrite) {
    ASSERT_TRUE(writer_.Open(test_file_));
    AddRequiredMetadata();

    osi3::GroundTruth ground_truth;
    ground_truth.mutable_timestamp()->set_seconds(123);

    const std::string topic = "/ground_truth";
    writer_.AddChannel(topic, osi3::GroundTruth::descriptor(), {});

    // Terminate file to force write failure
    writer_.GetMcapWriter()->terminate();

    EXPECT_FALSE(writer_.WriteMessage(ground_truth, topic));
}

TEST_F(MCAPTraceFileWriterTest, WriteMetadataFailedWrite) {
    ASSERT_TRUE(writer_.Open(test_file_));
    // Terminate file to force write failure
    writer_.GetMcapWriter()->terminate();

    EXPECT_THROW(AddRequiredMetadata(), std::runtime_error);
}

TEST_F(MCAPTraceFileWriterTest, AddChannelTopicExistsWithDifferentType) {
    ASSERT_TRUE(writer_.Open(test_file_));

    const std::string topic = "/test_topic";
    writer_.AddChannel(topic, osi3::GroundTruth::descriptor(), {});

    // Try to add same topic with different message type
    EXPECT_THROW(writer_.AddChannel(topic, osi3::SensorData::descriptor(), {}), std::runtime_error);
}

TEST_F(MCAPTraceFileWriterTest, AddChannelReuseExistingSchema) {
    ASSERT_TRUE(writer_.Open(test_file_));

    const std::string topic1 = "/topic1";
    const std::string topic2 = "/topic2";
    const uint16_t channel_id1 = writer_.AddChannel(topic1, osi3::GroundTruth::descriptor(), {});
    const uint16_t channel_id2 = writer_.AddChannel(topic2, osi3::GroundTruth::descriptor(), {});

    EXPECT_GT(channel_id1, 0);
    EXPECT_GT(channel_id2, 0);
    EXPECT_NE(channel_id1, channel_id2);
}

TEST_F(MCAPTraceFileWriterTest, WriteMultipleMessageTypes) {
    ASSERT_TRUE(writer_.Open(test_file_));
    AddRequiredMetadata();

    writer_.AddChannel("gt_topic", osi3::GroundTruth::descriptor());
    writer_.AddChannel("sd_topic", osi3::SensorData::descriptor());

    osi3::GroundTruth gt;
    gt.mutable_timestamp()->set_seconds(1);
    EXPECT_TRUE(writer_.WriteMessage(gt, "gt_topic"));

    osi3::SensorData sd;
    sd.mutable_timestamp()->set_seconds(2);
    EXPECT_TRUE(writer_.WriteMessage(sd, "sd_topic"));

    writer_.Close();
    EXPECT_TRUE(std::filesystem::exists(test_file_));
    EXPECT_GT(std::filesystem::file_size(test_file_), 0);
}

TEST_F(MCAPTraceFileWriterTest, AddChannelAutoOsiVersion) {
    ASSERT_TRUE(writer_.Open(test_file_));

    const std::string topic = "/test_channel";
    writer_.AddChannel(topic, osi3::GroundTruth::descriptor());

    AddRequiredMetadata();
    osi3::GroundTruth gt;
    gt.mutable_timestamp()->set_seconds(1);
    writer_.WriteMessage(gt, topic);
    writer_.Close();

    // Verify channel metadata by reading the MCAP file directly
    std::ifstream file(test_file_, std::ios::binary);
    mcap::McapReader mcap_reader;
    ASSERT_TRUE(mcap_reader.open(file).ok());
    ASSERT_TRUE(mcap_reader.readSummary(mcap::ReadSummaryMethod::AllowFallbackScan).ok());

    bool found_osi_version = false;
    for (const auto& [channel_id, channel] : mcap_reader.channels()) {
        if (channel->topic == topic) {
            if (channel->metadata.count("net.asam.osi.trace.channel.osi_version")) {
                found_osi_version = true;
                EXPECT_FALSE(channel->metadata.at("net.asam.osi.trace.channel.osi_version").empty());
            }
        }
    }
    EXPECT_TRUE(found_osi_version);
    mcap_reader.close();
}

TEST_F(MCAPTraceFileWriterTest, AddChannelAutoProtobufVersion) {
    ASSERT_TRUE(writer_.Open(test_file_));

    const std::string topic = "/test_channel";
    writer_.AddChannel(topic, osi3::GroundTruth::descriptor());

    AddRequiredMetadata();
    osi3::GroundTruth gt;
    gt.mutable_timestamp()->set_seconds(1);
    writer_.WriteMessage(gt, topic);
    writer_.Close();

    // Verify channel metadata
    std::ifstream file(test_file_, std::ios::binary);
    mcap::McapReader mcap_reader;
    ASSERT_TRUE(mcap_reader.open(file).ok());
    ASSERT_TRUE(mcap_reader.readSummary(mcap::ReadSummaryMethod::AllowFallbackScan).ok());

    bool found_protobuf_version = false;
    for (const auto& [channel_id, channel] : mcap_reader.channels()) {
        if (channel->topic == topic) {
            if (channel->metadata.count("net.asam.osi.trace.channel.protobuf_version")) {
                found_protobuf_version = true;
                EXPECT_FALSE(channel->metadata.at("net.asam.osi.trace.channel.protobuf_version").empty());
            }
        }
    }
    EXPECT_TRUE(found_protobuf_version);
    mcap_reader.close();
}
