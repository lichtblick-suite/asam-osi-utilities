//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/TimestampUtils.h"

#include <gtest/gtest.h>

#include <limits>

#include "osi_groundtruth.pb.h"

namespace {

auto MakeMessage(int64_t seconds, uint32_t nanos) -> osi3::GroundTruth {
    osi3::GroundTruth gt;
    gt.mutable_timestamp()->set_seconds(seconds);
    gt.mutable_timestamp()->set_nanos(nanos);
    return gt;
}

TEST(TimestampUtilsTest, TimestampToNanosecondsBasic) {
    auto msg = MakeMessage(10, 500);
    EXPECT_EQ(osi3::tracefile::TimestampToNanoseconds(msg), 10'000'000'500ULL);
}

TEST(TimestampUtilsTest, TimestampToNanosecondsZero) {
    auto msg = MakeMessage(0, 0);
    EXPECT_EQ(osi3::tracefile::TimestampToNanoseconds(msg), 0ULL);
}

TEST(TimestampUtilsTest, TimestampToNanosecondsNanosOnly) {
    auto msg = MakeMessage(0, 123'456'789);
    EXPECT_EQ(osi3::tracefile::TimestampToNanoseconds(msg), 123'456'789ULL);
}

TEST(TimestampUtilsTest, TimestampToNanosecondsSecondsOnly) {
    auto msg = MakeMessage(42, 0);
    EXPECT_EQ(osi3::tracefile::TimestampToNanoseconds(msg), 42'000'000'000ULL);
}

TEST(TimestampUtilsTest, TimestampToSecondsBasic) {
    auto msg = MakeMessage(1, 500'000'000);
    EXPECT_DOUBLE_EQ(osi3::tracefile::TimestampToSeconds(msg), 1.5);
}

TEST(TimestampUtilsTest, TimestampToSecondsZero) {
    auto msg = MakeMessage(0, 0);
    EXPECT_DOUBLE_EQ(osi3::tracefile::TimestampToSeconds(msg), 0.0);
}

TEST(TimestampUtilsTest, TimestampToNanosecondsRejectsNegativeSeconds) {
    auto msg = MakeMessage(-1, 0);
    EXPECT_THROW(osi3::tracefile::TimestampToNanoseconds(msg), std::out_of_range);
}

TEST(TimestampUtilsTest, NanosecondsToSecondsBasic) { EXPECT_DOUBLE_EQ(osi3::tracefile::NanosecondsToSeconds(1'500'000'000ULL), 1.5); }

TEST(TimestampUtilsTest, NanosecondsToSecondsZero) { EXPECT_DOUBLE_EQ(osi3::tracefile::NanosecondsToSeconds(0ULL), 0.0); }

TEST(TimestampUtilsTest, SecondsToNanosecondsBasic) { EXPECT_EQ(osi3::tracefile::SecondsToNanoseconds(1.5), 1'500'000'000ULL); }

TEST(TimestampUtilsTest, SecondsToNanosecondsZero) { EXPECT_EQ(osi3::tracefile::SecondsToNanoseconds(0.0), 0ULL); }

TEST(TimestampUtilsTest, SecondsToNanosecondsRejectsNegativeSeconds) { EXPECT_THROW(osi3::tracefile::SecondsToNanoseconds(-1.5), std::out_of_range); }

TEST(TimestampUtilsTest, SecondsToNanosecondsRejectsOverflow) {
    constexpr auto kOverflowSeconds = static_cast<double>(std::numeric_limits<uint64_t>::max()) / static_cast<double>(osi3::tracefile::config::kNanosecondsPerSecond) + 1.0;
    EXPECT_THROW(osi3::tracefile::SecondsToNanoseconds(kOverflowSeconds), std::out_of_range);
}

TEST(TimestampUtilsTest, Roundtrip) {
    auto msg = MakeMessage(3, 141'592'653);
    auto ns = osi3::tracefile::TimestampToNanoseconds(msg);
    auto seconds = osi3::tracefile::NanosecondsToSeconds(ns);
    EXPECT_NEAR(seconds, 3.141592653, 1e-9);
}

}  // namespace
