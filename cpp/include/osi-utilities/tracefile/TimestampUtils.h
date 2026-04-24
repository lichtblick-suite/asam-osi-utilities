//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#ifndef OSIUTILITIES_TRACEFILE_TIMESTAMPUTILS_H_
#define OSIUTILITIES_TRACEFILE_TIMESTAMPUTILS_H_

#include <google/protobuf/descriptor.h>
#include <google/protobuf/message.h>

#include <cmath>
#include <cstdint>
#include <limits>
#include <stdexcept>

#include "osi-utilities/tracefile/TraceFileConfig.h"

namespace osi3 {
namespace tracefile {

/**
 * @brief Extract timestamp from a protobuf message as nanoseconds.
 *
 * Converts the seconds and nanos fields of a protobuf message's timestamp
 * to a single uint64_t value in nanoseconds. The message type must have a
 * timestamp() accessor returning an object with seconds() and nanos() methods
 * (i.e. any OSI top-level message).
 *
 * @tparam T Protobuf message type with timestamp().seconds() and timestamp().nanos()
 * @param message The protobuf message to extract the timestamp from
 * @return Timestamp in nanoseconds as uint64_t
 * @throws std::out_of_range if the timestamp is negative or exceeds uint64 nanosecond range
 */
template <typename T>
auto TimestampToNanoseconds(const T& message) -> uint64_t {
    const auto seconds = message.timestamp().seconds();
    const auto nanos = static_cast<uint64_t>(message.timestamp().nanos());
    if (seconds < 0) {
        throw std::out_of_range("Timestamp seconds must be non-negative.");
    }

    const auto seconds_as_uint = static_cast<uint64_t>(seconds);
    if (seconds_as_uint > (std::numeric_limits<uint64_t>::max() - nanos) / config::kNanosecondsPerSecond) {
        throw std::out_of_range("Timestamp exceeds uint64 nanosecond range.");
    }

    return seconds_as_uint * config::kNanosecondsPerSecond + nanos;
}

/**
 * @brief Extract timestamp from a protobuf message as floating-point seconds.
 *
 * @tparam T Protobuf message type with timestamp().seconds() and timestamp().nanos()
 * @param message The protobuf message to extract the timestamp from
 * @return Timestamp in seconds as double
 */
template <typename T>
auto TimestampToSeconds(const T& message) -> double {
    return static_cast<double>(message.timestamp().seconds()) + static_cast<double>(message.timestamp().nanos()) / static_cast<double>(config::kNanosecondsPerSecond);
}

/**
 * @brief Convert nanoseconds to floating-point seconds.
 *
 * @param nanoseconds Time value in nanoseconds
 * @return Time in seconds as double
 */
inline auto NanosecondsToSeconds(const uint64_t nanoseconds) -> double { return static_cast<double>(nanoseconds) / static_cast<double>(config::kNanosecondsPerSecond); }

/**
 * @brief Convert floating-point seconds to nanoseconds.
 *
 * @param seconds Time value in seconds
 * @return Time in nanoseconds as uint64_t
 * @throws std::out_of_range if seconds is negative, not finite, or exceeds uint64 nanosecond range
 */
inline auto SecondsToNanoseconds(const double seconds) -> uint64_t {
    if (!std::isfinite(seconds) || seconds < 0.0) {
        throw std::out_of_range("Seconds must be finite and non-negative.");
    }

    constexpr auto kMaxSeconds = static_cast<double>(std::numeric_limits<uint64_t>::max()) / static_cast<double>(config::kNanosecondsPerSecond);
    if (seconds > kMaxSeconds) {
        throw std::out_of_range("Seconds exceed uint64 nanosecond range.");
    }

    return static_cast<uint64_t>(seconds * static_cast<double>(config::kNanosecondsPerSecond));
}

/**
 * @brief Extract timestamp from a type-erased protobuf message as nanoseconds.
 *
 * Uses protobuf reflection to access the timestamp field. Works with any
 * OSI top-level message type without requiring compile-time type information.
 * This enables polymorphic writing via the TraceFileWriter base class.
 *
 * @param message The protobuf message to extract the timestamp from
 * @return Timestamp in nanoseconds as uint64_t
 * @throws std::out_of_range if the message has no 'timestamp' field, or
 *         if the timestamp is negative or exceeds uint64 nanosecond range
 */
inline auto TimestampToNanoseconds(const google::protobuf::Message& message) -> uint64_t {
    const auto* descriptor = message.GetDescriptor();
    const auto* reflection = message.GetReflection();
    const auto* timestamp_field = descriptor->FindFieldByName("timestamp");
    if (!timestamp_field || timestamp_field->message_type() == nullptr) {
        throw std::out_of_range("Message type '" + descriptor->full_name() + "' does not have a 'timestamp' message field");
    }

    const auto& timestamp = reflection->GetMessage(message, timestamp_field);
    const auto* ts_descriptor = timestamp.GetDescriptor();
    const auto* ts_reflection = timestamp.GetReflection();

    const auto* seconds_field = ts_descriptor->FindFieldByName("seconds");
    const auto* nanos_field = ts_descriptor->FindFieldByName("nanos");
    if (!seconds_field || !nanos_field) {
        throw std::out_of_range("Timestamp message does not have 'seconds' and 'nanos' fields");
    }

    const auto seconds = ts_reflection->GetInt64(timestamp, seconds_field);
    const auto nanos = static_cast<uint64_t>(ts_reflection->GetUInt32(timestamp, nanos_field));

    if (seconds < 0) {
        throw std::out_of_range("Timestamp seconds must be non-negative.");
    }

    const auto seconds_as_uint = static_cast<uint64_t>(seconds);
    if (seconds_as_uint > (std::numeric_limits<uint64_t>::max() - nanos) / config::kNanosecondsPerSecond) {
        throw std::out_of_range("Timestamp exceeds uint64 nanosecond range.");
    }

    return seconds_as_uint * config::kNanosecondsPerSecond + nanos;
}

}  // namespace tracefile
}  // namespace osi3

#endif  // OSIUTILITIES_TRACEFILE_TIMESTAMPUTILS_H_
