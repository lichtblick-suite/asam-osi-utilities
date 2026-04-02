# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Demonstrate multi-channel MCAP writing with OSI and non-OSI topics.

This example shows two approaches for writing multi-channel MCAP files:

**Part 1 — MCAPTraceFileWriter (pure OSI, multi-topic)**
Uses the high-level writer to register multiple OSI channels (including
two channels sharing the same schema) and write messages in a simulation loop.

**Part 2 — MCAPChannel (mixed OSI / non-OSI)**
Uses an externally-managed ``mcap.writer.Writer`` together with the
MCAPChannel helper to mix OSI and non-OSI channels in a single
MCAP file, then reads it back with non-OSI message filtering.
"""

from __future__ import annotations

import json
import os
import sys
from pathlib import Path

from osi3.osi_groundtruth_pb2 import GroundTruth
from osi3.osi_sensorview_pb2 import SensorView

from osi_utilities import MCAPTraceFileReader, MCAPTraceFileWriter
from osi_utilities.tracefile.mcap_channel import MCAPChannel
from osi_utilities.tracefile.mcap_writer import prepare_required_file_metadata
from osi_utilities.tracefile.timestamp import timestamp_to_nanoseconds

try:
    from mcap.writer import Writer as McapRawWriter
except ImportError:
    print("Error: mcap package is required. Install with: pip install mcap", file=sys.stderr)
    sys.exit(1)

NANOSECONDS_PER_SECOND = 1_000_000_000
NUM_STEPS = 10
STEP_NS = 100_000_000  # 100 ms


# ---------------------------------------------------------------------------
# Helpers
# ---------------------------------------------------------------------------


def generate_temp_file_path(suffix: str) -> Path:
    """Generate a temporary output path for the multi-channel example."""
    output_dir = Path(__file__).resolve().parent.parent.parent / ".playground"
    output_dir.mkdir(exist_ok=True)
    return output_dir / f"multi_channel_example_{suffix}_{os.getpid()}.mcap"


def populate_ground_truth(ground_truth: GroundTruth) -> None:
    """Populate a GroundTruth message with basic test data."""
    host = ground_truth.moving_object.add()
    host.id.value = 1
    host.vehicle_classification.type = 2  # TYPE_SMALL_CAR
    host.base.dimension.length = 4.5
    host.base.dimension.width = 1.8
    host.base.dimension.height = 1.4
    host.base.velocity.x = 15.0


def populate_sensor_view(sensor_view: SensorView, sensor_id: int) -> None:
    """Populate a SensorView message with basic test data."""
    sensor_view.sensor_id.value = sensor_id
    sensor_view.host_vehicle_id.value = 1


def advance_timestamp(message: GroundTruth | SensorView, step_ns: int) -> None:
    """Advance the timestamp on a message by a fixed step."""
    time_ns = timestamp_to_nanoseconds(message) + step_ns
    message.timestamp.seconds = int(time_ns // NANOSECONDS_PER_SECOND)
    message.timestamp.nanos = int(time_ns % NANOSECONDS_PER_SECOND)

    # Keep embedded ground truth timestamp in sync for SensorView
    if hasattr(message, "global_ground_truth"):
        message.global_ground_truth.timestamp.CopyFrom(message.timestamp)


def read_back_and_print_summary(path: Path, skip_non_osi: bool) -> None:
    """Read an MCAP file back and print a per-channel message count summary."""
    reader = MCAPTraceFileReader()
    if not reader.open(path):
        print(f"  ERROR: could not open {path}", file=sys.stderr)
        return

    reader.set_silence_incompatible_topic_warnings(skip_non_osi)

    channel_counts: dict[str, int] = {}
    with reader:
        for result in reader:
            channel_counts[result.channel_name] = channel_counts.get(result.channel_name, 0) + 1

    print(f"  Read-back summary ({path.name}):")
    for channel, count in sorted(channel_counts.items()):
        print(f'    channel "{channel}": {count} messages')


# ===========================================================================
# Part 1 — Multi-topic writing with MCAPTraceFileWriter
# ===========================================================================


def part1_multi_topic_writer() -> None:
    """Demonstrate pure-OSI multi-channel writing via MCAPTraceFileWriter.

    Registers three channels (GroundTruth + two SensorView topics that share
    the same protobuf schema) and writes messages in a simulated time loop.
    """
    print("\n=== Part 1: Multi-topic writing with MCAPTraceFileWriter ===")

    path = generate_temp_file_path("part1")
    print(f"  Output: {path}")

    writer = MCAPTraceFileWriter()

    metadata = prepare_required_file_metadata()
    metadata["description"] = "Multi-channel example (Part 1) — pure OSI topics."
    metadata["authors"] = "ASAM OSI Utilities example"

    if not writer.open(path, metadata=metadata):
        print(f"  ERROR: could not open {path}", file=sys.stderr)
        return

    # --- channel registration -----------------------------------------------
    # Channel 1: GroundTruth (unique schema)
    writer.add_channel(
        "ground_truth",
        GroundTruth,
        metadata={"net.asam.osi.trace.channel.description": "Environment ground truth"},
    )

    # Channels 2 & 3: two SensorView topics sharing the same protobuf schema.
    # The library automatically deduplicates the schema — only one copy of the
    # osi3.SensorView file-descriptor-set is stored in the MCAP file.
    writer.add_channel(
        "sensor_1_input",
        SensorView,
        metadata={"net.asam.osi.trace.channel.description": "Front-radar SensorView input"},
    )
    writer.add_channel(
        "sensor_2_input",
        SensorView,
        metadata={"net.asam.osi.trace.channel.description": "Rear-camera SensorView input"},
    )

    print("  Registered 3 channels (1x GroundTruth, 2x SensorView)")

    # --- prepare test data --------------------------------------------------
    ground_truth = GroundTruth()
    populate_ground_truth(ground_truth)

    sensor_view_1 = SensorView()
    populate_sensor_view(sensor_view_1, sensor_id=10)

    sensor_view_2 = SensorView()
    populate_sensor_view(sensor_view_2, sensor_id=20)

    # --- simulation loop ----------------------------------------------------
    with writer:
        for _ in range(NUM_STEPS):
            advance_timestamp(ground_truth, STEP_NS)
            advance_timestamp(sensor_view_1, STEP_NS)
            advance_timestamp(sensor_view_2, STEP_NS)

            writer.write_message(ground_truth, "ground_truth")
            writer.write_message(sensor_view_1, "sensor_1_input")
            writer.write_message(sensor_view_2, "sensor_2_input")

    print(f"  Wrote {NUM_STEPS} steps (3 messages each) to {path.name}")

    # --- read back ----------------------------------------------------------
    read_back_and_print_summary(path, skip_non_osi=False)


# ===========================================================================
# Part 2 — Mixed OSI / non-OSI with MCAPChannel
# ===========================================================================


def part2_mixed_channel_writer() -> None:
    """Demonstrate the external-writer pattern for heterogeneous MCAP files.

    Creates a ``mcap.writer.Writer`` directly, attaches an MCAPChannel helper
    for OSI channels, and also registers a non-OSI channel to show how both
    coexist in a single MCAP file.
    """
    print("\n=== Part 2: Mixed OSI / non-OSI with MCAPChannel ===")

    path = generate_temp_file_path("part2")
    print(f"  Output: {path}")

    # --- open external writer -----------------------------------------------
    file = open(path, "wb")  # noqa: SIM115
    mcap_writer = McapRawWriter(file)
    mcap_writer.start(library="osi-utilities-python")

    # Best practice: write OSI file metadata before any messages.
    # When using an external writer you must do this yourself — the
    # MCAPChannel helper does NOT write metadata automatically.
    osi_metadata = prepare_required_file_metadata()
    osi_metadata["description"] = "Mixed-channel example (Part 2) — OSI + custom topics."
    mcap_writer.add_metadata(name="net.asam.osi.trace", data=osi_metadata)

    # --- register a non-OSI channel directly on the writer ------------------
    # Best practice: use a descriptive prefix (e.g. "vehicle/") for non-OSI
    # topics so they are easily distinguishable from "osi/" topics in tooling.
    json_schema_data = json.dumps(
        {"type": "object", "properties": {"speed_kmh": {"type": "number"}, "gear": {"type": "integer"}}}
    )
    json_schema_id = mcap_writer.register_schema(
        name="vehicle.Status", encoding="jsonschema", data=json_schema_data.encode()
    )
    custom_channel_id = mcap_writer.register_channel(
        topic="vehicle/status", message_encoding="json", schema_id=json_schema_id
    )
    print("  Registered non-OSI channel: vehicle/status (json)")

    # --- OSI channel helper -------------------------------------------------
    # MCAPChannel manages schema registration, buffer reuse, and
    # OSI-compliant channel metadata for you.
    osi_channels = MCAPChannel(mcap_writer)
    osi_channels.add_channel(
        "osi/ground_truth",
        GroundTruth,
        metadata={"net.asam.osi.trace.channel.description": "Environment ground truth (via external writer)"},
    )
    print("  Registered OSI channel: osi/ground_truth (protobuf)")

    # --- prepare test data --------------------------------------------------
    ground_truth = GroundTruth()
    populate_ground_truth(ground_truth)

    # --- simulation loop — interleave OSI and non-OSI messages --------------
    for i in range(NUM_STEPS):
        advance_timestamp(ground_truth, STEP_NS)

        # Write OSI message via the helper
        osi_channels.write_message(ground_truth, "osi/ground_truth")

        # Write non-OSI message directly to the mcap writer
        json_payload = json.dumps({"speed_kmh": 50.0 + i * 2.0, "gear": 3 + i // 4}).encode()
        log_time = timestamp_to_nanoseconds(ground_truth)
        mcap_writer.add_message(
            channel_id=custom_channel_id,
            log_time=log_time,
            data=json_payload,
            publish_time=log_time,
        )

    mcap_writer.finish()
    file.close()
    print(f"  Wrote {NUM_STEPS} steps (1 OSI + 1 non-OSI each) to {path.name}")

    # --- read back with non-OSI filtering -----------------------------------
    # Best practice: when reading a mixed file with MCAPTraceFileReader, enable
    # set_skip_non_osi_msgs(True) to silently skip channels that are not recognized
    # OSI types.  Without this flag the reader would log warnings on unknown schemas.
    print("  Reading back with set_skip_non_osi_msgs(True):")
    read_back_and_print_summary(path, skip_non_osi=True)


# ===========================================================================
# main
# ===========================================================================


def main() -> int:
    """Entry point for the multi-channel MCAP writer example."""
    print("Starting Multi-Channel MCAP Writer example")

    part1_multi_topic_writer()
    part2_mixed_channel_writer()

    print("\nFinished Multi-Channel MCAP Writer example")
    return 0


if __name__ == "__main__":
    sys.exit(main())
