# osi-utilities (Python)

Python utilities for reading and writing ASAM OSI (Open Simulation Interface) trace files.

## Supported Formats

- **MCAP** (`.mcap`) — Multi-channel container format with metadata
- **Binary** (`.osi`) — Single-channel binary format with length-prefixed protobuf messages
- **Text** (`.txth`) — Human-readable protobuf TextFormat

## Installation

```bash
pip install -e .
```

For development:

```bash
pip install -e ".[dev]"
```

Or use the Makefile:

```bash
make setup
```

## Quick Start

```python
from osi_utilities import open_trace_file

# Read any supported format (auto-detected from extension)
with open_trace_file("trace.mcap") as reader:
    for result in reader:
        print(result.message_type, result.message)
```

## Development

```bash
make setup      # Create venv and install dependencies
make test       # Run tests
make lint       # Run linter
make format     # Format code
make typecheck  # Run type checker
```
