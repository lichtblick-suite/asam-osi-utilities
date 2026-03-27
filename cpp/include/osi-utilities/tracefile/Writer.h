//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#ifndef OSIUTILITIES_TRACEFILE_WRITER_H_
#define OSIUTILITIES_TRACEFILE_WRITER_H_

#include <google/protobuf/message.h>

#include <filesystem>
#include <memory>
#include <string>

namespace osi3 {

/**
 * @brief Abstract base class for writing trace files in various formats
 *
 * This class provides an interface for writing protobuf messages to trace files.
 * Different implementations can support various file formats like .osi, .mcap or .txth.
 *
 * The WriteMessage() method accepts any protobuf message via the type-erased
 * google::protobuf::Message base class. For MCAP format, the topic parameter
 * selects the channel; for single-channel formats (.osi, .txth) it is ignored.
 *
 * Concrete implementations also provide template<typename T> WriteMessage()
 * overloads for compile-time type safety when the message type is known.
 *
 * @note Thread Safety: Instances are **not** thread-safe.
 * Concurrent calls on the same writer must be externally synchronized.
 *
 * @note Error Strategy: `Open` returns `false` and logs to `std::cerr` on failure.
 * `WriteMessage` returns `false` on write errors.
 */
class TraceFileWriter {
   public:
    /** @brief Virtual destructor */
    virtual ~TraceFileWriter() = default;

    /** @brief Default constructor */
    TraceFileWriter() = default;

    /** @brief Deleted copy constructor */
    TraceFileWriter(const TraceFileWriter&) = delete;

    /** @brief Deleted copy assignment operator */
    TraceFileWriter& operator=(const TraceFileWriter&) = delete;

    /** @brief Deleted move constructor */
    TraceFileWriter(TraceFileWriter&&) = delete;

    /** @brief Deleted move assignment operator */
    TraceFileWriter& operator=(TraceFileWriter&&) = delete;

    /**
     * @brief Opens a file for writing
     * @param file_path Path to the file to be created/opened
     * @return true if successful, false otherwise
     */
    virtual bool Open(const std::filesystem::path& file_path) = 0;

    /**
     * @brief Writes a protobuf message to the trace file
     *
     * Type-erased write method that accepts any protobuf message. The behavior
     * of the topic parameter depends on the writer implementation:
     * - **MCAP**: topic selects the channel. If the topic has not been registered
     *   via AddChannel(), it is auto-registered using the message's descriptor.
     *   Required file metadata is also auto-added on first write if not set.
     * - **Binary (.osi)** and **Text (.txth)**: topic is ignored (single-channel formats).
     *
     * @param message The protobuf message to write
     * @param topic Channel/topic name (required for MCAP, ignored for .osi/.txth)
     * @return true if successful, false otherwise
     */
    virtual bool WriteMessage(const google::protobuf::Message& message, const std::string& topic = "") = 0;

    /**
     * @brief Closes the trace file
     */
    virtual void Close() = 0;
};

/**
 * @brief Factory class for creating trace file writers based on file extensions
 *
 * Mirrors TraceFileReaderFactory. Creates the appropriate writer implementation
 * based on the file extension, allowing format-agnostic writing through the
 * TraceFileWriter base class.
 */
class TraceFileWriterFactory {
   public:
    /**
     * @brief Creates a writer instance based on the file extension
     * @param file_path Path to the trace file
     * @return Unique pointer to a TraceFileWriter instance
     * @throws std::invalid_argument if the file extension is not supported
     *
     * Supported formats:
     * - .osi: Single channel binary format (SingleChannelBinaryTraceFileWriter)
     * - .txth: Single channel human-readable format (TXTHTraceFileWriter)
     * - .mcap: Multi channel binary format (MCAPTraceFileWriter)
     *
     * @note It is still required to call Open(path) on the returned writer instance
     */
    static std::unique_ptr<TraceFileWriter> createWriter(const std::filesystem::path& file_path);
};

}  // namespace osi3
#endif
