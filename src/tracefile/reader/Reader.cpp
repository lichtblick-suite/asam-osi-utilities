//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/Reader.h"

#include "osi-utilities/tracefile/reader/MCAPTraceFileReader.h"
#include "osi-utilities/tracefile/reader/SingleChannelBinaryTraceFileReader.h"
#include "osi-utilities/tracefile/reader/TXTHTraceFileReader.h"

namespace osi3 {

auto TraceFileReaderFactory::createReader(const std::filesystem::path& path) -> std::unique_ptr<osi3::TraceFileReader> {
    if (path.extension().string() == ".osi") {
        return std::make_unique<osi3::SingleChannelBinaryTraceFileReader>();
    }
    if (path.extension().string() == ".mcap") {
        return std::make_unique<osi3::MCAPTraceFileReader>();
    }
    if (path.extension().string() == ".txth") {
        return std::make_unique<osi3::TXTHTraceFileReader>();
    }
    throw std::invalid_argument("Unsupported format: " + path.extension().string());
}

}  // namespace osi3
