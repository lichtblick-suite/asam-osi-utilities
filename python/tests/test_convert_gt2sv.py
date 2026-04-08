# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Tests for the GroundTruth to SensorView converter."""

from __future__ import annotations

import tempfile
from pathlib import Path

import pytest
from osi3.osi_groundtruth_pb2 import GroundTruth

from osi_utilities import (
    SingleTraceReader,
    SingleTraceWriter,
    MultiTraceReader,
    MultiTraceWriter,
    MessageType,
    convert_gt2sv,
)

# ===========================================================================
# Helpers
# ===========================================================================


def _make_ground_truth(index: int = 0, num_objects: int = 2) -> GroundTruth:
    """Create a GroundTruth message with a timestamp and moving objects."""
    gt = GroundTruth()
    gt.timestamp.seconds = index
    gt.timestamp.nanos = index * 100_000
    gt.host_vehicle_id.value = 0
    for obj_id in range(1, num_objects + 1):
        mo = gt.moving_object.add()
        mo.id.value = obj_id
        mo.base.position.x = float(index * 10 + obj_id)
        mo.base.position.y = float(obj_id)
        mo.base.position.z = 0.0
    return gt


@pytest.fixture()
def tmp_dir():
    with tempfile.TemporaryDirectory() as d:
        yield Path(d)


def _write_gt_binary(path: Path, count: int = 5) -> list[GroundTruth]:
    """Write GroundTruth messages to a binary .osi file and return them."""
    messages = [_make_ground_truth(i) for i in range(count)]
    with SingleTraceWriter() as writer:
        assert writer.open(path)
        for msg in messages:
            writer.write_message(msg)
    return messages


def _write_gt_mcap(path: Path, count: int = 5, topic: str = "GroundTruth") -> list[GroundTruth]:
    """Write GroundTruth messages to an MCAP file and return them."""
    messages = [_make_ground_truth(i) for i in range(count)]
    with MultiTraceWriter() as writer:
        assert writer.open(path)
        writer.add_channel(topic, GroundTruth)
        for msg in messages:
            writer.write_message(msg, topic)
    return messages


# ===========================================================================
# Tests
# ===========================================================================


class TestConvertGt2svBinary:
    """Test binary .osi → binary .osi conversion."""

    def test_basic_conversion(self, tmp_dir: Path):
        gt_path = tmp_dir / "input_gt.osi"
        sv_path = tmp_dir / "output_sv.osi"
        _write_gt_binary(gt_path, count=5)

        count = convert_gt2sv(gt_path, sv_path)

        assert count == 5
        assert sv_path.exists()

    def test_output_contains_sensorview_messages(self, tmp_dir: Path):
        gt_path = tmp_dir / "input_gt.osi"
        sv_path = tmp_dir / "output_sv.osi"
        _write_gt_binary(gt_path, count=3)

        convert_gt2sv(gt_path, sv_path)

        with SingleTraceReader(MessageType.SENSOR_VIEW) as reader:
            assert reader._open(sv_path)
            results = list(reader)
            assert len(results) == 3
            for r in results:
                assert r.message_type == MessageType.SENSOR_VIEW

    def test_ground_truth_preserved_in_sv(self, tmp_dir: Path):
        gt_path = tmp_dir / "input_gt.osi"
        sv_path = tmp_dir / "output_sv.osi"
        _write_gt_binary(gt_path, count=3)

        convert_gt2sv(gt_path, sv_path)

        with SingleTraceReader(MessageType.SENSOR_VIEW) as reader:
            assert reader._open(sv_path)
            for i, result in enumerate(reader):
                sv = result.message
                assert sv.HasField("global_ground_truth")
                gt_out = sv.global_ground_truth
                assert gt_out.timestamp.seconds == i
                assert len(gt_out.moving_object) == 2
                assert gt_out.moving_object[0].id.value == 1

    def test_timestamp_copied(self, tmp_dir: Path):
        gt_path = tmp_dir / "input_gt.osi"
        sv_path = tmp_dir / "output_sv.osi"
        _write_gt_binary(gt_path, count=2)

        convert_gt2sv(gt_path, sv_path)

        with SingleTraceReader(MessageType.SENSOR_VIEW) as reader:
            assert reader._open(sv_path)
            results = list(reader)
            assert results[0].message.timestamp.seconds == 0
            assert results[1].message.timestamp.seconds == 1

    def test_host_vehicle_id_copied(self, tmp_dir: Path):
        gt_path = tmp_dir / "input_gt.osi"
        sv_path = tmp_dir / "output_sv.osi"
        _write_gt_binary(gt_path, count=1)

        convert_gt2sv(gt_path, sv_path)

        with SingleTraceReader(MessageType.SENSOR_VIEW) as reader:
            assert reader._open(sv_path)
            result = next(iter(reader))
            assert result.message.HasField("host_vehicle_id")
            assert result.message.host_vehicle_id.value == 0


class TestConvertGt2svMCAP:
    """Test MCAP → MCAP conversion."""

    def test_mcap_to_mcap(self, tmp_dir: Path):
        gt_path = tmp_dir / "input.mcap"
        sv_path = tmp_dir / "output.mcap"
        _write_gt_mcap(gt_path, count=4)

        count = convert_gt2sv(gt_path, sv_path)

        assert count == 4
        with MultiTraceReader() as reader:
            assert reader._open(sv_path)
            results = list(reader)
            assert len(results) == 4
            for r in results:
                assert r.message_type == MessageType.SENSOR_VIEW

    def test_mcap_ground_truth_preserved(self, tmp_dir: Path):
        gt_path = tmp_dir / "input.mcap"
        sv_path = tmp_dir / "output.mcap"
        _write_gt_mcap(gt_path, count=2)

        convert_gt2sv(gt_path, sv_path)

        with MultiTraceReader() as reader:
            assert reader._open(sv_path)
            for i, result in enumerate(reader):
                sv = result.message
                assert sv.HasField("global_ground_truth")
                assert sv.global_ground_truth.timestamp.seconds == i

    def test_mcap_with_topic_filter(self, tmp_dir: Path):
        gt_path = tmp_dir / "input.mcap"
        sv_path = tmp_dir / "output.mcap"
        _write_gt_mcap(gt_path, count=3, topic="my_gt")

        count = convert_gt2sv(gt_path, sv_path, topic="my_gt")

        assert count == 3


class TestConvertGt2svCrossFormat:
    """Test cross-format conversion (binary ↔ MCAP)."""

    def test_binary_to_mcap(self, tmp_dir: Path):
        gt_path = tmp_dir / "input_gt.osi"
        sv_path = tmp_dir / "output_sv.mcap"
        _write_gt_binary(gt_path, count=3)

        count = convert_gt2sv(gt_path, sv_path)

        assert count == 3
        with MultiTraceReader() as reader:
            assert reader._open(sv_path)
            results = list(reader)
            assert len(results) == 3
            for r in results:
                assert r.message_type == MessageType.SENSOR_VIEW

    def test_mcap_to_binary(self, tmp_dir: Path):
        gt_path = tmp_dir / "input.mcap"
        sv_path = tmp_dir / "output_sv.osi"
        _write_gt_mcap(gt_path, count=3)

        count = convert_gt2sv(gt_path, sv_path)

        assert count == 3
        with SingleTraceReader(MessageType.SENSOR_VIEW) as reader:
            assert reader._open(sv_path)
            results = list(reader)
            assert len(results) == 3


class TestConvertGt2svEdgeCases:
    """Edge cases and error handling."""

    def test_empty_trace(self, tmp_dir: Path):
        gt_path = tmp_dir / "empty_gt.osi"
        sv_path = tmp_dir / "output_sv.osi"
        # Write empty file with valid binary format (no messages)
        with SingleTraceWriter() as writer:
            writer.open(gt_path)

        count = convert_gt2sv(gt_path, sv_path)
        assert count == 0

    def test_file_not_found(self, tmp_dir: Path):
        with pytest.raises(FileNotFoundError):
            convert_gt2sv(tmp_dir / "nonexistent.osi", tmp_dir / "out.osi")

    def test_unsupported_extension(self, tmp_dir: Path):
        bad_input = tmp_dir / "input.json"
        bad_input.write_text("{}")
        with pytest.raises(ValueError, match="Unsupported"):
            convert_gt2sv(bad_input, tmp_dir / "out.osi")

    def test_single_frame(self, tmp_dir: Path):
        gt_path = tmp_dir / "single_gt.osi"
        sv_path = tmp_dir / "single_sv.osi"
        _write_gt_binary(gt_path, count=1)

        count = convert_gt2sv(gt_path, sv_path)

        assert count == 1
        with SingleTraceReader(MessageType.SENSOR_VIEW) as reader:
            assert reader._open(sv_path)
            results = list(reader)
            assert len(results) == 1
            assert results[0].message.HasField("global_ground_truth")
