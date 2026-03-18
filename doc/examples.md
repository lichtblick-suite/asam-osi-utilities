<!--
SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
SPDX-License-Identifier: MPL-2.0
-->

# Examples

This project ships examples in both C++ and Python that demonstrate how to read/write OSI trace files and convert between formats. Use them as reference implementations for API usage and expected file formats.

## C++ examples

- [example_mcap_reader.cpp](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/cpp/examples/example_mcap_reader.cpp) — MCAP read
- [example_mcap_writer.cpp](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/cpp/examples/example_mcap_writer.cpp) — MCAP write
- [example_single_channel_binary_reader.cpp](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/cpp/examples/example_single_channel_binary_reader.cpp) — `.osi` binary read
- [example_single_channel_binary_writer.cpp](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/cpp/examples/example_single_channel_binary_writer.cpp) — `.osi` binary write
- [example_txth_reader.cpp](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/cpp/examples/example_txth_reader.cpp) — `.txth` text read
- [example_txth_writer.cpp](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/cpp/examples/example_txth_writer.cpp) — `.txth` text write
- [convert_osi2mcap.cpp](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/cpp/examples/convert_osi2mcap.cpp) — convert `.osi` to `.mcap`
- [convert_gt2sv.cpp](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/cpp/examples/convert_gt2sv.cpp) — convert GroundTruth to SensorView

### Build C++ examples

Examples are built as part of the normal build:

```bash
make build cpp
```

### Run C++ examples

Binaries are in `build-vcpkg/cpp/examples/` (or `build/cpp/examples/` for the base preset).

```bash
./build-vcpkg/cpp/examples/example_mcap_reader input.mcap
```

For more details, see [cpp/examples/README.md](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/cpp/examples/README.md).

## Python examples

- [example_mcap_reader.py](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/python/examples/example_mcap_reader.py) — MCAP read
- [example_mcap_writer.py](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/python/examples/example_mcap_writer.py) — MCAP write
- [example_single_channel_binary_reader.py](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/python/examples/example_single_channel_binary_reader.py) — `.osi` binary read
- [example_single_channel_binary_writer.py](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/python/examples/example_single_channel_binary_writer.py) — `.osi` binary write
- [example_txth_reader.py](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/python/examples/example_txth_reader.py) — `.txth` text read
- [example_txth_writer.py](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/python/examples/example_txth_writer.py) — `.txth` text write
- [convert_osi2mcap.py](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/python/examples/convert_osi2mcap.py) — convert `.osi` to `.mcap`

### Setup Python environment

```bash
make setup
```

### Run Python examples

```bash
python python/examples/example_mcap_writer.py
python python/examples/example_mcap_reader.py .playground/sv_example.mcap
```

For more details, see [python/examples/README.md](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/python/examples/README.md).
