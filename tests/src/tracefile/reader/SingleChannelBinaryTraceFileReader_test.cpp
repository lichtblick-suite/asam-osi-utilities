//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/reader/SingleChannelBinaryTraceFileReader.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <string>

#include "../../TestUtilities.h"
#include "osi_groundtruth.pb.h"
#include "osi_sensorview.pb.h"

class SingleChannelBinaryTraceFileReaderTest : public ::testing::Test {
   protected:
    osi3::SingleChannelBinaryTraceFileReader reader_;
    std::filesystem::path test_file_gt_;
    std::filesystem::path test_file_sv_;

    void SetUp() override {
        test_file_gt_ = osi3::testing::MakeTempPath("gt", osi3::testing::FileExtensions::kOsi);
        test_file_sv_ = osi3::testing::MakeTempPath("sv", osi3::testing::FileExtensions::kOsi);
        CreateTestGroundTruthFile();
        CreateTestSensorViewFile();
    }

    void TearDown() override {
        reader_.Close();
        osi3::testing::SafeRemoveTestFile(test_file_gt_);
        osi3::testing::SafeRemoveTestFile(test_file_sv_);
    }

   private:
    void CreateTestGroundTruthFile() const {
        std::ofstream file(test_file_gt_, std::ios::binary);
        osi3::GroundTruth ground_truth;
        ground_truth.mutable_timestamp()->set_seconds(123);
        ground_truth.mutable_timestamp()->set_nanos(456);

        std::string serialized = ground_truth.SerializeAsString();
        uint32_t size = serialized.size();

        file.write(reinterpret_cast<char*>(&size), sizeof(size));
        file.write(serialized.data(), size);
    }

    void CreateTestSensorViewFile() const {
        std::ofstream file(test_file_sv_, std::ios::binary);
        osi3::SensorView sensor_view;
        sensor_view.mutable_timestamp()->set_seconds(789);
        sensor_view.mutable_timestamp()->set_nanos(101);

        std::string serialized = sensor_view.SerializeAsString();
        uint32_t size = serialized.size();

        file.write(reinterpret_cast<char*>(&size), sizeof(size));
        file.write(serialized.data(), size);
    }
};

TEST_F(SingleChannelBinaryTraceFileReaderTest, OpenGroundTruthFile) { EXPECT_TRUE(reader_.Open(test_file_gt_)); }

TEST_F(SingleChannelBinaryTraceFileReaderTest, OpenSensorViewFile) { EXPECT_TRUE(reader_.Open(test_file_sv_)); }

TEST_F(SingleChannelBinaryTraceFileReaderTest, ReadGroundTruthMessage) {
    ASSERT_TRUE(reader_.Open(test_file_gt_));
    EXPECT_TRUE(reader_.HasNext());

    const auto result = reader_.ReadMessage();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->message_type, osi3::ReaderTopLevelMessage::kGroundTruth);

    auto* ground_truth = dynamic_cast<osi3::GroundTruth*>(result->message.get());
    ASSERT_NE(ground_truth, nullptr);
    EXPECT_EQ(ground_truth->timestamp().seconds(), 123);
    EXPECT_EQ(ground_truth->timestamp().nanos(), 456);
}

TEST_F(SingleChannelBinaryTraceFileReaderTest, ReadSensorViewMessage) {
    ASSERT_TRUE(reader_.Open(test_file_sv_));
    EXPECT_TRUE(reader_.HasNext());

    auto result = reader_.ReadMessage();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->message_type, osi3::ReaderTopLevelMessage::kSensorView);

    auto* sensor_view = dynamic_cast<osi3::SensorView*>(result->message.get());
    ASSERT_NE(sensor_view, nullptr);
    EXPECT_EQ(sensor_view->timestamp().seconds(), 789);
    EXPECT_EQ(sensor_view->timestamp().nanos(), 101);
}

TEST_F(SingleChannelBinaryTraceFileReaderTest, PreventMultipleFileOpens) {
    // First open should succeed
    EXPECT_TRUE(reader_.Open(test_file_gt_));

    // Second open should fail while first file is still open
    EXPECT_FALSE(reader_.Open("testdata/another.osi"));

    // After closing, opening a new file should work
    reader_.Close();
    EXPECT_TRUE(reader_.Open(test_file_gt_));
}

TEST_F(SingleChannelBinaryTraceFileReaderTest, HasNextReturnsFalseWhenEmpty) {
    ASSERT_TRUE(reader_.Open(test_file_gt_));
    ASSERT_TRUE(reader_.HasNext());
    reader_.ReadMessage();
    EXPECT_FALSE(reader_.HasNext());
}

TEST_F(SingleChannelBinaryTraceFileReaderTest, OpenNonexistentFile) { EXPECT_FALSE(reader_.Open("nonexistent_file.osi")); }

TEST_F(SingleChannelBinaryTraceFileReaderTest, OpenInvalidFileFormat) {
    const auto invalid_file = osi3::testing::MakeTempPath("invalid", "bin");
    {
        std::ofstream file(invalid_file, std::ios::binary);
        file << "Invalid data";
    }

    EXPECT_FALSE(reader_.Open(invalid_file));
    std::filesystem::remove(invalid_file);

    const auto invalid_file_osi = osi3::testing::MakeTempPath("invalid_filename", osi3::testing::FileExtensions::kOsi);
    {
        std::ofstream file(invalid_file_osi, std::ios::binary);
        file << "Invalid data";
    }
    EXPECT_FALSE(reader_.Open(invalid_file_osi));
    std::filesystem::remove(invalid_file_osi);
}

TEST_F(SingleChannelBinaryTraceFileReaderTest, OpenWithExplicitMessageType) {
    EXPECT_TRUE(reader_.Open(test_file_gt_, osi3::ReaderTopLevelMessage::kGroundTruth));
    reader_.Close();
    EXPECT_TRUE(reader_.Open(test_file_sv_, osi3::ReaderTopLevelMessage::kSensorView));
}

TEST_F(SingleChannelBinaryTraceFileReaderTest, ReadEmptyMessage) {
    const auto empty_file = osi3::testing::MakeTempPath("empty_sv_99", osi3::testing::FileExtensions::kOsi);
    {
        std::ofstream file(empty_file, std::ios::binary);
        uint32_t size = 0;
        file.write(reinterpret_cast<char*>(&size), sizeof(size));
    }

    ASSERT_TRUE(reader_.Open(empty_file));
    EXPECT_THROW(reader_.ReadMessage(), std::runtime_error);
    reader_.Close();
    std::filesystem::remove(empty_file);
}

TEST_F(SingleChannelBinaryTraceFileReaderTest, ReadCorruptedMessageSize) {
    const auto corrupted_file = osi3::testing::MakeTempPath("corrupted_size_sv_99", osi3::testing::FileExtensions::kOsi);
    {
        std::ofstream file(corrupted_file, std::ios::binary);
        uint32_t invalid_size = 0xFFFFFFFF;
        file.write(reinterpret_cast<char*>(&invalid_size), sizeof(invalid_size));
    }

    ASSERT_TRUE(reader_.Open(corrupted_file));
    EXPECT_THROW(reader_.ReadMessage(), std::runtime_error);
    reader_.Close();
    std::filesystem::remove(corrupted_file);
}

TEST_F(SingleChannelBinaryTraceFileReaderTest, ReadCorruptedMessageContent) {
    const auto corrupted_file = osi3::testing::MakeTempPath("corrupted_content_sv_99", osi3::testing::FileExtensions::kOsi);
    {
        std::ofstream file(corrupted_file, std::ios::binary);
        uint32_t size = 100;
        file.write(reinterpret_cast<char*>(&size), sizeof(size));
        // Write fewer data than specified in size
        std::string incomplete_data = "incomplete";
        file.write(incomplete_data.c_str(), static_cast<std::streamsize>(incomplete_data.size()));
    }

    ASSERT_TRUE(reader_.Open(corrupted_file));
    EXPECT_THROW(reader_.ReadMessage(), std::runtime_error);
    reader_.Close();
    std::filesystem::remove(corrupted_file);
}

TEST_F(SingleChannelBinaryTraceFileReaderTest, ReadMessageAfterClose) {
    ASSERT_TRUE(reader_.Open(test_file_gt_));
    reader_.Close();
    EXPECT_FALSE(reader_.HasNext());
    auto result = reader_.ReadMessage();
    EXPECT_FALSE(result.has_value());
}
