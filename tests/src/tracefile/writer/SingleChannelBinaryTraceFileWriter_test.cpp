//
// Copyright (c) 2024, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/writer/SingleChannelBinaryTraceFileWriter.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>

#include "../../TestUtilities.h"
#include "osi_groundtruth.pb.h"
#include "osi_sensorview.pb.h"

class SingleChannelBinaryTraceFileWriterTest : public ::testing::Test {
   protected:
    osi3::SingleChannelBinaryTraceFileWriter writer_;
    std::filesystem::path test_file_gt_;
    std::filesystem::path test_file_sv_;

    void SetUp() override {
        test_file_gt_ = osi3::testing::MakeTempPath("scb_gt", osi3::testing::FileExtensions::kOsi);
        test_file_sv_ = osi3::testing::MakeTempPath("scb_sv", osi3::testing::FileExtensions::kOsi);
    }

    void TearDown() override {
        writer_.Close();
        osi3::testing::SafeRemoveTestFile(test_file_gt_);
        osi3::testing::SafeRemoveTestFile(test_file_sv_);
    }
};

TEST_F(SingleChannelBinaryTraceFileWriterTest, OpenFile) { EXPECT_TRUE(writer_.Open(test_file_gt_)); }

TEST_F(SingleChannelBinaryTraceFileWriterTest, OpenInvalidExtension) { EXPECT_FALSE(writer_.Open("test.txt")); }

TEST_F(SingleChannelBinaryTraceFileWriterTest, WriteGroundTruthMessage) {
    ASSERT_TRUE(writer_.Open(test_file_gt_));

    osi3::GroundTruth ground_truth;
    ground_truth.mutable_timestamp()->set_seconds(123);
    ground_truth.mutable_timestamp()->set_nanos(456);

    EXPECT_TRUE(writer_.WriteMessage(ground_truth));
    writer_.Close();

    // Verify written content
    std::ifstream file(test_file_gt_, std::ios::binary);
    uint32_t size = 0;
    file.read(reinterpret_cast<char*>(&size), sizeof(size));
    std::vector<char> buffer(size);
    file.read(buffer.data(), size);

    osi3::GroundTruth read_gt;
    EXPECT_TRUE(read_gt.ParseFromArray(buffer.data(), size));
    EXPECT_EQ(read_gt.timestamp().seconds(), 123);
    EXPECT_EQ(read_gt.timestamp().nanos(), 456);
}

TEST_F(SingleChannelBinaryTraceFileWriterTest, WriteSensorViewMessage) {
    ASSERT_TRUE(writer_.Open(test_file_sv_));

    osi3::SensorView sensor_view;
    sensor_view.mutable_timestamp()->set_seconds(789);
    sensor_view.mutable_timestamp()->set_nanos(101);

    EXPECT_TRUE(writer_.WriteMessage(sensor_view));
    writer_.Close();

    // Verify written content
    std::ifstream file(test_file_sv_, std::ios::binary);
    uint32_t size = 0;
    file.read(reinterpret_cast<char*>(&size), sizeof(size));
    std::vector<char> buffer(size);
    file.read(buffer.data(), size);

    osi3::SensorView read_sv;
    EXPECT_TRUE(read_sv.ParseFromArray(buffer.data(), size));
    EXPECT_EQ(read_sv.timestamp().seconds(), 789);
    EXPECT_EQ(read_sv.timestamp().nanos(), 101);
}

TEST_F(SingleChannelBinaryTraceFileWriterTest, WriteMultipleMessages) {
    ASSERT_TRUE(writer_.Open(test_file_gt_));

    osi3::GroundTruth gt1;
    osi3::GroundTruth gt2;
    gt1.mutable_timestamp()->set_seconds(111);
    gt2.mutable_timestamp()->set_seconds(222);

    EXPECT_TRUE(writer_.WriteMessage(gt1));
    EXPECT_TRUE(writer_.WriteMessage(gt2));
}

TEST_F(SingleChannelBinaryTraceFileWriterTest, WriteToClosedFile) {
    writer_.Close();
    osi3::GroundTruth ground_truth;
    EXPECT_FALSE(writer_.WriteMessage(ground_truth));
}

TEST_F(SingleChannelBinaryTraceFileWriterTest, ReopenFile) {
    ASSERT_TRUE(writer_.Open(test_file_gt_));
    writer_.Close();
    EXPECT_TRUE(writer_.Open(test_file_gt_));
}

TEST_F(SingleChannelBinaryTraceFileWriterTest, WriteEmptyMessage) {
    ASSERT_TRUE(writer_.Open(test_file_gt_));
    osi3::GroundTruth empty_gt;
    EXPECT_TRUE(writer_.WriteMessage(empty_gt));
}
