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
#include <limits>
#include <sstream>

namespace osi3 {

std::optional<OsiFileStatistics> OsiFileAnalyzer::Analyze(const std::filesystem::path& file_path, size_t sample_size) {
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

    // Initialize min to max possible value for proper tracking
    stats.min_message_size = std::numeric_limits<size_t>::max();
    stats.max_message_size = 0;

    // Pass 1: scan message sizes and count total messages
    size_t total_messages = 0;
    while (file.peek() != EOF) {
        const uint32_t message_size = ReadMessageSize(file);
        if (message_size == 0) {
            break;  // Read error or end of file
        }

        if (message_size > tracefile::config::kMaxExpectedMessageSize) {
            std::cerr << "WARNING: Unusually large message size (" << message_size << " bytes) at message " << total_messages << ". File may be corrupted." << std::endl;
            break;
        }

        stats.min_message_size = std::min(stats.min_message_size, static_cast<size_t>(message_size));
        stats.max_message_size = std::max(stats.max_message_size, static_cast<size_t>(message_size));
        stats.total_message_bytes += message_size;
        total_messages++;

        if (!SkipMessage(file, message_size)) {
            std::cerr << "ERROR: Failed to skip message data at message " << (total_messages - 1) << std::endl;
            break;
        }
    }

    if (total_messages == 0) {
        std::cerr << "ERROR: No messages could be read from file" << std::endl;
        return std::nullopt;
    }

    stats.message_count = total_messages;
    stats.total_message_count_estimate = total_messages;
    stats.avg_message_size = static_cast<double>(stats.total_message_bytes) / static_cast<double>(total_messages);

    if (stats.min_message_size == std::numeric_limits<size_t>::max()) {
        stats.min_message_size = 0;
    }

    // Determine timestamp sample size (evenly spaced across the file)
    size_t timestamp_sample_size = sample_size;
    if (sample_size == 0 || sample_size >= total_messages) {
        timestamp_sample_size = total_messages;
        stats.is_sampled = false;
    } else {
        stats.is_sampled = true;
        if (total_messages > 1 && timestamp_sample_size < 2) {
            timestamp_sample_size = 2;
        }
    }
    if (timestamp_sample_size > total_messages) {
        timestamp_sample_size = total_messages;
    }
    stats.timestamp_sample_count = timestamp_sample_size;

    std::vector<size_t> sample_indices;
    const bool sample_all = (timestamp_sample_size == total_messages);
    if (!sample_all && timestamp_sample_size > 0) {
        sample_indices.reserve(timestamp_sample_size);
        if (timestamp_sample_size == 1) {
            sample_indices.push_back(0);
        } else {
            const size_t denom = timestamp_sample_size - 1;
            for (size_t i = 0; i < timestamp_sample_size; ++i) {
                const size_t idx = (i * (total_messages - 1)) / denom;
                sample_indices.push_back(idx);
            }
        }
    }

    // Pass 2: sample timestamps
    file.clear();
    file.seekg(0, std::ios::beg);
    if (!file) {
        std::cerr << "ERROR: Failed to rewind file for timestamp sampling" << std::endl;
        return std::nullopt;
    }

    size_t message_index = 0;
    size_t sample_cursor = 0;
    std::optional<uint64_t> first_timestamp;
    std::optional<uint64_t> last_timestamp;

    while (file.peek() != EOF && message_index < total_messages) {
        const uint32_t message_size = ReadMessageSize(file);
        if (message_size == 0) {
            break;
        }

        const bool should_sample = sample_all || (sample_cursor < sample_indices.size() && sample_indices[sample_cursor] == message_index);

        if (should_sample) {
            std::vector<char> message_data(message_size);
            if (!file.read(message_data.data(), message_size)) {
                std::cerr << "ERROR: Failed to read message data at message " << message_index << std::endl;
                break;
            }
            if (const auto timestamp_ns = ExtractTimestampNanoseconds(message_data)) {
                if (!first_timestamp) {
                    first_timestamp = timestamp_ns;
                }
                last_timestamp = timestamp_ns;
            }
            if (!sample_all && sample_cursor < sample_indices.size() && sample_indices[sample_cursor] == message_index) {
                sample_cursor++;
            }
        } else {
            if (!SkipMessage(file, message_size)) {
                std::cerr << "ERROR: Failed to skip message data at message " << message_index << std::endl;
                break;
            }
        }

        message_index++;
    }

    if (first_timestamp && last_timestamp && *last_timestamp >= *first_timestamp && total_messages > 1) {
        stats.first_timestamp_ns = *first_timestamp;
        stats.last_timestamp_ns = *last_timestamp;
        const uint64_t duration_ns = stats.last_timestamp_ns - stats.first_timestamp_ns;
        stats.duration_seconds = static_cast<double>(duration_ns) / 1e9;

        if (stats.duration_seconds > 0) {
            stats.avg_frame_interval_seconds = stats.duration_seconds / static_cast<double>(total_messages - 1);
            stats.frame_rate_hz = 1.0 / stats.avg_frame_interval_seconds;
            stats.bytes_per_second = static_cast<double>(stats.total_message_bytes) / stats.duration_seconds;
        }
    }

    // Handle edge case: couldn't determine frame rate
    if (stats.frame_rate_hz <= 0 || !std::isfinite(stats.frame_rate_hz)) {
        std::cerr << "WARNING: Could not determine frame rate from timestamps (checked fields 2 and 10). " << "Using default assumption of "
                  << tracefile::config::kDefaultAssumedFrameRateHz << " Hz." << std::endl;
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

RecommendedMcapOptions OsiFileAnalyzer::RecommendMcapOptions(const OsiFileStatistics& stats, double target_chunk_duration_seconds) {
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
    const auto calculated_chunk_size = static_cast<uint64_t>(stats.avg_message_size * messages_per_chunk);

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
        std::cout << " (" << stats.message_count << " total messages, " << stats.timestamp_sample_count << " timestamp samples)";
    } else {
        std::cout << " (" << stats.message_count << " messages)";
    }
    std::cout << ":" << std::endl;

    std::cout << "  Min size:  " << stats.min_message_size << " bytes (uncompressed)" << std::endl;
    std::cout << "  Max size:  " << stats.max_message_size << " bytes (uncompressed)" << std::endl;
    std::cout << "  Avg size:  " << static_cast<size_t>(stats.avg_message_size) << " bytes (uncompressed)" << std::endl;

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

auto OsiFileAnalyzer::ReadMessageSize(std::ifstream& file) -> uint32_t {
    uint32_t message_size = 0;
    if (!file.read(reinterpret_cast<char*>(&message_size), sizeof(message_size))) {
        return 0;
    }
    return message_size;
}

auto OsiFileAnalyzer::SkipMessage(std::ifstream& file, uint32_t message_size) -> bool {
    file.seekg(message_size, std::ios::cur);
    return file.good();
}

auto OsiFileAnalyzer::ExtractTimestampNanoseconds(const std::vector<char>& data) -> std::optional<uint64_t> {
    // OSI messages have a 'timestamp' field which is an osi3::Timestamp message.
    // The Timestamp message has:
    //   - seconds (field 1, int64)
    //   - nanos (field 2, uint32)
    //
    // Top-level OSI messages typically store the timestamp in field 2,
    // except HostVehicleData which uses field 10.

    if (data.size() < 6) {
        return std::nullopt;
    }

    const auto* bytes = reinterpret_cast<const uint8_t*>(data.data());
    const size_t size = data.size();
    size_t pos = 0;

    auto read_varint = [&](size_t limit, size_t& cursor, uint64_t& value) -> bool {
        value = 0;
        int shift = 0;
        while (cursor < limit && shift <= 63) {
            const uint8_t byte = bytes[cursor++];
            value |= static_cast<uint64_t>(byte & 0x7F) << shift;
            if ((byte & 0x80) == 0) {
                return true;
            }
            shift += 7;
        }
        return false;
    };

    auto parse_timestamp_submessage = [&](size_t start, size_t length) -> std::optional<uint64_t> {
        size_t cursor = start;
        const size_t end = start + length;
        int64_t seconds = 0;
        uint32_t nanos = 0;
        bool has_seconds = false;
        bool has_nanos = false;

        while (cursor < end) {
            uint64_t tag = 0;
            if (!read_varint(end, cursor, tag)) {
                return std::nullopt;
            }
            const uint32_t field_num = static_cast<uint32_t>(tag >> 3);
            const uint32_t wire_type = static_cast<uint32_t>(tag & 0x07);

            switch (wire_type) {
                case 0: {  // varint
                    uint64_t value = 0;
                    if (!read_varint(end, cursor, value)) {
                        return std::nullopt;
                    }
                    if (field_num == 1) {
                        seconds = static_cast<int64_t>(value);
                        has_seconds = true;
                    } else if (field_num == 2) {
                        nanos = static_cast<uint32_t>(value);
                        has_nanos = true;
                    }
                    break;
                }
                case 1:  // 64-bit
                    if (cursor + 8 > end) {
                        return std::nullopt;
                    }
                    cursor += 8;
                    break;
                case 2: {  // length-delimited
                    uint64_t len = 0;
                    if (!read_varint(end, cursor, len) || cursor + len > end) {
                        return std::nullopt;
                    }
                    cursor += static_cast<size_t>(len);
                    break;
                }
                case 5:  // 32-bit
                    if (cursor + 4 > end) {
                        return std::nullopt;
                    }
                    cursor += 4;
                    break;
                default:
                    return std::nullopt;
            }
        }

        if (!has_seconds && !has_nanos) {
            return std::nullopt;
        }
        if (nanos >= 1000000000U || seconds < 0) {
            return std::nullopt;
        }

        return static_cast<uint64_t>(seconds) * 1000000000ULL + nanos;
    };

    auto is_candidate_timestamp_field = [](uint32_t field_num) { return field_num == 2 || field_num == 10; };

    while (pos < size) {
        uint64_t tag = 0;
        if (!read_varint(size, pos, tag)) {
            break;
        }

        const uint32_t field_num = static_cast<uint32_t>(tag >> 3);
        const uint32_t wire_type = static_cast<uint32_t>(tag & 0x07);

        switch (wire_type) {
            case 0: {  // varint
                uint64_t value = 0;
                if (!read_varint(size, pos, value)) {
                    return std::nullopt;
                }
                break;
            }
            case 1:  // 64-bit
                if (pos + 8 > size) {
                    return std::nullopt;
                }
                pos += 8;
                break;
            case 2: {  // length-delimited
                uint64_t len = 0;
                if (!read_varint(size, pos, len) || pos + len > size) {
                    return std::nullopt;
                }
                if (is_candidate_timestamp_field(field_num)) {
                    if (auto timestamp = parse_timestamp_submessage(pos, static_cast<size_t>(len))) {
                        return timestamp;
                    }
                }
                pos += static_cast<size_t>(len);
                break;
            }
            case 5:  // 32-bit
                if (pos + 4 > size) {
                    return std::nullopt;
                }
                pos += 4;
                break;
            default:
                return std::nullopt;
        }
    }

    return std::nullopt;
}

}  // namespace osi3
