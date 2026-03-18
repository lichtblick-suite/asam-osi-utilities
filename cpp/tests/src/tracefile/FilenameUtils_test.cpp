//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/FilenameUtils.h"

#include <gtest/gtest.h>

TEST(FilenameUtilsTest, InferMessageTypeFromStrictPattern) {
    EXPECT_EQ(osi3::tracefile::InferMessageTypeFromFilename("20240101T120000Z_gt_3.7.0_4.25.0_10_PID_demo.osi"), osi3::ReaderTopLevelMessage::kGroundTruth);
}

TEST(FilenameUtilsTest, InferMessageTypeFromTypeSuffix) { EXPECT_EQ(osi3::tracefile::InferMessageTypeFromFilename("trace_sv.mcap"), osi3::ReaderTopLevelMessage::kSensorView); }

TEST(FilenameUtilsTest, InferMessageTypeFromFilenameIsCaseInsensitive) {
    EXPECT_EQ(osi3::tracefile::InferMessageTypeFromFilename("TRACE_GT.OSI"), osi3::ReaderTopLevelMessage::kGroundTruth);
}

TEST(FilenameUtilsTest, ParseOsiTraceFilenameExtractsComponents) {
    const auto parsed = osi3::tracefile::ParseOsiTraceFilename("20240101T120000Z_gt_3.7.0_4.25.0_10_PID42_demo_run.osi");

    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(parsed->timestamp, "20240101T120000Z");
    EXPECT_EQ(parsed->message_type, "gt");
    EXPECT_EQ(parsed->osi_version, "3.7.0");
    EXPECT_EQ(parsed->protobuf_version, "4.25.0");
    EXPECT_EQ(parsed->frame_count, "10");
    EXPECT_EQ(parsed->identifier, "PID42");
    EXPECT_EQ(parsed->description, "demo_run");
}

TEST(FilenameUtilsTest, ParseOsiTraceFilenameRejectsInvalidFormat) {
    EXPECT_FALSE(osi3::tracefile::ParseOsiTraceFilename("invalid.osi").has_value());
    EXPECT_FALSE(osi3::tracefile::ParseOsiTraceFilename("20240101T250000Z_gt_3.7.0_4.25.0_10_PID.osi").has_value());
}
