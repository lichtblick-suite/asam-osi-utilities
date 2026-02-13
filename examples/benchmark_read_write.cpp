//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//
/**
 * \file
 * \brief Benchmark read/write throughput for all three trace file formats.
 *
 * Usage: benchmark_read_write [N]
 *   N = number of SensorView messages to write/read (default 1000)
 */

#include <osi-utilities/tracefile/Reader.h>
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
#include <vector>

#include "osi_sensorview.pb.h"
#include "osi_version.pb.h"

namespace {

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

void PrintRow(const std::string& format, const std::string& operation, double seconds, double megabytes) {
    const double throughput = (seconds > 0.0) ? megabytes / seconds : 0.0;
    std::cout << std::left << std::setw(10) << format << std::setw(10) << operation << std::right << std::fixed << std::setprecision(3) << std::setw(10) << seconds << " s"
              << std::setw(12) << std::setprecision(1) << throughput << " MB/s" << std::endl;
}

}  // namespace

auto main(int argc, const char** argv) -> int {
    const int num_messages = (argc > 1) ? std::atoi(argv[1]) : 1000;
    if (num_messages <= 0) {
        std::cerr << "Usage: benchmark_read_write [N]  (N > 0)\n";
        return 1;
    }

    std::cout << "Generating " << num_messages << " SensorView messages (5 objects each)..." << std::endl;
    const auto messages = GenerateMessages(num_messages);

    // Estimate serialized size
    const auto single_size = messages.front().ByteSizeLong();
    const auto total_bytes = static_cast<double>(single_size) * static_cast<double>(num_messages);
    const auto total_mb = total_bytes / (1024.0 * 1024.0);
    std::cout << "Approx. payload: " << std::fixed << std::setprecision(1) << total_mb << " MB (" << single_size << " bytes/msg)\n" << std::endl;

    // Paths
    const auto tmp = std::filesystem::temp_directory_path();
    const auto mcap_path = tmp / "bench_sv_.mcap";
    const auto osi_path = tmp / "bench_sv_.osi";
    const auto txth_path = tmp / "bench_sv_.txth";

    Timer timer;

    // --- Header ---
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

    // ==================== Cleanup ====================
    std::filesystem::remove(mcap_path);
    std::filesystem::remove(osi_path);
    std::filesystem::remove(txth_path);

    std::cout << "\nDone. Temp files cleaned up." << std::endl;
    return 0;
}
