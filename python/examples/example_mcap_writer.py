# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Write an example OSI MCAP trace file with metadata and channels."""

from __future__ import annotations

import os
import sys
from datetime import datetime, timezone
from pathlib import Path

import google.protobuf
from osi3.osi_sensorview_pb2 import SensorView

from osi_utilities import MultiTraceWriter
from osi_utilities.timestamp import timestamp_to_nanoseconds
from osi_utilities.tracefile import DEFAULT_CHUNK_SIZE, MAX_CHUNK_SIZE, MIN_CHUNK_SIZE


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
    """Create a SensorView MCAP file with 10 frames of a moving vehicle."""
    print("Starting MCAP Writer example:")

    output_dir = Path(__file__).resolve().parent.parent.parent / ".playground"
    output_dir.mkdir(exist_ok=True)
    trace_file_path = output_dir / _generate_osi_filename("example_mcap_writer", "mcap")
    print(f"Creating trace file at {trace_file_path}")
    print(
        f"  Config: chunk_size={DEFAULT_CHUNK_SIZE // (1024 * 1024)} MiB"
        f" (range: {MIN_CHUNK_SIZE // (1024 * 1024)}-{MAX_CHUNK_SIZE // (1024 * 1024)} MiB)"
    )

    metadata = {
        "description": "Example MCAP trace file created with the ASAM OSI utilities Python SDK.",
    }

    writer = MultiTraceWriter()
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

        # Add custom metadata using the name + dict overload (alternative to constructor metadata)
        writer.add_file_metadata(
            "custom.example.metadata",
            {"source_tool": "example_mcap_writer", "scenario": "straight_road_acceleration"},
        )

    print("Finished MCAP Writer example")
    return 0


if __name__ == "__main__":
    sys.exit(main())
