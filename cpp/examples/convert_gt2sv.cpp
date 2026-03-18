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

#include <cstdio>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <iostream>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>

#include "osi_groundtruth.pb.h"
#include "osi_sensorview.pb.h"

namespace {

struct ProgramOptions {
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    std::string topic_filter;
};

struct OutputContext {
    std::unique_ptr<osi3::SingleChannelBinaryTraceFileWriter> binary_writer;
    std::unique_ptr<osi3::MCAPTraceFileWriter> mcap_writer;
    std::string output_topic;

    [[nodiscard]] auto WriteMessage(const osi3::SensorView& sensor_view) const -> bool {
        if (mcap_writer) {
            return mcap_writer->WriteMessage(sensor_view, output_topic);
        }

        return binary_writer->WriteMessage(sensor_view);
    }

    void Close() const {
        if (mcap_writer) {
            mcap_writer->Close();
        } else if (binary_writer) {
            binary_writer->Close();
        }
    }
};

void PrintUsage() {
    std::cerr << "Usage: convert_gt2sv <input.osi|.mcap> <output.osi|.mcap> [--topic <name>]\n"
              << "\n"
              << "Reads a GroundTruth trace (.osi or .mcap) and writes a SensorView\n"
              << "trace where each frame wraps the original GroundTruth in the\n"
              << "global_ground_truth field of a SensorView message.\n"
              << "\n"
              << "Options:\n"
              << "  --topic <name>  Filter MCAP input to a specific topic\n";
}

auto IsHelpRequested(const int argc, const char** argv) -> bool { return argc >= 2 && (std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h"); }

auto ParseArguments(const int argc, const char** argv) -> std::optional<ProgramOptions> {
    if (argc < 3) {
        PrintUsage();
        return std::nullopt;
    }

    ProgramOptions options{argv[1], argv[2], ""};
    for (int i = 3; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--topic") {
            if (i + 1 >= argc) {
                std::cerr << "ERROR: Missing value for --topic\n";
                return std::nullopt;
            }
            options.topic_filter = argv[++i];
        } else {
            std::cerr << "ERROR: Unknown argument: " << argument << "\n";
            return std::nullopt;
        }
    }

    return options;
}

auto CreateReader(const ProgramOptions& options) -> std::unique_ptr<osi3::TraceFileReader> {
    const auto input_ext = options.input_path.extension().string();
    if (input_ext == ".osi") {
        auto binary_reader = std::make_unique<osi3::SingleChannelBinaryTraceFileReader>();
        if (!binary_reader->Open(options.input_path, osi3::ReaderTopLevelMessage::kGroundTruth)) {
            throw std::runtime_error("Could not open input file: " + options.input_path.string());
        }
        return binary_reader;
    }

    if (input_ext == ".mcap") {
        auto mcap_reader = std::make_unique<osi3::MCAPTraceFileReader>();
        mcap_reader->SetSkipNonOSIMsgs(true);
        if (!options.topic_filter.empty()) {
            mcap_reader->SetTopics({options.topic_filter});
        }
        if (!mcap_reader->Open(options.input_path)) {
            throw std::runtime_error("Could not open input file: " + options.input_path.string());
        }
        return mcap_reader;
    }

    throw std::runtime_error("Unsupported input format: " + input_ext + " (use .osi or .mcap)");
}

auto CreateOutputContext(const ProgramOptions& options) -> OutputContext {
    OutputContext output_context;
    const auto output_ext = options.output_path.extension().string();
    output_context.output_topic = options.topic_filter.empty() ? "SensorView" : options.topic_filter;

    if (output_ext == ".mcap") {
        output_context.mcap_writer = std::make_unique<osi3::MCAPTraceFileWriter>();
        if (!output_context.mcap_writer->Open(options.output_path)) {
            throw std::runtime_error("Could not open output file: " + options.output_path.string());
        }
        if (!output_context.mcap_writer->AddFileMetadata(osi3::MCAPTraceFileWriter::PrepareRequiredFileMetadata())) {
            throw std::runtime_error("Could not add output file metadata.");
        }
        output_context.mcap_writer->AddChannel(output_context.output_topic, osi3::SensorView::descriptor());
        return output_context;
    }

    if (output_ext == ".osi") {
        output_context.binary_writer = std::make_unique<osi3::SingleChannelBinaryTraceFileWriter>();
        if (!output_context.binary_writer->Open(options.output_path)) {
            throw std::runtime_error("Could not open output file: " + options.output_path.string());
        }
        return output_context;
    }

    throw std::runtime_error("Unsupported output format: " + output_ext + " (use .osi or .mcap)");
}

auto BuildSensorViewMessage(const osi3::GroundTruth& ground_truth) -> osi3::SensorView {
    osi3::SensorView sensor_view;
    if (ground_truth.has_timestamp()) {
        sensor_view.mutable_timestamp()->CopyFrom(ground_truth.timestamp());
    }
    if (ground_truth.has_host_vehicle_id()) {
        sensor_view.mutable_host_vehicle_id()->CopyFrom(ground_truth.host_vehicle_id());
    }
    sensor_view.mutable_global_ground_truth()->CopyFrom(ground_truth);
    return sensor_view;
}

auto ConvertFrames(osi3::TraceFileReader& reader, OutputContext& output_context) -> int {
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

        const auto sensor_view = BuildSensorViewMessage(*gt);
        if (!output_context.WriteMessage(sensor_view)) {
            throw std::runtime_error("Failed to write frame " + std::to_string(frame_count));
        }
        ++frame_count;
    }

    return frame_count;
}

auto RunProgram(const int argc, const char** argv) -> int {
    if (IsHelpRequested(argc, argv)) {
        PrintUsage();
        return 0;
    }

    const auto options = ParseArguments(argc, argv);
    if (!options) {
        return 1;
    }

    const auto input_ext = options->input_path.extension().string();
    const auto output_ext = options->output_path.extension().string();
    std::cout << "Input:  " << options->input_path.string() << " (" << input_ext << ")\n";
    std::cout << "Output: " << options->output_path.string() << " (" << output_ext << ")\n";

    auto reader = CreateReader(*options);
    auto output_context = CreateOutputContext(*options);
    const auto frame_count = ConvertFrames(*reader, output_context);
    reader->Close();
    output_context.Close();
    std::cout << "Converted " << frame_count << " frames from GroundTruth to SensorView.\n";
    return 0;
}

auto RunMainNoThrow(const int argc, const char** argv) noexcept -> int {
    try {
        return RunProgram(argc, argv);
    } catch (const std::exception& error) {
        std::fputs("ERROR: ", stderr);
        std::fputs(error.what(), stderr);
        std::fputc('\n', stderr);
    } catch (...) {
        std::fputs("ERROR: Unknown exception\n", stderr);
    }

    return EXIT_FAILURE;
}

}  // namespace

auto main(const int argc, const char** argv) -> int { return RunMainNoThrow(argc, argv); }
