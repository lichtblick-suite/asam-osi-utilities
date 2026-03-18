<!--
SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
SPDX-License-Identifier: MPL-2.0
-->

# ASAM OSI Utilities

[![CI](https://github.com/lichtblick-suite/asam-osi-utilities/actions/workflows/ci.yml/badge.svg)](https://github.com/lichtblick-suite/asam-osi-utilities/actions/workflows/ci.yml)
[![PyPI](https://img.shields.io/pypi/v/asam-osi-utilities)](https://pypi.org/project/asam-osi-utilities/)
[![Documentation](https://img.shields.io/badge/docs-API_Reference-blue)](https://lichtblick-suite.github.io/asam-osi-utilities/)
[![License: MPL-2.0](https://img.shields.io/badge/License-MPL%202.0-brightgreen.svg)](LICENSE)

A C++ and Python utility library for working with [ASAM Open Simulation Interface (OSI)](https://opensimulationinterface.github.io/osi-antora-generator/asamosi/latest/specification/index.html) trace files in MCAP and other formats.

## Quick Start

### C++

```bash
# Clone with submodules
git clone https://github.com/lichtblick-suite/asam-osi-utilities.git
cd asam-osi-utilities
git submodule update --init --recursive

# Build (uses vcpkg preset — recommended)
make build cpp

# Build with tests
make build cpp tests

# Run tests
make test cpp
```

> **Note:** First build takes 10-15 minutes as vcpkg compiles dependencies from source.
>
> Power users can call cmake directly: `cmake --preset vcpkg && cmake --build --preset vcpkg --parallel`

### Python

```bash
pip install asam-osi-utilities
```

Or from source:

```bash
# From the repository root
make setup          # creates venv + installs all dependencies
make test python    # run Python tests
```

## Features

- Read/write OSI trace files in [MCAP format](https://mcap.dev/) with lz4/zstd compression
- Read/write single-channel binary (`.osi`) and text (`.txth`) trace files
- Convert between trace file formats
- Cross-platform: Linux, Windows, macOS
- **C++ library** with CMake/vcpkg integration
- **Python SDK** available on [PyPI](https://pypi.org/project/asam-osi-utilities/) (`pip install asam-osi-utilities`)

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
| [API Reference](https://lichtblick-suite.github.io/asam-osi-utilities/) | C++ and Python API documentation      |

## Examples

Both the C++ and Python SDKs ship with matching examples covering all supported formats (MCAP, binary `.osi`, text `.txth`), format conversion, and benchmarks.

| C++ | Python | Description |
|-----|--------|-------------|
| `example_mcap_writer` | `example_mcap_writer.py` | Write MCAP trace files |
| `example_mcap_reader` | `example_mcap_reader.py` | Read MCAP with metadata inspection and topic filtering |
| `example_single_channel_binary_writer` | `example_single_channel_binary_writer.py` | Write binary `.osi` files |
| `example_single_channel_binary_reader` | `example_single_channel_binary_reader.py` | Read binary `.osi` files |
| `example_txth_writer` | `example_txth_writer.py` | Write human-readable `.txth` files |
| `example_txth_reader` | `example_txth_reader.py` | Read `.txth` files |
| `example_mcap_multi_channel_writer` | `example_mcap_multi_channel_writer.py` | Multi-topic MCAP with mixed channels |
| `convert_osi2mcap` | `convert_osi2mcap.py` | Convert `.osi` → `.mcap` |
| `convert_gt2sv` | — | Convert GroundTruth → SensorView |
| — | `example_reader_factory.py` | Format auto-detection and timestamp utilities |
| `benchmark` | `benchmark.py` | Throughput benchmark for all formats |

See [cpp/examples/README.md](cpp/examples/README.md) and [python/examples/README.md](python/examples/README.md) for full details.

## Requirements

### C++

- C++17 compatible compiler
- CMake 3.16+
- vcpkg (recommended) or manual dependency installation

### Python

- Python 3.10+
- `pip install asam-osi-utilities`

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) for guidelines on how to contribute.

## License

This project is licensed under the [Mozilla Public License 2.0](LICENSE).

## Related Projects

- [Open Simulation Interface](https://github.com/OpenSimulationInterface/open-simulation-interface) - OSI standard
- [MCAP](https://mcap.dev/) - Open-source container file format
