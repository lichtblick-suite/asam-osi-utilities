//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/reader/SingleChannelBinaryTraceFileReader.h"

#include <filesystem>

#include "osi-utilities/tracefile/TraceFileConfig.h"

namespace osi3 {

auto SingleChannelBinaryTraceFileReader::Open(const std::filesystem::path& file_path, const ReaderTopLevelMessage message_type) -> bool {
    message_type_ = message_type;
    return this->Open(file_path);
}

auto SingleChannelBinaryTraceFileReader::Open(const std::filesystem::path& file_path) -> bool {
    // prevent opening again if already opened
    if (trace_file_.is_open()) {
        std::cerr << "ERROR: Opening file " << file_path << ", reader has already a file opened" << std::endl;
        return false;
    }

    // check if at least .osi ending is present
    if (file_path.extension().string() != ".osi") {
        std::cerr << "ERROR: The trace file '" << file_path << "' must have a '.osi' extension." << std::endl;
        return false;
    }

    // check if file exists
    if (!exists(file_path)) {
        std::cerr << "ERROR: The trace file '" << file_path << "' does not exist." << std::endl;
        return false;
    }

    // Determine message type based on filename if not specified in advance
    auto message_type_by_filename = ReaderTopLevelMessage::kUnknown;
    for (const auto& [key, value] : kFileNameMessageTypeMap) {
        if (file_path.filename().string().find(key) != std::string::npos) {
            message_type_by_filename = value;
            break;
        }
    }
    if (message_type_ != ReaderTopLevelMessage::kUnknown) {  // set manually by user
        // check if message_type_by_filename is the same as the one specified by the user
        if (message_type_ != message_type_by_filename) {
            std::cerr << "WARNING: The trace file '" << file_path
                      << "' has a filename that suggests a different message type than the one specified when opening the file (e.g. manually by the user). Using the manually "
                         "specified message type."
                      << std::endl;
        }
    } else {
        message_type_ = message_type_by_filename;
    }
    // if message_type_ is still unknown, return false
    if (message_type_ == ReaderTopLevelMessage::kUnknown) {
        std::cerr << "ERROR: Unable to determine message type from the filename '" << file_path
                  << "'. Please ensure the filename follows the recommended OSI naming conventions as specified in the documentation or specify the message type manually."
                  << std::endl;
        return false;
    }

    parser_ = kParserMap_.at(message_type_);

    trace_file_ = std::ifstream(file_path, std::ios::binary);
    if (!trace_file_) {
        std::cerr << "ERROR: Failed to open trace file: " << file_path << std::endl;
        return false;
    }
    return true;
}

void SingleChannelBinaryTraceFileReader::Close() { trace_file_.close(); }

auto SingleChannelBinaryTraceFileReader::HasNext() -> bool { return (trace_file_ && trace_file_.is_open() && trace_file_.peek() != EOF); }

auto SingleChannelBinaryTraceFileReader::ReadMessage() -> std::optional<ReadResult> {
    // check if ready and if there are messages left
    if (!this->HasNext()) {
        std::cerr << "Unable to read message: No more messages available in trace file or file not opened." << std::endl;
        return std::nullopt;
    }

    const auto serialized_msg = ReadNextMessageFromFile();

    if (serialized_msg.empty()) {
        throw std::runtime_error("Failed to read message");
    }

    ReadResult result;
    result.message = parser_(serialized_msg);
    result.message_type = message_type_;

    return result;
}

auto SingleChannelBinaryTraceFileReader::ReadNextMessageFromFile() -> std::vector<char> {
    uint32_t message_size = 0;

    if (!trace_file_.read(reinterpret_cast<char*>(&message_size), sizeof(message_size))) {
        throw std::runtime_error("ERROR: Failed to read message size from file.");
    }
    if (message_size == 0 || message_size > tracefile::config::kMaxExpectedMessageSize) {
        throw std::runtime_error("ERROR: Invalid message size: " + std::to_string(message_size));
    }
    std::vector<char> serialized_msg(message_size);
    if (!trace_file_.read(serialized_msg.data(), message_size)) {
        throw std::runtime_error("ERROR: Failed to read message from file");
    }
    return serialized_msg;
}

}  // namespace osi3
