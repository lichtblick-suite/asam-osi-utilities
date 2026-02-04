//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#ifndef OSIUTILITIES_TRACEFILE_WRITER_TXTHTraceFileWriter_H_
#define OSIUTILITIES_TRACEFILE_WRITER_TXTHTraceFileWriter_H_

#include <fstream>

#include "../Writer.h"

namespace osi3 {

/**
 * @brief Implementation of TraceFileWriter for text format files containing OSI messages
 *
 * This class provides functionality to write OSI messages to text format files.
 * It converts protobuf messages to their text representation for human-readable storage.
 */
class TXTHTraceFileWriter final : public TraceFileWriter {
   public:
    bool Open(const std::filesystem::path& file_path) override;
    void Close() override;

    /**
     * @brief Writes a protobuf message to the file
     * @tparam T Type of the protobuf message
     * @param top_level_message The message to write
     * @return true if successful, false otherwise
     */
    template <typename T>
    bool WriteMessage(const T& top_level_message);

   private:
    std::ofstream trace_file_;
};

}  // namespace osi3
#endif  // OSIUTILITIES_TRACEFILE_WRITER_TXTHTraceFileWriter_H_
