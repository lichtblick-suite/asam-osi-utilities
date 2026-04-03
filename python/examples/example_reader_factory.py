# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Demonstrate format auto-detection and timestamp conversion utilities.

Demonstrates:
- open_trace_file() — one-liner to open any .osi, .mcap, or .txth file
- TraceReaderFactory.create_reader() — explicit factory pattern
- timestamp_to_seconds(), timestamp_to_nanoseconds() — timestamp conversion
- nanoseconds_to_seconds(), seconds_to_nanoseconds() — unit conversion
- MessageType enum — OSI message type identification
- infer_message_type_from_filename() — detect message type from filename patterns
- parse_osi_trace_filename() — parse OSI naming convention filenames
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path

from osi_utilities import MessageType, TraceReaderFactory, open_trace_file
from osi_utilities.tracefile._types import infer_message_type_from_filename, parse_osi_trace_filename
from osi_utilities.tracefile.timestamp import (
    nanoseconds_to_seconds,
    seconds_to_nanoseconds,
    timestamp_to_nanoseconds,
    timestamp_to_seconds,
)


def main() -> int:
    """Open any OSI trace file, auto-detect its format, and print contents with timestamp details."""
    parser = argparse.ArgumentParser(
        description="Open any OSI trace file (.osi, .mcap, .txth) with format auto-detection.",
        epilog=(
            "Examples:\n"
            "  python example_reader_factory.py test-data/5frames_gt_esmini.osi\n"
            "  python example_reader_factory.py test-data/5frames_gt_esmini.mcap\n"
            "  python example_reader_factory.py /tmp/my_sv_trace.txth\n"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("input_file", help="Path to any OSI trace file (.osi, .mcap, or .txth)")
    type_choices = [t.name for t in MessageType if t != MessageType.UNKNOWN]
    parser.add_argument(
        "--message-type",
        choices=type_choices,
        default=None,
        help="Hint for message type (required for .osi/.txth if not in filename)",
    )
    args = parser.parse_args()

    input_path = Path(args.input_file)
    if not input_path.exists():
        print(f"Error: File '{input_path}' not found", file=sys.stderr)
        return 1

    # --- Method 1: open_trace_file() — simplest one-liner ---
    print("=== Method 1: open_trace_file() ===")
    print(f"Input: {input_path} (format auto-detected from extension)")
    msg_type = MessageType[args.message_type] if args.message_type else None
    reader = open_trace_file(input_path, message_type=msg_type)
    reader.open(input_path)

    count = 0
    with reader:
        for result in reader:
            ts_sec = timestamp_to_seconds(result.message)
            ts_ns = timestamp_to_nanoseconds(result.message)

            # Demonstrate round-trip conversion
            ns_back = seconds_to_nanoseconds(ts_sec)
            sec_back = nanoseconds_to_seconds(ts_ns)

            print(
                f"  [{count}] {result.message_type.name:20s}"
                f"  {ts_sec:.3f}s"
                f"  ({ts_ns} ns)"
                f"  roundtrip: {sec_back:.3f}s / {ns_back} ns"
            )
            count += 1

    print(f"Read {count} messages via open_trace_file()\n")

    # --- Method 2: TraceReaderFactory.create_reader() — explicit factory ---
    print("=== Method 2: TraceReaderFactory.create_reader() ===")
    print(f"Input: {input_path}")
    reader2 = TraceReaderFactory.create_reader(input_path, message_type=msg_type)
    reader2.open(input_path)

    count2 = 0
    with reader2:
        for result in reader2:
            count2 += 1

    print(f"Read {count2} messages via TraceReaderFactory (same result, explicit API)")
    print(
        "\nBoth methods auto-detect format from extension: .osi → BinaryReader, .mcap → MCAPReader, .txth → TXTHReader"
    )

    # --- Method 3: FilenameUtils — infer message type and parse naming convention ---
    print("\n=== Method 3: FilenameUtils ===")
    filename = input_path.name
    inferred = infer_message_type_from_filename(filename)
    print(f"  infer_message_type_from_filename('{filename}') -> {inferred.name}")

    parsed = parse_osi_trace_filename(filename)
    if parsed:
        print("  parse_osi_trace_filename (OSI naming convention detected):")
        for key, value in parsed.items():
            print(f"    {key}: {value}")
    else:
        print("  parse_osi_trace_filename -> no match (filename does not follow OSI naming convention)")

    return 0


if __name__ == "__main__":
    sys.exit(main())

