//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//
/**
 * \file
 * \brief Optimize MCAP trace file compression for OSI data.
 *
 * This tool re-encodes an existing MCAP file with optimal compression settings,
 * targeting maximum space savings for OSI (Open Simulation Interface) protobuf data.
 *
 * \section why_optimize WHY OPTIMIZE MCAP COMPRESSION FOR OSI DATA?
 *
 * OSI trace files store serialized protobuf messages describing simulation scenarios:
 * ground truth, sensor views, traffic updates, etc. These messages exhibit *high
 * structural redundancy*:
 *
 *   - Repeated protobuf field tags and wire types across messages
 *   - Nearly identical timestamps incrementing by small deltas
 *   - Object lists (vehicles, pedestrians) with stable IDs and slowly-changing fields
 *   - Large floating-point arrays (positions, velocities) with correlated values
 *
 * This redundancy makes OSI data an excellent candidate for dictionary-based
 * compression algorithms like Zstandard (zstd), especially at higher levels where
 * the compressor invests more CPU cycles to find longer matches.
 *
 * \section key_concepts KEY CONCEPTS
 *
 * \subsection chunk_size MCAP Chunk Size
 *
 * MCAP groups messages into "chunks" before compressing them (see MCAP spec:
 * https://mcap.dev/spec#chunk-op0x06). Larger chunks give the compressor more
 * context to find redundant patterns, but increase memory usage for readers.
 *
 *   - Default in this library: 16 MiB (see TraceFileConfig.h kDefaultChunkSize)
 *   - This tool uses 32 MiB (kMaxChunkSize) for maximum compression context
 *   - Trade-off: readers must decompress an entire chunk to seek within it,
 *     so large chunks can hurt random-access performance in viewers like
 *     Lichtblick (https://github.com/lichtblick-suite/lichtblick)
 *
 * \subsection zstd_levels Zstandard Compression Levels
 *
 * Zstd supports levels 1-22 (https://facebook.github.io/zstd/):
 *
 *   - Level 1-3:   Fast compression, moderate ratio (mcap C++ default: 1 via CompressionLevel::Default)
 *   - Level 4-9:   Better ratio, still practical for interactive workloads
 *   - Level 10-18: High compression, suitable for batch/offline processing
 *   - Level 19:    Best *practical* level — highest ratio before extreme RAM usage
 *   - Level 20-22: Ultra — requires --ultra flag, exponential memory cost with diminishing returns
 *
 * The MCAP C++ library maps CompressionLevel enum to zstd levels:
 *   - Fastest → -5, Fast → -3, Default → 1, Slow → 5, Slowest → 19
 *
 * For OSI archival this tool uses CompressionLevel::Slowest (= zstd level 19,
 * matching kHighZstdCompressionLevel).
 *
 * \subsection lz4_vs_zstd LZ4 vs Zstandard
 *
 * LZ4 (https://lz4.github.io/lz4/) prioritizes speed over ratio. Even at its
 * maximum level, LZ4 produces larger files than zstd at level 1. For OSI
 * archival/optimization, zstd is strictly superior. LZ4 is useful when
 * decompression speed is critical (e.g., real-time playback on constrained HW).
 *
 * \section references REFERENCES
 *
 * - MCAP format specification: https://mcap.dev/spec
 * - MCAP chunk record: https://mcap.dev/spec#chunk-op0x06
 * - Zstandard manual: https://facebook.github.io/zstd/zstd_manual.html
 * - ASAM OSI MCAP trace file spec: https://opensimulationinterface.github.io/osi-antora-generator/asamosi/latest/interface/architecture/trace_file_formats.html
 * - asam-osi-utilities config: see TraceFileConfig.h
 *
 * \section usage USAGE
 *
 *   optimize_mcap_compression <input.mcap> [output.mcap] [options]
 *
 * If output is omitted, the output filename is derived from the input with a
 * suffix indicating the applied settings (e.g., "_32MiB_zstd_slowest.mcap").
 *
 * Options:
 *   --chunk-size <MiB>        Chunk size in MiB (default: 32)
 *   --compression <type>      Algorithm: none, lz4, zstd (default: zstd)
 *   --compression-level <l>   Level: fastest, fast, default, slow, slowest (default: slowest)
 *   --dry-run                 Only analyze input, do not write output
 *   --force                   Overwrite output if it already exists
 */

#include <osi-utilities/tracefile/TraceFileConfig.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <map>
#include <mcap/reader.hpp>
#include <mcap/writer.hpp>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <vector>

#include "mcap_cli_utils.h"

using osi3::examples::kCompressionNameMap;
using osi3::examples::kCompressionStringMap;
using osi3::examples::kLevelNameMap;
using osi3::examples::kLevelStringMap;

namespace {

// ============================================================================
// Helper types and constants
// ============================================================================

constexpr uint64_t kMiBBytes = 1024ULL * 1024;

// Map CompressionLevel to actual zstd integer levels (derived from mcap writer.inl).
// Note: these values are coupled to the mcap library internals and may need updating
// if the mcap library changes its internal level mapping.
const std::map<mcap::CompressionLevel, int> kLevelToZstdInt = {
    {mcap::CompressionLevel::Fastest, -5},
    {mcap::CompressionLevel::Fast, -3},
    {mcap::CompressionLevel::Default, 1},
    {mcap::CompressionLevel::Slow, 5},
    {mcap::CompressionLevel::Slowest, osi3::tracefile::config::kHighZstdCompressionLevel},
};

struct ProgramOptions {
    std::filesystem::path input_path;
    std::filesystem::path output_path;
    uint64_t chunk_size = osi3::tracefile::config::kMaxChunkSize;
    mcap::Compression compression = mcap::Compression::Zstd;
    mcap::CompressionLevel compression_level = mcap::CompressionLevel::Slowest;
    bool dry_run = false;
    bool force = false;
};

struct InputStats {
    uint64_t file_size = 0;
    uint64_t message_count = 0;
    uint64_t chunk_count = 0;
    uint64_t channel_count = 0;
    uint64_t schema_count = 0;
    uint64_t total_uncompressed = 0;
    uint64_t total_compressed = 0;
    std::string compression;
};

// ============================================================================
// Formatting helpers
// ============================================================================

auto FormatSize(uint64_t bytes) -> std::string {
    std::ostringstream oss;
    if (bytes >= 1024ULL * 1024 * 1024) {
        oss << std::fixed << std::setprecision(2) << (static_cast<double>(bytes) / (1024.0 * 1024.0 * 1024.0)) << " GiB";
    } else if (bytes >= 1024ULL * 1024) {
        oss << std::fixed << std::setprecision(2) << (static_cast<double>(bytes) / (1024.0 * 1024.0)) << " MiB";
    } else if (bytes >= 1024) {
        oss << std::fixed << std::setprecision(2) << (static_cast<double>(bytes) / 1024.0) << " KiB";
    } else {
        oss << bytes << " B";
    }
    return oss.str();
}

auto BuildOutputPath(const std::filesystem::path& input, uint64_t chunk_mib, const std::string& compression, const std::string& level) -> std::filesystem::path {
    auto stem = input.stem().string();
    auto suffix = "_" + std::to_string(chunk_mib) + "MiB_" + compression + "_" + level;
    return input.parent_path() / (stem + suffix + ".mcap");
}

// ============================================================================
// Analysis
// ============================================================================

auto AnalyzeInput(const std::filesystem::path& path) -> InputStats {
    InputStats stats;
    stats.file_size = std::filesystem::file_size(path);

    mcap::McapReader reader;
    auto status = reader.open(path.string());
    if (!status.ok()) {
        std::cerr << "Error opening input: " << status.message << std::endl;
        return stats;
    }

    status = reader.readSummary(mcap::ReadSummaryMethod::AllowFallbackScan);
    if (!status.ok()) {
        std::cerr << "Warning: Could not read summary: " << status.message << std::endl;
    }

    if (const auto& statistics = reader.statistics(); statistics.has_value()) {
        stats.message_count = statistics->messageCount;
        stats.chunk_count = statistics->chunkCount;
        stats.schema_count = statistics->schemaCount;
        stats.channel_count = statistics->channelCount;
    }

    // Analyze chunk indexes for compression info
    const auto& chunk_indexes = reader.chunkIndexes();
    std::set<std::string> compressions;
    for (const auto& ci : chunk_indexes) {
        compressions.insert(ci.compression);
        stats.total_uncompressed += ci.uncompressedSize;
        stats.total_compressed += ci.compressedSize;
    }
    for (const auto& c : compressions) {
        if (!stats.compression.empty()) {
            stats.compression += ", ";
        }
        stats.compression += c.empty() ? "none" : c;
    }
    if (stats.compression.empty()) {
        stats.compression = "unknown";
    }

    reader.close();
    return stats;
}

// ============================================================================
// Recompression helpers
// ============================================================================

struct RecompressResult {
    uint64_t message_count = 0;
    uint64_t output_size = 0;
    double read_seconds = 0.0;
    double write_seconds = 0.0;
    double total_seconds = 0.0;
};

/// Copy metadata records from reader to writer via raw record parsing.
auto CopyMetadata(mcap::McapReader& reader, mcap::McapWriter& writer) -> void {
    for (const auto& [name, metadata_index] : reader.metadataIndexes()) {
        std::byte* raw_data = nullptr;
        auto bytes_read = reader.dataSource()->read(&raw_data, metadata_index.offset, metadata_index.length);
        if (bytes_read >= 9 && raw_data != nullptr) {
            mcap::Record record{};
            record.opcode = static_cast<mcap::OpCode>(raw_data[0]);
            std::memcpy(&record.dataSize, raw_data + 1, sizeof(uint64_t));
            record.data = raw_data + 9;
            mcap::Metadata metadata;
            if (mcap::McapReader::ParseMetadata(record, &metadata).ok()) {
                (void)writer.write(metadata);
            }
        }
    }
}

/// Copy attachment records from reader to writer via raw record parsing.
auto CopyAttachments(mcap::McapReader& reader, mcap::McapWriter& writer) -> void {
    for (const auto& [name, attachment_index] : reader.attachmentIndexes()) {
        std::byte* raw_data = nullptr;
        auto bytes_read = reader.dataSource()->read(&raw_data, attachment_index.offset, attachment_index.length);
        if (bytes_read >= 9 && raw_data != nullptr) {
            mcap::Record record{};
            record.opcode = static_cast<mcap::OpCode>(raw_data[0]);
            std::memcpy(&record.dataSize, raw_data + 1, sizeof(uint64_t));
            record.data = raw_data + 9;
            mcap::Attachment attachment;
            if (mcap::McapReader::ParseAttachment(record, &attachment).ok()) {
                (void)writer.write(attachment);
            }
        }
    }
}

// ============================================================================
// Recompression
// ============================================================================

auto Recompress(const ProgramOptions& options) -> std::optional<RecompressResult> {
    auto t_start = std::chrono::steady_clock::now();

    // --- Open input ---
    mcap::McapReader reader;
    auto status = reader.open(options.input_path.string());
    if (!status.ok()) {
        std::cerr << "Error opening input: " << status.message << std::endl;
        return std::nullopt;
    }

    status = reader.readSummary(mcap::ReadSummaryMethod::AllowFallbackScan);
    if (!status.ok()) {
        std::cerr << "Error reading summary: " << status.message << std::endl;
        reader.close();
        return std::nullopt;
    }

    auto t_read = std::chrono::steady_clock::now();

    // --- Open output writer ---
    // Preserve the MCAP header profile from input (e.g., "osi2mcap")
    mcap::McapWriter writer;
    std::string profile = reader.header().has_value() ? reader.header()->profile : "";
    mcap::McapWriterOptions writer_options(profile);
    writer_options.library = "osi-utilities/optimize_mcap_compression";
    writer_options.chunkSize = options.chunk_size;
    writer_options.compression = options.compression;
    writer_options.compressionLevel = options.compression_level;

    std::ofstream output_file(options.output_path, std::ios::binary);
    if (!output_file) {
        std::cerr << "Error opening output file: " << options.output_path << std::endl;
        reader.close();
        return std::nullopt;
    }

    writer.open(output_file, writer_options);

    // --- Re-register schemas (build ID mapping) ---
    std::unordered_map<mcap::SchemaId, mcap::SchemaId> schema_id_map;
    for (const auto& [old_id, schema_ptr] : reader.schemas()) {
        mcap::Schema new_schema(schema_ptr->name, schema_ptr->encoding, schema_ptr->data);
        writer.addSchema(new_schema);
        schema_id_map[old_id] = new_schema.id;
    }

    // --- Re-register channels (build ID mapping) ---
    std::unordered_map<mcap::ChannelId, mcap::ChannelId> channel_id_map;
    for (const auto& [old_id, channel_ptr] : reader.channels()) {
        auto new_schema_id = schema_id_map.count(channel_ptr->schemaId) ? schema_id_map[channel_ptr->schemaId] : 0;
        mcap::Channel new_channel(channel_ptr->topic, channel_ptr->messageEncoding, new_schema_id, channel_ptr->metadata);
        writer.addChannel(new_channel);
        channel_id_map[old_id] = new_channel.id;
    }

    // --- Stream messages from reader to writer ---
    uint64_t message_count = 0;
    auto messages = reader.readMessages();
    for (const auto& msg_view : messages) {
        auto it = channel_id_map.find(msg_view.message.channelId);
        if (it == channel_id_map.end()) {
            std::cerr << "Error: message references unknown channel ID " << msg_view.message.channelId << std::endl;
            writer.close();
            output_file.close();
            reader.close();
            std::filesystem::remove(options.output_path);
            return std::nullopt;
        }

        mcap::Message out_msg;
        out_msg.channelId = it->second;
        out_msg.logTime = msg_view.message.logTime;
        out_msg.publishTime = msg_view.message.publishTime;
        out_msg.sequence = msg_view.message.sequence;
        out_msg.dataSize = msg_view.message.dataSize;
        out_msg.data = msg_view.message.data;

        auto write_status = writer.write(out_msg);
        if (!write_status.ok()) {
            std::cerr << "Error writing message: " << write_status.message << std::endl;
            writer.close();
            output_file.close();
            reader.close();
            std::filesystem::remove(options.output_path);
            return std::nullopt;
        }
        ++message_count;
    }

    // --- Copy metadata and attachment records ---
    CopyMetadata(reader, writer);
    CopyAttachments(reader, writer);

    writer.close();
    output_file.close();
    reader.close();

    auto t_write = std::chrono::steady_clock::now();

    RecompressResult result;
    result.message_count = message_count;
    result.output_size = std::filesystem::file_size(options.output_path);
    result.read_seconds = std::chrono::duration<double>(t_read - t_start).count();
    result.write_seconds = std::chrono::duration<double>(t_write - t_read).count();
    result.total_seconds = std::chrono::duration<double>(t_write - t_start).count();
    return result;
}

// ============================================================================
// Report
// ============================================================================

auto PrintReport(const InputStats& input_stats, const std::optional<RecompressResult>& result, const ProgramOptions& options) -> void {
    std::cout << "\n========================================================================" << std::endl;
    std::cout << "  MCAP COMPRESSION OPTIMIZATION REPORT" << std::endl;
    std::cout << "========================================================================" << std::endl;
    std::cout << "\nINPUT FILE" << std::endl;
    std::cout << "  Path:               " << options.input_path.string() << std::endl;
    std::cout << "  File size:          " << FormatSize(input_stats.file_size) << std::endl;
    std::cout << "  Messages:           " << input_stats.message_count << std::endl;
    std::cout << "  Chunks:             " << input_stats.chunk_count << std::endl;
    std::cout << "  Channels:           " << input_stats.channel_count << std::endl;
    std::cout << "  Current compression: " << input_stats.compression << std::endl;

    if (input_stats.chunk_count > 0) {
        auto avg_chunk = input_stats.total_uncompressed / input_stats.chunk_count;
        std::cout << "  Avg chunk size:     " << FormatSize(avg_chunk) << std::endl;
    }
    if (input_stats.total_uncompressed > 0) {
        double ratio = static_cast<double>(input_stats.total_compressed) / static_cast<double>(input_stats.total_uncompressed);
        std::cout << "  Current ratio:      " << std::fixed << std::setprecision(3) << ratio << " (" << std::setprecision(1) << ((1.0 - ratio) * 100.0) << "% saved)" << std::endl;
    }

    auto chunk_mib = options.chunk_size / kMiBBytes;
    std::cout << "\nTARGET SETTINGS" << std::endl;
    std::cout << "  Chunk size:         " << chunk_mib << " MiB (" << options.chunk_size << " bytes)" << std::endl;
    std::cout << "  Compression:        " << kCompressionNameMap.at(options.compression) << std::endl;
    std::cout << "  Level:              " << kLevelNameMap.at(options.compression_level) << " (zstd int: " << kLevelToZstdInt.at(options.compression_level) << ")" << std::endl;

    if (result.has_value()) {
        auto reduction = static_cast<int64_t>(input_stats.file_size) - static_cast<int64_t>(result->output_size);
        double ratio = input_stats.file_size > 0 ? static_cast<double>(result->output_size) / static_cast<double>(input_stats.file_size) : 1.0;

        std::cout << "\nOUTPUT FILE" << std::endl;
        std::cout << "  File size:          " << FormatSize(result->output_size) << std::endl;
        std::cout << "  Messages written:   " << result->message_count << std::endl;
        std::cout << "\nIMPROVEMENT" << std::endl;
        std::cout << "  Size reduction:     " << FormatSize(static_cast<uint64_t>(reduction > 0 ? reduction : 0)) << " (" << std::setprecision(1) << ((1.0 - ratio) * 100.0) << "%)"
                  << std::endl;
        std::cout << "  Compression ratio:  " << std::setprecision(4) << ratio << std::endl;
        std::cout << "  Read time:          " << std::setprecision(1) << result->read_seconds << " s" << std::endl;
        std::cout << "  Write time:         " << result->write_seconds << " s" << std::endl;
        std::cout << "  Total time:         " << result->total_seconds << " s" << std::endl;

        if (result->write_seconds > 0 && input_stats.total_uncompressed > 0) {
            double throughput = static_cast<double>(input_stats.total_uncompressed) / (1024.0 * 1024.0) / result->write_seconds;
            std::cout << "  Write throughput:   " << throughput << " MiB/s (uncompressed input)" << std::endl;
        }
    }

    std::cout << "\n========================================================================" << std::endl;
}

// ============================================================================
// CLI
// ============================================================================

auto PrintHelp() -> void {
    std::cout << "Usage: optimize_mcap_compression <input.mcap> [output.mcap] [options]\n\n"
              << "Optimize MCAP trace file compression for OSI data.\n"
              << "Re-encodes with larger chunks and higher zstd level for maximum space savings.\n\n"
              << "Arguments:\n"
              << "  input                   Path to input MCAP file\n"
              << "  output                  Path to output MCAP file (auto-generated if omitted)\n\n"
              << "Options:\n"
              << "  --chunk-size <MiB>      Chunk size in MiB (default: 32, max for optimal context)\n"
              << "  --compression <type>    Algorithm: none, lz4, zstd (default: zstd)\n"
              << "  --compression-level <l> Level: fastest, fast, default, slow, slowest (default: slowest)\n"
              << "  --dry-run               Only analyze input, do not write output\n"
              << "  --force                 Overwrite output file if it already exists\n"
              << "  --help                  Show this help message\n\n"
              << "Compression levels map to zstd integer levels:\n"
              << "  fastest=-5  fast=-3  default=1  slow=5  slowest=19\n\n"
              << "Example:\n"
              << "  optimize_mcap_compression recording.mcap\n"
              << "  optimize_mcap_compression recording.mcap optimized.mcap --compression-level slow\n"
              << "  optimize_mcap_compression recording.mcap --dry-run\n";
}

/// Parse a single CLI option that takes a value argument.
/// Returns true if the option was handled, false otherwise.
/// On parse error, sets options.chunk_size = 0 as an error signal.
auto ParseOptionValue(const std::string& arg, int& i, int argc, const char** argv, ProgramOptions& options) -> bool {
    if (arg == "--chunk-size" && i + 1 < argc) {
        try {
            auto mib = std::stoull(argv[++i]);
            options.chunk_size = mib * kMiBBytes;
        } catch (const std::exception&) {
            std::cerr << "Error: Invalid chunk-size value '" << argv[i] << "'. Must be a positive integer.\n";
            options.chunk_size = 0;
        }
        return true;
    }
    if (arg == "--compression" && i + 1 < argc) {
        std::string comp = argv[++i];
        auto it = kCompressionStringMap.find(comp);
        if (it == kCompressionStringMap.end()) {
            std::cerr << "Error: Invalid compression '" << comp << "'. Must be: none, lz4, zstd\n";
            options.chunk_size = 0;
        } else {
            options.compression = it->second;
        }
        return true;
    }
    if (arg == "--compression-level" && i + 1 < argc) {
        std::string level = argv[++i];
        auto it = kLevelStringMap.find(level);
        if (it == kLevelStringMap.end()) {
            std::cerr << "Error: Invalid level '" << level << "'. Must be: fastest, fast, default, slow, slowest\n";
            options.chunk_size = 0;
        } else {
            options.compression_level = it->second;
        }
        return true;
    }
    return false;
}

auto ParseArgs(int argc, const char** argv) -> std::optional<ProgramOptions> {
    if (argc < 2) {
        PrintHelp();
        return std::nullopt;
    }

    ProgramOptions options;
    std::vector<std::string> positional;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--help" || arg == "-h") {
            PrintHelp();
            return std::nullopt;
        }
        if (arg == "--dry-run") {
            options.dry_run = true;
        } else if (arg == "--force") {
            options.force = true;
        } else if (ParseOptionValue(arg, i, argc, argv, options)) {
            if (options.chunk_size == 0) {
                return std::nullopt;
            }
        } else if (arg[0] == '-') {
            std::cerr << "Error: Unknown option '" << arg << "'\n";
            PrintHelp();
            return std::nullopt;
        } else {
            positional.push_back(arg);
        }
    }

    if (positional.empty()) {
        std::cerr << "Error: Input file required\n";
        PrintHelp();
        return std::nullopt;
    }
    options.input_path = positional[0];
    if (positional.size() >= 2) {
        options.output_path = positional[1];
    }

    // Validate chunk size
    if (options.chunk_size < osi3::tracefile::config::kMinChunkSize || options.chunk_size > osi3::tracefile::config::kMaxChunkSize) {
        std::cerr << "Error: chunk-size must be between " << (osi3::tracefile::config::kMinChunkSize / kMiBBytes) << " and " << (osi3::tracefile::config::kMaxChunkSize / kMiBBytes)
                  << " MiB\n";
        return std::nullopt;
    }

    return options;
}

}  // namespace

auto main(const int argc, const char** argv) -> int {
    auto parsed = ParseArgs(argc, argv);
    if (!parsed) {
        return 1;
    }
    auto options = *parsed;

    // Validate input
    if (!std::filesystem::exists(options.input_path)) {
        std::cerr << "Error: Input file not found: " << options.input_path << std::endl;
        return 1;
    }
    if (options.input_path.extension() != ".mcap") {
        std::cerr << "Error: Input must be an MCAP file (got " << options.input_path.extension() << ")" << std::endl;
        return 1;
    }

    // Determine output path if not specified
    if (options.output_path.empty()) {
        auto chunk_mib = options.chunk_size / kMiBBytes;
        options.output_path = BuildOutputPath(options.input_path, chunk_mib, kCompressionNameMap.at(options.compression), kLevelNameMap.at(options.compression_level));
    }

    // Guard against input/output collision (would truncate input)
    // std::filesystem::equivalent() requires both paths to exist, so check that first
    if (std::filesystem::exists(options.output_path) && std::filesystem::equivalent(options.input_path, options.output_path)) {
        std::cerr << "Error: Output path cannot be the same as input path" << std::endl;
        return 1;
    }

    // Guard against overwrite
    if (!options.dry_run && std::filesystem::exists(options.output_path) && !options.force) {
        std::cerr << "Error: Output file already exists: " << options.output_path << "\nUse --force to overwrite." << std::endl;
        return 1;
    }

    // Analyze input
    std::cout << "Analyzing: " << options.input_path.string() << std::endl;
    auto input_stats = AnalyzeInput(options.input_path);

    if (options.dry_run) {
        PrintReport(input_stats, std::nullopt, options);
        std::cout << "\n  [DRY RUN — no output written]" << std::endl;
        return 0;
    }

    // Perform optimization
    std::cout << "Optimizing: " << options.input_path.filename().string() << "\n"
              << "  -> " << options.output_path.filename().string() << "\n"
              << "  Chunk size: " << (options.chunk_size / kMiBBytes) << " MiB" << " | Compression: " << kCompressionNameMap.at(options.compression) << " level "
              << kLevelNameMap.at(options.compression_level) << "\n  Processing..." << std::flush;

    auto result = Recompress(options);
    if (!result) {
        std::cerr << "\nError: Recompression failed." << std::endl;
        return 1;
    }

    std::cout << " done." << std::endl;
    PrintReport(input_stats, result, options);
    std::cout << "\n  Output: " << options.output_path.string() << std::endl;
    return 0;
}
