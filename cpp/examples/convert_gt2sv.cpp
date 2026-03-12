//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//
/**
 * \file
 * \brief Convert a GroundTruth trace to a SensorView trace.
 *
 * Supports .osi (binary) and .mcap input/output in any combination.
 * Each GroundTruth frame is wrapped in a SensorView message whose
 * global_ground_truth field contains the original data.
 *
 * Usage: convert_gt2sv <input.osi|.mcap> <output.osi|.mcap> [--topic <name>]
 */

#include <osi-utilities/tracefile/Reader.h>
#include <osi-utilities/tracefile/reader/MCAPTraceFileReader.h>
#include <osi-utilities/tracefile/reader/SingleChannelBinaryTraceFileReader.h>
#include <osi-utilities/tracefile/writer/MCAPTraceFileWriter.h>
#include <osi-utilities/tracefile/writer/SingleChannelBinaryTraceFileWriter.h>

#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include "osi_groundtruth.pb.h"
#include "osi_sensorview.pb.h"

auto main(const int argc, const char** argv) -> int {
    const auto print_usage = []() {
        std::cerr << "Usage: convert_gt2sv <input.osi|.mcap> <output.osi|.mcap> [--topic <name>]\n"
                  << "\n"
                  << "Reads a GroundTruth trace (.osi or .mcap) and writes a SensorView\n"
                  << "trace where each frame wraps the original GroundTruth in the\n"
                  << "global_ground_truth field of a SensorView message.\n"
                  << "\n"
                  << "Options:\n"
                  << "  --topic <name>  Filter MCAP input to a specific topic\n";
    };

    if (argc >= 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h")) {
        print_usage();
        return 0;
    }

    if (argc < 3) {
        print_usage();
        return 1;
    }

    const std::filesystem::path input_path = argv[1];
    const std::filesystem::path output_path = argv[2];
    std::string topic_filter;

    // Parse optional --topic argument
    for (int i = 3; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--topic") {
            if (i + 1 >= argc) {
                std::cerr << "ERROR: Missing value for --topic\n";
                return 1;
            }
            topic_filter = argv[++i];
        } else {
            std::cerr << "ERROR: Unknown argument: " << argument << "\n";
            return 1;
        }
    }

    const auto input_ext = input_path.extension().string();
    const auto output_ext = output_path.extension().string();

    std::cout << "Input:  " << input_path << " (" << input_ext << ")\n";
    std::cout << "Output: " << output_path << " (" << output_ext << ")\n";

    // Create reader based on input format
    std::unique_ptr<osi3::TraceFileReader> reader;
    if (input_ext == ".osi") {
        auto binary_reader = std::make_unique<osi3::SingleChannelBinaryTraceFileReader>();
        if (!binary_reader->Open(input_path, osi3::ReaderTopLevelMessage::kGroundTruth)) {
            std::cerr << "ERROR: Could not open input file: " << input_path << "\n";
            return 1;
        }
        reader = std::move(binary_reader);
    } else if (input_ext == ".mcap") {
        auto mcap_reader = std::make_unique<osi3::MCAPTraceFileReader>();
        mcap_reader->SetSkipNonOSIMsgs(true);
        if (!topic_filter.empty()) {
            mcap_reader->SetTopics({topic_filter});
        }
        if (!mcap_reader->Open(input_path)) {
            std::cerr << "ERROR: Could not open input file: " << input_path << "\n";
            return 1;
        }
        reader = std::move(mcap_reader);
    } else {
        std::cerr << "ERROR: Unsupported input format: " << input_ext << " (use .osi or .mcap)\n";
        return 1;
    }

    // Create writer based on output format
    std::unique_ptr<osi3::SingleChannelBinaryTraceFileWriter> binary_writer;
    std::unique_ptr<osi3::MCAPTraceFileWriter> mcap_writer;
    bool output_is_mcap = (output_ext == ".mcap");
    const std::string output_topic = topic_filter.empty() ? "SensorView" : topic_filter;

    if (output_is_mcap) {
        mcap_writer = std::make_unique<osi3::MCAPTraceFileWriter>();
        if (!mcap_writer->Open(output_path)) {
            std::cerr << "ERROR: Could not open output file: " << output_path << "\n";
            return 1;
        }
        mcap_writer->AddFileMetadata(osi3::MCAPTraceFileWriter::PrepareRequiredFileMetadata());
        mcap_writer->AddChannel(output_topic, osi3::SensorView::descriptor());
    } else if (output_ext == ".osi") {
        binary_writer = std::make_unique<osi3::SingleChannelBinaryTraceFileWriter>();
        if (!binary_writer->Open(output_path)) {
            std::cerr << "ERROR: Could not open output file: " << output_path << "\n";
            return 1;
        }
    } else {
        std::cerr << "ERROR: Unsupported output format: " << output_ext << " (use .osi or .mcap)\n";
        return 1;
    }

    int frame_count = 0;
    while (reader->HasNext()) {
        auto result = reader->ReadMessage();
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

        bool write_ok = false;
        if (output_is_mcap) {
            write_ok = mcap_writer->WriteMessage(sv, output_topic);
        } else {
            write_ok = binary_writer->WriteMessage(sv);
        }

        if (!write_ok) {
            std::cerr << "ERROR: Failed to write frame " << frame_count << "\n";
            return 1;
        }
        ++frame_count;
    }

    reader->Close();
    if (output_is_mcap) {
        mcap_writer->Close();
    } else {
        binary_writer->Close();
    }

    std::cout << "Converted " << frame_count << " frames from GroundTruth to SensorView.\n";
    return 0;
}
