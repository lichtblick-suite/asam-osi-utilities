# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Convert GroundTruth traces to SensorView traces.

Python equivalent of ``cpp/examples/convert_gt2sv.cpp``.
Each GroundTruth frame is wrapped in a SensorView message whose
``global_ground_truth`` field contains the original data.

Supports all trace file formats (binary ``.osi``, ``.mcap``) via the
SDK reader/writer factories.

Usage as library::

    from osi_utilities.converters.gt2sv import convert_gt2sv

    convert_gt2sv("input_gt.osi", "output_sv.osi")
    convert_gt2sv("input_gt.mcap", "output_sv.mcap", topic="GroundTruth")

Usage as CLI::

    python -m osi_utilities.converters.gt2sv input_gt.osi output_sv.osi
"""

from __future__ import annotations

import argparse
import logging
import sys
from pathlib import Path

from google.protobuf.message import Message

from osi_utilities.api.types import ChannelSpecification, MessageType, TraceFileFormat
from osi_utilities.tracefile.configure import configure_reader, create_reader
from osi_utilities.tracefile.format import get_trace_file_format

logger = logging.getLogger(__name__)


def _wrap_gt_in_sv(gt_msg: Message) -> Message:
    """Wrap a GroundTruth message inside a SensorView message.

    Copies timestamp, host_vehicle_id, and sets global_ground_truth.

    Uses MergeFromString to avoid cross-class CopyFrom errors when
    the reader returns dynamically-generated protobuf classes (e.g. MCAP).

    Args:
        gt_msg: A deserialized osi3.GroundTruth protobuf message.

    Returns:
        A new osi3.SensorView message wrapping the GroundTruth.
    """
    from osi3.osi_groundtruth_pb2 import GroundTruth
    from osi3.osi_sensorview_pb2 import SensorView

    # Re-parse through canonical osi3 classes to avoid CopyFrom type mismatches
    gt_canonical = GroundTruth()
    gt_canonical.MergeFromString(gt_msg.SerializeToString())

    sv_msg = SensorView()
    if gt_canonical.HasField("timestamp"):
        sv_msg.timestamp.CopyFrom(gt_canonical.timestamp)
    if gt_canonical.HasField("host_vehicle_id"):
        sv_msg.host_vehicle_id.CopyFrom(gt_canonical.host_vehicle_id)
    sv_msg.global_ground_truth.CopyFrom(gt_canonical)
    return sv_msg


def convert_gt2sv(
    input_path: str | Path,
    output_path: str | Path,
    *,
    topic: str | None = None,
) -> int:
    """Convert a GroundTruth trace file to a SensorView trace file.

    Each GroundTruth frame is wrapped in a SensorView message whose
    ``global_ground_truth`` field contains the original data.
    Supports both binary ``.osi`` and ``.mcap`` formats for input and output
    independently (e.g. ``.osi`` input to ``.mcap`` output).

    Args:
        input_path: Path to the input GroundTruth trace file.
        output_path: Path to the output SensorView trace file.
        topic: Topic filter for MCAP input files.

    Returns:
        Number of frames converted.

    Raises:
        FileNotFoundError: If the input file does not exist.
        ValueError: If the input file format is not supported.
        RuntimeError: If the input file cannot be opened.
    """
    input_path = Path(input_path)
    output_path = Path(output_path)

    if not input_path.exists():
        raise FileNotFoundError(f"Input file not found: {input_path}")

    # Open reader — we know the input contains GroundTruth messages
    reader = create_reader(input_path)
    input_channel_spec = ChannelSpecification(path=input_path, message_type=MessageType.GROUND_TRUTH, topic=topic)
    configure_reader(reader, input_channel_spec)
    if not reader.open(input_path):
        raise RuntimeError(f"Failed to open input file: {input_path}")

    # Determine output format and create appropriate writer
    output_format = get_trace_file_format(output_path)

    if output_format == TraceFileFormat.MULTI_CHANNEL:
        from osi3.osi_sensorview_pb2 import SensorView

        from osi_utilities.tracefile.writers.multi import MultiTraceWriter

        writer = MultiTraceWriter()
        if not writer.open(output_path):
            raise RuntimeError(f"Failed to open output file: {output_path}")
        output_topic = topic or "SensorView"
        writer.add_channel(output_topic, SensorView)
    elif output_format == TraceFileFormat.SINGLE_CHANNEL:
        from osi_utilities.tracefile.writers.single import SingleTraceWriter

        writer = SingleTraceWriter()
        if not writer.open(output_path):
            raise RuntimeError(f"Failed to open output file: {output_path}")
        output_topic = ""
    else:
        raise ValueError(f"Unsupported output format: {output_path.suffix}")

    frame_count = 0
    with writer, reader:
        for result in reader:
            if result.message_type != MessageType.GROUND_TRUTH:
                logger.warning(
                    "Skipping non-GroundTruth message (type=%s) at frame %d",
                    result.message_type,
                    frame_count,
                )
                continue

            sv_msg = _wrap_gt_in_sv(result.message)
            if not writer.write_message(sv_msg, output_topic):
                raise RuntimeError(f"Failed to write SensorView frame {frame_count} to {output_path}")
            frame_count += 1

    logger.info("Converted %d frames from GroundTruth to SensorView.", frame_count)
    return frame_count


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Convert an OSI GroundTruth trace to a SensorView trace.",
        epilog="Each GroundTruth frame is wrapped in a SensorView message "
        "with the original data in the global_ground_truth field.",
    )
    parser.add_argument("input", help="Path to the input GroundTruth trace file (.osi or .mcap)")
    parser.add_argument("output", help="Path to the output SensorView trace file (.osi or .mcap)")
    parser.add_argument(
        "--topic",
        default=None,
        help="Topic name to read from MCAP input (default: first channel)",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Enable verbose logging",
    )
    return parser


def main() -> None:
    """CLI entry point for gt2sv conversion."""
    parser = _build_parser()
    args = parser.parse_args()

    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(levelname)s: %(message)s",
    )

    try:
        count = convert_gt2sv(args.input, args.output, topic=args.topic)
        print(f"Converted {count} frames from GroundTruth to SensorView.")
    except (FileNotFoundError, ValueError, RuntimeError, OSError) as e:
        logger.error("%s", e)
        sys.exit(1)


if __name__ == "__main__":
    main()
