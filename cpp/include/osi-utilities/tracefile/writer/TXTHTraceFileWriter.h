//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#ifndef OSIUTILITIES_TRACEFILE_WRITER_TXTHTRACEFILEWRITER_H_
#define OSIUTILITIES_TRACEFILE_WRITER_TXTHTRACEFILEWRITER_H_

#include <fstream>

#include "osi-utilities/tracefile/Writer.h"

namespace osi3 {

/**
 * @brief Implementation of TraceFileWriter for text format files containing OSI messages
 *
 * This class provides functionality to write OSI messages to text format files.
 * It converts protobuf messages to their text representation for human-readable storage.
 *
 * @deprecated The `.txth` text format is intended for human consumption only (debugging,
 * manual inspection). According to the OSI specification, it is "not unambiguously
 * deserializable" — protobuf text format output is not guaranteed to be stable across
 * library versions, and round-tripping (write then read) is unreliable. Prefer `.osi`
 * (binary) for single-channel or `.mcap` for multi-channel trace files.
 *
 * @note Thread Safety: Not thread-safe. External synchronization required for concurrent access.
 */
class [[deprecated("txth format is not reliably deserializable; use .osi or .mcap instead")]] TXTHTraceFileWriter final : public TraceFileWriter {
   public:
    /** @brief Destructor, closes the file if still open */
    ~TXTHTraceFileWriter() override;
    /**
     * @brief Opens a text trace file for writing
     * @param file_path Path to the file to be created/opened
     * @return true if successful, false otherwise
     */
    bool Open(const std::filesystem::path& file_path) override;

    /**
     * @brief Closes the trace file
     */
    void Close() override;

    /**
     * @brief Writes a protobuf message to the file (type-erased, virtual)
     *
     * The topic parameter is ignored for this single-channel format.
     *
     * @param message The protobuf message to write
     * @param topic Ignored (single-channel format)
     * @return true if successful, false otherwise
     */
    bool WriteMessage(const google::protobuf::Message& message, const std::string& topic = "") override;

    /**
     * @brief Writes a protobuf message to the file (compile-time typed)
     * @tparam T Type of the protobuf message
     * @param top_level_message The message to write
     * @return true if successful, false otherwise
     */
    template <typename T>
    bool WriteMessage(const T& top_level_message);

   private:
    std::ofstream trace_file_; /**< Output file stream. */
};

}  // namespace osi3
#endif  // OSIUTILITIES_TRACEFILE_WRITER_TXTHTRACEFILEWRITER_H_
