//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//
/**
 * \file
 * \brief Read OSI MCAP trace files — metadata inspection, topic filtering, and message iteration.
 *
 * Demonstrates:
 * - MCAPTraceFileReader::GetAvailableTopics()
 * - MCAPTraceFileReader::GetFileMetadata()
 * - MCAPTraceFileReader::GetChannelMetadata()
 * - MCAPTraceFileReader::GetMessageTypeForTopic()
 * - MCAPTraceFileReader::SetTopics() for topic filtering
 * - TraceFileReaderFactory::createReader() for format auto-detection
 */

#include <osi-utilities/tracefile/Reader.h>
#include <osi-utilities/tracefile/reader/MCAPTraceFileReader.h>

#include <filesystem>
#include <optional>
#include <unordered_set>

#include "osi_groundtruth.pb.h"
#include "osi_hostvehicledata.pb.h"
#include "osi_motionrequest.pb.h"
#include "osi_sensordata.pb.h"
#include "osi_sensorview.pb.h"
#include "osi_streamingupdate.pb.h"
#include "osi_trafficcommand.pb.h"
#include "osi_trafficcommandupdate.pb.h"
#include "osi_trafficupdate.pb.h"

namespace {

/** \brief Readable names for ReaderTopLevelMessage enum values. */
const std::unordered_map<osi3::ReaderTopLevelMessage, std::string> kMessageTypeNames = {
    {osi3::ReaderTopLevelMessage::kGroundTruth, "GroundTruth"},
    {osi3::ReaderTopLevelMessage::kSensorData, "SensorData"},
    {osi3::ReaderTopLevelMessage::kSensorView, "SensorView"},
    {osi3::ReaderTopLevelMessage::kSensorViewConfiguration, "SensorViewConfiguration"},
    {osi3::ReaderTopLevelMessage::kHostVehicleData, "HostVehicleData"},
    {osi3::ReaderTopLevelMessage::kTrafficCommand, "TrafficCommand"},
    {osi3::ReaderTopLevelMessage::kTrafficCommandUpdate, "TrafficCommandUpdate"},
    {osi3::ReaderTopLevelMessage::kTrafficUpdate, "TrafficUpdate"},
    {osi3::ReaderTopLevelMessage::kMotionRequest, "MotionRequest"},
    {osi3::ReaderTopLevelMessage::kStreamingUpdate, "StreamingUpdate"},
    {osi3::ReaderTopLevelMessage::kUnknown, "Unknown"},
};

/**
 * \brief Print the timestamp of a decoded OSI message.
 * \param reading_result Read result containing the decoded message.
 */
void PrintMessage(const osi3::ReadResult& reading_result) {
    // Use protobuf reflection to extract the timestamp from any OSI message type,
    // avoiding a verbose switch-case over all 10+ message types.
    double timestamp = 0.0;
    const auto* descriptor = reading_result.message->GetDescriptor();
    const auto* reflection = reading_result.message->GetReflection();
    const auto* ts_field = descriptor->FindFieldByName("timestamp");
    if (ts_field != nullptr && reflection->HasField(*reading_result.message, ts_field)) {
        const auto& ts_msg = reflection->GetMessage(*reading_result.message, ts_field);
        const auto* ts_desc = ts_msg.GetDescriptor();
        const auto seconds = ts_msg.GetReflection()->GetInt64(ts_msg, ts_desc->FindFieldByName("seconds"));
        const auto nanos = ts_msg.GetReflection()->GetUInt32(ts_msg, ts_desc->FindFieldByName("nanos"));
        timestamp = static_cast<double>(seconds) + static_cast<double>(nanos) / 1e9;
    }
    const auto& type_name = kMessageTypeNames.at(reading_result.message_type);
    std::cout << "  Type: osi3." << type_name << "  Timestamp: " << timestamp;
    if (!reading_result.channel_name.empty()) {
        std::cout << "  Channel: " << reading_result.channel_name;
    }
    std::cout << "\n";
}

/**
 * \brief Parsed command-line options for this example.
 */
struct ProgramOptions {
    std::filesystem::path file_path;        /**< Input trace file path. */
    std::unordered_set<std::string> topics; /**< Optional topic filter (empty = all). */
    bool use_factory = false;               /**< Use TraceFileReaderFactory instead of direct instantiation. */
};

/**
 * \brief Print CLI usage information.
 */
void PrintHelp() {
    std::cout << "Usage: example_mcap_reader <input_file> [options]\n\n"
              << "Arguments:\n"
              << "  input_file              Path to the input OSI MCAP trace file\n\n"
              << "Options:\n"
              << "  --topic <name>          Filter to specific topic(s) (repeatable)\n"
              << "  --factory               Use TraceFileReaderFactory for format auto-detection\n"
              << "  -h, --help              Show this help\n";
}

/**
 * \brief Parse CLI arguments into ProgramOptions.
 */
auto ParseArgs(const int argc, const char** argv) -> std::optional<ProgramOptions> {
    if (argc < 2) {
        PrintHelp();
        return std::nullopt;
    }

    ProgramOptions options{};

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            PrintHelp();
            return std::nullopt;
        }
        if (arg == "--topic" && i + 1 < argc) {
            options.topics.insert(argv[++i]);
        } else if (arg == "--factory") {
            options.use_factory = true;
        } else if (options.file_path.empty()) {
            options.file_path = arg;
        } else {
            std::cerr << "Error: Unexpected argument '" << arg << "'\n\n";
            PrintHelp();
            return std::nullopt;
        }
    }

    if (options.file_path.empty()) {
        PrintHelp();
        return std::nullopt;
    }

    return options;
}

/**
 * \brief Print available topics and their metadata.
 */
void PrintTopicsAndMetadata(osi3::MCAPTraceFileReader& reader) {
    const auto topics = reader.GetAvailableTopics();
    std::cout << "\nAvailable topics (" << topics.size() << "):\n";
    for (const auto& topic : topics) {
        const auto msg_type = reader.GetMessageTypeForTopic(topic);
        const auto& type_name = msg_type ? kMessageTypeNames.at(*msg_type) : std::string("non-OSI");
        std::cout << "  - " << topic << " (" << type_name << ")\n";

        const auto channel_meta = reader.GetChannelMetadata(topic);
        if (channel_meta && !channel_meta->empty()) {
            for (const auto& [key, value] : *channel_meta) {
                std::cout << "      " << key << ": " << value << "\n";
            }
        }
    }

    const auto file_metadata = reader.GetFileMetadata();
    if (!file_metadata.empty()) {
        std::cout << "\nFile metadata:\n";
        for (const auto& [name, entries] : file_metadata) {
            std::cout << "  [" << name << "]\n";
            for (const auto& [key, value] : entries) {
                std::cout << "    " << key << ": " << value << "\n";
            }
        }
    }
}

/**
 * \brief Run factory-mode demo: use TraceFileReaderFactory for format auto-detection.
 */
auto RunFactoryMode(const std::filesystem::path& file_path) -> int {
    std::cout << "\n[Using TraceFileReaderFactory for format auto-detection]\n";
    auto generic_reader = osi3::TraceFileReaderFactory::createReader(file_path);
    generic_reader->Open(file_path);

    while (generic_reader->HasNext()) {
        const auto result = generic_reader->ReadMessage();
        if (result) {
            PrintMessage(*result);
        }
    }
    generic_reader->Close();
    std::cout << "Finished MCAP Reader example (factory mode)\n";
    return 0;
}

/**
 * \brief Read and print all messages from the trace file.
 */
void ReadMessages(osi3::MCAPTraceFileReader& reader) {
    std::cout << "\nMessages:\n";
    int count = 0;
    while (reader.HasNext()) {
        const auto reading_result = reader.ReadMessage();
        if (!reading_result) {
            std::cerr << "Error reading message." << std::endl;
            continue;
        }
        PrintMessage(*reading_result);
        ++count;
    }
    std::cout << "Total: " << count << " messages read\n";
}

}  // namespace

/**
 * \brief Entry point for the MCAP reader example.
 */
auto main(const int argc, const char** argv) -> int {
    const auto options = ParseArgs(argc, argv);
    if (!options) {
        return 1;
    }

    std::cout << "Starting MCAP Reader example:\n";
    std::cout << "Reading trace file from " << options->file_path.string() << "\n";

    if (options->use_factory) {
        return RunFactoryMode(options->file_path);
    }

    // Direct MCAPTraceFileReader for full feature access
    auto trace_file_reader = osi3::MCAPTraceFileReader();
    if (!trace_file_reader.Open(options->file_path)) {
        std::cerr << "Error: Could not open file '" << options->file_path.string() << "'\n";
        return 1;
    }

    PrintTopicsAndMetadata(trace_file_reader);

    // Apply topic filtering if requested
    if (!options->topics.empty()) {
        std::cout << "\nFiltering to topic(s):";
        for (const auto& t : options->topics) {
            std::cout << " " << t;
        }
        std::cout << "\n";
        trace_file_reader.SetTopics(options->topics);
    }

    ReadMessages(trace_file_reader);

    trace_file_reader.Close();
    std::cout << "Finished MCAP Reader example\n";
    return 0;
}
