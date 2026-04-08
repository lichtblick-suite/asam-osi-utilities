# osi-utilities (Python)

Python utilities for reading and writing ASAM OSI (Open Simulation Interface) trace files.

## Supported Formats

- **MCAP** (`.mcap`) - Multi-channel container format with metadata
- **Binary** (`.osi`) - Single-channel binary format with length-prefixed protobuf messages
- **Text** (`.txth`) - Human-readable protobuf TextFormat

## Installation

From PyPI:

```bash
pip install asam-osi-utilities
```

For development (uses a virtual environment):

```bash
make setup    # creates .venv and installs in editable mode
```

## API Entrypoints

The package exposes two API levels:

1. `Channel abstraction` (simple API) via `open_channel(...)`
- Use this when you want to read one logical channel and get protobuf messages directly.
- Works across `.osi` and `.mcap`.

2. `Advanced tracefile API` via `create_reader(...)` and concrete readers/writers
- Use this when you need explicit control over topics, decoder mode, metadata, writer behavior, or multi-channel features.

## Quick Start

### 1) Channel abstraction (simple API)

```python
from pathlib import Path
from osi_utilities import ChannelSpecification, MessageType, open_channel

# Single channel from an MCAP file (explicit topic)
spec = ChannelSpecification(
    path=Path("trace.mcap"),
    topic="GroundTruthTopic",
    message_type=MessageType.GROUND_TRUTH,
)

with open_channel(spec) as channel:
    for message in channel:
        print(type(message).__name__)
```

Channel abstraction writing uses `open_channel_writer(...)`:

```python
from pathlib import Path
from osi3.osi_groundtruth_pb2 import GroundTruth
from osi_utilities import ChannelSpecification, MessageType
from osi_utilities.api.channel_writer import open_channel_writer

spec = ChannelSpecification(
    path=Path("output.osi"),
    message_type=MessageType.GROUND_TRUTH,
)

msg = GroundTruth()
msg.timestamp.seconds = 1

with open_channel_writer(spec) as channel_writer:
    channel_writer.write_message(msg)
```

### 2) Advanced reading API

```python
from pathlib import Path
from osi_utilities import MultiTraceReader, create_reader

# Create reader by extension (.osi/.mcap/.txth), then configure explicitly
path = Path("trace.mcap")
reader = create_reader(path)

if isinstance(reader, MultiTraceReader):
    reader.set_topics(["GroundTruthTopic"])

if not reader.open(path):
    raise RuntimeError(f"Failed to open {path}")

with reader:
    for result in reader:
        print(result.message_type, result.message)
```

Advanced configuration helpers based on `ChannelSpecification` are also available in `osi_utilities.tracefile.configure`:

```python
from pathlib import Path
from osi_utilities import create_reader
from osi_utilities.api import ChannelSpecification, MessageType
from osi_utilities.tracefile.configure import configure_reader, configure_reader_for_channels

path = Path("trace.mcap")
reader = create_reader(path)

# Configure from one channel spec
configure_reader(
    reader,
    ChannelSpecification(
        path=path,
        topic="GroundTruthTopic",
        message_type=MessageType.GROUND_TRUTH,
    ),
)

# Or configure from multiple channel specs (same file path)
configure_reader_for_channels(
    reader,
    [
        ChannelSpecification(path=path, topic="GroundTruthTopic", message_type=MessageType.GROUND_TRUTH),
        ChannelSpecification(path=path, topic="SensorDataTopic", message_type=MessageType.SENSOR_DATA),
    ],
)
```

### 3) Writing (advanced API)

```python
from pathlib import Path
from osi3.osi_sensorview_pb2 import SensorView
from osi_utilities import SingleTraceWriter

sensor_view = SensorView()
sensor_view.version.version_major = 3
sensor_view.timestamp.seconds = 123
sensor_view.timestamp.nanos = 456

with SingleTraceWriter() as writer:
    writer.open(Path("output.osi"))
    writer.write_message(sensor_view)
```

For multi-channel MCAP writing, use `MultiTraceWriter` or `MCAPChannel`.

## Development

```bash
make setup      # Create venv and install dependencies
make test       # Run tests
make lint       # Run linter
make format     # Format code
make typecheck  # Run type checker
```
