# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Benchmark read/write throughput for OSI trace files.

Two modes:
  benchmark.py synthetic [N]           — generate N SensorView messages, benchmark all 3 formats
  benchmark.py file <path> [--type T]  — benchmark read/write on a real .osi file
"""

from __future__ import annotations

import argparse
import sys
import tempfile
import time
from pathlib import Path

from osi3.osi_sensorview_pb2 import SensorView

from osi_utilities import (
    BinaryTraceFileReader,
    BinaryTraceFileWriter,
    MCAPTraceFileReader,
    MCAPTraceFileWriter,
    MessageType,
    TXTHTraceFileReader,
    TXTHTraceFileWriter,
)

# ---------------------------------------------------------------------------
# Shared helpers
# ---------------------------------------------------------------------------

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


def _print_row(fmt: str, operation: str, seconds: float, megabytes: float) -> None:
    throughput = megabytes / seconds if seconds > 0 else 0.0
    print(f"{fmt:<10}{operation:<10}{seconds:>10.3f} s{throughput:>12.1f} MB/s")


# ---------------------------------------------------------------------------
# Synthetic-mode helpers
# ---------------------------------------------------------------------------


def _generate_messages(count: int) -> list[SensorView]:
    """Pre-generate *count* SensorView messages with 5 moving objects each."""
    try:
        from osi3.osi_version_pb2 import DESCRIPTOR as VERSION_DESCRIPTOR
        from osi3.osi_version_pb2 import InterfaceVersion

        osi_version = VERSION_DESCRIPTOR.file_options.Extensions[InterfaceVersion.current_interface_version]
    except Exception:
        osi_version = None

    messages: list[SensorView] = []
    for i in range(count):
        sv = SensorView()
        if osi_version is not None:
            sv.version.CopyFrom(osi_version)
        sv.sensor_id.value = 0
        sv.host_vehicle_id.value = 12
        sv.timestamp.seconds = i // 10
        sv.timestamp.nanos = (i % 10) * 100_000_000

        gt = sv.global_ground_truth
        if osi_version is not None:
            gt.version.CopyFrom(osi_version)
        gt.timestamp.CopyFrom(sv.timestamp)

        for obj in range(5):
            mo = gt.moving_object.add()
            mo.id.value = 100 + obj
            mo.vehicle_classification.type = 2  # TYPE_SMALL_CAR
            mo.base.dimension.length = 4.5
            mo.base.dimension.width = 1.8
            mo.base.dimension.height = 1.4
            mo.base.position.x = float(i) * 10.0 + float(obj)
            mo.base.position.y = float(obj) * 3.5
            mo.base.velocity.x = 30.0

        messages.append(sv)
    return messages


# ---------------------------------------------------------------------------
# Synthetic mode
# ---------------------------------------------------------------------------


def _run_synthetic(num_messages: int) -> int:
    print(f"Generating {num_messages} SensorView messages (5 objects each)...")
    messages = _generate_messages(num_messages)

    single_size = messages[0].ByteSize()
    total_bytes = single_size * num_messages
    total_mb = total_bytes / (1024.0 * 1024.0)
    print(f"Approx. payload: {total_mb:.1f} MB ({single_size} bytes/msg)\n")

    tmp = Path(tempfile.gettempdir())
    mcap_path = tmp / "bench_sv_.mcap"
    osi_path = tmp / "bench_sv_.osi"
    txth_path = tmp / "bench_sv_.txth"

    # Header
    print(f"{'Format':<10}{'Op':<10}{'Time':>12}{'Throughput':>14}")
    print("-" * 46)

    # ====================== MCAP ======================
    writer = MCAPTraceFileWriter()
    writer.open(mcap_path)
    topic = "SensorView"
    writer.add_channel(topic, SensorView)

    t0 = time.perf_counter()
    with writer:
        for msg in messages:
            writer.write_message(msg, topic)
    _print_row("MCAP", "write", time.perf_counter() - t0, total_mb)

    reader = MCAPTraceFileReader()
    reader.open(mcap_path)

    t0 = time.perf_counter()
    count = 0
    with reader:
        for _ in reader:
            count += 1
    _print_row("MCAP", "read", time.perf_counter() - t0, total_mb)

    # ====================== Binary .osi ======================
    writer = BinaryTraceFileWriter()
    writer.open(osi_path)

    t0 = time.perf_counter()
    with writer:
        for msg in messages:
            writer.write_message(msg)
    _print_row(".osi", "write", time.perf_counter() - t0, total_mb)

    reader = BinaryTraceFileReader(message_type=MessageType.SENSOR_VIEW)
    reader.open(osi_path)

    t0 = time.perf_counter()
    count = 0
    with reader:
        for _ in reader:
            count += 1
    _print_row(".osi", "read", time.perf_counter() - t0, total_mb)

    # ====================== TXTH ======================
    writer = TXTHTraceFileWriter()
    writer.open(txth_path)

    t0 = time.perf_counter()
    with writer:
        for msg in messages:
            writer.write_message(msg)
    _print_row(".txth", "write", time.perf_counter() - t0, total_mb)

    reader = TXTHTraceFileReader(message_type=MessageType.SENSOR_VIEW)
    reader.open(txth_path)

    t0 = time.perf_counter()
    count = 0
    with reader:
        for _ in reader:
            count += 1
    _print_row(".txth", "read", time.perf_counter() - t0, total_mb)

    # ====================== File sizes ======================
    print("\nFile sizes:")
    for label, path in [("MCAP", mcap_path), (".osi", osi_path), (".txth", txth_path)]:
        size = path.stat().st_size
        print(f"  {label:<8}{size:>12} bytes ({size / (1024.0 * 1024.0):.1f} MB)")

    # Cleanup
    for path in (mcap_path, osi_path, txth_path):
        path.unlink(missing_ok=True)

    print("\nDone. Temp files cleaned up.")
    return 0


# ---------------------------------------------------------------------------
# File mode
# ---------------------------------------------------------------------------


def _print_metrics(label: str, frame_count: int, byte_count: float, elapsed_s: float) -> None:
    mib = byte_count / (1024.0 * 1024.0)
    print(f"\n--- {label} ---")
    print(f"  Frames:  {frame_count}")
    print(f"  Time:    {elapsed_s:.6f} s")
    if elapsed_s > 0:
        print(f"  Speed:   {mib / elapsed_s:.1f} MiB/s")
        print(f"  Rate:    {frame_count / elapsed_s:.1f} frames/s")


def _run_file(input_path: Path, message_type: MessageType) -> int:
    if not input_path.exists():
        print(f"ERROR: File not found: {input_path}", file=sys.stderr)
        return 1

    file_size = input_path.stat().st_size
    print(f"File:    {input_path}")
    print(f"Size:    {file_size} bytes ({file_size / (1024.0 * 1024.0):.1f} MiB)")

    # Read benchmark
    reader = BinaryTraceFileReader(message_type=message_type)
    if not reader.open(input_path):
        print(f"ERROR: Could not open: {input_path}", file=sys.stderr)
        return 1

    messages = []
    t0 = time.perf_counter()
    with reader:
        for result in reader:
            messages.append(result)
    read_elapsed = time.perf_counter() - t0

    _print_metrics("Read", len(messages), float(file_size), read_elapsed)

    # Write benchmark
    tmp_path = Path(tempfile.gettempdir()) / "benchmark_write_output.osi"
    writer = BinaryTraceFileWriter()
    if not writer.open(tmp_path):
        print(f"ERROR: Could not open temp file for write benchmark: {tmp_path}", file=sys.stderr)
        return 1

    written = 0
    t0 = time.perf_counter()
    with writer:
        for result in messages:
            if not writer.write_message(result.message):
                print(f"WARNING: Failed to write frame {written}", file=sys.stderr)
                break
            written += 1
    write_elapsed = time.perf_counter() - t0

    written_size = tmp_path.stat().st_size if tmp_path.exists() else 0
    tmp_path.unlink(missing_ok=True)

    _print_metrics("Write", written, float(written_size), write_elapsed)
    return 0


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------


def main() -> int:
    """Benchmark read/write throughput for OSI trace files."""
    parser = argparse.ArgumentParser(
        description="Benchmark read/write throughput for OSI trace files.",
        epilog=(
            "Examples:\n"
            "  python benchmark.py synthetic\n"
            "  python benchmark.py synthetic 5000\n"
            "  python benchmark.py file trace.osi --type SensorView\n"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    subparsers = parser.add_subparsers(dest="command")

    # synthetic sub-command
    syn_parser = subparsers.add_parser("synthetic", help="Generate synthetic messages and benchmark all 3 formats")
    syn_parser.add_argument(
        "n", nargs="?", type=int, default=1000, help="Number of SensorView messages (default: 1000)"
    )

    # file sub-command
    file_parser = subparsers.add_parser("file", help="Benchmark read/write on a real .osi file")
    file_parser.add_argument("path", help="Path to the .osi trace file")
    file_parser.add_argument(
        "--type", dest="message_type", default=None, choices=VALID_TYPES.keys(), help="Message type"
    )

    args = parser.parse_args()

    if args.command == "synthetic":
        if args.n <= 0:
            print("ERROR: N must be a positive integer", file=sys.stderr)
            return 1
        return _run_synthetic(args.n)

    if args.command == "file":
        msg_type = VALID_TYPES[args.message_type] if args.message_type else MessageType.UNKNOWN
        return _run_file(Path(args.path), msg_type)

    parser.print_help()
    return 1


if __name__ == "__main__":
    sys.exit(main())
