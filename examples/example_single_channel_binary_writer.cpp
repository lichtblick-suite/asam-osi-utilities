//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//
/**
 * \file
 * \brief Write a single-channel binary OSI `.osi` trace file.
 */

#include <osi-utilities/tracefile/TraceFileConfig.h>
#include <osi-utilities/tracefile/writer/SingleChannelBinaryTraceFileWriter.h>

#include <filesystem>

#include "osi_sensordata.pb.h"
#include "osi_version.pb.h"

/**
 * \brief Get the current time in OSI filename format.
 * \return Timestamp formatted as YYYYMMDDThhmmssZ.
 */
auto GetCurrentTimeAsString() -> std::string {
    const auto now = std::chrono::system_clock::now();
    const auto timer = std::chrono::system_clock::to_time_t(now);
    const std::tm utc_time_structure = *std::gmtime(&timer);  // Greenwich Mean Time (GMT) is in Coordinated Universal Time (UTC) zone
    std::ostringstream oss{};
    oss << std::put_time(&utc_time_structure, "%Y%m%dT%H%M%S");
    oss << "Z";  // As GMT is used as a reference time zone, add Z to indicate UTC (+00:00)
    return oss.str();
}

/**
 * \brief Generate a temporary output path for the `.osi` example.
 * \return Temporary file path.
 */
auto GenerateTempFilePath() -> std::filesystem::path {
    // create a path which follows the OSI specification recommendation
    std::string file_name = GetCurrentTimeAsString();
    const auto osi_version = osi3::InterfaceVersion::descriptor()->file()->options().GetExtension(osi3::current_interface_version);
    file_name += "_" + std::to_string(osi_version.version_major()) + "." + std::to_string(osi_version.version_minor()) + "." + std::to_string(osi_version.version_patch());
    file_name += "_" + google::protobuf::internal::VersionString(GOOGLE_PROTOBUF_VERSION);
    file_name += "_10";  // 10 frames
    file_name += "_example_single_channel_binary_writer.osi";
    auto path = std::filesystem::temp_directory_path() / file_name;
    return path;
}

/**
 * \brief Entry point for the single-channel binary writer example.
 */
auto main(int /*argc*/, const char** /*argv*/) -> int {
    std::cout << "Starting single channel binary writer example:" << std::endl;

    // Create writer and open file
    auto trace_file_writer = osi3::SingleChannelBinaryTraceFileWriter();
    const auto trace_file_path = GenerateTempFilePath();
    std::cout << "Creating trace file at " << trace_file_path << std::endl;
    trace_file_writer.Open(trace_file_path);

    // create OSI data to store
    const auto osi_version = osi3::InterfaceVersion::descriptor()->file()->options().GetExtension(osi3::current_interface_version);

    osi3::SensorView sensor_view;
    sensor_view.mutable_version()->CopyFrom(osi_version);
    sensor_view.mutable_sensor_id()->set_value(0);

    auto* const ground_truth = sensor_view.mutable_global_ground_truth();
    ground_truth->mutable_version()->CopyFrom(osi_version);

    auto* const host_vehicle = ground_truth->mutable_moving_object()->Add();
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
        auto timestamp = sensor_view.timestamp().seconds() * kNsPerSec + sensor_view.timestamp().nanos();
        timestamp += kTimeStepSizeS * kNsPerSec;
        sensor_view.mutable_timestamp()->set_nanos(timestamp % kNsPerSec);
        sensor_view.mutable_timestamp()->set_seconds(timestamp / kNsPerSec);
        ground_truth->mutable_timestamp()->set_nanos(timestamp % kNsPerSec);
        ground_truth->mutable_timestamp()->set_seconds(timestamp / kNsPerSec);
        const auto old_position = host_vehicle->base().position().x();
        const auto new_position = old_position + host_vehicle->base().velocity().x() * kTimeStepSizeS;
        host_vehicle->mutable_base()->mutable_position()->set_x(new_position);

        // write the data
        trace_file_writer.WriteMessage(sensor_view);
    }

    trace_file_writer.Close();

    std::cout << "Finished single channel binary writer example" << std::endl;
    return 0;
}
