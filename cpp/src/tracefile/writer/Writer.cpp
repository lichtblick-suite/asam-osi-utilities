//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/Writer.h"

#include "osi-utilities/tracefile/writer/MCAPTraceFileWriter.h"
#include "osi-utilities/tracefile/writer/SingleChannelBinaryTraceFileWriter.h"
#include "osi-utilities/tracefile/writer/TXTHTraceFileWriter.h"

namespace osi3 {

auto TraceFileWriterFactory::createWriter(const std::filesystem::path& path) -> std::unique_ptr<osi3::TraceFileWriter> {
    if (path.extension().string() == ".osi") {
        return std::make_unique<osi3::SingleChannelBinaryTraceFileWriter>();
    }
    if (path.extension().string() == ".mcap") {
        return std::make_unique<osi3::MCAPTraceFileWriter>();
    }
    if (path.extension().string() == ".txth") {
        return std::make_unique<osi3::TXTHTraceFileWriter>();
    }
    throw std::invalid_argument("Unsupported format: " + path.extension().string());
}

}  // namespace osi3
