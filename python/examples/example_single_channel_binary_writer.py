# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Write a single-channel binary OSI .osi trace file."""

from __future__ import annotations

import os
import sys
import tempfile
from pathlib import Path

from osi3.osi_sensorview_pb2 import SensorView

from osi_utilities import BinaryTraceFileWriter
from osi_utilities.tracefile.timestamp import timestamp_to_nanoseconds


def main() -> int:
    """Create a binary .osi file with 10 SensorView frames."""
    print("Starting single-channel binary writer example:")

    file_name = f"sv_example_{os.getpid()}.osi"
    trace_file_path = Path(tempfile.gettempdir()) / file_name
    print(f"Creating trace file at {trace_file_path}")

    writer = BinaryTraceFileWriter()
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

    print("Finished single-channel binary writer example")
    return 0


if __name__ == "__main__":
    sys.exit(main())
