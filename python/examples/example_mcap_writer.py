# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Write an example OSI MCAP trace file with metadata and channels."""

from __future__ import annotations

import sys
import tempfile
from pathlib import Path

from osi3.osi_sensorview_pb2 import SensorView

from osi_utilities import MCAPTraceFileWriter
from osi_utilities.tracefile.timestamp import timestamp_to_nanoseconds


def main() -> int:
    """Create a SensorView MCAP file with 10 frames of a moving vehicle."""
    print("Starting MCAP Writer example:")

    trace_file_path = Path(tempfile.gettempdir()) / "sv_example.mcap"
    print(f"Creating trace file at {trace_file_path}")

    metadata = {
        "description": "Example MCAP trace file created with the ASAM OSI utilities Python SDK.",
    }

    writer = MCAPTraceFileWriter()
    if not writer.open(trace_file_path, metadata=metadata):
        print(f"Error: Could not open '{trace_file_path}'", file=sys.stderr)
        return 1

    topic = "Sensor_1_Input"
    writer.add_channel(
        topic,
        SensorView,
        metadata={"net.asam.osi.trace.channel.description": "Input data (SensorView) for sensor 1"},
    )

    # Build a SensorView with one moving object
    sensor_view = SensorView()
    sensor_view.sensor_id.value = 0
    sensor_view.host_vehicle_id.value = 12
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

            if not writer.write_message(sensor_view, topic):
                print(f"Error: Could not write message {i}", file=sys.stderr)
                return 1

    print("Finished MCAP Writer example")
    return 0


if __name__ == "__main__":
    sys.exit(main())
