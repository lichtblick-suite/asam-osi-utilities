//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/reader/MCAPTraceFileReader.h"

#include <cstdio>
#include <cstring>
#include <exception>
#include <filesystem>

namespace osi3 {

MCAPTraceFileReader::~MCAPTraceFileReader() noexcept {
    if (trace_file_.is_open()) {
        try {
            Close();
        } catch (const std::exception& error) {
            std::fputs("ERROR: Failed to close MCAP reader during destruction: ", stderr);
            std::fputs(error.what(), stderr);
            std::fputc('\n', stderr);
        } catch (...) {
            std::fputs("ERROR: Failed to close MCAP reader during destruction due to an unknown exception.\n", stderr);
        }
    }
}

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

    // Read summary to populate channels, schemas, and metadata indexes
    (void)mcap_reader_.readSummary(mcap::ReadSummaryMethod::AllowFallbackScan);

    // Pre-load file-level metadata records from the summary indexes
    file_metadata_.clear();
    auto* data_source = mcap_reader_.dataSource();
    if (data_source) {
        for (const auto& [name, index] : mcap_reader_.metadataIndexes()) {
            mcap::Record raw_record{};
            if (const auto read_status = mcap::McapReader::ReadRecord(*data_source, index.offset, &raw_record); read_status.ok()) {
                mcap::Metadata metadata;
                if (const auto parse_status = mcap::McapReader::ParseMetadata(raw_record, &metadata); parse_status.ok()) {
                    file_metadata_.emplace_back(metadata.name, metadata.metadata);
                }
            }
        }
    }

    ResetMessageIteration();
    return true;
}

auto MCAPTraceFileReader::Open(const std::filesystem::path& file_path, const mcap::ReadMessageOptions& options) -> bool {
    mcap_options_ = options;
    return this->Open(file_path);
}

auto MCAPTraceFileReader::ProcessMessageView(const mcap::MessageView& msg_view) -> std::optional<ReadResult> {
    const auto& channel = msg_view.channel;
    const auto& schema = msg_view.schema;

    if (!channel || !schema) {
        throw std::runtime_error("ERROR: MCAP message has null channel or schema pointer.");
    }

    // Check for incompatible messages (non-OSI protobuf encoding/schema)
    if (schema->encoding != "protobuf" || schema->name.size() < 5 || schema->name.substr(0, 5) != "osi3.") {
        return HandleIncompatibleMessage(
            channel->topic, schema->encoding != "protobuf" ? "Incompatible message encoding: " + schema->encoding : "Incompatible schema: " + schema->name + " (expected osi3.*)");
    }

    // Look up the deserializer for this OSI schema
    auto deserializer_it = deserializer_map_.find(schema->name);
    if (deserializer_it == deserializer_map_.end()) {
        return HandleIncompatibleMessage(channel->topic, "Unsupported OSI message type: " + schema->name);
    }

    // Deserialize the message
    const auto& [deserialize_fn, message_type] = deserializer_it->second;
    try {
        ReadResult result;
        result.message = deserialize_fn(msg_view.message);
        result.message_type = message_type;
        result.channel_name = channel->topic;
        result.status = ReadStatus::kOk;
        return result;
    } catch (const std::exception& e) {
        // Always log deserialization errors — these indicate data corruption, not schema mismatches
        std::cerr << "ERROR: Failed to deserialize message on topic '" << channel->topic << "': " << e.what() << std::endl;
        ReadResult result;
        result.status = ReadStatus::kError;
        result.error_message = std::string("Deserialization failed: ") + e.what();
        result.channel_name = channel->topic;
        result.message_type = message_type;
        return result;
    }
}

auto MCAPTraceFileReader::HandleIncompatibleMessage(const std::string& topic, const std::string& reason) const -> std::optional<ReadResult> {
    if (log_incompatible_msgs_) {
        std::cerr << "WARNING: " << reason << " on topic '" << topic << "'" << std::endl;
    }
    if (skip_incompatible_msgs_) {
        return std::nullopt;  // signal caller to skip
    }
    ReadResult result;
    result.status = ReadStatus::kIncompatible;
    result.error_message = reason;
    result.channel_name = topic;
    return result;
}

auto MCAPTraceFileReader::ReadMessage() -> std::optional<ReadResult> {
    while (this->HasNext()) {
        auto result = ProcessMessageView(**message_iterator_);
        ++*message_iterator_;

        if (!result.has_value()) {
            continue;  // message was skipped (incompatible + skip enabled)
        }
        return result;
    }

    return std::nullopt;
}

void MCAPTraceFileReader::Close() {
    message_iterator_.reset();
    message_view_.reset();
    mcap_reader_.close();
    trace_file_.close();
    file_metadata_.clear();
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

void MCAPTraceFileReader::SetTopics(const std::unordered_set<std::string>& topics) {
    filtered_topics_.assign(topics.begin(), topics.end());

    if (filtered_topics_.empty()) {
        mcap_options_.topicFilter = {};
    } else {
        mcap_options_.topicFilter = [this](const std::string_view topic) noexcept { return TopicMatches(topic); };
    }

    if (trace_file_.is_open()) {
        ResetMessageIteration();
    }
}

auto MCAPTraceFileReader::GetAvailableTopics() const -> std::vector<std::string> {
    std::vector<std::string> topics;
    if (!trace_file_.is_open()) {
        return topics;
    }

    const auto channels = mcap_reader_.channels();
    for (const auto& [id, channel_ptr] : channels) {
        if (channel_ptr) {
            topics.push_back(channel_ptr->topic);
        }
    }
    return topics;
}

auto MCAPTraceFileReader::GetFileMetadata() const -> std::vector<std::pair<std::string, std::unordered_map<std::string, std::string>>> { return file_metadata_; }

auto MCAPTraceFileReader::GetChannelMetadata(const std::string& topic) const -> std::optional<std::unordered_map<std::string, std::string>> {
    if (!trace_file_.is_open()) {
        return std::nullopt;
    }

    const auto channels = mcap_reader_.channels();
    for (const auto& [id, channel_ptr] : channels) {
        if (channel_ptr && channel_ptr->topic == topic) {
            return channel_ptr->metadata;
        }
    }
    return std::nullopt;
}

auto MCAPTraceFileReader::GetMessageTypeForTopic(const std::string& topic) const -> std::optional<ReaderTopLevelMessage> {
    if (!trace_file_.is_open()) {
        return std::nullopt;
    }

    const auto channels = mcap_reader_.channels();
    for (const auto& [id, channel_ptr] : channels) {
        if (channel_ptr && channel_ptr->topic == topic) {
            const auto schema_ptr = mcap_reader_.schema(channel_ptr->schemaId);
            if (!schema_ptr) {
                return std::nullopt;
            }

            const auto it = deserializer_map_.find(schema_ptr->name);
            if (it != deserializer_map_.end()) {
                return it->second.second;
            }
            return std::nullopt;
        }
    }
    return std::nullopt;
}

void MCAPTraceFileReader::OnProblem(const mcap::Status& status) { std::cerr << "ERROR: The following MCAP problem occurred: " << status.message; }

auto MCAPTraceFileReader::TopicMatches(const std::string_view topic) const noexcept -> bool {
    // Keep this as a manual loop so the MCAP topicFilter callback stays trivially noexcept.
    for (const auto& candidate : filtered_topics_) {  // NOLINT(readability-use-anyofallof)
        if (candidate.size() == topic.size() && std::memcmp(candidate.data(), topic.data(), topic.size()) == 0) {
            return true;
        }
    }
    return false;
}

void MCAPTraceFileReader::ResetMessageIteration() {
    message_iterator_.reset();
    message_view_.reset();

    if (!trace_file_.is_open()) {
        return;
    }

    message_view_ = std::make_unique<mcap::LinearMessageView>(mcap_reader_.readMessages(OnProblem, mcap_options_));
    message_iterator_ = std::make_unique<mcap::LinearMessageView::Iterator>(message_view_->begin());
}

}  // namespace osi3
