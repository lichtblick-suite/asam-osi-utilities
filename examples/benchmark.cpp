//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//
/**
 * \file
 * \brief Benchmark read/write throughput for OSI trace files.
 *
 * Two modes:
 *   benchmark synthetic [N]           — generate N SensorView messages, benchmark all 3 formats
 *   benchmark file <path> [--type T]  — benchmark read/write on a real .osi file
 */

#include <osi-utilities/tracefile/reader/MCAPTraceFileReader.h>
#include <osi-utilities/tracefile/reader/SingleChannelBinaryTraceFileReader.h>
#include <osi-utilities/tracefile/reader/TXTHTraceFileReader.h>
#include <osi-utilities/tracefile/writer/MCAPTraceFileWriter.h>
#include <osi-utilities/tracefile/writer/SingleChannelBinaryTraceFileWriter.h>
#include <osi-utilities/tracefile/writer/TXTHTraceFileWriter.h>

#include <chrono>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
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
#include "osi_version.pb.h"

// =============================================================================
// Shared helpers
// =============================================================================

namespace {

/// Simple timer helper.
class Timer {
   public:
    void Start() { start_ = std::chrono::steady_clock::now(); }

    [[nodiscard]] auto ElapsedSeconds() const -> double {
        const auto end = std::chrono::steady_clock::now();
        return std::chrono::duration<double>(end - start_).count();
    }

   private:
    std::chrono::steady_clock::time_point start_;
};

/// Map CLI message type names to OSI enum values.
const std::unordered_map<std::string, osi3::ReaderTopLevelMessage> kValidTypes = {
    {"GroundTruth", osi3::ReaderTopLevelMessage::kGroundTruth},         {"SensorData", osi3::ReaderTopLevelMessage::kSensorData},
    {"SensorView", osi3::ReaderTopLevelMessage::kSensorView},           {"HostVehicleData", osi3::ReaderTopLevelMessage::kHostVehicleData},
    {"TrafficCommand", osi3::ReaderTopLevelMessage::kTrafficCommand},   {"TrafficCommandUpdate", osi3::ReaderTopLevelMessage::kTrafficCommandUpdate},
    {"TrafficUpdate", osi3::ReaderTopLevelMessage::kTrafficUpdate},     {"MotionRequest", osi3::ReaderTopLevelMessage::kMotionRequest},
    {"StreamingUpdate", osi3::ReaderTopLevelMessage::kStreamingUpdate},
};

// =============================================================================
// Synthetic-mode helpers
// =============================================================================

/// Pre-generate N SensorView messages with 5 moving objects each.
auto GenerateMessages(int count) -> std::vector<osi3::SensorView> {
    const auto osi_version = osi3::InterfaceVersion::descriptor()->file()->options().GetExtension(osi3::current_interface_version);

    std::vector<osi3::SensorView> messages;
    messages.reserve(count);

    for (int i = 0; i < count; ++i) {
        osi3::SensorView sv;
        sv.mutable_version()->CopyFrom(osi_version);
        sv.mutable_sensor_id()->set_value(0);
        sv.mutable_host_vehicle_id()->set_value(12);
        sv.mutable_timestamp()->set_seconds(i / 10);
        sv.mutable_timestamp()->set_nanos((i % 10) * 100'000'000);

        auto* gt = sv.mutable_global_ground_truth();
        gt->mutable_version()->CopyFrom(osi_version);
        gt->mutable_timestamp()->CopyFrom(sv.timestamp());

        for (int obj = 0; obj < 5; ++obj) {
            auto* mo = gt->mutable_moving_object()->Add();
            mo->mutable_id()->set_value(100 + obj);
            mo->mutable_vehicle_classification()->set_type(osi3::MovingObject_VehicleClassification_Type_TYPE_SMALL_CAR);
            mo->mutable_base()->mutable_dimension()->set_length(4.5);
            mo->mutable_base()->mutable_dimension()->set_width(1.8);
            mo->mutable_base()->mutable_dimension()->set_height(1.4);
            mo->mutable_base()->mutable_position()->set_x(static_cast<double>(i) * 10.0 + static_cast<double>(obj));
            mo->mutable_base()->mutable_position()->set_y(static_cast<double>(obj) * 3.5);
            mo->mutable_base()->mutable_velocity()->set_x(30.0);
        }

        messages.push_back(std::move(sv));
    }
    return messages;
}

void PrintRow(const std::string& format, const std::string& operation, double seconds, double megabytes) {
    const double throughput = (seconds > 0.0) ? megabytes / seconds : 0.0;
    std::cout << std::left << std::setw(10) << format << std::setw(10) << operation << std::right << std::fixed << std::setprecision(3) << std::setw(10) << seconds << " s"
              << std::setw(12) << std::setprecision(1) << throughput << " MB/s" << std::endl;
}

// =============================================================================
// File-mode helpers
// =============================================================================

/// Write a single message through the binary writer, dispatching on type.
auto WriteTypedMessage(osi3::SingleChannelBinaryTraceFileWriter& writer, const osi3::ReadResult& result) -> bool {
    switch (result.message_type) {
        case osi3::ReaderTopLevelMessage::kGroundTruth:
            return writer.WriteMessage(dynamic_cast<osi3::GroundTruth&>(*result.message));
        case osi3::ReaderTopLevelMessage::kSensorData:
            return writer.WriteMessage(dynamic_cast<osi3::SensorData&>(*result.message));
        case osi3::ReaderTopLevelMessage::kSensorView:
            return writer.WriteMessage(dynamic_cast<osi3::SensorView&>(*result.message));
        case osi3::ReaderTopLevelMessage::kHostVehicleData:
            return writer.WriteMessage(dynamic_cast<osi3::HostVehicleData&>(*result.message));
        case osi3::ReaderTopLevelMessage::kTrafficCommand:
            return writer.WriteMessage(dynamic_cast<osi3::TrafficCommand&>(*result.message));
        case osi3::ReaderTopLevelMessage::kTrafficCommandUpdate:
            return writer.WriteMessage(dynamic_cast<osi3::TrafficCommandUpdate&>(*result.message));
        case osi3::ReaderTopLevelMessage::kTrafficUpdate:
            return writer.WriteMessage(dynamic_cast<osi3::TrafficUpdate&>(*result.message));
        case osi3::ReaderTopLevelMessage::kMotionRequest:
            return writer.WriteMessage(dynamic_cast<osi3::MotionRequest&>(*result.message));
        case osi3::ReaderTopLevelMessage::kStreamingUpdate:
            return writer.WriteMessage(dynamic_cast<osi3::StreamingUpdate&>(*result.message));
        default:
            return false;
    }
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

// =============================================================================
// Modes
// =============================================================================

int RunSynthetic(int num_messages) {
    std::cout << "Generating " << num_messages << " SensorView messages (5 objects each)..." << std::endl;
    const auto messages = GenerateMessages(num_messages);

    const auto single_size = messages.front().ByteSizeLong();
    const auto total_bytes = static_cast<double>(single_size) * static_cast<double>(num_messages);
    const auto total_mb = total_bytes / (1024.0 * 1024.0);
    std::cout << "Approx. payload: " << std::fixed << std::setprecision(1) << total_mb << " MB (" << single_size << " bytes/msg)\n" << std::endl;

    const auto tmp = std::filesystem::temp_directory_path();
    const auto mcap_path = tmp / "bench_sv_.mcap";
    const auto osi_path = tmp / "bench_sv_.osi";
    const auto txth_path = tmp / "bench_sv_.txth";

    Timer timer;

    // Header
    std::cout << std::left << std::setw(10) << "Format" << std::setw(10) << "Op" << std::right << std::setw(12) << "Time" << std::setw(14) << "Throughput" << std::endl;
    std::cout << std::string(46, '-') << std::endl;

    // ==================== MCAP ====================
    {
        osi3::MCAPTraceFileWriter writer;
        writer.Open(mcap_path);
        auto metadata = osi3::MCAPTraceFileWriter::PrepareRequiredFileMetadata();
        writer.AddFileMetadata(metadata);
        const std::string topic = "SensorView";
        writer.AddChannel(topic, osi3::SensorView::descriptor());

        timer.Start();
        for (const auto& msg : messages) {
            writer.WriteMessage(msg, topic);
        }
        writer.Close();
        PrintRow("MCAP", "write", timer.ElapsedSeconds(), total_mb);
    }
    {
        osi3::MCAPTraceFileReader reader;
        reader.Open(mcap_path);

        timer.Start();
        int count = 0;
        while (reader.HasNext()) {
            reader.ReadMessage();
            ++count;
        }
        reader.Close();
        PrintRow("MCAP", "read", timer.ElapsedSeconds(), total_mb);
    }

    // ==================== Binary .osi ====================
    {
        osi3::SingleChannelBinaryTraceFileWriter writer;
        writer.Open(osi_path);

        timer.Start();
        for (const auto& msg : messages) {
            writer.WriteMessage(msg);
        }
        writer.Close();
        PrintRow(".osi", "write", timer.ElapsedSeconds(), total_mb);
    }
    {
        osi3::SingleChannelBinaryTraceFileReader reader;
        reader.Open(osi_path, osi3::ReaderTopLevelMessage::kSensorView);

        timer.Start();
        int count = 0;
        while (reader.HasNext()) {
            reader.ReadMessage();
            ++count;
        }
        reader.Close();
        PrintRow(".osi", "read", timer.ElapsedSeconds(), total_mb);
    }

    // ==================== TXTH ====================
    {
        osi3::TXTHTraceFileWriter writer;
        writer.Open(txth_path);

        timer.Start();
        for (const auto& msg : messages) {
            writer.WriteMessage(msg);
        }
        writer.Close();
        PrintRow(".txth", "write", timer.ElapsedSeconds(), total_mb);
    }
    {
        osi3::TXTHTraceFileReader reader;
        reader.Open(txth_path, osi3::ReaderTopLevelMessage::kSensorView);

        timer.Start();
        int count = 0;
        while (reader.HasNext()) {
            reader.ReadMessage();
            ++count;
        }
        reader.Close();
        PrintRow(".txth", "read", timer.ElapsedSeconds(), total_mb);
    }

    // ==================== File sizes ====================
    std::cout << "\nFile sizes:" << std::endl;
    for (const auto& [label, path] : std::vector<std::pair<std::string, std::filesystem::path>>{{"MCAP", mcap_path}, {".osi", osi_path}, {".txth", txth_path}}) {
        const auto size = std::filesystem::file_size(path);
        std::cout << "  " << std::left << std::setw(8) << label << std::right << std::setw(12) << size << " bytes (" << std::fixed << std::setprecision(1)
                  << (static_cast<double>(size) / (1024.0 * 1024.0)) << " MB)" << std::endl;
    }

    // Cleanup
    std::filesystem::remove(mcap_path);
    std::filesystem::remove(osi_path);
    std::filesystem::remove(txth_path);

    std::cout << "\nDone. Temp files cleaned up." << std::endl;
    return 0;
}

int RunFile(const std::filesystem::path& input_path, osi3::ReaderTopLevelMessage message_type) {
    std::error_code ec;
    const auto file_size = std::filesystem::file_size(input_path, ec);
    if (ec) {
        std::cerr << "ERROR: Cannot stat file: " << input_path << " (" << ec.message() << ")\n";
        return 1;
    }

    std::cout << "File:    " << input_path << "\n";
    std::cout << "Size:    " << file_size << " bytes (" << static_cast<double>(file_size) / (1024.0 * 1024.0) << " MiB)\n";

    // Read benchmark
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

    // Write benchmark
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

}  // namespace

// =============================================================================
// CLI
// =============================================================================

void PrintUsage() {
    std::cerr << "Usage: benchmark <command> [options]\n"
              << "\n"
              << "Commands:\n"
              << "  synthetic [N]                    Generate N SensorView messages (default 1000)\n"
              << "                                   and benchmark all 3 formats (MCAP, .osi, .txth)\n"
              << "  file <path> [--type <Type>]      Benchmark read/write throughput on a real .osi file\n"
              << "                                   Type is auto-detected from filename or set via --type\n"
              << "\n"
              << "Options:\n"
              << "  -h, --help                       Show this help\n"
              << "\n"
              << "Valid message types for --type:";
    for (const auto& [name, _] : kValidTypes) {
        std::cerr << " " << name;
    }
    std::cerr << "\n";
}

auto main(const int argc, const char** argv) -> int {
    if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
        PrintUsage();
        return 1;
    }

    const std::string command = argv[1];

    if (command == "synthetic") {
        int num_messages = 1000;
        if (argc > 2) {
            num_messages = std::atoi(argv[2]);
            if (num_messages <= 0) {
                std::cerr << "ERROR: N must be a positive integer\n";
                return 1;
            }
        }
        return RunSynthetic(num_messages);
    }

    if (command == "file") {
        if (argc < 3) {
            std::cerr << "ERROR: 'file' command requires a path argument\n";
            PrintUsage();
            return 1;
        }

        const std::filesystem::path input_path = argv[2];
        auto message_type = osi3::ReaderTopLevelMessage::kUnknown;

        for (int i = 3; i < argc; ++i) {
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

        return RunFile(input_path, message_type);
    }

    std::cerr << "ERROR: Unknown command: " << command << "\n";
    PrintUsage();
    return 1;
}
