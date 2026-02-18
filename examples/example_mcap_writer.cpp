//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//
/**
 * \file
 * \brief Write an example OSI MCAP trace file with metadata and channels.
 */

#include <osi-utilities/tracefile/TraceFileConfig.h>
#include <osi-utilities/tracefile/writer/MCAPTraceFileWriter.h>

#include <filesystem>
#ifdef _WIN32
#include <process.h>
#else
#include <unistd.h>
#endif

#include "osi_sensorview.pb.h"
#include "osi_version.pb.h"

/**
 * \brief Generate a temporary output path for the example MCAP file.
 * \return Temporary file path.
 */
auto GenerateTempFilePath() -> std::filesystem::path {
#ifdef _WIN32
    const auto pid = std::to_string(_getpid());
#else
    const auto pid = std::to_string(getpid());
#endif
    return std::filesystem::temp_directory_path() / ("sv_example_" + pid + ".mcap");  // add sv to indicate sensor view as recommended by the OSI-specification
}

/**
 * \brief Entry point for the MCAP writer example.
 */
auto main(int /*argc*/, const char** /*argv*/) -> int {
    std::cout << "Starting MCAP Writer example:" << std::endl;

    // Create writer and open file
    auto trace_file_writer = osi3::MCAPTraceFileWriter();
    const auto trace_file_path = GenerateTempFilePath();
    std::cout << "Creating trace_file at " << trace_file_path.string() << std::endl;

    mcap::McapWriterOptions mcap_options("osi");
    // Use the library default chunk size (16 MiB). Lichtblick plays back well
    // with 4-32 MiB chunks; adjust via kMinChunkSize / kMaxChunkSize bounds.
    mcap_options.chunkSize = osi3::tracefile::config::kDefaultChunkSize;
    // Default: zstd
    mcap_options.compression = mcap::Compression::Lz4;

    trace_file_writer.Open(trace_file_path, mcap_options);

    // add required and optional metadata to the net.asam.osi.trace metadata record
    auto net_asam_osi_trace_metadata = osi3::MCAPTraceFileWriter::PrepareRequiredFileMetadata();
    // Add optional metadata to the net.asam.osi.trace metadata record, as recommended by the OSI specification.
    net_asam_osi_trace_metadata.metadata["description"] = "Example mcap trace file created with the ASAM OSI utilities library.";  // optional field
    net_asam_osi_trace_metadata.metadata["creation_time"] = osi3::MCAPTraceFileWriter::GetCurrentTimeAsString();                   // optional field
    net_asam_osi_trace_metadata.metadata["authors"] = "Jane Doe, John Doe";                                                        // optional field
    if (!trace_file_writer.AddFileMetadata(net_asam_osi_trace_metadata)) {
        std::cerr << "Failed to add required metadata to trace_file." << std::endl;
        exit(1);
    }

    // add a channel to store some data
    const std::string topic = "Sensor_1_Input";
    const std::unordered_map<std::string, std::string> channel_metadata = {
        {"net.asam.osi.trace.channel.description", "This channel contains the input data (SensorView) for sensor 1"}};
    trace_file_writer.AddChannel(topic, osi3::SensorView::descriptor(), channel_metadata);

    // create OSI data to store
    const auto osi_version = osi3::InterfaceVersion::descriptor()->file()->options().GetExtension(osi3::current_interface_version);

    osi3::SensorView sensor_view_1;
    sensor_view_1.mutable_version()->CopyFrom(osi_version);
    sensor_view_1.mutable_sensor_id()->set_value(0);
    sensor_view_1.mutable_host_vehicle_id()->set_value(12);

    auto* const ground_truth_1 = sensor_view_1.mutable_global_ground_truth();
    ground_truth_1->mutable_version()->CopyFrom(osi_version);

    auto* const host_vehicle = ground_truth_1->mutable_moving_object()->Add();
    host_vehicle->mutable_id()->set_value(12);
    host_vehicle->mutable_vehicle_classification()->set_type(osi3::MovingObject_VehicleClassification_Type_TYPE_SMALL_CAR);
    host_vehicle->mutable_base()->mutable_dimension()->set_length(5);
    host_vehicle->mutable_base()->mutable_dimension()->set_width(2);
    host_vehicle->mutable_base()->mutable_dimension()->set_height(1.5);
    host_vehicle->mutable_base()->mutable_velocity()->set_x(10.0);

    // write the data continuously in a loop
    constexpr double kTimeStepSizeS = 0.1;  // NOLINT
    for (int i = 0; i < 10; ++i) {
        // manipulate the data so not every message is the same
        constexpr auto kNsPerSec = osi3::tracefile::config::kNanosecondsPerSecond;
        auto timestamp = sensor_view_1.timestamp().seconds() * kNsPerSec + sensor_view_1.timestamp().nanos();
        timestamp += kTimeStepSizeS * kNsPerSec;
        sensor_view_1.mutable_timestamp()->set_nanos(timestamp % kNsPerSec);
        sensor_view_1.mutable_timestamp()->set_seconds(static_cast<int64_t>(timestamp / kNsPerSec));
        ground_truth_1->mutable_timestamp()->set_nanos(timestamp % kNsPerSec);
        ground_truth_1->mutable_timestamp()->set_seconds(static_cast<int64_t>(timestamp / kNsPerSec));
        const auto old_position = host_vehicle->base().position().x();
        const auto new_position = old_position + host_vehicle->base().velocity().x() + kTimeStepSizeS;
        host_vehicle->mutable_base()->mutable_position()->set_x(new_position);

        // finally write the data using topic
        trace_file_writer.WriteMessage(sensor_view_1, topic);
    }

    // Close the file to flush the last piece of data from memory and ensure a clean exit state.
    trace_file_writer.Close();

    std::cout << "Finished MCAP Writer example" << std::endl;
}
