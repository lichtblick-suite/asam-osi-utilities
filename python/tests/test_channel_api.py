# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Tests for open_channel / open_channel_writer channel abstraction API."""

from __future__ import annotations

import tempfile
from pathlib import Path

import pytest
from osi3.osi_groundtruth_pb2 import GroundTruth
from osi3.osi_sensorview_pb2 import SensorView

from osi_utilities import (
    ChannelSpecification,
    MessageType,
    MultiTraceWriter,
    SingleTraceWriter,
    open_channel,
    open_channel_writer,
)

# ===========================================================================
# Fixtures and helpers
# ===========================================================================


def _make_ground_truth(index: int = 0) -> GroundTruth:
    gt = GroundTruth()
    gt.timestamp.seconds = index
    gt.timestamp.nanos = index * 1000
    return gt


def _make_sensor_view(index: int = 0) -> SensorView:
    sv = SensorView()
    sv.timestamp.seconds = index
    sv.timestamp.nanos = index * 500
    return sv


def _write_binary(path: Path, count: int = 3) -> None:
    with SingleTraceWriter() as writer:
        assert writer.open(path)
        for i in range(count):
            writer.write_message(_make_ground_truth(i))


def _write_mcap(path: Path, count: int = 3, topic: str = "GroundTruth") -> None:
    with MultiTraceWriter() as writer:
        assert writer.open(path)
        writer.add_channel(topic, GroundTruth)
        for i in range(count):
            writer.write_message(_make_ground_truth(i), topic)


def _write_mcap_multi_channel(path: Path) -> None:
    with MultiTraceWriter() as writer:
        assert writer.open(path)
        writer.add_channel("gt", GroundTruth)
        writer.add_channel("sv", SensorView)
        writer.write_message(_make_ground_truth(0), "gt")
        writer.write_message(_make_ground_truth(1), "gt")
        writer.write_message(_make_sensor_view(10), "sv")


@pytest.fixture()
def tmp_dir():
    with tempfile.TemporaryDirectory() as d:
        yield Path(d)


# ===========================================================================
# open_channel — reading
# ===========================================================================


class TestOpenChannelBinary:
    def test_read_binary_with_message_type(self, tmp_dir: Path):
        path = tmp_dir / "test_gt.osi"
        _write_binary(path, count=3)

        spec = ChannelSpecification(path=path, message_type=MessageType.GROUND_TRUTH)
        with open_channel(spec) as ch:
            messages = list(ch)
        assert len(messages) == 3
        for i, msg in enumerate(messages):
            assert msg.timestamp.seconds == i

    def test_read_binary_autodetect_type(self, tmp_dir: Path):
        path = tmp_dir / "trace_gt.osi"
        _write_binary(path, count=2)

        spec = ChannelSpecification(path=path)
        with open_channel(spec) as ch:
            messages = list(ch)
        assert len(messages) == 2

    def test_get_channel_specification(self, tmp_dir: Path):
        path = tmp_dir / "test_gt.osi"
        _write_binary(path, count=1)

        spec = ChannelSpecification(path=path, message_type=MessageType.GROUND_TRUTH)
        with open_channel(spec) as ch:
            resolved = ch.get_channel_specification()
            assert resolved.path == path
            assert resolved.message_type == MessageType.GROUND_TRUTH


class TestOpenChannelMCAP:
    def test_read_mcap_explicit_topic(self, tmp_dir: Path):
        path = tmp_dir / "test.mcap"
        _write_mcap(path, count=3, topic="GroundTruth")

        spec = ChannelSpecification(path=path, topic="GroundTruth", message_type=MessageType.GROUND_TRUTH)
        with open_channel(spec) as ch:
            messages = list(ch)
        assert len(messages) == 3

    def test_read_mcap_auto_topic(self, tmp_dir: Path):
        path = tmp_dir / "test.mcap"
        _write_mcap(path, count=2, topic="MyTopic")

        spec = ChannelSpecification(path=path)
        with open_channel(spec) as ch:
            messages = list(ch)
        assert len(messages) == 2

    def test_read_mcap_with_message_type_validation(self, tmp_dir: Path):
        path = tmp_dir / "test.mcap"
        _write_mcap(path, count=1, topic="GroundTruth")

        spec = ChannelSpecification(path=path, topic="GroundTruth", message_type=MessageType.GROUND_TRUTH)
        with open_channel(spec) as ch:
            msg = next(ch)
            assert isinstance(msg, GroundTruth)

    def test_read_mcap_selects_single_topic(self, tmp_dir: Path):
        path = tmp_dir / "multi.mcap"
        _write_mcap_multi_channel(path)

        spec = ChannelSpecification(path=path, topic="gt")
        with open_channel(spec) as ch:
            messages = list(ch)
        assert len(messages) == 2
        for msg in messages:
            assert isinstance(msg, GroundTruth)

    def test_get_channel_specification_mcap(self, tmp_dir: Path):
        path = tmp_dir / "test.mcap"
        _write_mcap(path, count=1, topic="GroundTruth")

        spec = ChannelSpecification(path=path, topic="GroundTruth")
        with open_channel(spec) as ch:
            resolved = ch.get_channel_specification()
            assert resolved.topic == "GroundTruth"
            assert resolved.message_type == MessageType.GROUND_TRUTH


class TestOpenChannelErrors:
    def test_file_not_found(self, tmp_dir: Path):
        spec = ChannelSpecification(path=tmp_dir / "nonexistent.osi")
        with pytest.raises(FileNotFoundError):
            open_channel(spec)

    def test_invalid_topic_mcap(self, tmp_dir: Path):
        path = tmp_dir / "test.mcap"
        _write_mcap(path, count=1, topic="GroundTruth")

        spec = ChannelSpecification(path=path, topic="NonExistent")
        with pytest.raises(ValueError, match="not found"):
            open_channel(spec)

    def test_message_type_mismatch_mcap(self, tmp_dir: Path):
        path = tmp_dir / "test.mcap"
        _write_mcap(path, count=1, topic="GroundTruth")

        spec = ChannelSpecification(path=path, topic="GroundTruth", message_type=MessageType.SENSOR_VIEW)
        with pytest.raises(ValueError, match="does not match"):
            open_channel(spec)

    def test_unsupported_extension(self, tmp_dir: Path):
        path = tmp_dir / "test.json"
        path.write_text("{}")
        spec = ChannelSpecification(path=path)
        with pytest.raises(ValueError, match="Unsupported"):
            open_channel(spec)


# ===========================================================================
# open_channel_writer — writing
# ===========================================================================


class TestOpenChannelWriterBinary:
    def test_write_binary(self, tmp_dir: Path):
        path = tmp_dir / "output.osi"
        spec = ChannelSpecification(path=path, message_type=MessageType.GROUND_TRUTH)

        with open_channel_writer(spec) as ch:
            for i in range(3):
                ch.write_message(_make_ground_truth(i))

        assert path.exists()
        read_spec = ChannelSpecification(path=path, message_type=MessageType.GROUND_TRUTH)
        with open_channel(read_spec) as ch:
            messages = list(ch)
        assert len(messages) == 3

    def test_write_binary_infers_type(self, tmp_dir: Path):
        path = tmp_dir / "output.osi"
        spec = ChannelSpecification(path=path)

        with open_channel_writer(spec) as ch:
            ch.write_message(_make_ground_truth(0))
            resolved = ch.get_channel_specification()
            assert resolved.message_type == MessageType.GROUND_TRUTH

    def test_write_binary_type_mismatch_raises(self, tmp_dir: Path):
        path = tmp_dir / "output.osi"
        spec = ChannelSpecification(path=path, message_type=MessageType.SENSOR_VIEW)

        with open_channel_writer(spec) as ch:
            with pytest.raises(ValueError, match="does not match"):
                ch.write_message(_make_ground_truth(0))


class TestOpenChannelWriterTXTH:
    def test_write_txth(self, tmp_dir: Path):
        path = tmp_dir / "output.txth"
        spec = ChannelSpecification(path=path, message_type=MessageType.GROUND_TRUTH)

        with open_channel_writer(spec) as ch:
            ch.write_message(_make_ground_truth(42))

        assert path.exists()
        assert path.stat().st_size > 0


class TestOpenChannelWriterMCAP:
    def test_write_mcap(self, tmp_dir: Path):
        path = tmp_dir / "output.mcap"
        spec = ChannelSpecification(path=path, topic="gt", message_type=MessageType.GROUND_TRUTH)

        with open_channel_writer(spec) as ch:
            for i in range(3):
                ch.write_message(_make_ground_truth(i))

        read_spec = ChannelSpecification(path=path, topic="gt")
        with open_channel(read_spec) as ch:
            messages = list(ch)
        assert len(messages) == 3

    def test_write_mcap_auto_topic(self, tmp_dir: Path):
        path = tmp_dir / "output.mcap"
        spec = ChannelSpecification(path=path, message_type=MessageType.GROUND_TRUTH)

        with open_channel_writer(spec) as ch:
            ch.write_message(_make_ground_truth(0))
            resolved = ch.get_channel_specification()
            assert resolved.topic is not None
            assert len(resolved.topic) > 0

    def test_get_channel_specification(self, tmp_dir: Path):
        path = tmp_dir / "output.mcap"
        spec = ChannelSpecification(path=path, topic="my_topic", message_type=MessageType.GROUND_TRUTH)

        with open_channel_writer(spec) as ch:
            ch.write_message(_make_ground_truth(0))
            resolved = ch.get_channel_specification()
            assert resolved.topic == "my_topic"
            assert resolved.message_type == MessageType.GROUND_TRUTH


class TestOpenChannelWriterErrors:
    def test_unsupported_extension(self, tmp_dir: Path):
        path = tmp_dir / "output.json"
        spec = ChannelSpecification(path=path)
        with pytest.raises(ValueError):
            open_channel_writer(spec)


# ===========================================================================
# Round-trip: write with channel_writer, read with channel_reader
# ===========================================================================


class TestChannelRoundtrip:
    def test_binary_roundtrip(self, tmp_dir: Path):
        path = tmp_dir / "roundtrip.osi"
        write_spec = ChannelSpecification(path=path, message_type=MessageType.GROUND_TRUTH)

        with open_channel_writer(write_spec) as ch:
            for i in range(5):
                ch.write_message(_make_ground_truth(i))

        read_spec = ChannelSpecification(path=path, message_type=MessageType.GROUND_TRUTH)
        with open_channel(read_spec) as ch:
            messages = list(ch)
        assert len(messages) == 5
        for i, msg in enumerate(messages):
            assert msg.timestamp.seconds == i

    def test_mcap_roundtrip(self, tmp_dir: Path):
        path = tmp_dir / "roundtrip.mcap"
        write_spec = ChannelSpecification(path=path, topic="gt", message_type=MessageType.GROUND_TRUTH)

        with open_channel_writer(write_spec) as ch:
            for i in range(4):
                ch.write_message(_make_ground_truth(i))

        read_spec = ChannelSpecification(path=path, topic="gt", message_type=MessageType.GROUND_TRUTH)
        with open_channel(read_spec) as ch:
            messages = list(ch)
        assert len(messages) == 4
        for i, msg in enumerate(messages):
            assert msg.timestamp.seconds == i
