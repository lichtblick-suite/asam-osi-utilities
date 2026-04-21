# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Convert single-channel binary OSI traces (.osi) to MCAP."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from osi_utilities import MessageType, MultiTraceWriter, SingleTraceReader

VALID_TYPES = {
    "GroundTruth": MessageType.GROUND_TRUTH,
    "SensorData": MessageType.SENSOR_DATA,
    "SensorView": MessageType.SENSOR_VIEW,
    "SensorViewConfiguration": MessageType.SENSOR_VIEW_CONFIGURATION,
    "HostVehicleData": MessageType.HOST_VEHICLE_DATA,
    "TrafficCommand": MessageType.TRAFFIC_COMMAND,
    "TrafficCommandUpdate": MessageType.TRAFFIC_COMMAND_UPDATE,
    "TrafficUpdate": MessageType.TRAFFIC_UPDATE,
    "MotionRequest": MessageType.MOTION_REQUEST,
    "StreamingUpdate": MessageType.STREAMING_UPDATE,
}


def main() -> int:
    """Convert a binary .osi trace to MCAP format."""
    parser = argparse.ArgumentParser(
        description="Convert an OSI binary trace file (.osi) to MCAP format.",
        epilog="Example: python convert_osi2mcap.py input.osi output.mcap --compression zstd",
    )
    parser.add_argument("input_file", help="Path to the input .osi trace file")
    parser.add_argument("output_file", help="Path to the output .mcap file")
    parser.add_argument(
        "--input-type",
        dest="input_type",
        default=None,
        choices=VALID_TYPES.keys(),
        help="Message type (auto-detected from filename if omitted)",
    )
    parser.add_argument(
        "--compression",
        default=None,
        choices=["none", "lz4", "zstd"],
        help="MCAP compression algorithm (default: mcap library default)",
    )
    parser.add_argument(
        "--chunk-size",
        dest="chunk_size",
        type=int,
        default=None,
        help="MCAP chunk size in bytes (default: 16 MiB)",
    )
    args = parser.parse_args()

    input_path = Path(args.input_file)
    output_path = Path(args.output_file)

    if not input_path.exists():
        print(f"Error: Input file '{input_path}' not found", file=sys.stderr)
        return 1

    print(f"Input:  {input_path}")
    print(f"Output: {output_path}")

    msg_type = VALID_TYPES[args.input_type] if args.input_type else MessageType.UNKNOWN
    reader = SingleTraceReader()
    reader.set_message_type(msg_type)
    if not reader.open(input_path):
        print(f"Error: Could not open input '{input_path}'", file=sys.stderr)
        return 1

    writer = MultiTraceWriter()
    metadata = {"description": f"Converted from {input_path.name}"}
    writer_kwargs: dict[str, object] = {}
    if args.compression is not None:
        writer_kwargs["compression"] = args.compression
    if args.chunk_size is not None:
        writer_kwargs["chunk_size"] = args.chunk_size

    if not writer.open(output_path, metadata=metadata, **writer_kwargs):  # type: ignore[arg-type]
        print(f"Error: Could not open output '{output_path}'", file=sys.stderr)
        return 1

    topic = "ConvertedTrace"
    message_count = 0
    channel_added = False

    with reader, writer:
        for result in reader:
            if not channel_added:
                writer.add_channel(topic, type(result.message))
                channel_added = True
            if not writer.write_message(result.message, topic):
                print(f"Error: Failed to write message {message_count}", file=sys.stderr)
                return 1
            message_count += 1

    print(f"Converted {message_count} messages")
    return 0


if __name__ == "__main__":
    sys.exit(main())
