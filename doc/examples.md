<!--
SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
SPDX-License-Identifier: MPL-2.0
-->

# Examples

This project ships small executables that demonstrate how to read/write OSI trace files and convert between formats. Use them as reference implementations for API usage and expected file formats.

## What the examples cover

- [example_mcap_reader.cpp](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/cpp/examples/example_mcap_reader.cpp) — MCAP read
- [example_mcap_writer.cpp](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/cpp/examples/example_mcap_writer.cpp) — MCAP write
- [example_single_channel_binary_reader.cpp](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/cpp/examples/example_single_channel_binary_reader.cpp) — `.osi` binary read
- [example_single_channel_binary_writer.cpp](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/cpp/examples/example_single_channel_binary_writer.cpp) — `.osi` binary write
- [example_txth_reader.cpp](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/cpp/examples/example_txth_reader.cpp) — `.txth` text read
- [example_txth_writer.cpp](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/cpp/examples/example_txth_writer.cpp) — `.txth` text write
- [convert_osi2mcap.cpp](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/cpp/examples/convert_osi2mcap.cpp) — convert `.osi` to `.mcap`

## Build examples

Examples are built as part of the normal build:

```bash
cmake --preset vcpkg
cmake --build --preset vcpkg --parallel
```

## Run examples

Binaries are in `build-vcpkg/cpp/examples/` (or `build/cpp/examples/` for the base preset).

```bash
# Example
./build-vcpkg/cpp/examples/example_mcap_reader input.mcap
```

For more details, see [cpp/examples/README.md](https://github.com/lichtblick-suite/asam-osi-utilities/blob/main/cpp/examples/README.md).
