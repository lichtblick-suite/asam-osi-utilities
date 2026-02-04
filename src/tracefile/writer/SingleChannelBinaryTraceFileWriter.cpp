//
// Copyright (c) 2024, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/writer/SingleChannelBinaryTraceFileWriter.h"

#include "osi_groundtruth.pb.h"
#include "osi_hostvehicledata.pb.h"
#include "osi_motionrequest.pb.h"
#include "osi_sensordata.pb.h"
#include "osi_sensorview.pb.h"
#include "osi_streamingupdate.pb.h"
#include "osi_trafficcommand.pb.h"
#include "osi_trafficcommandupdate.pb.h"
#include "osi_trafficupdate.pb.h"

namespace osi3 {

bool SingleChannelBinaryTraceFileWriter::Open(const std::filesystem::path& file_path) {
    // check if at least .osi ending is present
    if (file_path.extension().string() != ".osi") {
        std::cerr << "ERROR: The trace file '" << file_path << "' must have a '.osi' extension." << std::endl;
        return false;
    }

    // prevent opening again if already opened
    if (trace_file_.is_open()) {
        std::cerr << "ERROR: Opening file " << file_path << ", writer has already a file opened" << std::endl;
        return false;
    }

    // check if at least .osi ending is present
    if (file_path.extension().string() != ".osi") {
        std::cerr << "ERROR: The trace file '" << file_path << "' must have a '.osi' extension." << std::endl;
        return false;
    }

    trace_file_.open(file_path, std::ios::binary);
    if (!trace_file_) {
        std::cerr << "ERROR: Opening file " << file_path << std::endl;
        return false;
    }
    return true;
}

void SingleChannelBinaryTraceFileWriter::Close() { trace_file_.close(); }

template <typename T>
bool SingleChannelBinaryTraceFileWriter::WriteMessage(const T& top_level_message) {
    if (!(trace_file_ && trace_file_.is_open())) {
        std::cerr << "ERROR: cannot write message, file is not open\n";
        return false;
    }

    const std::string serialized_message = top_level_message.SerializeAsString();
    const uint32_t message_size = static_cast<uint32_t>(serialized_message.size());

    trace_file_.write(reinterpret_cast<const char*>(&message_size), sizeof(message_size));
    trace_file_.write(serialized_message.data(), message_size);

    return trace_file_.good();
}

// Template instantiations for allowed OSI top-level messages
template bool SingleChannelBinaryTraceFileWriter::WriteMessage<osi3::GroundTruth>(const osi3::GroundTruth&);
template bool SingleChannelBinaryTraceFileWriter::WriteMessage<osi3::SensorData>(const osi3::SensorData&);
template bool SingleChannelBinaryTraceFileWriter::WriteMessage<osi3::SensorView>(const osi3::SensorView&);
template bool SingleChannelBinaryTraceFileWriter::WriteMessage<osi3::HostVehicleData>(const osi3::HostVehicleData&);
template bool SingleChannelBinaryTraceFileWriter::WriteMessage<osi3::TrafficCommand>(const osi3::TrafficCommand&);
template bool SingleChannelBinaryTraceFileWriter::WriteMessage<osi3::TrafficCommandUpdate>(const osi3::TrafficCommandUpdate&);
template bool SingleChannelBinaryTraceFileWriter::WriteMessage<osi3::TrafficUpdate>(const osi3::TrafficUpdate&);
template bool SingleChannelBinaryTraceFileWriter::WriteMessage<osi3::MotionRequest>(const osi3::MotionRequest&);
template bool SingleChannelBinaryTraceFileWriter::WriteMessage<osi3::StreamingUpdate>(const osi3::StreamingUpdate&);

}  // namespace osi3