# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Tests for trace file readers and writers."""

from __future__ import annotations

import struct
import tempfile
from pathlib import Path

import pytest
from osi3.osi_groundtruth_pb2 import GroundTruth
from osi3.osi_sensorview_pb2 import SensorView

from osi_utilities import (
    SingleTraceReader,
    SingleTraceWriter,
    MultiTraceReader,
    MultiTraceWriter,
    MessageType,
    TraceReaderFactory,
    ProtobufTextFormatTraceReader,
    ProtobufTextFormatTraceWriter,
    open_trace_file,
)
from osi_utilities.tracefile._types import (
    ChannelSpecification,
    TraceFileFormat,
    get_trace_file_format,
    infer_message_type_from_filename,
    parse_osi_trace_filename,
)

# ===========================================================================
# Fixtures
# ===========================================================================

TEST_DATA_DIR = Path(__file__).resolve().parent.parent.parent / "test-data"


def _make_ground_truth(index: int = 0) -> GroundTruth:
    gt = GroundTruth()
    gt.timestamp.seconds = index
    gt.timestamp.nanos = index * 1000
    return gt


@pytest.fixture()
def tmp_dir():
    with tempfile.TemporaryDirectory() as d:
        yield Path(d)


# ===========================================================================
# Types and utilities
# ===========================================================================


class TestMessageType:
    def test_all_members(self):
        assert len(MessageType) == 11
        assert MessageType.GROUND_TRUTH.value == 1
        assert MessageType.UNKNOWN.value == 0

    def test_infer_from_filename(self):
        assert infer_message_type_from_filename("trace_gt_data.osi") == MessageType.GROUND_TRUTH
        assert infer_message_type_from_filename("trace_sv_data.osi") == MessageType.SENSOR_VIEW
        assert infer_message_type_from_filename("trace_sd_data.osi") == MessageType.SENSOR_DATA
        assert infer_message_type_from_filename("unknown.osi") == MessageType.UNKNOWN

    def test_parse_osi_trace_filename(self):
        result = parse_osi_trace_filename("20240101T120000Z_gt_3.7.0_4.25.0_100_myapp.osi")
        assert result is not None
        assert len(result) > 0

    def test_get_trace_file_format(self):
        assert get_trace_file_format(Path("file.osi")) == TraceFileFormat.SINGLE_CHANNEL
        assert get_trace_file_format(Path("file.mcap")) == TraceFileFormat.MULTI_CHANNEL
        assert get_trace_file_format(Path("file.txth")) == TraceFileFormat.SINGLE_CHANNEL

    def test_channel_specification(self):
        spec = ChannelSpecification(path=Path("test.mcap"))
        spec = spec.with_topic("ground_truth").with_message_type("GroundTruth")
        assert spec.topic == "ground_truth"
        assert spec.message_type == "GroundTruth"


# ===========================================================================
# Binary reader/writer
# ===========================================================================


class TestBinaryTraceFile:
    def test_write_and_read(self, tmp_dir: Path):
        path = tmp_dir / "test_gt.osi"
        messages = [_make_ground_truth(i) for i in range(5)]

        with SingleTraceWriter() as writer:
            assert writer.open(path)
            for msg in messages:
                assert writer.write_message(msg)
            assert writer.written_count == 5

        with SingleTraceReader(MessageType.GROUND_TRUTH) as reader:
            assert reader.open(path)
            results = list(reader)
            assert len(results) == 5
            for i, r in enumerate(results):
                assert r.message_type == MessageType.GROUND_TRUTH
                assert r.message.timestamp.seconds == i

    def test_reject_wrong_extension(self, tmp_dir: Path):
        path = tmp_dir / "test.mcap"
        writer = SingleTraceWriter()
        assert not writer.open(path)

    def test_open_already_opened(self, tmp_dir: Path):
        path = tmp_dir / "test_gt.osi"
        writer = SingleTraceWriter()
        assert writer.open(path)
        assert not writer.open(path)
        writer.close()

    def test_read_truncated_header(self, tmp_dir: Path):
        path = tmp_dir / "truncated_gt.osi"
        path.write_bytes(b"\x05\x00")  # only 2 bytes of length header

        reader = SingleTraceReader(MessageType.GROUND_TRUTH)
        assert reader.open(path)
        with pytest.raises(RuntimeError, match="Truncated length header"):
            reader.read_message()
        reader.close()

    def test_read_truncated_body(self, tmp_dir: Path):
        path = tmp_dir / "truncated_gt.osi"
        path.write_bytes(struct.pack("<I", 100) + b"\x00" * 10)

        reader = SingleTraceReader(MessageType.GROUND_TRUTH)
        assert reader.open(path)
        with pytest.raises(RuntimeError, match="Truncated message body"):
            reader.read_message()
        reader.close()

    def test_empty_file(self, tmp_dir: Path):
        path = tmp_dir / "empty_gt.osi"
        path.write_bytes(b"")

        reader = SingleTraceReader(MessageType.GROUND_TRUTH)
        assert reader.open(path)
        assert not reader.has_next()
        assert reader.read_message() is None
        reader.close()


# ===========================================================================
# MCAP reader/writer
# ===========================================================================


class TestMCAPTraceFile:
    def test_write_and_read(self, tmp_dir: Path):
        path = tmp_dir / "test.mcap"
        messages = [_make_ground_truth(i) for i in range(3)]

        with MultiTraceWriter() as writer:
            assert writer.open(path)
            writer.add_channel("ground_truth", GroundTruth)
            for msg in messages:
                assert writer.write_message(msg, "ground_truth")
            assert writer.written_count == 3

        with MultiTraceReader() as reader:
            assert reader.open(path)
            results = list(reader)
            assert len(results) == 3
            for i, r in enumerate(results):
                assert r.message_type == MessageType.GROUND_TRUTH
                assert r.channel_name == "ground_truth"
                assert r.message.timestamp.seconds == i

    def test_auto_channel_creation(self, tmp_dir: Path):
        path = tmp_dir / "auto.mcap"
        gt = _make_ground_truth()

        with MultiTraceWriter() as writer:
            assert writer.open(path)
            assert writer.write_message(gt)

        with MultiTraceReader() as reader:
            assert reader.open(path)
            results = list(reader)
            assert len(results) == 1

    def test_multi_channel(self, tmp_dir: Path):
        path = tmp_dir / "multi.mcap"

        with MultiTraceWriter() as writer:
            assert writer.open(path)
            writer.add_channel("gt", GroundTruth)
            writer.add_channel("sv", SensorView)
            assert writer.write_message(_make_ground_truth(0), "gt")
            sv = SensorView()
            sv.timestamp.seconds = 1
            assert writer.write_message(sv, "sv")

        with MultiTraceReader() as reader:
            assert reader.open(path)
            results = list(reader)
            assert len(results) == 2
            types = {r.message_type for r in results}
            assert MessageType.GROUND_TRUTH in types
            assert MessageType.SENSOR_VIEW in types

    def test_open_already_opened(self, tmp_dir: Path):
        path = tmp_dir / "test.mcap"
        writer = MultiTraceWriter()
        assert writer.open(path)
        assert not writer.open(path)
        writer.close()

    def test_file_metadata(self, tmp_dir: Path):
        path = tmp_dir / "meta.mcap"
        with MultiTraceWriter() as writer:
            assert writer.open(path)
            writer.write_message(_make_ground_truth())

        with MultiTraceReader() as reader:
            assert reader.open(path)
            metadata = reader.get_file_metadata()
            assert any(m["name"] == "net.asam.osi.trace" for m in metadata)

    def test_get_available_topics(self, tmp_dir: Path):
        path = tmp_dir / "topics.mcap"
        with MultiTraceWriter() as writer:
            assert writer.open(path)
            writer.add_channel("ch1", GroundTruth)
            writer.add_channel("ch2", SensorView)
            writer.write_message(_make_ground_truth(), "ch1")

        with MultiTraceReader() as reader:
            assert reader.open(path)
            topics = reader.get_available_topics()
            assert "ch1" in topics
            assert "ch2" in topics


# ===========================================================================
# TXTH reader/writer
# ===========================================================================


class TestTXTHTraceFile:
    def test_write_and_read_single(self, tmp_dir: Path):
        path = tmp_dir / "test_gt.txth"
        gt = _make_ground_truth(42)

        with ProtobufTextFormatTraceWriter() as writer:
            assert writer.open(path)
            assert writer.write_message(gt)

        with ProtobufTextFormatTraceReader(MessageType.GROUND_TRUTH) as reader:
            assert reader.open(path)
            results = list(reader)
            assert len(results) == 1
            assert results[0].message.timestamp.seconds == 42

    def test_reject_wrong_extension(self, tmp_dir: Path):
        path = tmp_dir / "test.osi"
        writer = ProtobufTextFormatTraceWriter()
        assert not writer.open(path)

    def test_open_already_opened(self, tmp_dir: Path):
        path = tmp_dir / "test_gt.txth"
        writer = ProtobufTextFormatTraceWriter()
        assert writer.open(path)
        assert not writer.open(path)
        writer.close()

    def test_write_and_read_multiple(self, tmp_dir: Path):
        path = tmp_dir / "multi_gt.txth"
        messages = [_make_ground_truth(i) for i in range(5)]
        with ProtobufTextFormatTraceWriter() as writer:
            assert writer.open(path)
            for msg in messages:
                assert writer.write_message(msg)
            assert writer.written_count == 5
        with ProtobufTextFormatTraceReader(MessageType.GROUND_TRUTH) as reader:
            assert reader.open(path)
            results = list(reader)
            assert len(results) == 5
            for i, r in enumerate(results):
                assert r.message.timestamp.seconds == i

    def test_read_sensor_view(self, tmp_dir: Path):
        path = tmp_dir / "test_sv.txth"
        sv = SensorView()
        sv.timestamp.seconds = 10
        with ProtobufTextFormatTraceWriter() as writer:
            assert writer.open(path)
            assert writer.write_message(sv)
        with ProtobufTextFormatTraceReader(MessageType.SENSOR_VIEW) as reader:
            assert reader.open(path)
            results = list(reader)
            assert len(results) == 1
            assert results[0].message_type == MessageType.SENSOR_VIEW
            assert results[0].message.timestamp.seconds == 10

    def test_open_nonexistent_file(self):
        reader = ProtobufTextFormatTraceReader(MessageType.GROUND_TRUTH)
        assert not reader.open(Path("/nonexistent/path/file_gt.txth"))

    def test_open_unknown_type_from_filename(self, tmp_dir: Path):
        path = tmp_dir / "unknown.txth"
        path.write_text("", encoding="utf-8")
        reader = ProtobufTextFormatTraceReader()
        assert not reader.open(path)

    def test_read_empty_file(self, tmp_dir: Path):
        path = tmp_dir / "empty_gt.txth"
        path.write_text("", encoding="utf-8")
        reader = ProtobufTextFormatTraceReader(MessageType.GROUND_TRUTH)
        assert reader.open(path)
        assert not reader.has_next()
        assert reader.read_message() is None
        reader.close()

    def test_write_to_closed_file(self, tmp_dir: Path):
        path = tmp_dir / "closed_gt.txth"
        writer = ProtobufTextFormatTraceWriter()
        writer.open(path)
        writer.close()
        assert not writer.write_message(_make_ground_truth())

    def test_close_and_reopen(self, tmp_dir: Path):
        path1 = tmp_dir / "first_gt.txth"
        path2 = tmp_dir / "second_gt.txth"
        writer = ProtobufTextFormatTraceWriter()
        writer.open(path1)
        writer.write_message(_make_ground_truth(1))
        writer.close()
        writer.open(path2)
        writer.write_message(_make_ground_truth(2))
        writer.close()
        with ProtobufTextFormatTraceReader(MessageType.GROUND_TRUTH) as r:
            r.open(path2)
            results = list(r)
            assert len(results) == 1
            assert results[0].message.timestamp.seconds == 2

    def test_open_with_explicit_message_type(self, tmp_dir: Path):
        """Opening with explicit type that matches content."""
        path = tmp_dir / "typed_gt.txth"
        with ProtobufTextFormatTraceWriter() as w:
            w.open(path)
            w.write_message(_make_ground_truth(7))
        with ProtobufTextFormatTraceReader(MessageType.GROUND_TRUTH) as reader:
            reader.open(path)
            results = list(reader)
            assert len(results) == 1
            assert results[0].message_type == MessageType.GROUND_TRUTH


# ===========================================================================
# Factory and convenience
# ===========================================================================


class TestFactory:
    def test_unsupported_extension(self):
        with pytest.raises(ValueError, match="Unsupported"):
            TraceReaderFactory.create_reader("file.json")

    def test_open_trace_file_binary(self, tmp_dir: Path):
        path = tmp_dir / "factory_gt_data.osi"
        with SingleTraceWriter() as w:
            w.open(path)
            w.write_message(_make_ground_truth())

        with open_trace_file(path) as reader:
            results = list(reader)
            assert len(results) == 1

    def test_open_trace_file_mcap(self, tmp_dir: Path):
        path = tmp_dir / "factory.mcap"
        with MultiTraceWriter() as w:
            w.open(path)
            w.write_message(_make_ground_truth())

        with open_trace_file(path) as reader:
            results = list(reader)
            assert len(results) == 1

    def test_factory_creates_binary_reader(self, tmp_dir: Path):
        path = tmp_dir / "test_gt.osi"
        with SingleTraceWriter() as w:
            w.open(path)
            w.write_message(_make_ground_truth())
        reader = TraceReaderFactory.create_reader(path)
        assert isinstance(reader, SingleTraceReader)
        reader.close()

    def test_factory_creates_mcap_reader(self, tmp_dir: Path):
        path = tmp_dir / "test.mcap"
        with MultiTraceWriter() as w:
            w.open(path)
            w.write_message(_make_ground_truth())
        reader = TraceReaderFactory.create_reader(path)
        assert isinstance(reader, MultiTraceReader)
        reader.close()

    def test_factory_creates_txth_reader(self, tmp_dir: Path):
        path = tmp_dir / "test_gt.txth"
        with ProtobufTextFormatTraceWriter() as w:
            w.open(path)
            w.write_message(_make_ground_truth())
        reader = TraceReaderFactory.create_reader(path)
        assert isinstance(reader, ProtobufTextFormatTraceReader)
        reader.close()

    def test_factory_no_extension(self):
        with pytest.raises(ValueError, match="Unsupported"):
            TraceReaderFactory.create_reader("noextension")

    def test_factory_empty_path(self):
        with pytest.raises((ValueError, Exception)):
            TraceReaderFactory.create_reader("")

    def test_open_trace_file_txth(self, tmp_dir: Path):
        path = tmp_dir / "factory_gt_data.txth"
        with ProtobufTextFormatTraceWriter() as w:
            w.open(path)
            w.write_message(_make_ground_truth())
        with open_trace_file(path) as reader:
            results = list(reader)
            assert len(results) == 1


# ===========================================================================
# Cross-language validation with test-data/
# ===========================================================================


def _is_lfs_pointer(path: Path) -> bool:
    """Check if a file is a Git LFS pointer (not actual content)."""
    prefix = b"version https://git-lfs.github.com/spec/v1"
    try:
        header = path.read_bytes()[: len(prefix)]
        return header.startswith(prefix)
    except (OSError, ValueError):
        return False


@pytest.mark.skipif(not TEST_DATA_DIR.exists(), reason="test-data/ not available")
class TestCrossLanguageValidation:
    def test_read_binary_5frames(self):
        path = TEST_DATA_DIR / "5frames_gt_esmini.osi"
        if not path.exists() or _is_lfs_pointer(path):
            pytest.skip("Test file not found or is LFS pointer")
        with open_trace_file(path) as reader:
            results = list(reader)
            assert len(results) == 5
            for r in results:
                assert r.message_type == MessageType.GROUND_TRUTH

    def test_read_mcap_5frames(self):
        path = TEST_DATA_DIR / "5frames_gt_esmini.mcap"
        if not path.exists() or _is_lfs_pointer(path):
            pytest.skip("Test file not found or is LFS pointer")
        with open_trace_file(path) as reader:
            results = list(reader)
            assert len(results) == 5
            for r in results:
                assert r.message_type == MessageType.GROUND_TRUTH

    def test_binary_mcap_content_match(self):
        osi_path = TEST_DATA_DIR / "5frames_gt_esmini.osi"
        mcap_path = TEST_DATA_DIR / "5frames_gt_esmini.mcap"
        if not osi_path.exists() or not mcap_path.exists() or _is_lfs_pointer(osi_path) or _is_lfs_pointer(mcap_path):
            pytest.skip("Test files not found or are LFS pointers")

        with open_trace_file(osi_path) as r:
            osi_msgs = [res.message for res in r]
        with open_trace_file(mcap_path) as r:
            mcap_msgs = [res.message for res in r]

        assert len(osi_msgs) == len(mcap_msgs)
        for osi_msg, mcap_msg in zip(osi_msgs, mcap_msgs):
            assert osi_msg.SerializeToString() == mcap_msg.SerializeToString()


# ===========================================================================
# NCAP parameterized fixture tests (mirrors C++ EsminiFixture_test.cpp)
# ===========================================================================

_NCAP_GT_FIXTURES = [
    ("ccrs_gt_ncap.osi", MessageType.GROUND_TRUTH),
    ("ccftap_gt_ncap.osi", MessageType.GROUND_TRUTH),
    ("cpna_gt_ncap.osi", MessageType.GROUND_TRUTH),
    ("cbla_gt_ncap.osi", MessageType.GROUND_TRUTH),
]

_NCAP_SV_FIXTURES = [
    ("ccrs_sv_ncap.osi", MessageType.SENSOR_VIEW),
    ("ccftap_sv_ncap.osi", MessageType.SENSOR_VIEW),
    ("cpna_sv_ncap.osi", MessageType.SENSOR_VIEW),
    ("cbla_sv_ncap.osi", MessageType.SENSOR_VIEW),
]


def _skip_if_lfs(path: Path) -> None:
    if not path.exists() or _is_lfs_pointer(path):
        pytest.skip(f"{path.name} not found or is LFS pointer")


@pytest.mark.skipif(not TEST_DATA_DIR.exists(), reason="test-data/ not available")
class TestNcapGroundTruth:
    """Parameterized NCAP GroundTruth tests — mirrors C++ NcapGroundTruthTest."""

    @pytest.mark.parametrize("filename,msg_type", _NCAP_GT_FIXTURES, ids=[f[0] for f in _NCAP_GT_FIXTURES])
    def test_read_all_frames(self, filename: str, msg_type: MessageType):
        path = TEST_DATA_DIR / filename
        _skip_if_lfs(path)
        with SingleTraceReader(msg_type) as reader:
            assert reader.open(path)
            results = list(reader)
            assert len(results) > 0
            for r in results:
                assert r.message_type == msg_type

    @pytest.mark.parametrize("filename,msg_type", _NCAP_GT_FIXTURES, ids=[f[0] for f in _NCAP_GT_FIXTURES])
    def test_has_moving_objects(self, filename: str, msg_type: MessageType):
        path = TEST_DATA_DIR / filename
        _skip_if_lfs(path)
        with SingleTraceReader(msg_type) as reader:
            assert reader.open(path)
            result = reader.read_message()
            assert result is not None
            assert len(result.message.moving_object) > 0

    @pytest.mark.parametrize("filename,msg_type", _NCAP_GT_FIXTURES, ids=[f[0] for f in _NCAP_GT_FIXTURES])
    def test_timestamps_increasing(self, filename: str, msg_type: MessageType):
        path = TEST_DATA_DIR / filename
        _skip_if_lfs(path)
        with SingleTraceReader(msg_type) as reader:
            assert reader.open(path)
            results = list(reader)
            assert len(results) >= 2
            for i in range(1, len(results)):
                prev_ts = results[i - 1].message.timestamp
                curr_ts = results[i].message.timestamp
                prev_ns = prev_ts.seconds * 1_000_000_000 + prev_ts.nanos
                curr_ns = curr_ts.seconds * 1_000_000_000 + curr_ts.nanos
                assert curr_ns > prev_ns


@pytest.mark.skipif(not TEST_DATA_DIR.exists(), reason="test-data/ not available")
class TestNcapSensorView:
    """Parameterized NCAP SensorView tests — mirrors C++ NcapSensorViewTest."""

    @pytest.mark.parametrize("filename,msg_type", _NCAP_SV_FIXTURES, ids=[f[0] for f in _NCAP_SV_FIXTURES])
    def test_read_all_frames(self, filename: str, msg_type: MessageType):
        path = TEST_DATA_DIR / filename
        _skip_if_lfs(path)
        with SingleTraceReader(msg_type) as reader:
            assert reader.open(path)
            results = list(reader)
            assert len(results) > 0
            for r in results:
                assert r.message_type == msg_type

    @pytest.mark.parametrize("filename,msg_type", _NCAP_SV_FIXTURES, ids=[f[0] for f in _NCAP_SV_FIXTURES])
    def test_contains_ground_truth(self, filename: str, msg_type: MessageType):
        path = TEST_DATA_DIR / filename
        _skip_if_lfs(path)
        with SingleTraceReader(msg_type) as reader:
            assert reader.open(path)
            result = reader.read_message()
            assert result is not None
            assert result.message.HasField("global_ground_truth")

    @pytest.mark.parametrize("filename,msg_type", _NCAP_SV_FIXTURES, ids=[f[0] for f in _NCAP_SV_FIXTURES])
    def test_timestamp_matches_gt(self, filename: str, msg_type: MessageType):
        path = TEST_DATA_DIR / filename
        _skip_if_lfs(path)
        with SingleTraceReader(msg_type) as reader:
            assert reader.open(path)
            result = reader.read_message()
            assert result is not None
            sv_ts = result.message.timestamp
            gt_ts = result.message.global_ground_truth.timestamp
            assert sv_ts.seconds == gt_ts.seconds
            assert sv_ts.nanos == gt_ts.nanos


@pytest.mark.skipif(not TEST_DATA_DIR.exists(), reason="test-data/ not available")
class TestNcapMcap:
    """NCAP MCAP format test — mirrors C++ NcapMcapTest."""

    def test_read_ccrs_mcap(self):
        path = TEST_DATA_DIR / "ccrs_gt_ncap.mcap"
        _skip_if_lfs(path)
        with MultiTraceReader() as reader:
            assert reader.open(path)
            results = list(reader)
            assert len(results) > 0
            for r in results:
                assert r.message_type == MessageType.GROUND_TRUTH
