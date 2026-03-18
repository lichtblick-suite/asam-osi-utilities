//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//
/**
 * \file
 * \brief Demonstrate multi-channel MCAP writing with OSI and non-OSI topics.
 *
 * This example shows two approaches for writing multi-channel MCAP files:
 *
 * **Part 1 — MCAPTraceFileWriter (pure OSI, multi-topic)**
 * Uses the high-level writer to register multiple OSI channels (including
 * two channels sharing the same schema) and write messages in a simulation loop.
 *
 * **Part 2 — MCAPTraceFileChannel (mixed OSI / non-OSI)**
 * Uses an externally-managed mcap::McapWriter together with the
 * MCAPTraceFileChannel helper to mix OSI and non-OSI channels in a single
 * MCAP file, then reads it back with non-OSI message filtering.
 */

#include <osi-utilities/tracefile/TimestampUtils.h>
#include <osi-utilities/tracefile/reader/MCAPTraceFileReader.h>
#include <osi-utilities/tracefile/writer/MCAPTraceFileChannel.h>
#include <osi-utilities/tracefile/writer/MCAPTraceFileWriter.h>

#include <exception>
#include <filesystem>
#include <fstream>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include "osi_groundtruth.pb.h"
#include "osi_sensorview.pb.h"
#include "osi_version.pb.h"

namespace {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/**
 * \brief Generate a temporary output path for the multi-channel example.
 * \param suffix Disambiguating suffix (e.g. "part1", "part2").
 * \return Temporary file path.
 */
auto GenerateTempFilePath(const std::string& suffix) -> std::filesystem::path {
#ifdef _WIN32
    const auto pid = std::to_string(_getpid());
#else
    const auto pid = std::to_string(getpid());
#endif
    auto output_dir = std::filesystem::current_path() / ".playground";
    std::filesystem::create_directories(output_dir);
    return output_dir / ("multi_channel_example_" + suffix + "_" + pid + ".mcap");
}

/**
 * \brief Populate a GroundTruth message with basic test data.
 * \param ground_truth Message to populate.
 * \param osi_version OSI interface version to embed.
 */
void PopulateGroundTruth(osi3::GroundTruth& ground_truth, const osi3::InterfaceVersion& osi_version) {
    ground_truth.mutable_version()->CopyFrom(osi_version);

    auto* const host_vehicle = ground_truth.mutable_moving_object()->Add();
    host_vehicle->mutable_id()->set_value(1);
    host_vehicle->mutable_vehicle_classification()->set_type(osi3::MovingObject_VehicleClassification_Type_TYPE_SMALL_CAR);
    host_vehicle->mutable_base()->mutable_dimension()->set_length(4.5);
    host_vehicle->mutable_base()->mutable_dimension()->set_width(1.8);
    host_vehicle->mutable_base()->mutable_dimension()->set_height(1.4);
    host_vehicle->mutable_base()->mutable_velocity()->set_x(15.0);
}

/**
 * \brief Populate a SensorView message with basic test data.
 * \param sensor_view Message to populate.
 * \param sensor_id Sensor identifier value.
 * \param osi_version OSI interface version to embed.
 */
void PopulateSensorView(osi3::SensorView& sensor_view, const uint64_t sensor_id, const osi3::InterfaceVersion& osi_version) {
    sensor_view.mutable_version()->CopyFrom(osi_version);
    sensor_view.mutable_sensor_id()->set_value(sensor_id);
    sensor_view.mutable_host_vehicle_id()->set_value(1);

    auto* const ground_truth = sensor_view.mutable_global_ground_truth();
    ground_truth->mutable_version()->CopyFrom(osi_version);
}

/**
 * \brief Advance timestamps on a GroundTruth message by a fixed step.
 * \param ground_truth Message whose timestamp to advance.
 * \param step_ns Time step in nanoseconds.
 */
void AdvanceTimestamp(osi3::GroundTruth& ground_truth, const uint64_t step_ns) {
    constexpr auto kNsPerSec = osi3::tracefile::config::kNanosecondsPerSecond;
    auto time_ns = osi3::tracefile::TimestampToNanoseconds(ground_truth) + step_ns;
    ground_truth.mutable_timestamp()->set_seconds(static_cast<int64_t>(time_ns / kNsPerSec));
    ground_truth.mutable_timestamp()->set_nanos(time_ns % kNsPerSec);
}

/**
 * \brief Advance timestamps on a SensorView (and its embedded ground truth).
 * \param sensor_view Message whose timestamp to advance.
 * \param step_ns Time step in nanoseconds.
 */
void AdvanceTimestamp(osi3::SensorView& sensor_view, const uint64_t step_ns) {
    constexpr auto kNsPerSec = osi3::tracefile::config::kNanosecondsPerSecond;
    auto time_ns = osi3::tracefile::TimestampToNanoseconds(sensor_view) + step_ns;
    sensor_view.mutable_timestamp()->set_seconds(static_cast<int64_t>(time_ns / kNsPerSec));
    sensor_view.mutable_timestamp()->set_nanos(time_ns % kNsPerSec);

    // keep embedded ground truth timestamp in sync
    sensor_view.mutable_global_ground_truth()->mutable_timestamp()->set_seconds(static_cast<int64_t>(time_ns / kNsPerSec));
    sensor_view.mutable_global_ground_truth()->mutable_timestamp()->set_nanos(time_ns % kNsPerSec);
}

/**
 * \brief Read an MCAP file back and print a per-channel message count summary.
 * \param path Path to the MCAP file to read.
 * \param skip_non_osi If true, non-OSI messages are silently skipped.
 */
void ReadBackAndPrintSummary(const std::filesystem::path& path, const bool skip_non_osi) {
    osi3::MCAPTraceFileReader reader;
    reader.Open(path);
    reader.SetSkipNonOSIMsgs(skip_non_osi);

    std::map<std::string, int> channel_counts;
    while (reader.HasNext()) {
        const auto result = reader.ReadMessage();
        if (!result) {
            continue;
        }
        channel_counts[result->channel_name]++;
    }
    reader.Close();

    std::cout << "  Read-back summary (" << path.filename().string() << "):\n";
    for (const auto& [channel, count] : channel_counts) {
        std::cout << "    channel \"" << channel << "\": " << count << " messages\n";
    }
}

// ===========================================================================
// Part 1 — Multi-topic writing with MCAPTraceFileWriter
// ===========================================================================

/**
 * \brief Demonstrate pure-OSI multi-channel writing via MCAPTraceFileWriter.
 *
 * Registers three channels (GroundTruth + two SensorView topics that share
 * the same protobuf schema) and writes messages in a simulated time loop.
 */
void Part1_MultiTopicWriter() {
    std::cout << "\n=== Part 1: Multi-topic writing with MCAPTraceFileWriter ===\n";

    const auto path = GenerateTempFilePath("part1");
    std::cout << "  Output: " << path.string() << "\n";

    // --- writer setup -------------------------------------------------------
    osi3::MCAPTraceFileWriter writer;

    // Best practice: choose compression and chunk size explicitly.
    //  - Zstd offers the best compression ratio for archival / post-processing.
    //  - Lz4 is faster when CPU budget is tight (real-time logging).
    //  - A 16 MiB chunk size (library default) balances compression ratio,
    //    playback smoothness in Lichtblick, and crash-safety granularity.
    mcap::McapWriterOptions options("protobuf");
    options.compression = mcap::Compression::Zstd;
    options.chunkSize = osi3::tracefile::config::kDefaultChunkSize;

    if (!writer.Open(path, options)) {
        std::cerr << "  ERROR: could not open " << path << "\n";
        return;
    }

    // Best practice: write OSI file metadata *before* any messages.
    // The required "net.asam.osi.trace" record contains OSI and protobuf
    // version information.  Optional fields (description, creation_time,
    // authors) improve interoperability with analysis tooling.
    auto metadata = osi3::MCAPTraceFileWriter::PrepareRequiredFileMetadata();
    metadata.metadata["description"] = "Multi-channel example (Part 1) — pure OSI topics.";
    metadata.metadata["creation_time"] = osi3::MCAPTraceFileWriter::GetCurrentTimeAsString();
    metadata.metadata["authors"] = "ASAM OSI Utilities example";
    if (!writer.AddFileMetadata(metadata)) {
        std::cerr << "  ERROR: could not write metadata\n";
        return;
    }

    // --- channel registration -----------------------------------------------
    // Channel 1: GroundTruth (unique schema)
    writer.AddChannel("ground_truth", osi3::GroundTruth::descriptor(), {{"net.asam.osi.trace.channel.description", "Environment ground truth"}});

    // Channels 2 & 3: two SensorView topics sharing the same protobuf schema.
    // The library automatically deduplicates the schema — only one copy of the
    // osi3.SensorView file-descriptor-set is stored in the MCAP file.
    writer.AddChannel("sensor_1_input", osi3::SensorView::descriptor(), {{"net.asam.osi.trace.channel.description", "Front-radar SensorView input"}});

    writer.AddChannel("sensor_2_input", osi3::SensorView::descriptor(), {{"net.asam.osi.trace.channel.description", "Rear-camera SensorView input"}});

    std::cout << "  Registered 3 channels (1x GroundTruth, 2x SensorView)\n";

    // --- prepare test data --------------------------------------------------
    const auto osi_version = osi3::InterfaceVersion::descriptor()->file()->options().GetExtension(osi3::current_interface_version);

    osi3::GroundTruth ground_truth;
    PopulateGroundTruth(ground_truth, osi_version);

    osi3::SensorView sensor_view_1;
    PopulateSensorView(sensor_view_1, /*sensor_id=*/10, osi_version);

    osi3::SensorView sensor_view_2;
    PopulateSensorView(sensor_view_2, /*sensor_id=*/20, osi_version);

    // --- simulation loop ----------------------------------------------------
    constexpr int kNumSteps = 10;
    constexpr uint64_t kStepNs = 100'000'000;  // 100 ms

    for (int i = 0; i < kNumSteps; ++i) {
        AdvanceTimestamp(ground_truth, kStepNs);
        AdvanceTimestamp(sensor_view_1, kStepNs);
        AdvanceTimestamp(sensor_view_2, kStepNs);

        writer.WriteMessage(ground_truth, "ground_truth");
        writer.WriteMessage(sensor_view_1, "sensor_1_input");
        writer.WriteMessage(sensor_view_2, "sensor_2_input");
    }

    writer.Close();
    std::cout << "  Wrote " << kNumSteps << " steps (3 messages each) to " << path.filename().string() << "\n";

    // --- read back ----------------------------------------------------------
    ReadBackAndPrintSummary(path, /*skip_non_osi=*/false);
}

// ===========================================================================
// Part 2 — Mixed OSI / non-OSI with MCAPTraceFileChannel
// ===========================================================================

/**
 * \brief Demonstrate the external-writer pattern for heterogeneous MCAP files.
 *
 * Creates a mcap::McapWriter directly, attaches an MCAPTraceFileChannel helper
 * for OSI channels, and also registers a non-OSI channel to show how both
 * coexist in a single MCAP file.
 */
void Part2_MixedChannelWriter() {
    std::cout << "\n=== Part 2: Mixed OSI / non-OSI with MCAPTraceFileChannel ===\n";

    const auto path = GenerateTempFilePath("part2");
    std::cout << "  Output: " << path.string() << "\n";

    // --- open external writer -----------------------------------------------
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        std::cerr << "  ERROR: could not open " << path << "\n";
        return;
    }
    mcap::McapWriter mcap_writer;
    mcap::McapWriterOptions options("protobuf");
    options.compression = mcap::Compression::Zstd;
    options.chunkSize = osi3::tracefile::config::kDefaultChunkSize;
    mcap_writer.open(file, options);

    // Best practice: write OSI file metadata before any messages.
    // When using an external writer you must do this yourself — the
    // MCAPTraceFileChannel helper does NOT write metadata automatically.
    auto osi_metadata = osi3::MCAPTraceFileChannel::PrepareRequiredFileMetadata();
    osi_metadata.metadata["description"] = "Mixed-channel example (Part 2) — OSI + custom topics.";
    osi_metadata.metadata["creation_time"] = osi3::MCAPTraceFileChannel::GetCurrentTimeAsString();
    if (const auto status = mcap_writer.write(osi_metadata); status.code != mcap::StatusCode::Success) {
        std::cerr << "  ERROR: could not write metadata: " << status.message << "\n";
        mcap_writer.close();
        return;
    }

    // --- register a non-OSI channel directly on the writer ------------------
    // Best practice: use a descriptive prefix (e.g. "vehicle/") for non-OSI
    // topics so they are easily distinguishable from "osi/" topics in tooling.
    mcap::Schema json_schema("vehicle.Status", "jsonschema", R"({"type":"object","properties":{"speed_kmh":{"type":"number"},"gear":{"type":"integer"}}})");
    mcap_writer.addSchema(json_schema);

    mcap::Channel custom_channel("vehicle/status", "json", json_schema.id);
    mcap_writer.addChannel(custom_channel);
    std::cout << "  Registered non-OSI channel: vehicle/status (json)\n";

    // --- OSI channel helper -------------------------------------------------
    // MCAPTraceFileChannel manages schema registration, buffer reuse, and
    // OSI-compliant channel metadata for you.
    osi3::MCAPTraceFileChannel osi_channels(mcap_writer);

    osi_channels.AddChannel("osi/ground_truth", osi3::GroundTruth::descriptor(), {{"net.asam.osi.trace.channel.description", "Environment ground truth (via external writer)"}});
    std::cout << "  Registered OSI channel: osi/ground_truth (protobuf)\n";

    // --- prepare test data --------------------------------------------------
    const auto osi_version = osi3::InterfaceVersion::descriptor()->file()->options().GetExtension(osi3::current_interface_version);
    osi3::GroundTruth ground_truth;
    PopulateGroundTruth(ground_truth, osi_version);

    // --- simulation loop — interleave OSI and non-OSI messages --------------
    constexpr int kNumSteps = 10;
    constexpr uint64_t kStepNs = 100'000'000;  // 100 ms

    for (int i = 0; i < kNumSteps; ++i) {
        AdvanceTimestamp(ground_truth, kStepNs);

        // Write OSI message via the helper
        osi_channels.WriteMessage(ground_truth, "osi/ground_truth");

        // Write non-OSI message directly to the mcap writer
        const std::string json_payload = R"({"speed_kmh":)" + std::to_string(50.0 + i * 2.0) + R"(,"gear":)" + std::to_string(3 + i / 4) + "}";

        mcap::Message custom_msg;
        custom_msg.channelId = custom_channel.id;
        custom_msg.logTime = osi3::tracefile::TimestampToNanoseconds(ground_truth);
        custom_msg.publishTime = custom_msg.logTime;
        custom_msg.data = reinterpret_cast<const std::byte*>(json_payload.data());
        custom_msg.dataSize = json_payload.size();
        if (const auto status = mcap_writer.write(custom_msg); status.code != mcap::StatusCode::Success) {
            std::cerr << "  ERROR: could not write custom message: " << status.message << "\n";
        }
    }

    mcap_writer.close();
    file.close();
    std::cout << "  Wrote " << kNumSteps << " steps (1 OSI + 1 non-OSI each) to " << path.filename().string() << "\n";

    // --- read back with non-OSI filtering -----------------------------------
    // Best practice: when reading a mixed file with MCAPTraceFileReader, enable
    // SetSkipNonOSIMsgs(true) to silently skip channels that are not recognized
    // OSI types.  Without this flag the reader would throw on unknown schemas.
    std::cout << "  Reading back with SetSkipNonOSIMsgs(true):\n";
    ReadBackAndPrintSummary(path, /*skip_non_osi=*/true);
}

auto RunExample() -> int {
    std::cout << "Starting Multi-Channel MCAP Writer example" << std::endl;

    Part1_MultiTopicWriter();
    Part2_MixedChannelWriter();

    std::cout << "\nFinished Multi-Channel MCAP Writer example" << std::endl;
    return 0;
}

}  // namespace

// ===========================================================================
// main
// ===========================================================================

/**
 * \brief Entry point for the multi-channel MCAP writer example.
 */
auto main(int /*argc*/, const char** /*argv*/) -> int {
    try {
        return RunExample();
    } catch (const std::exception& error) {
        std::cerr << "ERROR: " << error.what() << std::endl;
        return 1;
    }
}
