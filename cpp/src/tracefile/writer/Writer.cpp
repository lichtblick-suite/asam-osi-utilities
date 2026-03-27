//
// Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
// SPDX-License-Identifier: MPL-2.0
//

#include "osi-utilities/tracefile/Writer.h"

#include <iostream>

#include "osi-utilities/tracefile/writer/MCAPTraceFileWriter.h"
#include "osi-utilities/tracefile/writer/SingleChannelBinaryTraceFileWriter.h"

// Suppress deprecation warning for the factory — it must instantiate the deprecated class
#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable : 4996)
#endif
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
        std::cerr << "WARNING: The .txth text format is deprecated. It is not reliably deserializable "
                     "(protobuf text format is not stable across versions). Use .osi or .mcap instead.\n";
        return std::make_unique<osi3::TXTHTraceFileWriter>();
    }
    throw std::invalid_argument("Unsupported format: " + path.extension().string());
}

}  // namespace osi3

#if defined(__GNUC__) || defined(__clang__)
#pragma GCC diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
