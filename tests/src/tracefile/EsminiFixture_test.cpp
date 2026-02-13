//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

#include "../TestUtilities.h"
#include "osi-utilities/tracefile/Reader.h"
#include "osi-utilities/tracefile/reader/MCAPTraceFileReader.h"
#include "osi-utilities/tracefile/reader/SingleChannelBinaryTraceFileReader.h"
#include "osi_groundtruth.pb.h"
#include "osi_sensorview.pb.h"

#ifndef TEST_DATA_DIR
#define TEST_DATA_DIR ""
#endif

namespace {
const std::filesystem::path kTestDataDir = TEST_DATA_DIR;

// Legacy esmini fixtures
const std::filesystem::path kEsminiBinaryFixture = kTestDataDir / "5frames_gt_esmini.osi";
const std::filesystem::path kEsminiMcapFixture = kTestDataDir / "5frames_gt_esmini.mcap";

// NCAP GroundTruth fixtures
const std::filesystem::path kCcrsGt = kTestDataDir / "ccrs_gt_ncap.osi";
const std::filesystem::path kCcftapGt = kTestDataDir / "ccftap_gt_ncap.osi";
const std::filesystem::path kCpnaGt = kTestDataDir / "cpna_gt_ncap.osi";
const std::filesystem::path kCblaGt = kTestDataDir / "cbla_gt_ncap.osi";

// NCAP SensorView fixtures
const std::filesystem::path kCcrsSv = kTestDataDir / "ccrs_sv_ncap.osi";
const std::filesystem::path kCcftapSv = kTestDataDir / "ccftap_sv_ncap.osi";
const std::filesystem::path kCpnaSv = kTestDataDir / "cpna_sv_ncap.osi";
const std::filesystem::path kCblaSv = kTestDataDir / "cbla_sv_ncap.osi";

// NCAP MCAP fixture
const std::filesystem::path kCcrsGtMcap = kTestDataDir / "ccrs_gt_ncap.mcap";
}  // namespace

#define SKIP_IF_FIXTURE_MISSING(path)                                                                                     \
    do {                                                                                                                  \
        if (!std::filesystem::exists(path)) {                                                                             \
            GTEST_SKIP() << "Fixture file not found: " << (path) << ". Run scripts/generate_test_traces.sh to generate."; \
        }                                                                                                                 \
    } while (0)

// ============================================================================
// Legacy esmini fixture tests (unchanged)
// ============================================================================
class EsminiFixtureTest : public ::testing::Test {};

TEST_F(EsminiFixtureTest, ReadBinaryEsminiTrace) {
    SKIP_IF_FIXTURE_MISSING(kEsminiBinaryFixture);

    osi3::SingleChannelBinaryTraceFileReader reader;
    ASSERT_TRUE(reader.Open(kEsminiBinaryFixture));

    int count = 0;
    while (reader.HasNext()) {
        const auto result = reader.ReadMessage();
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->message_type, osi3::ReaderTopLevelMessage::kGroundTruth);
        ++count;
    }
    EXPECT_EQ(count, 5);
    reader.Close();
}

TEST_F(EsminiFixtureTest, ReadMcapEsminiTrace) {
    SKIP_IF_FIXTURE_MISSING(kEsminiMcapFixture);

    osi3::MCAPTraceFileReader reader;
    ASSERT_TRUE(reader.Open(kEsminiMcapFixture));

    int count = 0;
    while (reader.HasNext()) {
        const auto result = reader.ReadMessage();
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->message_type, osi3::ReaderTopLevelMessage::kGroundTruth);
        ++count;
    }
    EXPECT_EQ(count, 5);
    reader.Close();
}

TEST_F(EsminiFixtureTest, EsminiTraceHasMovingObjects) {
    SKIP_IF_FIXTURE_MISSING(kEsminiBinaryFixture);

    osi3::SingleChannelBinaryTraceFileReader reader;
    ASSERT_TRUE(reader.Open(kEsminiBinaryFixture));
    ASSERT_TRUE(reader.HasNext());

    const auto result = reader.ReadMessage();
    ASSERT_TRUE(result.has_value());

    auto* gt = dynamic_cast<osi3::GroundTruth*>(result->message.get());
    ASSERT_NE(gt, nullptr);
    EXPECT_GT(gt->moving_object_size(), 0);
    reader.Close();
}

TEST_F(EsminiFixtureTest, EsminiTraceTimestampsIncreasing) {
    SKIP_IF_FIXTURE_MISSING(kEsminiBinaryFixture);

    osi3::SingleChannelBinaryTraceFileReader reader;
    ASSERT_TRUE(reader.Open(kEsminiBinaryFixture));

    int64_t prev_nanos = -1;
    while (reader.HasNext()) {
        const auto result = reader.ReadMessage();
        ASSERT_TRUE(result.has_value());

        auto* gt = dynamic_cast<osi3::GroundTruth*>(result->message.get());
        ASSERT_NE(gt, nullptr);

        const int64_t current_nanos = gt->timestamp().seconds() * 1'000'000'000LL + gt->timestamp().nanos();
        if (prev_nanos >= 0) {
            EXPECT_GT(current_nanos, prev_nanos);
        }
        prev_nanos = current_nanos;
    }
    reader.Close();
}

TEST_F(EsminiFixtureTest, FactoryReadsEsminiOsi) {
    SKIP_IF_FIXTURE_MISSING(kEsminiBinaryFixture);

    auto reader = osi3::TraceFileReaderFactory::createReader(kEsminiBinaryFixture);
    ASSERT_NE(reader, nullptr);

    auto* binary_reader = dynamic_cast<osi3::SingleChannelBinaryTraceFileReader*>(reader.get());
    ASSERT_NE(binary_reader, nullptr);
    ASSERT_TRUE(binary_reader->Open(kEsminiBinaryFixture));
    ASSERT_TRUE(binary_reader->HasNext());

    const auto result = binary_reader->ReadMessage();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->message_type, osi3::ReaderTopLevelMessage::kGroundTruth);
    binary_reader->Close();
}

TEST_F(EsminiFixtureTest, FactoryReadsEsminiMcap) {
    SKIP_IF_FIXTURE_MISSING(kEsminiMcapFixture);

    auto reader = osi3::TraceFileReaderFactory::createReader(kEsminiMcapFixture);
    ASSERT_NE(reader, nullptr);
    ASSERT_TRUE(reader->Open(kEsminiMcapFixture));
    ASSERT_TRUE(reader->HasNext());

    const auto result = reader->ReadMessage();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->message_type, osi3::ReaderTopLevelMessage::kGroundTruth);
    reader->Close();
}

// ============================================================================
// NCAP GroundTruth tests (parameterized over 4 GT fixtures)
// ============================================================================
class NcapGroundTruthTest : public ::testing::TestWithParam<std::filesystem::path> {};

TEST_P(NcapGroundTruthTest, ReadAllFrames) {
    SKIP_IF_FIXTURE_MISSING(GetParam());

    osi3::SingleChannelBinaryTraceFileReader reader;
    ASSERT_TRUE(reader.Open(GetParam()));

    int count = 0;
    while (reader.HasNext()) {
        const auto result = reader.ReadMessage();
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->message_type, osi3::ReaderTopLevelMessage::kGroundTruth);
        ++count;
    }
    EXPECT_GT(count, 0);
    reader.Close();
}

TEST_P(NcapGroundTruthTest, HasMovingObjects) {
    SKIP_IF_FIXTURE_MISSING(GetParam());

    osi3::SingleChannelBinaryTraceFileReader reader;
    ASSERT_TRUE(reader.Open(GetParam()));
    ASSERT_TRUE(reader.HasNext());

    const auto result = reader.ReadMessage();
    ASSERT_TRUE(result.has_value());

    auto* gt = dynamic_cast<osi3::GroundTruth*>(result->message.get());
    ASSERT_NE(gt, nullptr);
    EXPECT_GT(gt->moving_object_size(), 0);
    reader.Close();
}

TEST_P(NcapGroundTruthTest, TimestampsIncreasing) {
    SKIP_IF_FIXTURE_MISSING(GetParam());

    osi3::SingleChannelBinaryTraceFileReader reader;
    ASSERT_TRUE(reader.Open(GetParam()));

    int64_t prev_nanos = -1;
    while (reader.HasNext()) {
        const auto result = reader.ReadMessage();
        ASSERT_TRUE(result.has_value());

        auto* gt = dynamic_cast<osi3::GroundTruth*>(result->message.get());
        ASSERT_NE(gt, nullptr);

        const int64_t current_nanos = gt->timestamp().seconds() * 1'000'000'000LL + gt->timestamp().nanos();
        if (prev_nanos >= 0) {
            EXPECT_GT(current_nanos, prev_nanos);
        }
        prev_nanos = current_nanos;
    }
    reader.Close();
}

INSTANTIATE_TEST_SUITE_P(NcapGt, NcapGroundTruthTest, ::testing::Values(kCcrsGt, kCcftapGt, kCpnaGt, kCblaGt),
                         [](const ::testing::TestParamInfo<std::filesystem::path>& info) { return info.param.stem().string(); });

// ============================================================================
// NCAP SensorView tests (parameterized over 4 SV fixtures)
// ============================================================================
class NcapSensorViewTest : public ::testing::TestWithParam<std::filesystem::path> {};

TEST_P(NcapSensorViewTest, ReadAllFrames) {
    SKIP_IF_FIXTURE_MISSING(GetParam());

    osi3::SingleChannelBinaryTraceFileReader reader;
    ASSERT_TRUE(reader.Open(GetParam()));

    int count = 0;
    while (reader.HasNext()) {
        const auto result = reader.ReadMessage();
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->message_type, osi3::ReaderTopLevelMessage::kSensorView);
        ++count;
    }
    EXPECT_GT(count, 0);
    reader.Close();
}

TEST_P(NcapSensorViewTest, ContainsGroundTruth) {
    SKIP_IF_FIXTURE_MISSING(GetParam());

    osi3::SingleChannelBinaryTraceFileReader reader;
    ASSERT_TRUE(reader.Open(GetParam()));

    while (reader.HasNext()) {
        const auto result = reader.ReadMessage();
        ASSERT_TRUE(result.has_value());

        auto* sv = dynamic_cast<osi3::SensorView*>(result->message.get());
        ASSERT_NE(sv, nullptr);
        EXPECT_TRUE(sv->has_global_ground_truth()) << "SensorView frame missing global_ground_truth";
    }
    reader.Close();
}

TEST_P(NcapSensorViewTest, TimestampMatchesGT) {
    SKIP_IF_FIXTURE_MISSING(GetParam());

    osi3::SingleChannelBinaryTraceFileReader reader;
    ASSERT_TRUE(reader.Open(GetParam()));

    while (reader.HasNext()) {
        const auto result = reader.ReadMessage();
        ASSERT_TRUE(result.has_value());

        auto* sv = dynamic_cast<osi3::SensorView*>(result->message.get());
        ASSERT_NE(sv, nullptr);
        ASSERT_TRUE(sv->has_global_ground_truth());
        ASSERT_TRUE(sv->has_timestamp());

        const auto& sv_ts = sv->timestamp();
        const auto& gt_ts = sv->global_ground_truth().timestamp();
        EXPECT_EQ(sv_ts.seconds(), gt_ts.seconds());
        EXPECT_EQ(sv_ts.nanos(), gt_ts.nanos());
    }
    reader.Close();
}

INSTANTIATE_TEST_SUITE_P(NcapSv, NcapSensorViewTest, ::testing::Values(kCcrsSv, kCcftapSv, kCpnaSv, kCblaSv),
                         [](const ::testing::TestParamInfo<std::filesystem::path>& info) { return info.param.stem().string(); });

// ============================================================================
// NCAP MCAP test
// ============================================================================
class NcapMcapTest : public ::testing::Test {};

TEST_F(NcapMcapTest, ReadNcapMcap) {
    SKIP_IF_FIXTURE_MISSING(kCcrsGtMcap);

    osi3::MCAPTraceFileReader reader;
    ASSERT_TRUE(reader.Open(kCcrsGtMcap));

    int count = 0;
    while (reader.HasNext()) {
        const auto result = reader.ReadMessage();
        ASSERT_TRUE(result.has_value());
        EXPECT_EQ(result->message_type, osi3::ReaderTopLevelMessage::kGroundTruth);
        ++count;
    }
    EXPECT_GT(count, 0);
    reader.Close();
}

TEST_F(NcapMcapTest, FactoryReadsNcapMcap) {
    SKIP_IF_FIXTURE_MISSING(kCcrsGtMcap);

    auto reader = osi3::TraceFileReaderFactory::createReader(kCcrsGtMcap);
    ASSERT_NE(reader, nullptr);

    auto* mcap_reader = dynamic_cast<osi3::MCAPTraceFileReader*>(reader.get());
    ASSERT_NE(mcap_reader, nullptr);
    ASSERT_TRUE(mcap_reader->Open(kCcrsGtMcap));
    ASSERT_TRUE(mcap_reader->HasNext());

    const auto result = mcap_reader->ReadMessage();
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(result->message_type, osi3::ReaderTopLevelMessage::kGroundTruth);
    mcap_reader->Close();
}

// ============================================================================
// NCAP variety test — confirms distinct traces have different frame counts
// ============================================================================
class NcapVarietyTest : public ::testing::Test {};

TEST_F(NcapVarietyTest, DifferentFrameCounts) {
    const std::vector<std::filesystem::path> fixtures = {kCcrsGt, kCcftapGt, kCpnaGt, kCblaGt};

    // Skip if any fixture is missing
    for (const auto& path : fixtures) {
        SKIP_IF_FIXTURE_MISSING(path);
    }

    std::vector<int> counts;
    for (const auto& path : fixtures) {
        osi3::SingleChannelBinaryTraceFileReader reader;
        ASSERT_TRUE(reader.Open(path)) << "Failed to open: " << path;

        int count = 0;
        while (reader.HasNext()) {
            const auto result = reader.ReadMessage();
            ASSERT_TRUE(result.has_value());
            ++count;
        }
        counts.push_back(count);
        reader.Close();
    }

    // At least two scenarios should have different frame counts
    bool found_different = false;
    for (size_t i = 1; i < counts.size(); ++i) {
        if (counts[i] != counts[0]) {
            found_different = true;
            break;
        }
    }
    EXPECT_TRUE(found_different) << "All 4 NCAP scenarios have identical frame counts (" << counts[0] << "), which suggests they may not be distinct traces.";
}
