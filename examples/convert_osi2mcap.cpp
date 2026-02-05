//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//
/**
 * \file
 * \brief Convert single-channel binary OSI traces to MCAP.
 */

#include <osi-utilities/tracefile/reader/SingleChannelBinaryTraceFileReader.h>
#include <osi-utilities/tracefile/writer/MCAPTraceFileWriter.h>

#include <cstddef>
#include <filesystem>

#include "osi_groundtruth.pb.h"
#include "osi_hostvehicledata.pb.h"
#include "osi_motionrequest.pb.h"
#include "osi_sensordata.pb.h"
#include "osi_sensorview.pb.h"
#include "osi_streamingupdate.pb.h"
#include "osi_trafficcommand.pb.h"
#include "osi_trafficcommandupdate.pb.h"
#include "osi_trafficupdate.pb.h"

/** \brief Map compression enum values to string names. */
static const std::map<mcap::Compression, std::string> kCompressionEnumStringMap = {
    {mcap::Compression::None, "none"}, {mcap::Compression::Lz4, "lz4"}, {mcap::Compression::Zstd, "zstd"}};
/** \brief Map compression string names to enum values. */
static const std::map<std::string, mcap::Compression> kCompressionStringEnumMap = {
    {"none", mcap::Compression::None}, {"lz4", mcap::Compression::Lz4}, {"zstd", mcap::Compression::Zstd}};
/** \brief Map compression level enum values to string names. */
static const std::map<mcap::CompressionLevel, std::string> kCompressionLevelEnumStringMap = {
    {mcap::CompressionLevel::Fastest, "fastest"}, {mcap::CompressionLevel::Fast, "fast"}, {mcap::CompressionLevel::Default, "default"}};
/** \brief Map compression level string names to enum values. */
static const std::map<std::string, mcap::CompressionLevel> kCompressionLevelStringEnumMap = {
    {"fastest", mcap::CompressionLevel::Fastest}, {"fast", mcap::CompressionLevel::Fast}, {"default", mcap::CompressionLevel::Default}};

/**
 * \brief Extract an OSI timestamp from the input file name if present.
 * \param file_path Path to the input trace file.
 * \return Timestamp in ISO format or nullopt if none found.
 */
std::optional<std::string> ExtractTimestampFromFileName(const std::filesystem::path& file_path) {
    // Get first 16 characters which should be the timestamp
    auto possible_timestamp = file_path.filename().string().substr(0, 16);

    // Parse the timestamp using std::get_time
    tm tm_struct = {};
    std::istringstream string_stream(possible_timestamp);
    string_stream >> std::get_time(&tm_struct, "%Y%m%dT%H%M%SZ");

    // Return nullopt if parsing failed
    if (string_stream.fail()) {
        return std::nullopt;
    }

    // Format the timestamp in the by OSI specified mcap metadata format for the zero_time field
    std::ostringstream formatted_timestamp;
    formatted_timestamp << std::put_time(&tm_struct, "%Y-%m-%dT%H:%M:%SZ");

    std::cout << "Found timestamp for MCAP metadata 'zero_time' from tracefile name: " << formatted_timestamp.str() << std::endl;
    return formatted_timestamp.str();
}

/** \brief Map OSI message types to protobuf descriptors. */
const std::unordered_map<osi3::ReaderTopLevelMessage, const google::protobuf::Descriptor*> kMessageTypeToDescriptor = {
    {osi3::ReaderTopLevelMessage::kGroundTruth, osi3::GroundTruth::descriptor()},
    {osi3::ReaderTopLevelMessage::kSensorData, osi3::SensorData::descriptor()},
    {osi3::ReaderTopLevelMessage::kSensorView, osi3::SensorView::descriptor()},
    {osi3::ReaderTopLevelMessage::kHostVehicleData, osi3::HostVehicleData::descriptor()},
    {osi3::ReaderTopLevelMessage::kTrafficCommand, osi3::TrafficCommand::descriptor()},
    {osi3::ReaderTopLevelMessage::kTrafficCommandUpdate, osi3::TrafficCommandUpdate::descriptor()},
    {osi3::ReaderTopLevelMessage::kTrafficUpdate, osi3::TrafficUpdate::descriptor()},
    {osi3::ReaderTopLevelMessage::kMotionRequest, osi3::MotionRequest::descriptor()},
    {osi3::ReaderTopLevelMessage::kStreamingUpdate, osi3::StreamingUpdate::descriptor()},
};

/**
 * \brief Resolve a protobuf descriptor for the given OSI message type.
 * \param messageType OSI message type enum.
 * \return Descriptor for the corresponding protobuf type.
 * \throws std::runtime_error if the message type is unknown.
 */
const google::protobuf::Descriptor* GetDescriptorForMessageType(const osi3::ReaderTopLevelMessage messageType) {
    if (const auto iterator = kMessageTypeToDescriptor.find(messageType); iterator != kMessageTypeToDescriptor.end()) {
        return iterator->second;
    }
    throw std::runtime_error("Unknown message type");
}

/**
 * \brief Write a typed OSI message into the MCAP writer.
 * \tparam T Protobuf message type to write.
 * \param read_result Parsed message container.
 * \param writer MCAP writer instance.
 * \param topic Topic name to write to.
 */
template <typename T>
void WriteTypedMessage(const std::optional<osi3::ReadResult>& read_result, osi3::MCAPTraceFileWriter& writer, const std::string& topic) {
    writer.WriteMessage(*static_cast<T*>(read_result->message.get()), topic);
}

/**
 * \brief Convert a read OSI message into MCAP output.
 * \param read_result Parsed message container.
 * \param writer MCAP writer instance.
 */
void ProcessMessage(const std::optional<osi3::ReadResult>& read_result, osi3::MCAPTraceFileWriter& writer) {
    const std::string topic = "ConvertedTrace";
    switch (read_result->message_type) {
        case osi3::ReaderTopLevelMessage::kGroundTruth:
            WriteTypedMessage<osi3::GroundTruth>(read_result, writer, topic);
            break;
        case osi3::ReaderTopLevelMessage::kSensorData:
            WriteTypedMessage<osi3::SensorData>(read_result, writer, topic);
            break;
        case osi3::ReaderTopLevelMessage::kSensorView:
            WriteTypedMessage<osi3::SensorView>(read_result, writer, topic);
            break;
        case osi3::ReaderTopLevelMessage::kHostVehicleData:
            WriteTypedMessage<osi3::HostVehicleData>(read_result, writer, topic);
            break;
        case osi3::ReaderTopLevelMessage::kTrafficCommand:
            WriteTypedMessage<osi3::TrafficCommand>(read_result, writer, topic);
            break;
        case osi3::ReaderTopLevelMessage::kTrafficCommandUpdate:
            WriteTypedMessage<osi3::TrafficCommandUpdate>(read_result, writer, topic);
            break;
        case osi3::ReaderTopLevelMessage::kTrafficUpdate:
            WriteTypedMessage<osi3::TrafficUpdate>(read_result, writer, topic);
            break;
        case osi3::ReaderTopLevelMessage::kMotionRequest:
            WriteTypedMessage<osi3::MotionRequest>(read_result, writer, topic);
            break;
        case osi3::ReaderTopLevelMessage::kStreamingUpdate:
            WriteTypedMessage<osi3::StreamingUpdate>(read_result, writer, topic);
            break;
        default:
            std::cout << "Could not determine type of message" << std::endl;
            break;
    }
}

/**
 * \brief Parsed command-line options for this converter.
 */
struct ProgramOptions {
    std::filesystem::path input_file_path;                                 /**< Input `.osi` trace file. */
    std::filesystem::path output_file_path;                                /**< Output `.mcap` file. */
    osi3::ReaderTopLevelMessage message_type = osi3::ReaderTopLevelMessage::kUnknown; /**< Optional message type hint. */
    size_t chunk_size = static_cast<size_t>(1024 * 768);                   /**< MCAP chunk size in bytes. */
    mcap::Compression compression = mcap::Compression::Zstd;               /**< MCAP compression type. */
    mcap::CompressionLevel compression_level = mcap::CompressionLevel::Default; /**< MCAP compression level. */
};

/** \brief Map CLI message type names to OSI enum values. */
const std::unordered_map<std::string, osi3::ReaderTopLevelMessage> kValidTypes = {
    {"GroundTruth", osi3::ReaderTopLevelMessage::kGroundTruth},        {"SensorData", osi3::ReaderTopLevelMessage::kSensorData},
    {"SensorView", osi3::ReaderTopLevelMessage::kSensorView},          {"HostVehicleData", osi3::ReaderTopLevelMessage::kHostVehicleData},
    {"TrafficCommand", osi3::ReaderTopLevelMessage::kTrafficCommand},  {"TrafficCommandUpdate", osi3::ReaderTopLevelMessage::kTrafficCommandUpdate},
    {"TrafficUpdate", osi3::ReaderTopLevelMessage::kTrafficUpdate},    {"MotionRequest", osi3::ReaderTopLevelMessage::kMotionRequest},
    {"StreamingUpdate", osi3::ReaderTopLevelMessage::kStreamingUpdate}};

/**
 * \brief Print CLI usage information.
 */
void printHelp() {
    std::cout << "Usage: convert_osi2mcap <input_file> <output_file> [--input-type <message_type>]\n\n"
              << "Arguments:\n"
              << "  input_file              Path to the input OSI trace file\n"
              << "  output_file             Path to the output MCAP file\n"
              << "  --input-type <message_type>   Optional: Specify input message type if not stated in filename\n\n"
              << "\tValid message types:\n";
    for (const auto& [type, _] : kValidTypes) {
        std::cout << "\t\t" << type << "\n";
    }
    std::cout << "  --chunk_size <size>           Optional: Chunk size in bytes (default: 786432)\n"
              << "  --compression <type>          Optional: Compression type (none, lz4, zstd) (default: zstd)\n"
              << "  --compression_level <type>    Optional: Compression level (fastest, fast, default) (default: default)\n\n";
}

/**
 * \brief Parse a compression type string into an MCAP enum value.
 * \param compression_str Compression type name.
 * \return MCAP compression enum.
 */
mcap::Compression parseCompressionType(const std::string& compression_str) {
    std::string lower_compression_str = compression_str;
    std::transform(lower_compression_str.begin(), lower_compression_str.end(), lower_compression_str.begin(), ::tolower);
    return kCompressionStringEnumMap.at(lower_compression_str);
}

/**
 * \brief Parse a compression level string into an MCAP enum value.
 * \param level_str Compression level name.
 * \return MCAP compression level enum.
 */
mcap::CompressionLevel parseCompressionLevel(const std::string& level_str) {
    std::string lower_level = level_str;
    std::transform(lower_level.begin(), lower_level.end(), lower_level.begin(), ::tolower);
    return kCompressionLevelStringEnumMap.at(lower_level);
}

/**
 * \brief Parse CLI arguments into ProgramOptions.
 * \param argc Argument count.
 * \param argv Argument vector.
 * \return Parsed options or nullopt on error/help.
 */
std::optional<ProgramOptions> parseArgs(const int argc, const char** argv) {
    if (argc < 3 || std::string(argv[1]) == "--help" || std::string(argv[1]) == "-h") {
        printHelp();
        return std::nullopt;
    }

    ProgramOptions options;
    options.input_file_path = argv[1];
    options.output_file_path = argv[2];
    options.message_type = osi3::ReaderTopLevelMessage::kUnknown;

    for (int i = 3; i < argc; i++) {
        const std::string arg = argv[i];
        try {
            if (arg == "--input-type" && i + 1 < argc) {
                const std::string type_str = argv[++i];
                auto types_it = kValidTypes.find(type_str);
                if (types_it == kValidTypes.end()) {
                    throw std::invalid_argument("Invalid message type: " + type_str);
                }
                options.message_type = types_it->second;
            } else if (arg == "--chunk_size" && i + 1 < argc) {
                options.chunk_size = std::stoull(argv[++i]);
            } else if (arg == "--compression" && i + 1 < argc) {
                options.compression = parseCompressionType(argv[++i]);
            } else if (arg == "--compression_level" && i + 1 < argc) {
                options.compression_level = parseCompressionLevel(argv[++i]);
            } else {
                throw std::invalid_argument("Invalid argument: " + arg);
            }
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n\n";
            printHelp();
            return std::nullopt;
        }
    }
    return options;
}

/**
 * \brief Entry point for the `.osi` to `.mcap` converter.
 */
int main(const int argc, const char** argv) {
    const auto options = parseArgs(argc, argv);
    if (!options) {
        return 1;
    }

    std::cout << "Input file:  " << options->input_file_path << std::endl;
    std::cout << "Output file: " << options->output_file_path << std::endl;

    // create single channel trace file (.osi) reader
    auto trace_file_reader = osi3::SingleChannelBinaryTraceFileReader();
    if (!trace_file_reader.Open(options->input_file_path, options->message_type)) {
        std::cerr << "ERROR: Could not open input file " << options->input_file_path << std::endl;
        return 1;
    }

    // create MCAP writer
    auto trace_file_writer = osi3::MCAPTraceFileWriter();

    // set MCAP options
    mcap::McapWriterOptions mcap_options("osi2mcap");
    mcap_options.chunkSize = options->chunk_size;
    mcap_options.compression = options->compression;
    mcap_options.compressionLevel = options->compression_level;

    // print information about chunk size and compression
    std::cout << "MCAP options:" << "\n";
    std::cout << "\tchunk size: " << mcap_options.chunkSize << "\n";

    std::cout << "\tcompression: " << kCompressionEnumStringMap.at(mcap_options.compression) << "\n";
    std::cout << "\tcompression level: " << kCompressionLevelEnumStringMap.at(mcap_options.compressionLevel) << "\n";

    // open output file with options
    if (!trace_file_writer.Open(options->output_file_path, mcap_options)) {
        std::cerr << "ERROR: Could not open output file " << options->output_file_path << std::endl;
        return 1;
    }

    // add required and optional metadata to the net.asam.osi.trace metadata record
    auto net_asam_osi_trace_metadata = osi3::MCAPTraceFileWriter::PrepareRequiredFileMetadata();
    // Add optional metadata to the net.asam.osi.trace metadata record, as recommended by the OSI specification.
    net_asam_osi_trace_metadata.metadata["description"] = "Converted from " + options->input_file_path.string();  // optional field
    net_asam_osi_trace_metadata.metadata["creation_time"] = osi3::MCAPTraceFileWriter::GetCurrentTimeAsString();  // optional field
    if (const auto timestamp_from_osi_file = ExtractTimestampFromFileName(options->input_file_path)) {
        net_asam_osi_trace_metadata.metadata["zero_time"] = timestamp_from_osi_file.value();  // optional field
    }
    if (!trace_file_writer.AddFileMetadata(net_asam_osi_trace_metadata)) {
        std::cerr << "Failed to add required metadata to trace_file." << std::endl;
        exit(1);
    }

    const google::protobuf::Descriptor* descriptor = nullptr;
    try {
        descriptor = GetDescriptorForMessageType(trace_file_reader.GetMessageType());
    } catch (const std::runtime_error& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
    if (!descriptor) {
        std::cerr << "ERROR: Failed to get message descriptor" << std::endl;
        return 1;
    }

    trace_file_writer.AddChannel("ConvertedTrace", descriptor);

    while (trace_file_reader.HasNext()) {
        auto reading_result = trace_file_reader.ReadMessage();
        ProcessMessage(reading_result, trace_file_writer);
    }
    std::cout << "Finished single channel binary to mcap converter" << std::endl;
    return 0;
}
