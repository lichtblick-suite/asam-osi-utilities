//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#ifndef OSIUTILITIES_TRACEFILE_TRACEFILECONFIG_H_
#define OSIUTILITIES_TRACEFILE_TRACEFILECONFIG_H_

#include <cstddef>
#include <cstdint>

namespace osi3 {
namespace tracefile {

/**
 * @brief Configuration constants for OSI trace file reading and writing.
 *
 * This namespace centralizes all configurable parameters and default values
 * used throughout the tracefile reader and writer implementations.
 * Users can reference these constants to understand the default behavior
 * and make informed decisions when overriding settings.
 */
namespace config {

// ============================================================================
// MCAP Chunk Size Configuration
// ============================================================================

/**
 * @brief Default MCAP chunk size in bytes (768 KiB).
 *
 * This matches the upstream MCAP library default. A chunk is the unit of
 * compression and indexing in MCAP files. Messages are accumulated into
 * chunks until this threshold is reached.
 */
constexpr uint64_t kDefaultChunkSize = 1024 * 768;  // 786,432 bytes = 768 KiB

/**
 * @brief Minimum allowed chunk size (1 MiB).
 *
 * Chunks smaller than this create excessive indexing overhead and slow
 * down sequential reading. This is enforced during auto-optimization.
 */
constexpr uint64_t kMinChunkSize = 1024 * 1024;  // 1,048,576 bytes = 1 MiB

/**
 * @brief Maximum allowed chunk size (32 MiB).
 *
 * Very large chunks increase memory requirements for readers and may
 * cause issues with memory-constrained systems and coarse buffering.
 * This upper bound ensures reasonable memory usage and smoother playback.
 */
constexpr uint64_t kMaxChunkSize = 32 * 1024 * 1024;  // 33,554,432 bytes = 32 MiB

/**
 * @brief Recommended chunk size for optimized playback (32 MiB).
 *
 * Based on real-world testing with Lichtblick and similar MCAP viewers,
 * 32 MiB chunks provide a good balance between compression efficiency
 * and buffering performance for typical OSI trace files.
 */
constexpr uint64_t kRecommendedChunkSize = 32 * 1024 * 1024;  // 33,554,432 bytes = 32 MiB

// ============================================================================
// Auto-Optimization Configuration
// ============================================================================

/**
 * @brief Target duration per chunk in seconds for auto-optimization.
 *
 * When auto-optimizing chunk size, the algorithm aims to pack approximately
 * this many seconds of data into each chunk. This provides smooth buffering
 * during playback - each chunk read loads ~1 second of data.
 *
 * Rationale: Video players typically buffer a few seconds ahead. Aligning
 * chunk boundaries with time makes seek operations efficient and predictable.
 */
constexpr double kTargetChunkDurationSeconds = 1.0;

/**
 * @brief Minimum target duration per chunk in seconds.
 *
 * For high-frequency, small-message traces, we don't want chunks smaller
 * than this duration to avoid excessive chunk count.
 */
constexpr double kMinChunkDurationSeconds = 0.5;

/**
 * @brief Maximum target duration per chunk in seconds.
 *
 * For low-frequency traces, we cap chunk duration to ensure reasonable
 * chunk sizes and memory usage.
 */
constexpr double kMaxChunkDurationSeconds = 5.0;

/**
 * @brief Number of messages to sample for statistics during analysis.
 *
 * When analyzing an OSI file to determine optimal settings, we sample
 * this many messages evenly across the file. This provides a balance
 * between accuracy and analysis speed for variable-size traces.
 *
 * Set to 0 for full file scan (slower but more accurate for variable-rate traces).
 */
constexpr size_t kAnalysisSampleSize = 100;

/**
 * @brief Minimum messages required for reliable analysis.
 *
 * If a file has fewer messages than this, analysis results may be unreliable
 * and a warning should be issued.
 */
constexpr size_t kMinMessagesForReliableAnalysis = 10;

// ============================================================================
// Compression Configuration
// ============================================================================

/**
 * @brief Whether to use compression by default.
 *
 * Zstd compression typically achieves 10-20x compression ratios on OSI
 * protobuf data with minimal CPU overhead during decompression.
 */
constexpr bool kDefaultUseCompression = true;

/**
 * @brief Minimum average message size (bytes) to recommend compression.
 *
 * Very small messages don't compress well and the overhead may not be
 * worth it. Below this threshold, consider disabling compression.
 */
constexpr size_t kMinMessageSizeForCompression = 1024;  // 1 KiB

/**
 * @brief Compression ratio threshold for recommending compression.
 *
 * If estimated compression ratio is below this value, compression
 * might not provide significant benefits (1.5 = 33% size reduction).
 */
constexpr double kMinCompressionRatioThreshold = 1.5;

// ============================================================================
// Binary OSI Format Constants
// ============================================================================

/**
 * @brief Size of the message length prefix in binary OSI files.
 *
 * Binary .osi files use a simple format: each message is preceded by
 * a 4-byte little-endian unsigned integer indicating the message size.
 */
constexpr size_t kBinaryOsiMessageLengthPrefixSize = sizeof(uint32_t);

/**
 * @brief Maximum expected single message size (sanity check).
 *
 * OSI messages can be large (especially SensorView with many objects),
 * but anything larger than this is likely a corrupted file or format error.
 */
constexpr size_t kMaxExpectedMessageSize = 512 * 1024 * 1024;  // 512 MiB

// ============================================================================
// Frame Rate Estimation
// ============================================================================

/**
 * @brief Expected minimum frame rate in Hz.
 *
 * Traces below this rate are unusual and may indicate timestamp issues.
 */
constexpr double kMinExpectedFrameRateHz = 1.0;

/**
 * @brief Expected maximum frame rate in Hz.
 *
 * Traces above this rate are unusual (1000 Hz = 1ms intervals).
 */
constexpr double kMaxExpectedFrameRateHz = 1000.0;

/**
 * @brief Default assumed frame rate when estimation fails.
 *
 * If frame rate cannot be determined (e.g., single message file),
 * assume this typical automotive sensor rate.
 */
constexpr double kDefaultAssumedFrameRateHz = 100.0;

}  // namespace config
}  // namespace tracefile
}  // namespace osi3

#endif  // OSIUTILITIES_TRACEFILE_TRACEFILECONFIG_H_
