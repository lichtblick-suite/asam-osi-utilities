//
// Copyright (c) 2024, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/reader/MCAPTraceFileReader.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

#include "../../TestUtilities.h"
#include "osi-utilities/tracefile/writer/MCAPTraceFileWriter.h"
#include "osi_groundtruth.pb.h"
#include "osi_sensorview.pb.h"

static const auto kJsonSchemaText = R"({
  "test_field1": "abc",
})";

class McapTraceFileReaderTest : public ::testing::Test {
   protected:
    osi3::MCAPTraceFileReader reader_;
    osi3::MCAPTraceFileWriter writer_;
    std::filesystem::path test_file_;

    void SetUp() override {
        test_file_ = osi3::testing::MakeTempPath("mcap", osi3::testing::FileExtensions::kMcap);
        CreateTestMcapFile();
    }

    void TearDown() override {
        reader_.Close();
        osi3::testing::SafeRemoveTestFile(test_file_);
    }

   private:
    void CreateTestMcapFile() {
        ASSERT_TRUE(writer_.Open(test_file_));

        // Add required metadata
        writer_.AddFileMetadata(osi3::MCAPTraceFileWriter::PrepareRequiredFileMetadata());

        // Add channels for different message types
        writer_.AddChannel("gt", osi3::GroundTruth::descriptor());
        writer_.AddChannel("sv", osi3::SensorView::descriptor());

        // Write GroundTruth message
        auto ground_truth = std::make_unique<osi3::GroundTruth>();
        ground_truth->mutable_timestamp()->set_seconds(0);
        ground_truth->mutable_timestamp()->set_nanos(456);
        ASSERT_TRUE(writer_.WriteMessage(*ground_truth, "gt"));

        // Write SensorView message
        auto sensor_view = std::make_unique<osi3::SensorView>();
        sensor_view->mutable_timestamp()->set_seconds(1);
        sensor_view->mutable_timestamp()->set_nanos(101);
        ASSERT_TRUE(writer_.WriteMessage(*sensor_view, "sv"));

        // Add non-OSI JSON channel and message
        auto* mcap_writer = writer_.GetMcapWriter();

        auto json_schema = mcap::Schema("my_json_schema", "jsonschema", kJsonSchemaText);
        mcap_writer->addSchema(json_schema);

        mcap::Channel channel("json_topic", "json", json_schema.id);
        mcap_writer->addChannel(channel);

        std::string json_data = "{\"test_field1\": \"data\"}";
        mcap::Message msg;
        msg.channelId = channel.id;
        msg.data = reinterpret_cast<const std::byte*>(json_data.data());
        msg.dataSize = json_data.size();
        msg.logTime = 2;
        msg.publishTime = msg.logTime;
        auto status = mcap_writer->write(msg);
        std::cout << "Status: " << status.message << std::endl;
        ASSERT_EQ(status.code, mcap::StatusCode::Success);
        writer_.Close();
    }
};

TEST_F(McapTraceFileReaderTest, OpenValidFile) { EXPECT_TRUE(reader_.Open(test_file_)); }

TEST_F(McapTraceFileReaderTest, OpenNonexistentFile) { EXPECT_FALSE(reader_.Open("nonexistent.mcap")); }

TEST_F(McapTraceFileReaderTest, OpenWithReaderOptions) {
    mcap::ReadMessageOptions options;
    options.startTime = 1000000;
    options.endTime = options.startTime + 1;
    ASSERT_TRUE(reader_.Open(test_file_.string(), options));

    // Verify behavior with the configured options
    // that no messages should be returned
    EXPECT_FALSE(reader_.HasNext());
}

TEST_F(McapTraceFileReaderTest, ReadGroundTruthMessage) {
    ASSERT_TRUE(reader_.Open(test_file_));
    reader_.SetSkipNonOSIMsgs(true);
    EXPECT_TRUE(reader_.HasNext());

    const auto result = reader_.ReadMessage();
    ASSERT_TRUE(result.has_value());

    auto* ground_truth = dynamic_cast<osi3::GroundTruth*>(result->message.get());
    ASSERT_NE(ground_truth, nullptr);
    EXPECT_EQ(ground_truth->timestamp().seconds(), 0);
    EXPECT_EQ(ground_truth->timestamp().nanos(), 456);
    EXPECT_EQ(result->channel_name, "gt");
}

TEST_F(McapTraceFileReaderTest, ReadSensorViewMessage) {
    ASSERT_TRUE(reader_.Open(test_file_));
    reader_.SetSkipNonOSIMsgs(true);
    ASSERT_TRUE(reader_.HasNext());

    // Skip first message
    reader_.ReadMessage();

    const auto result = reader_.ReadMessage();
    ASSERT_TRUE(result.has_value());

    auto* sensor_view = dynamic_cast<osi3::SensorView*>(result->message.get());
    ASSERT_NE(sensor_view, nullptr);
    EXPECT_EQ(sensor_view->timestamp().seconds(), 1);
    EXPECT_EQ(sensor_view->timestamp().nanos(), 101);
    EXPECT_EQ(result->channel_name, "sv");
}

TEST_F(McapTraceFileReaderTest, PreventMultipleFileOpens) {
    // First open should succeed
    EXPECT_TRUE(reader_.Open(test_file_));

    // Second open should fail while first file is still open
    EXPECT_FALSE(reader_.Open("testdata/another.mcap"));

    // After closing, opening a new file should work
    reader_.Close();
    EXPECT_TRUE(reader_.Open(test_file_));
}

TEST_F(McapTraceFileReaderTest, HasNextReturnsFalseWhenEmpty) {
    ASSERT_TRUE(reader_.Open(test_file_));
    reader_.SetSkipNonOSIMsgs(true);
    ASSERT_TRUE(reader_.HasNext());

    reader_.ReadMessage();  // Read first message
    ASSERT_TRUE(reader_.HasNext());
    reader_.ReadMessage();  // Read second message
    reader_.ReadMessage();  // Read third non-osi message
    EXPECT_FALSE(reader_.HasNext());
}

TEST_F(McapTraceFileReaderTest, ReadMessageReturnsNulloptWhenEmpty) {
    ASSERT_TRUE(reader_.Open(test_file_));
    reader_.SetSkipNonOSIMsgs(true);

    reader_.ReadMessage();  // Read first message
    reader_.ReadMessage();  // Read second message
    reader_.ReadMessage();  // Read third non-osi message

    const auto result = reader_.ReadMessage();
    EXPECT_FALSE(result.has_value());
}

TEST_F(McapTraceFileReaderTest, HasNextReturnsFalseWhenNotOpened) { EXPECT_FALSE(reader_.HasNext()); }

TEST_F(McapTraceFileReaderTest, ReadInvalidMessageFormat) {
    const std::string invalid_file = "invalid.mcap";
    {
        std::ofstream file(invalid_file, std::ios::binary);
        file << "Invalid MCAP format";
    }

    EXPECT_FALSE(reader_.Open(invalid_file));
    reader_.Close();  // Ensure any partial open is closed before removing
    std::filesystem::remove(invalid_file);
}

TEST_F(McapTraceFileReaderTest, CloseAndReopenFile) {
    ASSERT_TRUE(reader_.Open(test_file_));
    reader_.Close();
    EXPECT_TRUE(reader_.Open(test_file_));
    EXPECT_TRUE(reader_.HasNext());
}

TEST_F(McapTraceFileReaderTest, SkipNonOSIMessagesWhenEnabled) {
    ASSERT_TRUE(reader_.Open(test_file_));
    reader_.SetSkipNonOSIMsgs(true);

    // Read first OSI message (GroundTruth)
    auto result1 = reader_.ReadMessage();
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1->channel_name, "gt");

    // Read second OSI message (SensorView)
    auto result2 = reader_.ReadMessage();
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2->channel_name, "sv");

    // Third message (JSON) should be skipped automatically
    auto result3 = reader_.ReadMessage();
    EXPECT_FALSE(result3.has_value());
}

TEST_F(McapTraceFileReaderTest, ThrowExceptionForNonOSIMessagesWhenSkipDisabled) {
    ASSERT_TRUE(reader_.Open(test_file_));
    reader_.SetSkipNonOSIMsgs(false);

    // Read first OSI message (GroundTruth)
    auto result1 = reader_.ReadMessage();
    ASSERT_TRUE(result1.has_value());

    // Read second OSI message (SensorView)
    auto result2 = reader_.ReadMessage();
    ASSERT_TRUE(result2.has_value());

    // Third message (JSON) should throw an exception
    EXPECT_THROW({ reader_.ReadMessage(); }, std::runtime_error);
}
