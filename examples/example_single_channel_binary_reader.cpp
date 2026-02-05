//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//
/**
 * \file
 * \brief Read single-channel binary OSI `.osi` trace files.
 */

#include <osi-utilities/tracefile/reader/SingleChannelBinaryTraceFileReader.h>

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
    auto timestamp = msg->timestamp().seconds() + msg->timestamp().nanos() / 1000000000.0;
    std::cout << "Type: " << msg->descriptor()->full_name() << " Timestamp " << timestamp << "\n";
}

/**
 * \brief Dispatch the decoded message to the correct type and print its timestamp.
 * \param reading_result Read result containing the decoded message.
 */
void CastMsgAndPrintTimestamp(const osi3::ReadResult& reading_result) {
    // while reading_result->message_type already contains the correct deserialized OSI message type,
    // we need to cast the pointer during runtime to allow multiple different trace files to be read
    switch (reading_result.message_type) {
        case osi3::ReaderTopLevelMessage::kGroundTruth: {
            auto* const ground_truth = dynamic_cast<osi3::GroundTruth*>(reading_result.message.get());
            PrintTimestamp(ground_truth);
            break;
        }
        case osi3::ReaderTopLevelMessage::kSensorData: {
            auto* const sensor_data = dynamic_cast<osi3::SensorData*>(reading_result.message.get());
            PrintTimestamp(sensor_data);
            break;
        }
        case osi3::ReaderTopLevelMessage::kSensorView: {
            auto* const sensor_view = dynamic_cast<osi3::SensorView*>(reading_result.message.get());
            PrintTimestamp(sensor_view);
            break;
        }
        case osi3::ReaderTopLevelMessage::kHostVehicleData: {
            auto* const host_vehicle_data = dynamic_cast<osi3::HostVehicleData*>(reading_result.message.get());
            PrintTimestamp(host_vehicle_data);
            break;
        }
        case osi3::ReaderTopLevelMessage::kTrafficCommand: {
            auto* const traffic_command = dynamic_cast<osi3::TrafficCommand*>(reading_result.message.get());
            PrintTimestamp(traffic_command);
            break;
        }
        case osi3::ReaderTopLevelMessage::kTrafficCommandUpdate: {
            auto* const traffic_command_update = dynamic_cast<osi3::TrafficCommandUpdate*>(reading_result.message.get());
            PrintTimestamp(traffic_command_update);
            break;
        }
        case osi3::ReaderTopLevelMessage::kTrafficUpdate: {
            auto* const traffic_update = dynamic_cast<osi3::TrafficUpdate*>(reading_result.message.get());
            PrintTimestamp(traffic_update);
            break;
        }
        case osi3::ReaderTopLevelMessage::kMotionRequest: {
            auto* const motion_request = dynamic_cast<osi3::MotionRequest*>(reading_result.message.get());
            PrintTimestamp(motion_request);
            break;
        }
        case osi3::ReaderTopLevelMessage::kStreamingUpdate: {
            auto* const streaming_update = dynamic_cast<osi3::StreamingUpdate*>(reading_result.message.get());
            PrintTimestamp(streaming_update);
            break;
        }
        default: {
            std::cout << "Could not determine type of message" << std::endl;
            break;
        }
    }
}

/**
 * \brief Parsed command-line options for this example.
 */
struct ProgramOptions {
    std::filesystem::path file_path;                                                  /**< Input trace file path. */
    osi3::ReaderTopLevelMessage message_type = osi3::ReaderTopLevelMessage::kUnknown; /**< Optional message type hint. */
};

/** \brief Map CLI message type names to OSI enum values. */
const std::unordered_map<std::string, osi3::ReaderTopLevelMessage> kValidTypes = {{"GroundTruth", osi3::ReaderTopLevelMessage::kGroundTruth},
                                                                                  {"SensorData", osi3::ReaderTopLevelMessage::kSensorData},
                                                                                  {"SensorView", osi3::ReaderTopLevelMessage::kSensorView},
                                                                                  {"SensorViewConfiguration", osi3::ReaderTopLevelMessage::kSensorViewConfiguration},
                                                                                  {"HostVehicleData", osi3::ReaderTopLevelMessage::kHostVehicleData},
                                                                                  {"TrafficCommand", osi3::ReaderTopLevelMessage::kTrafficCommand},
                                                                                  {"TrafficCommandUpdate", osi3::ReaderTopLevelMessage::kTrafficCommandUpdate},
                                                                                  {"TrafficUpdate", osi3::ReaderTopLevelMessage::kTrafficUpdate},
                                                                                  {"MotionRequest", osi3::ReaderTopLevelMessage::kMotionRequest},
                                                                                  {"StreamingUpdate", osi3::ReaderTopLevelMessage::kStreamingUpdate}};

/**
 * \brief Print CLI usage information.
 */
void printHelp() {
    std::cout << "Usage: example_single_channel_binary_reader <file_path> [--type <message_type>]\n\n"
              << "Arguments:\n"
              << "  file_path               Path to the OSI trace file\n"
              << "  --type <message_type>   Optional: Specify messages type if not stated in filename\n\n"
              << "Valid message types:\n";
    for (const auto& [type, _] : kValidTypes) {
        std::cout << "  " << type << "\n";
    }
}

/**
 * \brief Parse CLI arguments into ProgramOptions.
 * \param argc Argument count.
 * \param argv Argument vector.
 * \return Parsed options or nullopt on error/help.
 */
auto parseArgs(const int argc, const char** argv) -> std::optional<ProgramOptions> {
    if (argc < 2 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
        printHelp();
        return std::nullopt;
    }

    ProgramOptions options{};
    options.file_path = argv[1];
    options.message_type = osi3::ReaderTopLevelMessage::kUnknown;

    for (int i = 2; i < argc; i++) {
        if (const std::string arg = argv[i]; arg == "--type" && i + 1 < argc) {
            const std::string type_str = argv[++i];
            auto types_it = kValidTypes.find(type_str);
            if (types_it == kValidTypes.end()) {
                std::cerr << "Error: Invalid message type '" << type_str << "'\n\n";
                printHelp();
                return std::nullopt;
            }
            options.message_type = types_it->second;
        }
    }

    return options;
}

/**
 * \brief Entry point for the single-channel binary reader example.
 */
auto main(const int argc, const char** argv) -> int {
    // Parse program options
    const auto options = parseArgs(argc, argv);
    if (!options) {
        return 1;
    }

    std::cout << "Starting single channel binary reader example:" << std::endl;

    // Open the trace file
    // downstream functions of Open will guess the message type from the filename (options->message_type has the unknown value)
    // or use the provided cli argument value for the message type
    auto trace_file_reader = osi3::SingleChannelBinaryTraceFileReader();
    if (!trace_file_reader.Open(options->file_path, options->message_type)) {
        std::cerr << "Error: Could not open file '" << options->file_path << "'\n\n";
        return 1;
    }
    std::cout << "Opened file " << options->file_path << std::endl;

    // Continuously read messages from file
    while (trace_file_reader.HasNext()) {
        std::cout << "reading next message\n";
        const auto reading_result = trace_file_reader.ReadMessage();
        if (!reading_result) {
            std::cerr << "Error reading message." << std::endl;
            continue;
        }
        CastMsgAndPrintTimestamp(*reading_result);
    }

    std::cout << "Finished single channel binary reader example" << std::endl;
    return 0;
}
