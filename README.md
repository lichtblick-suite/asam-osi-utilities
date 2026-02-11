<!--
SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
SPDX-License-Identifier: MPL-2.0
-->

# ASAM OSI Utilities

[![CI](https://github.com/Lichtblick-Suite/asam-osi-utilities/actions/workflows/ci.yml/badge.svg)](https://github.com/Lichtblick-Suite/asam-osi-utilities/actions/workflows/ci.yml)
[![Documentation](https://img.shields.io/badge/docs-doxygen-blue)](https://lichtblick-suite.github.io/asam-osi-utilities/)
[![License: MPL-2.0](https://img.shields.io/badge/License-MPL%202.0-brightgreen.svg)](LICENSE)

A C++ utility library for working with [ASAM Open Simulation Interface (OSI)](https://opensimulationinterface.github.io/osi-antora-generator/asamosi/latest/specification/index.html) trace files in MCAP and other formats.

## Quick Start

```bash
# Clone with submodules
git clone https://github.com/Lichtblick-Suite/asam-osi-utilities.git
cd asam-osi-utilities

# Init submodules once after cloning
git submodule update --init --recursive

# Update after remote changes
git pull --recurse-submodules

# Build with vcpkg (recommended)
cmake --preset vcpkg
cmake --build --preset vcpkg --parallel

# Run tests (configure with -DBUILD_TESTING=ON to build them)
ctest --test-dir build-vcpkg --output-on-failure
```

> **Note:** First build takes 10-15 minutes as vcpkg compiles dependencies from source.

## Features

- Read/write OSI trace files in [MCAP format](https://mcap.dev/) with lz4/zstd compression
- Read/write single-channel binary (`.osi`) and text (`.txth`) trace files
- Convert between trace file formats
- Cross-platform: Linux, Windows, macOS

## Documentation

| Document                                                                | Description                                     |
| ----------------------------------------------------------------------- | ----------------------------------------------- |
| [Overview](doc/overview.md)                                             | What is this library and OSI trace file formats |
| [Building](doc/building.md)                                             | Build instructions for all platforms            |
| [Development Guide](doc/development.md)                                 | Short index for setup and workflow              |
| [Development Setup](doc/setup.md)                                       | Tooling and environment setup                   |
| [Developer Workflow](doc/workflow.md)                                   | Day-to-day commands                             |
| [Contributing](doc/contributing.md)                                     | How to contribute to this project               |
| [Examples](doc/examples.md)                                             | Example programs and their purpose              |
| [CI Pipeline](doc/ci-pipeline.md)                                       | Continuous integration documentation            |
| [API Reference](https://lichtblick-suite.github.io/asam-osi-utilities/) | Doxygen-generated API docs                      |

## Examples

See [examples/](examples/README.md) for usage examples:

- `example_mcap_reader` / `example_mcap_writer` - MCAP trace file I/O
- `example_single_channel_binary_reader` / `example_single_channel_binary_writer` - Binary `.osi` files
- `example_txth_reader` / `example_txth_writer` - Human-readable `.txth` files
- `convert_osi2mcap` - Convert `.osi` files to `.mcap` format

## Requirements

- C++17 compatible compiler
- CMake 3.16+
- vcpkg (recommended) or manual dependency installation

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines on how to contribute.

## License

This project is licensed under the [Mozilla Public License 2.0](LICENSE).

## Related Projects

- [Open Simulation Interface](https://github.com/OpenSimulationInterface/open-simulation-interface) - OSI standard
- [MCAP](https://mcap.dev/) - Open-source container file format
