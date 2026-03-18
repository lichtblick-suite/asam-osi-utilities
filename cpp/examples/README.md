<!--
SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
SPDX-License-Identifier: MPL-2.0
-->

# Examples

This folder contains application examples that demonstrate how to use the OSI utilities library.
After build, the executables can be found in the examples folder in the build directory.

### convert_osi2mcap

This example demonstrates how to convert an OSI native binary trace file to an MCAP file.
Simply pass a .osi file and a .mcap destination file name as arguments to the executable.

```bash
./convert_osi2mcap <input_osi_file> <output_mcap_file>
```

### convert_gt2sv

This example demonstrates how to convert GroundTruth trace files to SensorView format.
It supports both MCAP and binary `.osi` input/output. A minimal SensorView is created from each GroundTruth message by copying the global ground truth reference.

```bash
./convert_gt2sv <input_file> <output_file>
```

### example_mcap_reader

This example demonstrates how to read an MCAP file into your application.
It prints message types and timestamps, inspects file/channel metadata, and demonstrates `FilenameUtils` (`InferMessageTypeFromFilename`, `ParseOsiTraceFilename`) for automatic message type detection from file names.
You can try it with an example mcap file generated with `example_mcap_writer` (see following section) with:

```bash
./example_mcap_reader .playground/sv_example.mcap
```

### example_mcap_writer

This example demonstrates how to write OSI data to an MCAP file from your application.
As an example, a SensorView message with one moving object is created and written to the MCAP file.
It shows both metadata overloads (`AddFileMetadata(mcap::Metadata)` and `AddFileMetadata(name, map)`), config constants (`kDefaultChunkSize`, `kMinChunkSize`, `kMaxChunkSize`), and compression options.
It creates the example file `sv_example.mcap` in the `.playground/` directory at the repository root.

```bash
./example_mcap_writer
```

### example_mcap_multi_channel_writer

This example demonstrates two approaches for writing multi-channel MCAP files:

**Part 1 — MCAPTraceFileWriter (pure OSI, multi-topic):**
Uses the high-level `MCAPTraceFileWriter` API to register multiple OSI channels (including two channels sharing the same SensorView schema for automatic deduplication) and writes messages in a simulation loop.

**Part 2 — MCAPTraceFileChannel (mixed OSI / non-OSI):**
Uses an externally-managed `mcap::McapWriter` together with the `MCAPTraceFileChannel` helper to mix OSI and non-OSI channels (JSON) in a single MCAP file, then reads it back with non-OSI message filtering via `SetSkipNonOSIMsgs(true)`.

The example also highlights best practices: zstd compression, chunk sizing, metadata-before-messages ordering, and descriptive channel naming.

```bash
./example_mcap_multi_channel_writer
```

### example_single_channel_binary_reader

This example demonstrates how to read a native binary trace file into your application.
It simply prints the message types and timestamps of the OSI trace file.
You can try it with an example osi trace file generated with `example_single_channel_binary_writer` (see following section) with:

```bash
./example_single_channel_binary_reader .playground/<timestamp>_<osi-version>_<protobuf-version>_<number-of-frames>_sv_example_single_channel_binary_writer.osi --type SensorView
```

### example_single_channel_binary_writer

This example demonstrates how to write OSI data to a native binary file from your application.
As an example, a SensorView message with one moving object is created and written to the OSI trace file.
It creates the example file `<timestamp>_<osi-version>_<protobuf-version>_<number-of-frames>_sv_example_single_channel_binary_writer.osi` in the `.playground/` directory at the repository root.

```bash
./example_single_channel_binary_writer
```

### example_txth_reader

This example demonstrates how to read a human-readable txth trace file into your application.
It simply prints the message types and timestamps of the OSI trace file.
You can try it with an example osi trace file generated with `example_txth_writer` (see following section) with:

```bash
./example_txth_reader .playground/<timestamp>_<osi-version>_<protobuf-version>_<number-of-frames>_sv_example-txth-writer.txth --type SensorView
```

### example_txth_writer

This example demonstrates how to write OSI data to a human-readable txth file from your application.
As an example, a SensorView message with one moving object is created and written to the txth file.
It creates the example file `<timestamp>_<osi-version>_<protobuf-version>_<number-of-frames>_sv_example-txth-writer.txth` in the `.playground/` directory at the repository root.

```bash
./example_txth_writer
```
