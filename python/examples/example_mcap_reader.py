# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Read OSI MCAP trace files and print message timestamps."""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from osi_utilities import MCAPTraceFileReader
from osi_utilities.tracefile.timestamp import timestamp_to_seconds


def main() -> int:
    """Read all messages from an MCAP file and print their type and timestamp."""
    parser = argparse.ArgumentParser(
        description="Read an OSI MCAP trace file and print message timestamps.",
        epilog="Example: python example_mcap_reader.py /path/to/file.mcap",
    )
    parser.add_argument("input_file", help="Path to the input MCAP trace file")
    args = parser.parse_args()

    input_path = Path(args.input_file)
    if not input_path.exists():
        print(f"Error: File '{input_path}' not found", file=sys.stderr)
        return 1

    print("Starting MCAP Reader example:")

    reader = MCAPTraceFileReader()
    if not reader.open(input_path):
        print(f"Error: Could not open '{input_path}'", file=sys.stderr)
        return 1

    with reader:
        for result in reader:
            ts = timestamp_to_seconds(result.message)
            print(f"Type: osi3.{type(result.message).__name__}  Timestamp: {ts}")

    print("Finished MCAP Reader example")
    return 0


if __name__ == "__main__":
    sys.exit(main())
