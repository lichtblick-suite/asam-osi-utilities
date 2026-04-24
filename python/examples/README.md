<!--
SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
SPDX-License-Identifier: MPL-2.0
-->

# Python Examples

This folder contains Python examples that demonstrate how to use the ASAM OSI utilities SDK.

## Prerequisites

Install the SDK in a virtual environment (from the repository root):

```bash
make setup
```

## Examples

### convert_osi2mcap

Converts an OSI native binary trace file (.osi) to an MCAP file.

```bash
python convert_osi2mcap.py <input_osi_file> <output_mcap_file>
python convert_osi2mcap.py input.osi output.mcap --input-type SensorView
```

### example_channel_reader

Opens a single logical OSI channel via `ChannelSpecification` and `open_channel()`.
Works with `.mcap`, `.osi`, and `.txth` files.

```bash
python example_channel_reader.py /path/to/file.mcap --topic GroundTruth
python example_channel_reader.py /path/to/file.osi --type GroundTruth
```

### example_mcap_reader

Reads an MCAP file with metadata inspection, topic filtering, and message iteration.

```bash
# Basic usage — prints available topics, metadata, and all messages
python example_mcap_reader.py /path/to/file.mcap

# Filter to a specific topic
python example_mcap_reader.py /path/to/file.mcap --topic ground_truth

# Use create_reader() for format auto-detection
python example_mcap_reader.py /path/to/file.mcap --factory
```

### example_mcap_writer

Writes 10 SensorView frames to an MCAP file in the system temp directory.

```bash
python example_mcap_writer.py
# Output: .playground/<timestamp>_sv_<version>_<pid>_example_mcap_writer.mcap
```

### example_mcap_multi_channel_writer

Writes multi-topic MCAP files (Part 1: pure OSI channels, Part 2: mixed OSI + non-OSI channels).

```bash
python example_mcap_multi_channel_writer.py
# Output: .playground/multi_channel_example_part1_<pid>.mcap
#         .playground/multi_channel_example_part2_<pid>.mcap
```

### example_reader_factory

Demonstrates format auto-detection and timestamp conversion utilities.

```bash
# Auto-detect format from extension (.osi, .mcap, or .txth)
python example_reader_factory.py ../../test-data/5frames_gt_esmini.osi
python example_reader_factory.py ../../test-data/5frames_gt_esmini.mcap
```

### example_single_channel_binary_reader

Reads a native binary trace file (.osi) and prints message types and timestamps.

```bash
python example_single_channel_binary_reader.py /path/to/file.osi --type SensorView
```

### example_single_channel_binary_writer

Writes 10 SensorView frames to a binary .osi file in the system temp directory.

```bash
python example_single_channel_binary_writer.py
# Output: .playground/<timestamp>_sv_<version>_<pid>_example_single_channel_binary_writer.osi
```

### example_txth_reader

Reads a human-readable text trace file (.txth) and prints message types and timestamps.

```bash
python example_txth_reader.py /path/to/file.txth --type SensorView
```

### example_txth_writer

Writes 10 SensorView frames to a text .txth file in the system temp directory.

```bash
python example_txth_writer.py
# Output: .playground/<timestamp>_sv_<version>_<pid>_example_txth_writer.txth
```

### benchmark

Benchmarks read/write throughput for all three formats (MCAP, .osi, .txth).

```bash
# Synthetic mode — generate N messages and benchmark
python benchmark.py synthetic 100

# File mode — benchmark an existing .osi file
python benchmark.py file /path/to/file.osi
```

## Quick Start

```bash
# Write → Read roundtrip for each format
python example_mcap_writer.py
python example_mcap_reader.py ../../.playground/*_example_mcap_writer.mcap

python example_single_channel_binary_writer.py
python example_single_channel_binary_reader.py ../../.playground/*_sv_*_example_single_channel_binary_writer.osi

python example_txth_writer.py
python example_txth_reader.py ../../.playground/*_sv_*_example_txth_writer.txth

# Format auto-detection with factory
python example_reader_factory.py ../../test-data/5frames_gt_esmini.mcap
```
