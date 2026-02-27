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
 * @brief Default MCAP chunk size in bytes (16 MiB).
 *
 * Real-world testing with Lichtblick and similar MCAP viewers shows that
 * 4-32 MiB chunks provide the best playback performance for OSI trace files.
 * 16 MiB is a good middle ground: large enough for efficient compression
 * and smooth buffering, small enough to avoid excessive memory usage.
 */
constexpr uint64_t kDefaultChunkSize = 16 * 1024 * 1024;  // 16,777,216 bytes = 16 MiB

/**
 * @brief Minimum allowed chunk size (1 MiB).
 *
 * Chunks smaller than this create excessive indexing overhead and slow
 * down sequential reading.
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

// ============================================================================
// Time Constants
// ============================================================================

/** @brief Number of nanoseconds in one second. */
constexpr uint64_t kNanosecondsPerSecond = 1'000'000'000;

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
// TXTH Format Constants
// ============================================================================

/** @brief Initial string reserve size when reading a TXTH text message. */
constexpr size_t kTxthReadBufferReserveSize = 4096;

}  // namespace config
}  // namespace tracefile
}  // namespace osi3

#endif  // OSIUTILITIES_TRACEFILE_TRACEFILECONFIG_H_
