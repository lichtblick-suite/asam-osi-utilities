# osi-utilities (Python)

Python utilities for reading and writing ASAM OSI (Open Simulation Interface) trace files.

## Supported Formats

- **MCAP** (`.mcap`) — Multi-channel container format with metadata
- **Binary** (`.osi`) — Single-channel binary format with length-prefixed protobuf messages
- **Text** (`.txth`) — Human-readable protobuf TextFormat

## Installation

From PyPI:

```bash
pip install asam-osi-utilities
```

For development (uses a virtual environment):

```bash
make setup    # creates .venv and installs in editable mode
```

## Quick Start

### Reading

```python
from osi_utilities.tracefile import open_trace_file

# Read any supported format (auto-detected from extension)
with open_trace_file("trace.mcap") as reader:
    for result in reader:
        print(result.message_type, result.message)
```

### Writing

```python
from pathlib import Path
from osi3.osi_sensorview_pb2 import SensorView
from osi_utilities.tracefile import SingleTraceWriter

sensor_view = SensorView()
sensor_view.version.version_major = 3
sensor_view.timestamp.seconds = 123
sensor_view.timestamp.nanos = 456

with SingleTraceWriter() as writer:
    writer.open(Path("output.osi"))
    writer.write_message(sensor_view)
```

## Development

```bash
make setup      # Create venv and install dependencies
make test       # Run tests
make lint       # Run linter
make format     # Format code
make typecheck  # Run type checker
```
