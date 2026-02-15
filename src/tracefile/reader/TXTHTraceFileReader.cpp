//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/reader/TXTHTraceFileReader.h"

#include <filesystem>

#include "osi-utilities/tracefile/TraceFileConfig.h"

namespace osi3 {

TXTHTraceFileReader::~TXTHTraceFileReader() {
    if (trace_file_.is_open()) {
        Close();
    }
}

auto TXTHTraceFileReader::Open(const std::filesystem::path& file_path) -> bool {
    // prevent opening again if already opened
    if (trace_file_.is_open()) {
        std::cerr << "ERROR: Opening file " << file_path << ", reader has already a file opened" << std::endl;
        return false;
    }

    // check if at least .txth ending is present
    if (file_path.extension().string() != ".txth") {
        std::cerr << "ERROR: The trace file '" << file_path << "' must have a '.txth' extension." << std::endl;
        return false;
    }

    // check if file exists
    if (!exists(file_path)) {
        std::cerr << "ERROR: The trace file '" << file_path << "' does not exist." << std::endl;
        return false;
    }

    // Determine message type based on filename if not specified in advance
    if (message_type_ == ReaderTopLevelMessage::kUnknown) {
        for (const auto& [key, value] : kFileNameMessageTypeMap) {
            if (file_path.filename().string().find(key) != std::string::npos) {
                message_type_ = value;
                break;
            }
        }
    }

    if (message_type_ == ReaderTopLevelMessage::kUnknown) {
        std::cerr << "ERROR: Unable to determine message type from filename." << std::endl;
        return false;
    }

    parser_ = kParserMap_.at(message_type_);

    trace_file_ = std::ifstream(file_path);

    // find top-level message delimiter by peeking into the file and assuming the first line
    // will be the pattern to indicate a new message
    std::getline(trace_file_, line_indicating_msg_start_);
    trace_file_.seekg(0);

    return trace_file_.is_open();
}

auto TXTHTraceFileReader::Open(const std::filesystem::path& file_path, const ReaderTopLevelMessage message_type) -> bool {
    message_type_ = message_type;
    return Open(file_path);
}

void TXTHTraceFileReader::Close() { trace_file_.close(); }

auto TXTHTraceFileReader::HasNext() -> bool { return (trace_file_ && trace_file_.is_open() && trace_file_.peek() != EOF); }

auto TXTHTraceFileReader::ReadMessage() -> std::optional<ReadResult> {
    // check if ready and if there are messages left
    if (!this->HasNext()) {
        std::cerr << "Unable to read message: No more messages available in trace file or file not opened." << std::endl;
        return std::nullopt;
    }

    const std::string text_message = ReadNextMessageFromFile();
    if (text_message.empty()) {
        return std::nullopt;
    }

    ReadResult result;
    result.message = parser_(text_message);
    result.message_type = message_type_;
    return result;
}

auto TXTHTraceFileReader::ReadNextMessageFromFile() -> std::string {
    std::string message;
    message.reserve(tracefile::config::kTxthReadBufferReserveSize);
    std::string line;
    std::streampos last_position = 0;

    // make sure the first line starts with line_indicating_msg_start_
    // read everything until:
    //   - 1. the next occurrence of line_indicating_msg_start_ and do not include this line
    //   - 2. the end of the file
    // append content message
    std::getline(trace_file_, line);
    message += line;
    line = "";
    while (!trace_file_.eof() && line != line_indicating_msg_start_) {
        message += line;
        message += '\n';
        // Get current position
        last_position = trace_file_.tellg();
        // read next line
        std::getline(trace_file_, line);
    }
    if (!trace_file_.eof()) {  // go back to the line before line_indicating_msg_start_
        trace_file_.seekg(last_position, std::ios_base::beg);
    }

    return message;
}

}  // namespace osi3
