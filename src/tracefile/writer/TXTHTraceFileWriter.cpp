//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/writer/TXTHTraceFileWriter.h"

#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/text_format.h>

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

TXTHTraceFileWriter::~TXTHTraceFileWriter() {
    if (trace_file_.is_open()) {
        Close();
    }
}

auto TXTHTraceFileWriter::Open(const std::filesystem::path& file_path) -> bool {
    // check if at least .osi ending is present
    if (file_path.extension().string() != ".txth") {
        std::cerr << "ERROR: The trace file '" << file_path << "' must have a '.txth' extension." << std::endl;
        return false;
    }

    // prevent opening again if already opened
    if (trace_file_.is_open()) {
        std::cerr << "ERROR: Opening file " << file_path << ", writer has already a file opened" << std::endl;
        return false;
    }

    trace_file_.open(file_path);
    if (!trace_file_) {
        std::cerr << "ERROR: Opening file " << file_path << std::endl;
        return false;
    }
    return true;
}

void TXTHTraceFileWriter::Close() { trace_file_.close(); }

template <typename T>
auto TXTHTraceFileWriter::WriteMessage(const T& top_level_message) -> bool {
    if (!(trace_file_ && trace_file_.is_open())) {
        std::cerr << "ERROR: Cannot write message, file is not open\n";
        return false;
    }

    google::protobuf::io::OstreamOutputStream output_stream(&trace_file_);
    if (!google::protobuf::TextFormat::Print(top_level_message, &output_stream)) {
        std::cerr << "ERROR: Failed to convert message to text format\n";
        return false;
    }

    if (!trace_file_.good()) {
        std::cerr << "ERROR: Failed to write text message to file\n";
        return false;
    }

    return true;
}

// Template instantiations for allowed OSI top-level messages
template bool TXTHTraceFileWriter::WriteMessage<osi3::GroundTruth>(const osi3::GroundTruth&);
template bool TXTHTraceFileWriter::WriteMessage<osi3::SensorData>(const osi3::SensorData&);
template bool TXTHTraceFileWriter::WriteMessage<osi3::SensorView>(const osi3::SensorView&);
template bool TXTHTraceFileWriter::WriteMessage<osi3::HostVehicleData>(const osi3::HostVehicleData&);
template bool TXTHTraceFileWriter::WriteMessage<osi3::TrafficCommand>(const osi3::TrafficCommand&);
template bool TXTHTraceFileWriter::WriteMessage<osi3::TrafficCommandUpdate>(const osi3::TrafficCommandUpdate&);
template bool TXTHTraceFileWriter::WriteMessage<osi3::TrafficUpdate>(const osi3::TrafficUpdate&);
template bool TXTHTraceFileWriter::WriteMessage<osi3::MotionRequest>(const osi3::MotionRequest&);
template bool TXTHTraceFileWriter::WriteMessage<osi3::StreamingUpdate>(const osi3::StreamingUpdate&);

}  // namespace osi3
