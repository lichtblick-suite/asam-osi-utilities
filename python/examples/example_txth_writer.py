# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Write an OSI .txth text trace file."""

from __future__ import annotations

import os
import sys
from datetime import datetime, timezone
from pathlib import Path

import google.protobuf
from osi3.osi_sensorview_pb2 import SensorView

from osi_utilities import TXTHTraceFileWriter
from osi_utilities.tracefile.timestamp import timestamp_to_nanoseconds


def _generate_osi_filename(description: str, extension: str, frame_count: int = 10) -> str:
    """Generate filename following OSI naming convention."""
    now = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    try:
        from osi3.osi_version_pb2 import DESCRIPTOR as VERSION_DESCRIPTOR
        from osi3.osi_version_pb2 import InterfaceVersion

        osi_ver = VERSION_DESCRIPTOR.file_options.Extensions[InterfaceVersion.current_interface_version]
        osi_version = f"{osi_ver.version_major}.{osi_ver.version_minor}.{osi_ver.version_patch}"
    except Exception:
        osi_version = "0.0.0"
    proto_version = google.protobuf.__version__
    pid = os.getpid()
    return f"{now}_sv_{osi_version}_{proto_version}_{frame_count}_{pid}_{description}.{extension}"


def main() -> int:
    """Create a .txth file with 10 SensorView frames in text format."""
    print("Starting TXTH writer example:")

    file_name = _generate_osi_filename("example_txth_writer", "txth")
    output_dir = Path(__file__).resolve().parent.parent.parent / ".playground"
    output_dir.mkdir(exist_ok=True)
    trace_file_path = output_dir / file_name
    print(f"Creating trace file at {trace_file_path}")

    writer = TXTHTraceFileWriter()
    if not writer.open(trace_file_path):
        print(f"Error: Could not open '{trace_file_path}'", file=sys.stderr)
        return 1

    # Build a SensorView with one moving object
    sensor_view = SensorView()
    sensor_view.sensor_id.value = 0
    gt = sensor_view.global_ground_truth
    host = gt.moving_object.add()
    host.id.value = 12
    host.vehicle_classification.type = 2  # TYPE_SMALL_CAR
    host.base.dimension.length = 5
    host.base.dimension.width = 2
    host.base.dimension.height = 1.5
    host.base.velocity.x = 10.0

    time_step_ns = int(0.1 * 1e9)
    with writer:
        for i in range(10):
            ts_ns = timestamp_to_nanoseconds(sensor_view) + time_step_ns
            sensor_view.timestamp.seconds = int(ts_ns // 1e9)
            sensor_view.timestamp.nanos = int(ts_ns % 1e9)
            gt.timestamp.CopyFrom(sensor_view.timestamp)

            host.base.position.x += host.base.velocity.x * 0.1

            if not writer.write_message(sensor_view):
                print(f"Error: Could not write message {i}", file=sys.stderr)
                return 1

    print("Finished TXTH writer example")
    return 0


if __name__ == "__main__":
    sys.exit(main())
