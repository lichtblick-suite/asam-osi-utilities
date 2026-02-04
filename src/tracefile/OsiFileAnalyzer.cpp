//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/OsiFileAnalyzer.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace osi3 {

std::optional<OsiFileStatistics> OsiFileAnalyzer::Analyze(const std::filesystem::path& file_path, size_t sample_size) const {
    // Validate file exists and has .osi extension
    if (!std::filesystem::exists(file_path)) {
        std::cerr << "ERROR: File does not exist: " << file_path << std::endl;
        return std::nullopt;
    }

    if (file_path.extension() != ".osi") {
        std::cerr << "ERROR: File must have .osi extension: " << file_path << std::endl;
        return std::nullopt;
    }

    std::ifstream file(file_path, std::ios::binary);
    if (!file) {
        std::cerr << "ERROR: Failed to open file: " << file_path << std::endl;
        return std::nullopt;
    }

    OsiFileStatistics stats;
    stats.file_path = file_path;
    stats.file_size_bytes = std::filesystem::file_size(file_path);
    stats.is_sampled = (sample_size > 0);

    // Initialize min to max possible value for proper tracking
    stats.min_message_size = std::numeric_limits<size_t>::max();
    stats.max_message_size = 0;

    std::vector<uint64_t> timestamps;
    std::vector<size_t> message_sizes;

    // Reserve space for expected sample size
    const size_t expected_messages = sample_size > 0 ? sample_size : 1000;
    timestamps.reserve(expected_messages);
    message_sizes.reserve(expected_messages);

    size_t messages_read = 0;
    bool limit_reached = false;

    while (file.peek() != EOF) {
        // Check sample limit
        if (sample_size > 0 && messages_read >= sample_size) {
            limit_reached = true;
            break;
        }

        // Read message size (4-byte prefix)
        const uint32_t message_size = ReadMessageSize(file);
        if (message_size == 0) {
            break;  // Read error or end of file
        }

        // Sanity check message size
        if (message_size > tracefile::config::kMaxExpectedMessageSize) {
            std::cerr << "WARNING: Unusually large message size (" << message_size << " bytes) at message " << messages_read << ". File may be corrupted." << std::endl;
            break;
        }

        // Read the message data for timestamp extraction
        std::vector<char> message_data(message_size);
        if (!file.read(message_data.data(), message_size)) {
            std::cerr << "ERROR: Failed to read message data at message " << messages_read << std::endl;
            break;
        }

        // Extract timestamp
        const uint64_t timestamp_ns = ExtractTimestampNanoseconds(message_data);

        // Track statistics
        message_sizes.push_back(message_size);
        timestamps.push_back(timestamp_ns);

        stats.min_message_size = std::min(stats.min_message_size, static_cast<size_t>(message_size));
        stats.max_message_size = std::max(stats.max_message_size, static_cast<size_t>(message_size));
        stats.total_message_bytes += message_size;

        messages_read++;
    }

    // Handle edge case: no messages read
    if (messages_read == 0) {
        std::cerr << "ERROR: No messages could be read from file" << std::endl;
        return std::nullopt;
    }

    stats.message_count = messages_read;
    stats.avg_message_size = static_cast<double>(stats.total_message_bytes) / messages_read;

    // Estimate total message count if sampled
    if (stats.is_sampled && limit_reached && stats.avg_message_size > 0) {
        // Estimate based on file size and average message size (including 4-byte prefix)
        const double avg_message_with_prefix = stats.avg_message_size + tracefile::config::kBinaryOsiMessageLengthPrefixSize;
        stats.total_message_count_estimate = static_cast<size_t>(stats.file_size_bytes / avg_message_with_prefix);
    } else {
        stats.total_message_count_estimate = messages_read;
    }

    // Calculate timing statistics
    if (!timestamps.empty()) {
        stats.first_timestamp_ns = timestamps.front();
        stats.last_timestamp_ns = timestamps.back();

        if (timestamps.size() > 1) {
            const uint64_t duration_ns = stats.last_timestamp_ns - stats.first_timestamp_ns;
            stats.duration_seconds = static_cast<double>(duration_ns) / 1e9;

            if (stats.duration_seconds > 0) {
                stats.avg_frame_interval_seconds = stats.duration_seconds / (timestamps.size() - 1);
                stats.frame_rate_hz = 1.0 / stats.avg_frame_interval_seconds;
                stats.bytes_per_second = stats.total_message_bytes / stats.duration_seconds;
            }
        }
    }

    // Handle edge case: couldn't determine frame rate
    if (stats.frame_rate_hz <= 0 || !std::isfinite(stats.frame_rate_hz)) {
        std::cerr << "WARNING: Could not determine frame rate from timestamps. "
                  << "Using default assumption of " << tracefile::config::kDefaultAssumedFrameRateHz << " Hz." << std::endl;
        stats.frame_rate_hz = tracefile::config::kDefaultAssumedFrameRateHz;
        stats.avg_frame_interval_seconds = 1.0 / stats.frame_rate_hz;
        if (stats.avg_message_size > 0) {
            stats.bytes_per_second = stats.avg_message_size * stats.frame_rate_hz;
        }
    }

    // Clamp frame rate to reasonable bounds
    if (stats.frame_rate_hz < tracefile::config::kMinExpectedFrameRateHz) {
        std::cerr << "WARNING: Detected frame rate (" << stats.frame_rate_hz << " Hz) is unusually low. Timestamps may be incorrect." << std::endl;
    }
    if (stats.frame_rate_hz > tracefile::config::kMaxExpectedFrameRateHz) {
        std::cerr << "WARNING: Detected frame rate (" << stats.frame_rate_hz << " Hz) is unusually high. Timestamps may be incorrect." << std::endl;
    }

    return stats;
}

RecommendedMcapOptions OsiFileAnalyzer::RecommendMcapOptions(const OsiFileStatistics& stats, double target_chunk_duration_seconds) const {
    RecommendedMcapOptions options;

    // Clamp target duration to reasonable bounds
    target_chunk_duration_seconds = std::clamp(target_chunk_duration_seconds, tracefile::config::kMinChunkDurationSeconds, tracefile::config::kMaxChunkDurationSeconds);

    // Calculate optimal chunk size based on target duration
    // Formula: chunk_size = avg_message_size × frame_rate × target_duration
    //
    // Rationale: We want each chunk to contain approximately target_duration
    // seconds of data. This ensures:
    // - Smooth buffering during playback (each chunk read = ~1s of data)
    // - Efficient compression (more data per chunk = better ratio)
    // - Reasonable memory usage during reading

    const double messages_per_chunk = stats.frame_rate_hz * target_chunk_duration_seconds;
    uint64_t calculated_chunk_size = static_cast<uint64_t>(stats.avg_message_size * messages_per_chunk);

    // Clamp to allowed range
    options.chunk_size = std::clamp(calculated_chunk_size, tracefile::config::kMinChunkSize, tracefile::config::kMaxChunkSize);

    // Build rationale string
    std::ostringstream rationale;
    rationale << std::fixed << std::setprecision(1);
    rationale << "Target " << target_chunk_duration_seconds << "s per chunk × " << stats.frame_rate_hz << " Hz × " << static_cast<size_t>(stats.avg_message_size)
              << " B/msg = " << (calculated_chunk_size / (1024.0 * 1024.0)) << " MiB";

    if (options.chunk_size != calculated_chunk_size) {
        rationale << " (clamped to ";
        if (options.chunk_size == tracefile::config::kMinChunkSize) {
            rationale << "min " << (tracefile::config::kMinChunkSize / (1024 * 1024)) << " MiB)";
        } else {
            rationale << "max " << (tracefile::config::kMaxChunkSize / (1024 * 1024)) << " MiB)";
        }
    }

    options.chunk_size_rationale = rationale.str();

    // Compression recommendation
    // Use Zstd by default - it provides excellent compression with fast decompression
    // Only consider disabling for very small messages where overhead outweighs benefits

    if (stats.avg_message_size < tracefile::config::kMinMessageSizeForCompression) {
        options.compression = mcap::Compression::None;
        options.compression_rationale =
            "Messages are small (<" + std::to_string(tracefile::config::kMinMessageSizeForCompression) + " B avg), compression overhead may outweigh benefits";
    } else {
        options.compression = mcap::Compression::Zstd;
        options.compression_level = mcap::CompressionLevel::Default;
        options.compression_rationale = "Zstd provides excellent compression for protobuf data with fast decompression";
    }

    return options;
}

void OsiFileAnalyzer::PrintStatistics(const OsiFileStatistics& stats) {
    std::cout << "\n=== OSI File Analysis ===" << std::endl;
    std::cout << "File: " << stats.file_path.filename() << std::endl;
    std::cout << "File size: " << (stats.file_size_bytes / (1024.0 * 1024.0)) << " MiB" << std::endl;

    std::cout << "\nMessage Statistics";
    if (stats.is_sampled) {
        std::cout << " (sampled " << stats.message_count << " messages, ~" << stats.total_message_count_estimate << " total estimated)";
    } else {
        std::cout << " (" << stats.message_count << " messages)";
    }
    std::cout << ":" << std::endl;

    std::cout << "  Min size:  " << stats.min_message_size << " bytes" << std::endl;
    std::cout << "  Max size:  " << stats.max_message_size << " bytes" << std::endl;
    std::cout << "  Avg size:  " << static_cast<size_t>(stats.avg_message_size) << " bytes" << std::endl;

    std::cout << "\nTiming Statistics:" << std::endl;
    std::cout << std::fixed << std::setprecision(2);
    std::cout << "  Duration:   " << stats.duration_seconds << " s" << std::endl;
    std::cout << "  Frame rate: " << stats.frame_rate_hz << " Hz" << std::endl;
    std::cout << "  Data rate:  " << (stats.bytes_per_second / (1024.0 * 1024.0)) << " MiB/s" << std::endl;

    if (!stats.IsValid()) {
        std::cout << "\nWARNING: Analysis may be unreliable (insufficient data or invalid metrics)" << std::endl;
    }
}

void OsiFileAnalyzer::PrintRecommendation(const RecommendedMcapOptions& options) {
    std::cout << "\n=== Recommended MCAP Settings ===" << std::endl;
    std::cout << "Chunk size:  " << options.chunk_size << " bytes (" << (options.chunk_size / (1024.0 * 1024.0)) << " MiB)" << std::endl;
    std::cout << "  Rationale: " << options.chunk_size_rationale << std::endl;

    std::cout << "Compression: ";
    switch (options.compression) {
        case mcap::Compression::None:
            std::cout << "none";
            break;
        case mcap::Compression::Lz4:
            std::cout << "lz4";
            break;
        case mcap::Compression::Zstd:
            std::cout << "zstd";
            break;
    }
    std::cout << std::endl;
    std::cout << "  Rationale: " << options.compression_rationale << std::endl;
}

uint32_t OsiFileAnalyzer::ReadMessageSize(std::ifstream& file) {
    uint32_t message_size = 0;
    if (!file.read(reinterpret_cast<char*>(&message_size), sizeof(message_size))) {
        return 0;
    }
    return message_size;
}

bool OsiFileAnalyzer::SkipMessage(std::ifstream& file, uint32_t message_size) {
    file.seekg(message_size, std::ios::cur);
    return file.good();
}

uint64_t OsiFileAnalyzer::ExtractTimestampNanoseconds(const std::vector<char>& data) {
    // OSI messages have a 'timestamp' field which is an osi3::Timestamp message.
    // The Timestamp message has:
    //   - seconds (field 1, int64)
    //   - nanos (field 2, uint32)
    //
    // We do a lightweight parse to find these fields without full protobuf deserialization.
    // This is faster but assumes the timestamp field is at the expected position.
    //
    // Protobuf wire format:
    //   - Field tag: (field_number << 3) | wire_type
    //   - Wire type 0 = varint, wire type 2 = length-delimited
    //
    // For most OSI top-level messages, timestamp is typically field 1 or a low field number.

    if (data.size() < 10) {  // Minimum size for a valid timestamp
        return 0;
    }

    const auto* bytes = reinterpret_cast<const uint8_t*>(data.data());
    const size_t size = data.size();
    size_t pos = 0;

    // Helper lambda to read varint
    auto read_varint = [&]() -> uint64_t {
        uint64_t result = 0;
        int shift = 0;
        while (pos < size) {
            uint8_t byte = bytes[pos++];
            result |= static_cast<uint64_t>(byte & 0x7F) << shift;
            if ((byte & 0x80) == 0) break;
            shift += 7;
        }
        return result;
    };

    // Search for timestamp field (usually field 1 with wire type 2 = length-delimited submessage)
    // The tag would be (1 << 3) | 2 = 0x0A
    while (pos < size - 2) {
        const uint8_t tag_byte = bytes[pos];

        // Check for timestamp field (field 1, wire type 2)
        if (tag_byte == 0x0A) {
            pos++;  // Skip tag
            uint64_t submsg_len = read_varint();
            if (submsg_len == 0 || pos + submsg_len > size) {
                return 0;
            }

            // Parse the Timestamp submessage
            int64_t seconds = 0;
            uint32_t nanos = 0;
            size_t submsg_end = pos + submsg_len;

            while (pos < submsg_end) {
                uint8_t field_tag = bytes[pos++];
                uint8_t field_num = field_tag >> 3;
                uint8_t wire_type = field_tag & 0x07;

                if (wire_type == 0) {  // varint
                    uint64_t value = read_varint();
                    if (field_num == 1) {
                        seconds = static_cast<int64_t>(value);
                    } else if (field_num == 2) {
                        nanos = static_cast<uint32_t>(value);
                    }
                } else {
                    // Unknown wire type in timestamp, skip to end
                    break;
                }
            }

            return static_cast<uint64_t>(seconds) * 1000000000ULL + nanos;
        }

        // Skip this field
        uint8_t wire_type = tag_byte & 0x07;
        pos++;  // Skip tag

        switch (wire_type) {
            case 0:  // varint
                read_varint();
                break;
            case 1:  // 64-bit
                pos += 8;
                break;
            case 2:  // length-delimited
                pos += read_varint();
                break;
            case 5:  // 32-bit
                pos += 4;
                break;
            default:
                return 0;  // Unknown wire type
        }
    }

    return 0;  // Timestamp field not found
}

}  // namespace osi3
