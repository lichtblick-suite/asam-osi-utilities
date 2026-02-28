//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#ifndef OSIUTILITIES_TRACEFILE_READER_SINGLECHANNELBINARYTRACEFILEREADER_H_
#define OSIUTILITIES_TRACEFILE_READER_SINGLECHANNELBINARYTRACEFILEREADER_H_

#include <fstream>
#include <functional>

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
 * @brief Implementation of TraceFileReader for binary format files containing OSI messages
 *
 * This class provides functionality to read OSI messages in the single binary channel format.
 *
 * @note Thread Safety: Instances are **not** thread-safe.
 */
class SingleChannelBinaryTraceFileReader final : public osi3::TraceFileReader {
   public:
    /** @brief Destructor, closes the file if still open */
    ~SingleChannelBinaryTraceFileReader() override;

    /**
     * @brief Opens a trace file for reading and infers message type from filename
     * @param file_path Path to the trace file
     * @return true if successful, false otherwise
     */
    bool Open(const std::filesystem::path& file_path) override;

    /**
     * @brief Opens a trace file with specified message type
     * @param file_path Path to the trace file
     * @param message_type Expected message type in the file
     * @return true if successful, false otherwise
     */
    bool Open(const std::filesystem::path& file_path, ReaderTopLevelMessage message_type);
    /**
     * @brief Reads the next message from the trace file
     * @return Optional ReadResult containing the message if available
     */
    std::optional<ReadResult> ReadMessage() override;

    /**
     * @brief Closes the trace file
     */
    void Close() override;

    /**
     * @brief Checks whether more messages are available
     * @return true if there are more messages to read, false otherwise
     */
    bool HasNext() override;

    /**
     * @brief Gets the current message type being read
     * @return The message type enum value
     */
    ReaderTopLevelMessage GetMessageType() const { return message_type_; };

   private:
    /**
     * @brief Function type for parsing binary buffers (as char vector) into protobuf objects
     */
    using MessageParserFunc = std::function<std::unique_ptr<google::protobuf::Message>(const std::vector<char>&)>;

    std::ifstream trace_file_;                                            /**< File stream for reading */
    MessageParserFunc parser_;                                            /**< Message parsing function */
    ReaderTopLevelMessage message_type_{ReaderTopLevelMessage::kUnknown}; /**< Current message type */
    std::vector<char> read_buffer_;                                       /**< Reusable read buffer to avoid per-message allocation */

    /**
     * @brief Reads raw binary message data from file into the internal buffer
     * @return Reference to the internal buffer containing the raw message bytes
     */
    const std::vector<char>& ReadNextMessageFromFile();

    /**
     * @brief Parses a binary buffer into the requested protobuf message type
     * @tparam T Protobuf message type to parse into
     * @param data Raw message bytes
     * @return Unique pointer to the parsed message
     * @throws std::runtime_error if parsing fails
     */
    template <typename T>
    std::unique_ptr<google::protobuf::Message> ParseMessage(const std::vector<char>& data) {
        auto msg = std::make_unique<T>();
        if (!msg->ParseFromArray(data.data(), static_cast<int>(data.size()))) {
            throw std::runtime_error("Failed to parse message");
        }
        return std::move(msg);
    }

    /**
     * @brief Creates a parser function for a specific message type
     * @tparam T Protobuf message type to parse into
     * @return MessageParserFunc that can parse the specified message type
     */
    template <typename T>
    MessageParserFunc CreateParser() {
        return [this](const std::vector<char>& data) { return ParseMessage<T>(data); };
    }

    /** @brief Map of message types to parser functions. */
    const std::unordered_map<ReaderTopLevelMessage, MessageParserFunc> kParserMap_ = {{ReaderTopLevelMessage::kGroundTruth, CreateParser<GroundTruth>()},
                                                                                      {ReaderTopLevelMessage::kSensorData, CreateParser<SensorData>()},
                                                                                      {ReaderTopLevelMessage::kSensorView, CreateParser<SensorView>()},
                                                                                      {ReaderTopLevelMessage::kSensorViewConfiguration, CreateParser<SensorViewConfiguration>()},
                                                                                      {ReaderTopLevelMessage::kHostVehicleData, CreateParser<HostVehicleData>()},
                                                                                      {ReaderTopLevelMessage::kTrafficCommand, CreateParser<TrafficCommand>()},
                                                                                      {ReaderTopLevelMessage::kTrafficCommandUpdate, CreateParser<TrafficCommandUpdate>()},
                                                                                      {ReaderTopLevelMessage::kTrafficUpdate, CreateParser<TrafficUpdate>()},
                                                                                      {ReaderTopLevelMessage::kMotionRequest, CreateParser<MotionRequest>()},
                                                                                      {ReaderTopLevelMessage::kStreamingUpdate, CreateParser<StreamingUpdate>()}};
};
}  // namespace osi3

#endif  // OSIUTILITIES_TRACEFILE_READER_SINGLECHANNELBINARYTRACEFILEREADER_H_
