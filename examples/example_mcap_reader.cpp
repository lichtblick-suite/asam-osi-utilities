//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//
/**
 * \file
 * \brief Read OSI MCAP trace files and print message timestamps.
 */

#include <osi-utilities/tracefile/TraceFileConfig.h>
#include <osi-utilities/tracefile/reader/MCAPTraceFileReader.h>

#include <filesystem>
#include <optional>

#include "osi_groundtruth.pb.h"
#include "osi_hostvehicledata.pb.h"
#include "osi_motionrequest.pb.h"
#include "osi_sensordata.pb.h"
#include "osi_sensorview.pb.h"
#include "osi_streamingupdate.pb.h"
#include "osi_trafficcommand.pb.h"
#include "osi_trafficcommandupdate.pb.h"
#include "osi_trafficupdate.pb.h"

/**
 * \brief Print the timestamp of a decoded OSI message.
 * \tparam T Protobuf message pointer type.
 * \param msg Parsed OSI message instance.
 */
template <typename T>
void PrintTimestamp(T msg) {
    auto timestamp = msg->timestamp().seconds() + msg->timestamp().nanos() / static_cast<double>(osi3::tracefile::config::kNanosecondsPerSecond);
    std::cout << "Type: " << msg->descriptor()->full_name() << " Timestamp " << timestamp << "\n";
}

/**
 * \brief Print CLI usage information.
 */
void printHelp() {
    std::cout << "Usage: example_mcap_reader <input_file> \n\n"
              << "Arguments:\n"
              << "  input_file              Path to the input OSI MCAP trace file\n";
}

/**
 * \brief Parse CLI arguments and return the input file path.
 * \param argc Argument count.
 * \param argv Argument vector.
 * \return Path to the input file or empty path on error/help.
 */
auto parseArgs(const int argc, const char** argv) -> std::filesystem::path {
    if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
        printHelp();
        return "";
    }

    return std::filesystem::path{argv[1]};
}

/**
 * \brief Entry point for the MCAP reader example.
 */
auto main(const int argc, const char** argv) -> int {
    std::cout << "Starting MCAP Reader example:" << std::endl;

    const auto trace_file_path = parseArgs(argc, argv);
    if (trace_file_path.empty()) {
        return 1;
    }

    // Create reader and open file
    auto trace_file_reader = osi3::MCAPTraceFileReader();
    std::cout << "Reading trace file from " << trace_file_path << std::endl;
    trace_file_reader.Open(trace_file_path);

    // Read messages in a loop until no more messages are available
    while (trace_file_reader.HasNext()) {
        const auto reading_result = trace_file_reader.ReadMessage();
        if (!reading_result) {
            std::cerr << "Error reading message." << std::endl;
            continue;
        }

        // we need to cast the pointer during runtime to allow multiple different trace files to be read
        switch (reading_result->message_type) {
            case osi3::ReaderTopLevelMessage::kGroundTruth: {
                auto* const ground_truth = dynamic_cast<osi3::GroundTruth*>(reading_result->message.get());
                PrintTimestamp(ground_truth);
                break;
            }
            case osi3::ReaderTopLevelMessage::kSensorData: {
                auto* const sensor_data = dynamic_cast<osi3::SensorData*>(reading_result->message.get());
                PrintTimestamp(sensor_data);
                break;
            }
            case osi3::ReaderTopLevelMessage::kSensorView: {
                auto* const sensor_view = dynamic_cast<osi3::SensorView*>(reading_result->message.get());
                PrintTimestamp(sensor_view);
                break;
            }
            case osi3::ReaderTopLevelMessage::kHostVehicleData: {
                auto* const host_vehicle_data = dynamic_cast<osi3::HostVehicleData*>(reading_result->message.get());
                PrintTimestamp(host_vehicle_data);
                break;
            }
            case osi3::ReaderTopLevelMessage::kTrafficCommand: {
                auto* const traffic_command = dynamic_cast<osi3::TrafficCommand*>(reading_result->message.get());
                PrintTimestamp(traffic_command);
                break;
            }
            case osi3::ReaderTopLevelMessage::kTrafficCommandUpdate: {
                auto* const traffic_command_update = dynamic_cast<osi3::TrafficCommandUpdate*>(reading_result->message.get());
                PrintTimestamp(traffic_command_update);
                break;
            }
            case osi3::ReaderTopLevelMessage::kTrafficUpdate: {
                auto* const traffic_update = dynamic_cast<osi3::TrafficUpdate*>(reading_result->message.get());
                PrintTimestamp(traffic_update);
                break;
            }
            case osi3::ReaderTopLevelMessage::kMotionRequest: {
                auto* const motion_request = dynamic_cast<osi3::MotionRequest*>(reading_result->message.get());
                PrintTimestamp(motion_request);
                break;
            }
            case osi3::ReaderTopLevelMessage::kStreamingUpdate: {
                auto* const streaming_update = dynamic_cast<osi3::StreamingUpdate*>(reading_result->message.get());
                PrintTimestamp(streaming_update);
                break;
            }
            default: {
                std::cout << "Could not determine type of message" << std::endl;
                break;
            }
        }
    }

    trace_file_reader.Close();

    std::cout << "Finished MCAP Reader example" << std::endl;
    return 0;
}
