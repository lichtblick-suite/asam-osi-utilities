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

### example_mcap_reader

Reads an MCAP file and prints message types and timestamps.

```bash
python example_mcap_reader.py /path/to/file.mcap
```

### example_mcap_writer

Writes 10 SensorView frames to an MCAP file in the system temp directory.

```bash
python example_mcap_writer.py
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
```

## Quick Start

```bash
# Write an MCAP file, then read it back
python example_mcap_writer.py
python example_mcap_reader.py /tmp/sv_example.mcap

# Write a binary .osi file, then read it back
python example_single_channel_binary_writer.py
python example_single_channel_binary_reader.py /tmp/sv_example_<pid>.osi --type SensorView
```
