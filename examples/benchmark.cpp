//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//
/**
 * \file
 * \brief Benchmark read and write throughput of a binary .osi trace file.
 *
 * Reads all frames from the input file, then writes them back to a temporary
 * file. Reports frame count, total bytes, wall-clock time, MiB/s, and
 * frames/s for both operations.
 *
 * Usage: benchmark <file.osi> [--type GroundTruth|SensorView|...]
 */

#include <osi-utilities/tracefile/reader/SingleChannelBinaryTraceFileReader.h>
#include <osi-utilities/tracefile/writer/SingleChannelBinaryTraceFileWriter.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "osi_groundtruth.pb.h"
#include "osi_hostvehicledata.pb.h"
#include "osi_motionrequest.pb.h"
#include "osi_sensordata.pb.h"
#include "osi_sensorview.pb.h"
#include "osi_streamingupdate.pb.h"
#include "osi_trafficcommand.pb.h"
#include "osi_trafficcommandupdate.pb.h"
#include "osi_trafficupdate.pb.h"

/** \brief Map CLI message type names to OSI enum values. */
static const std::unordered_map<std::string, osi3::ReaderTopLevelMessage> kValidTypes = {
    {"GroundTruth", osi3::ReaderTopLevelMessage::kGroundTruth},         {"SensorData", osi3::ReaderTopLevelMessage::kSensorData},
    {"SensorView", osi3::ReaderTopLevelMessage::kSensorView},           {"HostVehicleData", osi3::ReaderTopLevelMessage::kHostVehicleData},
    {"TrafficCommand", osi3::ReaderTopLevelMessage::kTrafficCommand},   {"TrafficCommandUpdate", osi3::ReaderTopLevelMessage::kTrafficCommandUpdate},
    {"TrafficUpdate", osi3::ReaderTopLevelMessage::kTrafficUpdate},     {"MotionRequest", osi3::ReaderTopLevelMessage::kMotionRequest},
    {"StreamingUpdate", osi3::ReaderTopLevelMessage::kStreamingUpdate},
};

/**
 * \brief Write a single message through the binary writer, dispatching on type.
 * \return true on success.
 */
bool WriteTypedMessage(osi3::SingleChannelBinaryTraceFileWriter& writer, const osi3::ReadResult& result) {
    switch (result.message_type) {
        case osi3::ReaderTopLevelMessage::kGroundTruth:
            return writer.WriteMessage(*static_cast<osi3::GroundTruth*>(result.message.get()));
        case osi3::ReaderTopLevelMessage::kSensorData:
            return writer.WriteMessage(*static_cast<osi3::SensorData*>(result.message.get()));
        case osi3::ReaderTopLevelMessage::kSensorView:
            return writer.WriteMessage(*static_cast<osi3::SensorView*>(result.message.get()));
        case osi3::ReaderTopLevelMessage::kHostVehicleData:
            return writer.WriteMessage(*static_cast<osi3::HostVehicleData*>(result.message.get()));
        case osi3::ReaderTopLevelMessage::kTrafficCommand:
            return writer.WriteMessage(*static_cast<osi3::TrafficCommand*>(result.message.get()));
        case osi3::ReaderTopLevelMessage::kTrafficCommandUpdate:
            return writer.WriteMessage(*static_cast<osi3::TrafficCommandUpdate*>(result.message.get()));
        case osi3::ReaderTopLevelMessage::kTrafficUpdate:
            return writer.WriteMessage(*static_cast<osi3::TrafficUpdate*>(result.message.get()));
        case osi3::ReaderTopLevelMessage::kMotionRequest:
            return writer.WriteMessage(*static_cast<osi3::MotionRequest*>(result.message.get()));
        case osi3::ReaderTopLevelMessage::kStreamingUpdate:
            return writer.WriteMessage(*static_cast<osi3::StreamingUpdate*>(result.message.get()));
        default:
            return false;
    }
}

void PrintUsage() {
    std::cerr << "Usage: benchmark <file.osi> [--type <MessageType>]\n"
              << "\n"
              << "Benchmarks read and write throughput for a binary .osi trace.\n"
              << "Reads all frames, then writes them to a temporary file.\n"
              << "\n"
              << "Options:\n"
              << "  --type <type>  Specify message type if not inferable from filename.\n"
              << "                 Valid types:";
    for (const auto& [name, _] : kValidTypes) {
        std::cerr << " " << name;
    }
    std::cerr << "\n";
}

void PrintMetrics(const char* label, int frame_count, double bytes, double elapsed_s) {
    const double mib = bytes / (1024.0 * 1024.0);
    std::cout << "\n--- " << label << " ---\n";
    std::cout << "  Frames:  " << frame_count << "\n";
    std::cout << "  Time:    " << elapsed_s << " s\n";
    if (elapsed_s > 0.0) {
        std::cout << "  Speed:   " << mib / elapsed_s << " MiB/s\n";
        std::cout << "  Rate:    " << static_cast<double>(frame_count) / elapsed_s << " frames/s\n";
    }
}

auto main(const int argc, const char** argv) -> int {
    if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
        PrintUsage();
        return 1;
    }

    const std::filesystem::path input_path = argv[1];
    auto message_type = osi3::ReaderTopLevelMessage::kUnknown;

    for (int i = 2; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--type" && i + 1 < argc) {
            const std::string type_str = argv[++i];
            auto it = kValidTypes.find(type_str);
            if (it == kValidTypes.end()) {
                std::cerr << "ERROR: Unknown message type: " << type_str << "\n";
                PrintUsage();
                return 1;
            }
            message_type = it->second;
        } else {
            std::cerr << "ERROR: Unknown argument: " << arg << "\n";
            PrintUsage();
            return 1;
        }
    }

    // Get file size
    std::error_code ec;
    const auto file_size = std::filesystem::file_size(input_path, ec);
    if (ec) {
        std::cerr << "ERROR: Cannot stat file: " << input_path << " (" << ec.message() << ")\n";
        return 1;
    }

    std::cout << "File:    " << input_path << "\n";
    std::cout << "Size:    " << file_size << " bytes (" << static_cast<double>(file_size) / (1024.0 * 1024.0) << " MiB)\n";

    // ========================================================================
    // Read benchmark — read all frames into memory
    // ========================================================================
    osi3::SingleChannelBinaryTraceFileReader reader;
    if (!reader.Open(input_path, message_type)) {
        std::cerr << "ERROR: Could not open: " << input_path << "\n";
        return 1;
    }

    std::vector<osi3::ReadResult> messages;
    const auto read_start = std::chrono::steady_clock::now();

    while (reader.HasNext()) {
        auto result = reader.ReadMessage();
        if (!result) {
            std::cerr << "WARNING: Failed to read frame " << messages.size() << "\n";
            break;
        }
        messages.push_back(std::move(*result));
    }

    const auto read_end = std::chrono::steady_clock::now();
    reader.Close();

    PrintMetrics("Read", static_cast<int>(messages.size()), static_cast<double>(file_size), std::chrono::duration<double>(read_end - read_start).count());

    // ========================================================================
    // Write benchmark — write all frames to a temporary file
    // ========================================================================
    auto tmp_path = std::filesystem::temp_directory_path() / "benchmark_write_output.osi";

    osi3::SingleChannelBinaryTraceFileWriter writer;
    if (!writer.Open(tmp_path)) {
        std::cerr << "ERROR: Could not open temp file for write benchmark: " << tmp_path << "\n";
        return 1;
    }

    const auto write_start = std::chrono::steady_clock::now();

    int written = 0;
    for (const auto& msg : messages) {
        if (!WriteTypedMessage(writer, msg)) {
            std::cerr << "WARNING: Failed to write frame " << written << "\n";
            break;
        }
        ++written;
    }

    const auto write_end = std::chrono::steady_clock::now();
    writer.Close();

    const auto written_size = std::filesystem::file_size(tmp_path, ec);
    std::filesystem::remove(tmp_path, ec);

    PrintMetrics("Write", written, static_cast<double>(written_size), std::chrono::duration<double>(write_end - write_start).count());

    return 0;
}
