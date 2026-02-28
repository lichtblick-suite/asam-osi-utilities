//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//
/**
 * \file
 * \brief Convert a binary GroundTruth .osi trace to a SensorView .osi trace.
 *
 * Each GroundTruth frame is wrapped in a SensorView message whose
 * global_ground_truth field contains the original data.
 *
 * Usage: convert_gt2sv <input_gt.osi> <output_sv.osi>
 */

#include <osi-utilities/tracefile/reader/SingleChannelBinaryTraceFileReader.h>
#include <osi-utilities/tracefile/writer/SingleChannelBinaryTraceFileWriter.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>

#include "osi_groundtruth.pb.h"
#include "osi_sensorview.pb.h"

auto main(const int argc, const char** argv) -> int {
    if (argc != 3 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
        std::cerr << "Usage: convert_gt2sv <input_gt.osi> <output_sv.osi>\n"
                  << "\n"
                  << "Reads a binary .osi GroundTruth trace and writes a binary .osi\n"
                  << "SensorView trace where each frame wraps the original GroundTruth\n"
                  << "in the global_ground_truth field of a SensorView message.\n";
        return 1;
    }

    const std::filesystem::path input_path = argv[1];
    const std::filesystem::path output_path = argv[2];

    std::cout << "Input:  " << input_path << "\n";
    std::cout << "Output: " << output_path << "\n";

    // Open reader with explicit GroundTruth type
    osi3::SingleChannelBinaryTraceFileReader reader;
    if (!reader.Open(input_path, osi3::ReaderTopLevelMessage::kGroundTruth)) {
        std::cerr << "ERROR: Could not open input file: " << input_path << "\n";
        return 1;
    }

    osi3::SingleChannelBinaryTraceFileWriter writer;
    if (!writer.Open(output_path)) {
        std::cerr << "ERROR: Could not open output file: " << output_path << "\n";
        return 1;
    }

    int frame_count = 0;
    while (reader.HasNext()) {
        auto result = reader.ReadMessage();
        if (!result) {
            std::cerr << "WARNING: Failed to read frame " << frame_count << ", skipping.\n";
            continue;
        }

        auto* gt = dynamic_cast<osi3::GroundTruth*>(result->message.get());
        if (gt == nullptr) {
            std::cerr << "WARNING: Frame " << frame_count << " is not a GroundTruth, skipping.\n";
            continue;
        }

        // Build SensorView wrapping the GroundTruth
        osi3::SensorView sv;
        if (gt->has_timestamp()) {
            sv.mutable_timestamp()->CopyFrom(gt->timestamp());
        }
        if (gt->has_host_vehicle_id()) {
            sv.mutable_host_vehicle_id()->CopyFrom(gt->host_vehicle_id());
        }
        sv.mutable_global_ground_truth()->CopyFrom(*gt);

        if (!writer.WriteMessage(sv)) {
            std::cerr << "ERROR: Failed to write frame " << frame_count << "\n";
            return 1;
        }
        ++frame_count;
    }

    reader.Close();
    writer.Close();

    std::cout << "Converted " << frame_count << " frames from GroundTruth to SensorView.\n";
    return 0;
}
