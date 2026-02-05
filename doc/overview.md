# Overview

ASAM OSI Utilities is a C++ library that provides tools for working with [ASAM Open Simulation Interface (OSI)](https://opensimulationinterface.github.io/osi-antora-generator/asamosi/latest/specification/index.html) trace files.

## What is OSI?

The **Open Simulation Interface (OSI)** is an open standard developed by ASAM (Association for Standardization of Automation and Measuring Systems) for the exchange of simulation data between traffic participants, sensors, and simulation environments in autonomous driving development.

OSI uses [Protocol Buffers](https://protobuf.dev/) for efficient binary serialization of structured data.

## OSI Trace File Formats

This library supports three OSI trace file formats:

### MCAP (Multi-Channel)

**File extension:** `.mcap`

The [MCAP format](https://mcap.dev/) is the recommended format for storing OSI data. It provides:

- **Multiple channels** - Store different OSI message types in the same file
- **Random access** - Quickly seek to any point in time
- **Compression** - Built-in support for lz4 and zstd compression
- **Metadata** - Rich metadata including schemas, timestamps, and custom fields
- **Cross-platform** - Tooling available in C++, Python, Go, TypeScript, and more

OSI defines a specialization of the MCAP format with additional constraints. See the
[OSI trace file specification](https://opensimulationinterface.github.io/osi-antora-generator/asamosi/latest/specification/index.html).

| Requirement   | Value                                             |
| ------------- | ------------------------------------------------- |
| Version       | MCAP 0x30                                         |
| Indexing      | Required (chunk index records in summary section) |
| Compression   | None, `lz4`, or `zstd`                            |
| Metadata name | `net.asam.osi.trace`                              |

#### Required Metadata Fields

The `net.asam.osi.trace` metadata record must include:

- `version` - OSI release version (e.g., `3.8.0`)
- `min_osi_version` - Minimum OSI schema version used
- `max_osi_version` - Maximum OSI schema version used
- `min_protobuf_version` - Minimum protobuf version used
- `max_protobuf_version` - Maximum protobuf version used

### Single-Channel Binary

**File extension:** `.osi`

A simple binary format for storing a sequence of OSI messages of the same type:

```
[4-byte length][protobuf message][4-byte length][protobuf message]...
```

- Length is a **little-endian unsigned 32-bit integer**
- Length does not include the 4-byte length field itself
- No metadata, no random access, no compression

Best for: Simple use cases, streaming, and interoperability with legacy tools.

### Single-Channel Text

**File extension:** `.txth`

Human-readable format where each line is a Protocol Buffer text-format message:

```
timestamp { seconds: 0 nanos: 0 } version { ... }
timestamp { seconds: 0 nanos: 100000000 } version { ... }
```

- One message per line
- Uses [Protocol Buffer text format](https://protobuf.dev/reference/protobuf/textformat-spec/)
- **Intended for debugging and manual inspection only**
- Not recommended for production use due to parsing ambiguities

## Supported OSI Message Types

The library supports all OSI top-level message types:

| Message Type                    | Description              |
| ------------------------------- | ------------------------ |
| `osi3::GroundTruth`             | Ground truth world state |
| `osi3::SensorView`              | Sensor input data        |
| `osi3::SensorViewConfiguration` | Sensor configuration     |
| `osi3::SensorData`              | Processed sensor output  |
| `osi3::HostVehicleData`         | Ego vehicle state        |
| `osi3::TrafficCommand`          | Traffic control commands |
| `osi3::TrafficCommandUpdate`    | Traffic command updates  |
| `osi3::TrafficUpdate`           | Traffic state updates    |
| `osi3::MotionRequest`           | Motion planning requests |
| `osi3::StreamingUpdate`         | Streaming data updates   |
| `osi3::ReductionConfiguration`  | Data reduction config    |

## Library Components

### Trace File Readers

| Class                                      | Description              |
| ------------------------------------------ | ------------------------ |
| `osi3::MCAPTraceFileReader`                | Read MCAP trace files    |
| `osi3::SingleChannelBinaryTraceFileReader` | Read `.osi` binary files |
| `osi3::TXTHTraceFileReader`                | Read `.txth` text files  |

### Trace File Writers

| Class                                      | Description               |
| ------------------------------------------ | ------------------------- |
| `osi3::MCAPTraceFileWriter`                | Write MCAP trace files    |
| `osi3::SingleChannelBinaryTraceFileWriter` | Write `.osi` binary files |
| `osi3::TXTHTraceFileWriter`                | Write `.txth` text files  |

## Example Usage

```cpp
#include "osi-utilities/tracefile/reader/MCAPTraceFileReader.h"

osi3::MCAPTraceFileReader reader;
if (reader.Open("trace.mcap")) {
    while (reader.HasNext()) {
        auto reading = reader.ReadMessage();
        if (reading.has_value()) {
            // Process message based on type
            std::visit([](auto&& msg) {
                // Handle different message types
            }, reading.value().message);
        }
    }
    reader.Close();
}
```

## Examples

The examples provide small, focused programs for reading/writing each supported trace format and for converting `.osi` to `.mcap`.
See [Examples](examples.md) for purpose and usage details.

## Further Resources

- [ASAM OSI Specification](https://opensimulationinterface.github.io/osi-antora-generator/asamosi/latest/specification/index.html)
- [MCAP Documentation](https://mcap.dev/docs)
- [Protocol Buffers](https://protobuf.dev/)
- [Examples](examples.md)
