//
// Copyright (c) 2024, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/reader/TXTHTraceFileReader.h"

#include <gtest/gtest.h>

#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "osi_groundtruth.pb.h"
#include "osi_sensorview.pb.h"

class TxthTraceFileReaderTest : public ::testing::Test {
   protected:
    osi3::TXTHTraceFileReader reader_;
    std::filesystem::path test_file_gt_;
    std::filesystem::path test_file_sv_;

    void SetUp() override {
        test_file_gt_ = MakeTempPath("gt", "txth");
        test_file_sv_ = MakeTempPath("sv", "txth");
        CreateTestGroundTruthFile();
        CreateTestSensorViewFile();
    }

    void TearDown() override {
        reader_.Close();
        std::error_code ec;
        std::filesystem::remove(test_file_gt_, ec);
        std::filesystem::remove(test_file_sv_, ec);
    }

   private:
    static std::filesystem::path MakeTempPath(const std::string& prefix, const std::string& extension) {
        const auto* test_info = ::testing::UnitTest::GetInstance()->current_test_info();
        std::string name = std::string(test_info->test_suite_name()) + "_" + test_info->name();
        for (auto& ch : name) {
            if (!std::isalnum(static_cast<unsigned char>(ch))) {
                ch = '_';
            }
        }
        return std::filesystem::temp_directory_path() / (prefix + "_" + name + "." + extension);
    }

    void CreateTestGroundTruthFile() const {
        std::ofstream file(test_file_gt_);
        file << "version {\n";
        file << "  version_major: 3\n";
        file << "  version_minor: 7\n";
        file << "  version_patch: 0\n";
        file << "}\n";
        file << "timestamp {\n";
        file << "  seconds: 123\n";
        file << "  nanos: 456\n";
        file << "}\n";
        file << "version {\n";  // Second message delimiter
        file << "  version_major: 3\n";
        file << "  version_minor: 7\n";
        file << "  version_patch: 0\n";
        file << "}\n";
        file << "timestamp {\n";
        file << "  seconds: 789\n";
        file << "  nanos: 101112\n";
        file << "}\n";
    }

    void CreateTestSensorViewFile() const {
        std::ofstream file(test_file_sv_);
        file << "version {\n";
        file << "  version_major: 3\n";
        file << "  version_minor: 7\n";
        file << "  version_patch: 0\n";
        file << "}\n";
        file << "timestamp {\n";
        file << "  seconds: 789\n";
        file << "  nanos: 101\n";
        file << "}\n";
    }
};

TEST_F(TxthTraceFileReaderTest, OpenGroundTruthFile) { EXPECT_TRUE(reader_.Open(test_file_gt_)); }

TEST_F(TxthTraceFileReaderTest, OpenSensorViewFile) { EXPECT_TRUE(reader_.Open(test_file_sv_)); }

TEST_F(TxthTraceFileReaderTest, OpenWithExplicitMessageType) { EXPECT_TRUE(reader_.Open(test_file_gt_, osi3::ReaderTopLevelMessage::kGroundTruth)); }

TEST_F(TxthTraceFileReaderTest, ReadGroundTruthMessage) {
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

TEST_F(TxthTraceFileReaderTest, ReadSensorViewMessage) {
    ASSERT_TRUE(reader_.Open(test_file_sv_));
    EXPECT_TRUE(reader_.HasNext());

    const auto result = reader_.ReadMessage();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->message_type, osi3::ReaderTopLevelMessage::kSensorView);

    auto* sensor_view = dynamic_cast<osi3::SensorView*>(result->message.get());
    ASSERT_NE(sensor_view, nullptr);
    EXPECT_EQ(sensor_view->timestamp().seconds(), 789);
    EXPECT_EQ(sensor_view->timestamp().nanos(), 101);
}

TEST_F(TxthTraceFileReaderTest, PreventMultipleFileOpens) {
    // First open should succeed
    EXPECT_TRUE(reader_.Open(test_file_gt_));

    // Second open should fail while first file is still open
    EXPECT_FALSE(reader_.Open("testdata/another.txth"));

    // After closing, opening a new file should work
    reader_.Close();
    EXPECT_TRUE(reader_.Open(test_file_gt_));
}

TEST_F(TxthTraceFileReaderTest, HasNextReturnsFalseWhenEmpty) {
    ASSERT_TRUE(reader_.Open(test_file_gt_));
    ASSERT_TRUE(reader_.HasNext());
    reader_.ReadMessage();
    reader_.ReadMessage();
    EXPECT_FALSE(reader_.HasNext());
}

TEST_F(TxthTraceFileReaderTest, OpenNonexistentFile) { EXPECT_FALSE(reader_.Open("nonexistent_file.txth")); }

TEST_F(TxthTraceFileReaderTest, OpenInvalidFileExtension) {
    const auto invalid_file = MakeTempPath("invalid", "txt");
    {
        std::ofstream file(invalid_file);
        file << "Invalid data";
    }
    EXPECT_FALSE(reader_.Open(invalid_file));
    std::filesystem::remove(invalid_file);
}

TEST_F(TxthTraceFileReaderTest, OpenInvalidMessageType) {
    const auto invalid_file = MakeTempPath("invalid_type", "txth");
    {
        std::ofstream file(invalid_file);
        file << "InvalidType:\n";
        file << "some data\n";
    }
    EXPECT_FALSE(reader_.Open(invalid_file));
    std::filesystem::remove(invalid_file);
}

TEST_F(TxthTraceFileReaderTest, ReadEmptyFile) {
    const auto empty_file = MakeTempPath("empty", "txth");
    { std::ofstream file(empty_file); }
    ASSERT_TRUE(reader_.Open(empty_file, osi3::ReaderTopLevelMessage::kGroundTruth));
    EXPECT_FALSE(reader_.HasNext());
    reader_.Close();
    std::filesystem::remove(empty_file);
}

TEST_F(TxthTraceFileReaderTest, ReadInvalidMessageFormat) {
    const auto invalid_format_file = MakeTempPath("invalid_format_gt_99", "txth");
    {
        std::ofstream file(invalid_format_file);
        file << "GroundTruth:\n";
        file << "invalid protobuf format\n";
    }
    ASSERT_TRUE(reader_.Open(invalid_format_file));
    EXPECT_THROW(reader_.ReadMessage(), std::runtime_error);
    reader_.Close();
    std::filesystem::remove(invalid_format_file);
}
