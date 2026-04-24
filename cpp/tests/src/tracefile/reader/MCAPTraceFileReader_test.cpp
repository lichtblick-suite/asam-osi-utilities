//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/reader/MCAPTraceFileReader.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <mcap/writer.hpp>
#include <string>
#include <type_traits>

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

        std::string json_data = R"({"test_field1": "data"})";
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
    reader_.SetSkipIncompatibleMessages(true);
    EXPECT_TRUE(reader_.HasNext());

    const auto result = reader_.ReadMessage();
    if (!result.has_value()) {
        FAIL() << "Expected GroundTruth message";
        return;
    }

    auto* ground_truth = dynamic_cast<osi3::GroundTruth*>(result->message.get());
    ASSERT_NE(ground_truth, nullptr);
    EXPECT_EQ(ground_truth->timestamp().seconds(), 0);
    EXPECT_EQ(ground_truth->timestamp().nanos(), 456);
    EXPECT_EQ(result->channel_name, "gt");
}

TEST_F(McapTraceFileReaderTest, ReadSensorViewMessage) {
    ASSERT_TRUE(reader_.Open(test_file_));
    reader_.SetSkipIncompatibleMessages(true);
    ASSERT_TRUE(reader_.HasNext());

    // Skip first message
    reader_.ReadMessage();

    const auto result = reader_.ReadMessage();
    if (!result.has_value()) {
        FAIL() << "Expected SensorView message";
        return;
    }

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
    reader_.SetSkipIncompatibleMessages(true);
    ASSERT_TRUE(reader_.HasNext());

    reader_.ReadMessage();  // Read first message
    ASSERT_TRUE(reader_.HasNext());
    reader_.ReadMessage();  // Read second message
    reader_.ReadMessage();  // Read third non-osi message
    EXPECT_FALSE(reader_.HasNext());
}

TEST_F(McapTraceFileReaderTest, ReadMessageReturnsNulloptWhenEmpty) {
    ASSERT_TRUE(reader_.Open(test_file_));
    reader_.SetSkipIncompatibleMessages(true);

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
    reader_.SetSkipIncompatibleMessages(true);

    // Read first OSI message (GroundTruth)
    auto result1 = reader_.ReadMessage();
    if (!result1.has_value()) {
        FAIL() << "Expected GroundTruth message";
        return;
    }
    EXPECT_EQ(result1->channel_name, "gt");

    // Read second OSI message (SensorView)
    auto result2 = reader_.ReadMessage();
    if (!result2.has_value()) {
        FAIL() << "Expected SensorView message";
        return;
    }
    EXPECT_EQ(result2->channel_name, "sv");

    // Third message (JSON) should be skipped automatically
    auto result3 = reader_.ReadMessage();
    EXPECT_FALSE(result3.has_value());
}

TEST_F(McapTraceFileReaderTest, ThrowExceptionForNonOSIMessagesWhenSkipDisabled) {
    ASSERT_TRUE(reader_.Open(test_file_));
    reader_.SetSkipIncompatibleMessages(false);

    // Read first OSI message (GroundTruth)
    auto result1 = reader_.ReadMessage();
    ASSERT_TRUE(result1.has_value());

    // Read second OSI message (SensorView)
    auto result2 = reader_.ReadMessage();
    ASSERT_TRUE(result2.has_value());

    // Third message (JSON) should return kIncompatible status
    auto result3 = reader_.ReadMessage();
    ASSERT_TRUE(result3.has_value());
    EXPECT_EQ(result3->status, osi3::ReadStatus::kIncompatible);
    EXPECT_EQ(result3->message, nullptr);
    EXPECT_FALSE(result3->error_message.empty());
    EXPECT_EQ(result3->channel_name, "json_topic");
}

TEST_F(McapTraceFileReaderTest, ReadEmptyMcapFile) {
    // Create a valid MCAP file with 0 messages
    const auto empty_file = osi3::testing::MakeTempPath("empty", osi3::testing::FileExtensions::kMcap);
    {
        osi3::MCAPTraceFileWriter empty_writer;
        ASSERT_TRUE(empty_writer.Open(empty_file));
        empty_writer.AddFileMetadata(osi3::MCAPTraceFileWriter::PrepareRequiredFileMetadata());
        empty_writer.Close();
    }

    osi3::MCAPTraceFileReader empty_reader;
    ASSERT_TRUE(empty_reader.Open(empty_file));
    EXPECT_FALSE(empty_reader.HasNext());
    empty_reader.Close();
    osi3::testing::SafeRemoveTestFile(empty_file);
}

TEST_F(McapTraceFileReaderTest, CloseAndReopenWithDifferentOptions) {
    ASSERT_TRUE(reader_.Open(test_file_));
    reader_.Close();

    mcap::ReadMessageOptions options;
    options.startTime = 0;
    options.endTime = 1;
    EXPECT_TRUE(reader_.Open(test_file_, options));
    reader_.Close();
}

TEST_F(McapTraceFileReaderTest, SetTopicsFiltersMessages) {
    reader_.SetTopics({"gt"});
    reader_.SetSkipIncompatibleMessages(true);
    ASSERT_TRUE(reader_.Open(test_file_));

    // Should only get GroundTruth messages
    auto result1 = reader_.ReadMessage();
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1->channel_name, "gt");
    EXPECT_EQ(result1->message_type, osi3::ReaderTopLevelMessage::kGroundTruth);

    // No more messages (sv and json_topic are filtered out)
    auto result2 = reader_.ReadMessage();
    EXPECT_FALSE(result2.has_value());
}

TEST_F(McapTraceFileReaderTest, SetTopicsFiltersMessagesAfterOpen) {
    ASSERT_TRUE(reader_.Open(test_file_));
    reader_.SetSkipIncompatibleMessages(true);
    reader_.SetTopics({"gt"});

    auto result1 = reader_.ReadMessage();
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1->channel_name, "gt");
    EXPECT_EQ(result1->message_type, osi3::ReaderTopLevelMessage::kGroundTruth);

    auto result2 = reader_.ReadMessage();
    EXPECT_FALSE(result2.has_value());
}

TEST_F(McapTraceFileReaderTest, SetTopicsWithEmptySetClearsFilterAndRestartsIteration) {
    ASSERT_TRUE(reader_.Open(test_file_));
    reader_.SetSkipIncompatibleMessages(true);
    reader_.SetTopics({"gt"});

    auto filtered_result = reader_.ReadMessage();
    ASSERT_TRUE(filtered_result.has_value());
    EXPECT_EQ(filtered_result->channel_name, "gt");
    EXPECT_FALSE(reader_.ReadMessage().has_value());

    reader_.SetTopics({});

    auto first_result = reader_.ReadMessage();
    ASSERT_TRUE(first_result.has_value());
    EXPECT_EQ(first_result->channel_name, "gt");

    auto second_result = reader_.ReadMessage();
    ASSERT_TRUE(second_result.has_value());
    EXPECT_EQ(second_result->channel_name, "sv");

    EXPECT_FALSE(reader_.ReadMessage().has_value());
}

TEST_F(McapTraceFileReaderTest, GetAvailableTopics) {
    ASSERT_TRUE(reader_.Open(test_file_));

    auto topics = reader_.GetAvailableTopics();
    EXPECT_EQ(topics.size(), 3);

    // Check all expected topics are present (order is unspecified)
    std::sort(topics.begin(), topics.end());
    EXPECT_EQ(topics[0], "gt");
    EXPECT_EQ(topics[1], "json_topic");
    EXPECT_EQ(topics[2], "sv");
}

TEST_F(McapTraceFileReaderTest, GetAvailableTopicsBeforeOpen) {
    auto topics = reader_.GetAvailableTopics();
    EXPECT_TRUE(topics.empty());
}

TEST_F(McapTraceFileReaderTest, GetFileMetadata) {
    ASSERT_TRUE(reader_.Open(test_file_));

    auto metadata = reader_.GetFileMetadata();
    EXPECT_FALSE(metadata.empty());

    // The test file has required metadata added via PrepareRequiredFileMetadata
    bool found_osi_trace = false;
    for (const auto& [name, kv_map] : metadata) {
        if (name == "net.asam.osi.trace") {
            found_osi_trace = true;
            EXPECT_FALSE(kv_map.empty());
        }
    }
    EXPECT_TRUE(found_osi_trace) << "Expected 'net.asam.osi.trace' metadata from PrepareRequiredFileMetadata";
}

TEST_F(McapTraceFileReaderTest, GetFileMetadataBeforeOpen) {
    auto metadata = reader_.GetFileMetadata();
    EXPECT_TRUE(metadata.empty());
}

TEST_F(McapTraceFileReaderTest, GetChannelMetadata) {
    ASSERT_TRUE(reader_.Open(test_file_));

    // The "gt" channel should have metadata (added by AddChannel with OSI descriptor)
    auto metadata = reader_.GetChannelMetadata("gt");
    ASSERT_TRUE(metadata.has_value());
}

TEST_F(McapTraceFileReaderTest, GetChannelMetadataNotFound) {
    ASSERT_TRUE(reader_.Open(test_file_));

    auto metadata = reader_.GetChannelMetadata("nonexistent_topic");
    EXPECT_FALSE(metadata.has_value());
}

TEST_F(McapTraceFileReaderTest, GetChannelMetadataBeforeOpen) {
    auto metadata = reader_.GetChannelMetadata("gt");
    EXPECT_FALSE(metadata.has_value());
}

TEST_F(McapTraceFileReaderTest, GetMessageTypeForTopic) {
    ASSERT_TRUE(reader_.Open(test_file_));

    auto gt_type = reader_.GetMessageTypeForTopic("gt");
    ASSERT_TRUE(gt_type.has_value());
    EXPECT_EQ(gt_type.value(), osi3::ReaderTopLevelMessage::kGroundTruth);

    auto sv_type = reader_.GetMessageTypeForTopic("sv");
    ASSERT_TRUE(sv_type.has_value());
    EXPECT_EQ(sv_type.value(), osi3::ReaderTopLevelMessage::kSensorView);
}

TEST_F(McapTraceFileReaderTest, GetMessageTypeForNonOSITopic) {
    ASSERT_TRUE(reader_.Open(test_file_));

    // JSON topic has a non-OSI schema, should return nullopt
    auto type = reader_.GetMessageTypeForTopic("json_topic");
    EXPECT_FALSE(type.has_value());
}

TEST_F(McapTraceFileReaderTest, GetMessageTypeForTopicNotFound) {
    ASSERT_TRUE(reader_.Open(test_file_));

    auto type = reader_.GetMessageTypeForTopic("nonexistent");
    EXPECT_FALSE(type.has_value());
}

TEST_F(McapTraceFileReaderTest, GetMessageTypeForTopicBeforeOpen) {
    auto type = reader_.GetMessageTypeForTopic("gt");
    EXPECT_FALSE(type.has_value());
}

TEST_F(McapTraceFileReaderTest, ReadStatusIsOkForValidMessages) {
    ASSERT_TRUE(reader_.Open(test_file_));
    reader_.SetSkipIncompatibleMessages(true);

    auto result = reader_.ReadMessage();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->status, osi3::ReadStatus::kOk);
    EXPECT_TRUE(result->error_message.empty());
    EXPECT_NE(result->message, nullptr);
}

TEST_F(McapTraceFileReaderTest, SetLogIncompatibleMessagesSuppressesLogging) {
    ASSERT_TRUE(reader_.Open(test_file_));
    reader_.SetSkipIncompatibleMessages(false);
    reader_.SetLogIncompatibleMessages(false);

    // Read two OSI messages
    reader_.ReadMessage();
    reader_.ReadMessage();

    // Third message (JSON) should return kIncompatible without logging
    auto result = reader_.ReadMessage();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->status, osi3::ReadStatus::kIncompatible);
    EXPECT_EQ(result->message, nullptr);
}

TEST_F(McapTraceFileReaderTest, SkipAndLogAreIndependent) {
    ASSERT_TRUE(reader_.Open(test_file_));

    // Skip enabled, log disabled — incompatible messages should be silently skipped
    reader_.SetSkipIncompatibleMessages(true);
    reader_.SetLogIncompatibleMessages(false);

    auto result1 = reader_.ReadMessage();
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1->status, osi3::ReadStatus::kOk);

    auto result2 = reader_.ReadMessage();
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2->status, osi3::ReadStatus::kOk);

    // JSON message skipped, should be EOF
    auto result3 = reader_.ReadMessage();
    EXPECT_FALSE(result3.has_value());
}

TEST_F(McapTraceFileReaderTest, IncompatibleResultHasChannelName) {
    ASSERT_TRUE(reader_.Open(test_file_));
    reader_.SetSkipIncompatibleMessages(false);
    reader_.SetLogIncompatibleMessages(false);

    // Skip two OSI messages
    reader_.ReadMessage();
    reader_.ReadMessage();

    auto result = reader_.ReadMessage();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->status, osi3::ReadStatus::kIncompatible);
    EXPECT_EQ(result->channel_name, "json_topic");
    EXPECT_FALSE(result->error_message.empty());
}

TEST_F(McapTraceFileReaderTest, DeserializationFailureReturnsKError) {
    // Create MCAP file with a valid OSI schema but corrupt protobuf payload
    const auto corrupt_file = osi3::testing::MakeTempPath("corrupt_proto", osi3::testing::FileExtensions::kMcap);
    {
        // Write a file with a valid OSI channel but corrupt protobuf data
        mcap::McapWriter raw_writer;
        mcap::McapWriterOptions opts("osi-utilities");
        auto out = std::ofstream(corrupt_file, std::ios::binary);
        raw_writer.open(out, opts);

        mcap::Schema osi_schema("osi3.GroundTruth", "protobuf", "");
        raw_writer.addSchema(osi_schema);

        mcap::Channel ch("gt", "protobuf", osi_schema.id);
        raw_writer.addChannel(ch);

        // Write garbage bytes as protobuf payload — will fail to deserialize
        std::string garbage = "THIS_IS_NOT_VALID_PROTOBUF";
        mcap::Message msg;
        msg.channelId = ch.id;
        msg.data = reinterpret_cast<const std::byte*>(garbage.data());
        msg.dataSize = garbage.size();
        msg.logTime = 0;
        msg.publishTime = 0;
        auto write_status = raw_writer.write(msg);
        ASSERT_EQ(write_status.code, mcap::StatusCode::Success);
        raw_writer.close();
    }

    osi3::MCAPTraceFileReader corrupt_reader;
    ASSERT_TRUE(corrupt_reader.Open(corrupt_file));
    ASSERT_TRUE(corrupt_reader.HasNext());

    auto result = corrupt_reader.ReadMessage();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->status, osi3::ReadStatus::kError);
    EXPECT_FALSE(result->error_message.empty());
    EXPECT_EQ(result->channel_name, "gt");
    EXPECT_EQ(result->message, nullptr);

    corrupt_reader.Close();
    std::filesystem::remove(corrupt_file);
}

// Verify the deprecated SetSkipNonOSIMsgs still works as an alias
TEST_F(McapTraceFileReaderTest, DeprecatedSetSkipNonOSIMsgsStillWorks) {
    ASSERT_TRUE(reader_.Open(test_file_));

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
    reader_.SetSkipNonOSIMsgs(true);
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif

    // Should behave like SetSkipIncompatibleMessages(true): skip JSON, get 2 OSI messages then EOF
    auto result1 = reader_.ReadMessage();
    ASSERT_TRUE(result1.has_value());
    EXPECT_EQ(result1->status, osi3::ReadStatus::kOk);

    auto result2 = reader_.ReadMessage();
    ASSERT_TRUE(result2.has_value());
    EXPECT_EQ(result2->status, osi3::ReadStatus::kOk);

    auto result3 = reader_.ReadMessage();
    EXPECT_FALSE(result3.has_value());
}

// Verify TraceFileFormat enum values are distinct and accessible
TEST(ReadStatusAndFormatEnumTest, TraceFileFormatValues) {
    EXPECT_NE(static_cast<uint8_t>(osi3::TraceFileFormat::kSingleChannel), static_cast<uint8_t>(osi3::TraceFileFormat::kMultiChannel));

    // Verify round-trip through integer
    auto single = osi3::TraceFileFormat::kSingleChannel;
    auto multi = osi3::TraceFileFormat::kMultiChannel;
    EXPECT_EQ(single, osi3::TraceFileFormat::kSingleChannel);
    EXPECT_EQ(multi, osi3::TraceFileFormat::kMultiChannel);
}

// Verify ReadStatus enum values
TEST(ReadStatusAndFormatEnumTest, ReadStatusValues) {
    EXPECT_NE(static_cast<uint8_t>(osi3::ReadStatus::kOk), static_cast<uint8_t>(osi3::ReadStatus::kIncompatible));
    EXPECT_NE(static_cast<uint8_t>(osi3::ReadStatus::kOk), static_cast<uint8_t>(osi3::ReadStatus::kError));
    EXPECT_NE(static_cast<uint8_t>(osi3::ReadStatus::kIncompatible), static_cast<uint8_t>(osi3::ReadStatus::kError));
}

// Verify type aliases resolve to the correct types
TEST(TypeAliasTest, MultiTraceFileReaderIsAlias) {
    static_assert(std::is_same_v<osi3::MultiTraceFileReader, osi3::MCAPTraceFileReader>, "MultiTraceFileReader must alias MCAPTraceFileReader");
}
