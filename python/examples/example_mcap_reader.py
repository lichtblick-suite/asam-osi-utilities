# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Read OSI MCAP trace files — metadata inspection, topic filtering, and message iteration.

Demonstrates:
- MultiTraceReader.get_available_topics()
- MultiTraceReader.get_file_metadata()
- MultiTraceReader.get_channel_metadata()
- MultiTraceReader.get_message_type_for_topic()
- MultiTraceReader.set_topics() for topic filtering
- create_reader() for format auto-detection
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from osi_utilities import MultiTraceReader, create_reader
from osi_utilities.timestamp import timestamp_to_seconds


def main() -> int:
    """Read all messages from an MCAP file, inspect metadata, and print timestamps."""
    parser = argparse.ArgumentParser(
        description="Read an OSI MCAP trace file — inspect metadata, filter topics, print messages.",
        epilog="Example: python example_mcap_reader.py /path/to/file.mcap --topic ground_truth",
    )
    parser.add_argument("input_file", help="Path to the input MCAP trace file")
    parser.add_argument("--topic", action="append", default=[], help="Filter to specific topic(s) (repeatable)")
    parser.add_argument("--factory", action="store_true", help="Use create_reader for format auto-detection")
    args = parser.parse_args()

    input_path = Path(args.input_file)
    if not input_path.exists():
        print(f"Error: File '{input_path}' not found", file=sys.stderr)
        return 1

    print("Starting MCAP Reader example:")
    print(f"Reading trace file from {input_path}")

    # --- Option A: Use create_reader for format auto-detection ---
    if args.factory:
        print("\n[Using create_reader for format auto-detection]")
        reader = create_reader(input_path)
        reader.open(input_path)
        with reader:
            for result in reader:
                ts = timestamp_to_seconds(result.message)
                print(f"  Type: osi3.{type(result.message).__name__}  Timestamp: {ts}")
        print("Finished MCAP Reader example (factory mode)")
        return 0

    # --- Option B: Direct MultiTraceReader for full feature access ---
    reader = MultiTraceReader()
    if not reader._open(input_path):
        print(f"Error: Could not open '{input_path}'", file=sys.stderr)
        return 1

    with reader:
        # --- Metadata inspection ---
        topics = reader.get_available_topics()
        print(f"\nAvailable topics ({len(topics)}):")
        for topic in topics:
            msg_type = reader.get_message_type_for_topic(topic)
            type_name = msg_type.name if msg_type else "non-OSI"
            print(f"  - {topic} ({type_name})")

            channel_meta = reader.get_channel_metadata(topic)
            if channel_meta:
                for key, value in channel_meta.items():
                    print(f"      {key}: {value}")

        file_metadata = reader.get_file_metadata()
        if file_metadata:
            print("\nFile metadata:")
            for entry in file_metadata:
                name = entry.get("name", "unknown")
                data = entry.get("data", {})
                print(f"  [{name}]")
                for key, value in data.items():
                    print(f"    {key}: {value}")

        # --- Topic filtering ---
        if args.topic:
            print(f"\nFiltering to topic(s): {', '.join(args.topic)}")
            reader.set_topics(args.topic)

        # --- Message iteration ---
        print("\nMessages:")
        count = 0
        for result in reader:
            ts = timestamp_to_seconds(result.message)
            channel_info = f"  Channel: {result.channel_name}" if result.channel_name else ""
            print(f"  Type: osi3.{type(result.message).__name__}  Timestamp: {ts}{channel_info}")
            count += 1
        print(f"Total: {count} messages read")

    print("Finished MCAP Reader example")
    return 0


if __name__ == "__main__":
    sys.exit(main())
