//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#ifndef OSIUTILITIES_TRACEFILE_READER_TXTHTRACEFILEREADER_H_
#define OSIUTILITIES_TRACEFILE_READER_TXTHTRACEFILEREADER_H_

#include <google/protobuf/text_format.h>

#include <fstream>
#include <functional>

#include "../Reader.h"
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
 * @brief Implementation of TraceFileReader for text format files containing OSI messages
 *
 * This class provides functionality to read and parse OSI messages from text format files.
 * It supports various OSI message types and handles their parsing using Google's protobuf TextFormat.
 */
class TXTHTraceFileReader final : public TraceFileReader {
    /**
     * @brief Function type for parsing protobuf TextFormat strings into protobuf objects
     */
    using MessageParserFunc = std::function<std::unique_ptr<google::protobuf::Message>(const std::string&)>;

   public:
    bool Open(const std::filesystem::path& file_path) override;
    /**
     * @brief Opens a trace file with specified message type
     * @param file_path Path to the trace file
     * @param message_type Expected message type in the file
     * @return true if successful, false otherwise
     */
    bool Open(const std::filesystem::path& file_path, ReaderTopLevelMessage message_type);
    void Close() override;
    bool HasNext() override;
    std::optional<ReadResult> ReadMessage() override;

   private:
    std::ifstream trace_file_;
    MessageParserFunc parser_;
    std::string line_indicating_msg_start_;
    ReaderTopLevelMessage message_type_{ReaderTopLevelMessage::kUnknown};
    /**
     * @brief Reads the next complete message from the trace file
     * @return String containing the complete message in text format
     */
    std::string ReadNextMessageFromFile();

    /**
     * @brief Template function to parse text format messages into specific OSI message types
     * @tparam T The OSI message type to parse into
     * @param data The text format message data
     * @return Unique pointer to the parsed protobuf message
     * @throws std::runtime_error if parsing fails
     */
    template <typename T>
    std::unique_ptr<google::protobuf::Message> ParseMessage(const std::string& data) {
        auto msg = std::make_unique<T>();
        if (!google::protobuf::TextFormat::ParseFromString(data, msg.get())) {
            throw std::runtime_error("Failed to parse message");
        }
        return std::move(msg);
    }

    /**
     * @brief Creates a parser function for a specific message type
     * @tparam T The OSI message type to create a parser for
     * @return MessageParserFunc that can parse the specified message type
     */
    template <typename T>
    MessageParserFunc CreateParser() {
        return [this](const std::string& data) { return ParseMessage<T>(data); };
    }

    /**
     * @brief Map containing message type specific parser functions
     *
     * Maps ReaderTopLevelMessage enums to corresponding parser functions for each OSI message type.
     */
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

#endif  // OSIUTILITIES_TRACEFILE_READER_TXTHTRACEFILEREADER_H_
