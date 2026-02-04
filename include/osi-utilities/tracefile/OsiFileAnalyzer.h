//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#ifndef OSIUTILITIES_TRACEFILE_OSIFILEANALYZER_H_
#define OSIUTILITIES_TRACEFILE_OSIFILEANALYZER_H_

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <mcap/mcap.hpp>
#include <optional>
#include <string>

#include "TraceFileConfig.h"

namespace osi3 {

/**
 * @brief Statistics gathered from analyzing an OSI trace file.
 *
 * This structure contains metrics about message sizes, timing, and data rates
 * that can be used to determine optimal MCAP writer configuration.
 */
struct OsiFileStatistics {
    // File information
    std::filesystem::path file_path;          ///< Path to the analyzed file
    uint64_t file_size_bytes = 0;             ///< Total file size in bytes
    size_t message_count = 0;                 ///< Number of messages analyzed
    bool is_sampled = false;                  ///< True if only a sample was analyzed
    size_t total_message_count_estimate = 0;  ///< Estimated total messages (if sampled)

    // Message size statistics (in bytes)
    size_t min_message_size = 0;       ///< Smallest message size
    size_t max_message_size = 0;       ///< Largest message size
    double avg_message_size = 0.0;     ///< Average message size
    uint64_t total_message_bytes = 0;  ///< Sum of all message sizes

    // Timing statistics
    uint64_t first_timestamp_ns = 0;          ///< Timestamp of first message (nanoseconds)
    uint64_t last_timestamp_ns = 0;           ///< Timestamp of last message (nanoseconds)
    double duration_seconds = 0.0;            ///< Total duration in seconds
    double avg_frame_interval_seconds = 0.0;  ///< Average time between frames

    // Derived metrics
    double frame_rate_hz = 0.0;     ///< Estimated frame rate in Hz
    double bytes_per_second = 0.0;  ///< Data rate in bytes/second

    /**
     * @brief Check if the analysis produced valid results.
     * @return true if statistics are valid and usable
     */
    [[nodiscard]] bool IsValid() const { return message_count >= tracefile::config::kMinMessagesForReliableAnalysis && avg_message_size > 0 && frame_rate_hz > 0; }
};

/**
 * @brief Recommended MCAP writer options based on file analysis.
 */
struct RecommendedMcapOptions {
    uint64_t chunk_size = tracefile::config::kDefaultChunkSize;
    mcap::Compression compression = mcap::Compression::Zstd;
    mcap::CompressionLevel compression_level = mcap::CompressionLevel::Default;

    // Explanation of why these settings were chosen
    std::string chunk_size_rationale;
    std::string compression_rationale;
};

/**
 * @brief Analyzes OSI trace files to determine optimal MCAP configuration.
 *
 * This class performs a pre-scan of OSI trace files to gather statistics
 * about message sizes and timing. Based on these statistics, it recommends
 * optimal MCAP writer settings for efficient playback.
 *
 * The key insight is that MCAP chunk size should be tuned to contain
 * approximately 1 second of data. This ensures:
 * - Efficient buffering during playback (each chunk = ~1s of data)
 * - Good compression ratios (more data per chunk)
 * - Reasonable memory usage (chunks aren't too large)
 *
 * ## Usage Example
 *
 * ```cpp
 * OsiFileAnalyzer analyzer;
 * auto stats = analyzer.Analyze("trace.osi");
 * if (stats && stats->IsValid()) {
 *     auto options = analyzer.RecommendMcapOptions(*stats);
 *     // Use options.chunk_size, options.compression, etc.
 * }
 * ```
 */
class OsiFileAnalyzer {
   public:
    /**
     * @brief Analyze an OSI trace file to gather statistics.
     *
     * This method reads messages from the file to collect size and timing
     * information. By default, it samples the first N messages for speed.
     *
     * @param file_path Path to the OSI trace file
     * @param sample_size Number of messages to sample (0 for full scan)
     * @return Statistics about the file, or nullopt if analysis failed
     */
    std::optional<OsiFileStatistics> Analyze(const std::filesystem::path& file_path, size_t sample_size = tracefile::config::kAnalysisSampleSize) const;

    /**
     * @brief Recommend MCAP writer options based on file statistics.
     *
     * Uses the analyzed statistics to calculate optimal chunk size and
     * compression settings for efficient MCAP file creation.
     *
     * @param stats Statistics from Analyze()
     * @param target_chunk_duration_seconds Target duration per chunk (default: 1.0s)
     * @return Recommended MCAP options with rationale
     */
    RecommendedMcapOptions RecommendMcapOptions(const OsiFileStatistics& stats, double target_chunk_duration_seconds = tracefile::config::kTargetChunkDurationSeconds) const;

    /**
     * @brief Print analysis results to stdout.
     *
     * Outputs a human-readable summary of the file statistics,
     * useful for debugging and user feedback.
     *
     * @param stats Statistics to print
     */
    static void PrintStatistics(const OsiFileStatistics& stats);

    /**
     * @brief Print recommended options with rationale to stdout.
     *
     * @param options Recommended options to print
     */
    static void PrintRecommendation(const RecommendedMcapOptions& options);

   private:
    /**
     * @brief Read the raw message size from a binary OSI file without parsing.
     *
     * This is faster than fully parsing each message when we only need sizes.
     *
     * @param file Input file stream positioned at message start
     * @return Message size, or 0 if read failed
     */
    static uint32_t ReadMessageSize(std::ifstream& file);

    /**
     * @brief Skip a message in the file stream.
     *
     * @param file Input file stream
     * @param message_size Size of message to skip
     * @return true if skip succeeded
     */
    static bool SkipMessage(std::ifstream& file, uint32_t message_size);

    /**
     * @brief Extract timestamp from a serialized protobuf message.
     *
     * Parses just enough of the protobuf to extract the timestamp field,
     * without fully deserializing the message.
     *
     * @param data Serialized protobuf message
     * @return Timestamp in nanoseconds, or 0 if extraction failed
     */
    static uint64_t ExtractTimestampNanoseconds(const std::vector<char>& data);
};

}  // namespace osi3

#endif  // OSIUTILITIES_TRACEFILE_OSIFILEANALYZER_H_
