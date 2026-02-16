//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/reader/MCAPTraceFileReader.h"

#include <filesystem>

namespace osi3 {

auto MCAPTraceFileReader::Open(const std::filesystem::path& file_path) -> bool {
    // prevent opening again if already opened
    if (message_view_ != nullptr) {
        std::cerr << "ERROR: Opening file " << file_path << ", reader has already a file opened" << std::endl;
        return false;
    }

    // check if file exists
    if (!exists(file_path)) {
        std::cerr << "ERROR: The trace file '" << file_path << "' does not exist." << std::endl;
        return false;
    }

    // open but instead of mcap::McapReader::open(std::string_view filename) use internal ifstream to support
    // even the strangest paths with std::filesystem::path
    trace_file_ = std::ifstream(file_path, std::ios::binary);
    if (!trace_file_) {
        std::cerr << "ERROR: Failed to open file stream: " << file_path << std::endl;
        return false;
    }
    if (const auto status = mcap_reader_.open(trace_file_); !status.ok()) {
        std::cerr << "ERROR: Failed to open MCAP file: " << status.message << std::endl;
        trace_file_.close();
        return false;
    }
    message_view_ = std::make_unique<mcap::LinearMessageView>(mcap_reader_.readMessages(OnProblem, mcap_options_));
    message_iterator_ = std::make_unique<mcap::LinearMessageView::Iterator>(message_view_->begin());
    return true;
}

auto MCAPTraceFileReader::Open(const std::string& file_path, const mcap::ReadMessageOptions& options) -> bool {
    mcap_options_ = options;
    return this->Open(file_path);
}

auto MCAPTraceFileReader::ReadMessage() -> std::optional<ReadResult> {
    while (this->HasNext()) {
        const auto& msg_view = **message_iterator_;
        const auto msg = msg_view.message;
        const auto channel = msg_view.channel;
        const auto schema = msg_view.schema;

        // this function only supports osi3 protobuf messages
        if (schema->encoding != "protobuf" || schema->name.size() < 5 || schema->name.substr(0, 5) != "osi3.") {
            if (skip_non_osi_msgs_) {
                ++*message_iterator_;
                continue;
            }
            throw std::runtime_error("Unsupported message encoding: " + schema->encoding + ". Only OSI3 protobuf is supported.");
        }

        // deserialize message into pre-known osi top-level message
        // use a map based on the schema->name (which is the full protobuf message name according to the osi spec)
        // the map returns the right deserializer function which was instantiated from a template
        // the map also directly returns the top-level message type for the result struct
        ReadResult result;
        auto deserializer_it = deserializer_map_.find(schema->name);
        if (deserializer_it != deserializer_map_.end()) {
            const auto& [deserialize_fn, message_type] = deserializer_it->second;
            result.message = deserialize_fn(msg);
            result.message_type = message_type;
            result.channel_name = channel->topic;
        } else {
            throw std::runtime_error("Unsupported OSI message type: " + schema->name);
        }

        ++*message_iterator_;
        return result;
    }

    std::cerr << "Unable to read message: No more messages available in trace file or file not opened." << std::endl;
    return std::nullopt;
}

void MCAPTraceFileReader::Close() {
    message_iterator_.reset();
    message_view_.reset();
    mcap_reader_.close();
    trace_file_.close();
}

auto MCAPTraceFileReader::HasNext() -> bool {
    // not opened yet
    if (!(trace_file_ && trace_file_.is_open())) {
        return false;
    }
    if (!message_iterator_) {
        return false;
    }
    // no more messages
    if (*message_iterator_ == message_view_->end()) {
        return false;
    }
    return true;
}

void MCAPTraceFileReader::OnProblem(const mcap::Status& status) { std::cerr << "ERROR: The following MCAP problem occurred: " << status.message; }

}  // namespace osi3
