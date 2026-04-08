# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Read one logical channel via open_channel(ChannelSpecification).

Demonstrates:
- ChannelSpecification-based channel selection
- open_channel() for both .mcap and single-channel files (.osi/.txth)
- Optional message type expectation checks
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from osi_utilities import ChannelSpecification, MessageType, open_channel
from osi_utilities.timestamp import timestamp_to_seconds


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Open a single logical OSI channel via ChannelSpecification.",
        epilog=(
            "Examples:\n"
            "  python example_channel_reader.py /path/to/file.mcap --topic GroundTruth\n"
            "  python example_channel_reader.py /path/to/file.osi --type GroundTruth"
        ),
        formatter_class=argparse.RawTextHelpFormatter,
    )
    parser.add_argument("input_file", help="Path to .mcap, .osi, or .txth trace file")
    parser.add_argument(
        "--topic",
        default=None,
        help="Topic to read from .mcap (optional; defaults to first available topic)",
    )
    parser.add_argument(
        "--type",
        dest="message_type",
        default=None,
        help="Expected message type (e.g. GroundTruth, SensorView)",
    )
    args = parser.parse_args()

    try:
        requested_spec = ChannelSpecification(
            path=Path(args.input_file),
            topic=args.topic,
            message_type=args.message_type,
        )
        print(f"Parsed channel specification: {requested_spec}")
    except ValueError as exc:
        print(f"Error: {exc}.", file=sys.stderr)
        return 2

    print("Opening channel with:")
    print(f"  path: {requested_spec.path}")
    print(f"  topic: {requested_spec.topic}")
    print(f"  message_type: {requested_spec.message_type}")

    with open_channel(requested_spec) as channel_reader:
        resolved = channel_reader.get_channel_specification()
        print("\nResolved channel:")
        print(f"  path: {resolved.path}")
        print(f"  topic: {resolved.topic}")
        print(f"  message_type: {resolved.message_type}")

        print("\nMessages:")
        count = 0
        for message in channel_reader:
            ts = timestamp_to_seconds(message)
            print(f"  [{count}] type=osi3.{type(message).__name__} timestamp={ts}")
            count += 1
        print(f"\nTotal: {count} messages read")

    return 0


if __name__ == "__main__":
    sys.exit(main())
