//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#ifndef OSIUTILITIES_TRACEFILE_READER_MCAPTRACEFILEREADER_H_
#define OSIUTILITIES_TRACEFILE_READER_MCAPTRACEFILEREADER_H_

#include <mcap/reader.hpp>
#include <string_view>
#include <unordered_set>
#include <vector>

#include "osi-utilities/tracefile/Reader.h"
#include "osi_groundtruth.pb.h"
#include "osi_hostvehicledata.pb.h"
#include "osi_motionrequest.pb.h"
#include "osi_sensordata.pb.h"
#include "osi_sensorview.pb.h"
#include "osi_sensorviewconfiguration.pb.h"
#include "osi_streamingupdate.pb.h"
#include "osi_trafficcommand.pb.h"
#include "osi_trafficcommandupdate.pb.h"
#include "osi_trafficupdate.pb.h"

namespace osi3 {

/**
 * @brief Implementation of TraceFileReader for MCAP format files containing OSI messages
 *
 * This class provides functionality to read and deserialize OSI messages from MCAP files.
 * It supports various OSI message types including GroundTruth, SensorData, SensorView, etc.
 *
 * @note Thread Safety: Instances are **not** thread-safe.
 */
class MCAPTraceFileReader final : public TraceFileReader {
   public:
    /** @brief Destructor, closes the file if still open */
    ~MCAPTraceFileReader() noexcept override;

    /**
     * @brief Opens a trace file for reading with default options
     * @param file_path Path to the file to be opened
     * @return true if successful, false otherwise
     */
    bool Open(const std::filesystem::path& file_path) override;

    /**
     * @brief Opens a trace file for reading
     *
     * This alternative opening method allows specifying additional options
     * like only requesting a certain topic
     *
     * @param file_path Path to the file to be opened
     * @param options Options for the MCAP writer
     * @return true if successful, false otherwise
     */
    bool Open(const std::filesystem::path& file_path, const mcap::ReadMessageOptions& options);

    /**
     * @brief Reads the next OSI message from the trace file
     * @return Optional ReadResult containing the message if available
     */
    std::optional<ReadResult> ReadMessage() override;

    /**
     * @brief Closes the trace file and releases reader resources
     */
    void Close() override;

    /**
     * @brief Checks whether more messages are available
     * @return true if there are more messages to read, false otherwise
     */
    bool HasNext() override;

    /**
     * @brief Sets whether to skip incompatible messages during reading
     * @param skip If true, incompatible messages (non-OSI encoding/schema) will be silently skipped.
     *             If false, they are returned as ReadResult with status == ReadStatus::kIncompatible.
     *
     * Incompatible messages include non-protobuf encodings and non-OSI schemas.
     * Actual deserialization errors are always surfaced as ReadResult with
     * status == ReadStatus::kError and are never skipped by this setting.
     */
    void SetSkipIncompatibleMessages(const bool skip) { skip_incompatible_msgs_ = skip; }

    /**
     * @brief Sets whether to log incompatible messages during reading
     * @param log If true (default), incompatible messages are logged to stderr.
     *            If false, they are silently skipped or returned without logging.
     *
     * This setting is independent of SetSkipIncompatibleMessages: logging and skipping
     * can be combined arbitrarily.
     */
    void SetLogIncompatibleMessages(const bool log) { log_incompatible_msgs_ = log; }

    /**
     * @brief Sets whether to skip non-OSI messages during reading
     * @param skip If true, non-OSI messages will be skipped during reading. If false, all messages will be processed
     *
     * If the file contains non-OSI messages and this option is not set to true, an exception will be thrown.
     * @deprecated Use SetSkipIncompatibleMessages() instead
     */
    [[deprecated("Use SetSkipIncompatibleMessages() instead")]] void SetSkipNonOSIMsgs(const bool skip) { SetSkipIncompatibleMessages(skip); }

    /**
     * @brief Set topic filter for message iteration.
     *
     * Can be called before or after Open(). If the reader is already open,
     * the message iteration restarts from the beginning with the updated filter.
     * Passing an empty set clears the topic filter and reads all topics.
     *
     * @param topics Set of topic names to include
     */
    void SetTopics(const std::unordered_set<std::string>& topics);

    /**
     * @brief Get all available topics in the opened MCAP file.
     * @return Vector of topic names, empty if file not opened
     */
    std::vector<std::string> GetAvailableTopics() const;

    /**
     * @brief Get file-level metadata records.
     *
     * Returns all metadata records from the MCAP file as a vector of
     * (name, key-value) pairs.
     *
     * @return Vector of pairs: (metadata_name, metadata_map)
     */
    std::vector<std::pair<std::string, std::unordered_map<std::string, std::string>>> GetFileMetadata() const;

    /**
     * @brief Get channel-specific metadata for a given topic.
     * @param topic The channel topic name
     * @return Metadata key-value map if found, std::nullopt otherwise
     */
    std::optional<std::unordered_map<std::string, std::string>> GetChannelMetadata(const std::string& topic) const;

    /**
     * @brief Get the OSI message type for a given topic.
     *
     * Looks up the channel's schema name and maps it to a ReaderTopLevelMessage.
     *
     * @param topic The channel topic name
     * @return The message type if found, std::nullopt otherwise
     */
    std::optional<ReaderTopLevelMessage> GetMessageTypeForTopic(const std::string& topic) const;

   private:
    std::ifstream trace_file_;                                            /**< File stream for reading */
    mcap::McapReader mcap_reader_;                                        /**< Upstream MCAP reader object */
    std::unique_ptr<mcap::LinearMessageView> message_view_;               /**< Message view over MCAP records. */
    std::unique_ptr<mcap::LinearMessageView::Iterator> message_iterator_; /**< Iterator over the message view. */

    bool skip_incompatible_msgs_ = false;   /**< Flag to skip incompatible messages during reading */
    bool log_incompatible_msgs_ = true;     /**< Flag to log incompatible messages to stderr */
    mcap::ReadMessageOptions mcap_options_; /**< Options for the mcap reader */

    /** @brief Cached file-level metadata records, populated during Open() */
    std::vector<std::pair<std::string, std::unordered_map<std::string, std::string>>> file_metadata_;

    /**
     * @brief Template function to deserialize MCAP messages into specific OSI message types
     * @tparam T The OSI message type to deserialize into
     * @param mcap_msg The MCAP message containing serialized data
     * @return Unique pointer to the deserialized OSI protobuf message
     * @throws std::runtime_error if deserialization fails
     */
    template <typename T>
    std::unique_ptr<google::protobuf::Message> Deserialize(const mcap::Message& mcap_msg) {
        auto output = std::make_unique<T>();
        if (!output->ParseFromArray(mcap_msg.data, mcap_msg.dataSize)) {
            throw std::runtime_error("Error: Failed to deserialize message.");
        }
        return output;
    }

    using DeserializeFunction = std::function<std::unique_ptr<google::protobuf::Message>(const mcap::Message& msg)>;
    /**
     * @brief Map containing message type specific deserializer functions and corresponding OSI message types
     *
     * Maps OSI message type strings to pairs of deserializer functions and ReaderTopLevelMessage enums
     * which are part of the ReadResult struct.Used for dynamic message deserialization
     * based on channel schema which is defined by the OSI specification.
     */
    const std::unordered_map<std::string, std::pair<DeserializeFunction, ReaderTopLevelMessage>> deserializer_map_ = {
        {"osi3.GroundTruth", {[this](const mcap::Message& msg) { return Deserialize<osi3::GroundTruth>(msg); }, ReaderTopLevelMessage::kGroundTruth}},
        {"osi3.SensorData", {[this](const mcap::Message& msg) { return Deserialize<osi3::SensorData>(msg); }, ReaderTopLevelMessage::kSensorData}},
        {"osi3.SensorView", {[this](const mcap::Message& msg) { return Deserialize<osi3::SensorView>(msg); }, ReaderTopLevelMessage::kSensorView}},
        {"osi3.SensorViewConfiguration",
         {[this](const mcap::Message& msg) { return Deserialize<osi3::SensorViewConfiguration>(msg); }, ReaderTopLevelMessage::kSensorViewConfiguration}},
        {"osi3.HostVehicleData", {[this](const mcap::Message& msg) { return Deserialize<osi3::HostVehicleData>(msg); }, ReaderTopLevelMessage::kHostVehicleData}},
        {"osi3.TrafficCommand", {[this](const mcap::Message& msg) { return Deserialize<osi3::TrafficCommand>(msg); }, ReaderTopLevelMessage::kTrafficCommand}},
        {"osi3.TrafficCommandUpdate", {[this](const mcap::Message& msg) { return Deserialize<osi3::TrafficCommandUpdate>(msg); }, ReaderTopLevelMessage::kTrafficCommandUpdate}},
        {"osi3.TrafficUpdate", {[this](const mcap::Message& msg) { return Deserialize<osi3::TrafficUpdate>(msg); }, ReaderTopLevelMessage::kTrafficUpdate}},
        {"osi3.MotionRequest", {[this](const mcap::Message& msg) { return Deserialize<osi3::MotionRequest>(msg); }, ReaderTopLevelMessage::kMotionRequest}},
        {"osi3.StreamingUpdate", {[this](const mcap::Message& msg) { return Deserialize<osi3::StreamingUpdate>(msg); }, ReaderTopLevelMessage::kStreamingUpdate}}};

    /**
     * @brief Internal callback function invoked by the MCAP reader when problems occur during reading operations
     * @param status The MCAP status object containing error information
     *
     * This is an internal method called by the underlying MCAP reader implementation
     * when it encounters issues during file reading operations.
     */
    static void OnProblem(const mcap::Status& status);

    /** @brief Recreate the message view using the current read options. */
    void ResetMessageIteration();

    /** @brief Check whether a topic matches the configured filter. */
    auto TopicMatches(std::string_view topic) const noexcept -> bool;

    /**
     * @brief Process a single MCAP message view and return a ReadResult.
     * @return ReadResult on success/error/incompatible, or std::nullopt if the message should be skipped
     */
    auto ProcessMessageView(const mcap::MessageView& msg_view) -> std::optional<ReadResult>;

    /**
     * @brief Handle an incompatible message (log, skip, or return kIncompatible).
     * @return ReadResult with kIncompatible status, or std::nullopt if skip is enabled
     */
    auto HandleIncompatibleMessage(const std::string& topic, const std::string& reason) const -> std::optional<ReadResult>;

    /** @brief Stable topic filter storage used by the MCAP callback. */
    std::vector<std::string> filtered_topics_;
};

/** @brief Alias for MCAPTraceFileReader matching Python naming convention */
using MultiTraceFileReader = MCAPTraceFileReader;

}  // namespace osi3
#endif
