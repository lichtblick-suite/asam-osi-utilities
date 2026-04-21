# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Read OSI .txth text trace files."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from osi_utilities import MessageType, ProtobufTextFormatTraceReader
from osi_utilities.timestamp import timestamp_to_seconds

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
    """Read all messages from a .txth file and print their type and timestamp."""
    parser = argparse.ArgumentParser(
        description="Read an OSI .txth text trace file.",
        epilog="Example: python example_txth_reader.py trace.txth --type SensorView",
    )
    parser.add_argument("file_path", help="Path to the .txth trace file")
    parser.add_argument(
        "--type",
        dest="message_type",
        default=None,
        choices=VALID_TYPES.keys(),
        help="Message type (auto-detected from filename if omitted)",
    )
    args = parser.parse_args()

    input_path = Path(args.file_path)
    if not input_path.exists():
        print(f"Error: File '{input_path}' not found", file=sys.stderr)
        return 1

    print("Starting TXTH reader example:")

    msg_type = VALID_TYPES[args.message_type] if args.message_type else MessageType.UNKNOWN
    reader = ProtobufTextFormatTraceReader()
    reader.set_message_type(msg_type)
    if not reader.open(input_path):
        print(f"Error: Could not open '{input_path}'", file=sys.stderr)
        return 1

    print(f"Opened {input_path}")

    with reader:
        for result in reader:
            ts = timestamp_to_seconds(result.message)
            print(f"Type: osi3.{type(result.message).__name__}  Timestamp: {ts}")

    print("Finished TXTH reader example")
    return 0


if __name__ == "__main__":
    sys.exit(main())
