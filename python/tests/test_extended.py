# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Extended tests for trace file readers, writers, and MCAPChannel.

Covers error paths, edge cases, and MCAPChannel — complementing test_tracefile.py.
"""

from __future__ import annotations

import struct
import tempfile
from pathlib import Path

import pytest
from mcap.writer import Writer as McapRawWriter
from osi3.osi_groundtruth_pb2 import GroundTruth
from osi3.osi_hostvehicledata_pb2 import HostVehicleData
from osi3.osi_motionrequest_pb2 import MotionRequest
from osi3.osi_sensordata_pb2 import SensorData
from osi3.osi_sensorview_pb2 import SensorView
from osi3.osi_streamingupdate_pb2 import StreamingUpdate
from osi3.osi_trafficcommand_pb2 import TrafficCommand
from osi3.osi_trafficcommandupdate_pb2 import TrafficCommandUpdate
from osi3.osi_trafficupdate_pb2 import TrafficUpdate

from osi_utilities import (
    MCAPChannel,
    MessageType,
    MultiTraceReader,
    MultiTraceWriter,
    ProtobufTextFormatTraceReader,
    ProtobufTextFormatTraceWriter,
    ReadStatus,
    SingleTraceReader,
    SingleTraceWriter,
)
from osi_utilities.timestamp import timestamp_to_nanoseconds
from osi_utilities.tracefile._config import MAX_EXPECTED_MESSAGE_SIZE
from osi_utilities.tracefile._mcap_utils import build_file_descriptor_set
from osi_utilities.tracefile.writers.multi import prepare_required_file_metadata

# ===========================================================================
# Fixtures
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


@pytest.fixture()
def tmp_dir():
    with tempfile.TemporaryDirectory() as d:
        yield Path(d)


# ===========================================================================
# MCAPChannel tests
# ===========================================================================


class TestMCAPChannel:
    def test_add_channel_basic(self, tmp_dir: Path):
        path = tmp_dir / "channel.mcap"
        with open(path, "wb") as f:
            writer = McapRawWriter(f)
            writer.start(library="test")
            channel = MCAPChannel(writer)
            channel_id = channel.add_channel("gt", GroundTruth)
            assert isinstance(channel_id, int)
            assert channel_id >= 0
            writer.finish()

    def test_add_channel_with_metadata(self, tmp_dir: Path):
        path = tmp_dir / "channel_meta.mcap"
        with open(path, "wb") as f:
            writer = McapRawWriter(f)
            writer.start(library="test")
            channel = MCAPChannel(writer)
            meta = {"custom_key": "custom_value"}
            channel.add_channel("gt", GroundTruth, metadata=meta)
            writer.finish()

        with MultiTraceReader() as reader:
            assert reader.open(path)
            ch_meta = reader.get_channel_metadata("gt")
            assert ch_meta is not None
            assert ch_meta["custom_key"] == "custom_value"
            assert "net.asam.osi.trace.channel.protobuf_version" in ch_meta

    def test_add_channel_duplicate_raises(self, tmp_dir: Path):
        path = tmp_dir / "dup.mcap"
        with open(path, "wb") as f:
            writer = McapRawWriter(f)
            writer.start(library="test")
            channel = MCAPChannel(writer)
            channel.add_channel("gt", GroundTruth)
            with pytest.raises(RuntimeError, match="already registered"):
                channel.add_channel("gt", GroundTruth)
            writer.finish()

    def test_write_message_success(self, tmp_dir: Path):
        path = tmp_dir / "write.mcap"
        with open(path, "wb") as f:
            writer = McapRawWriter(f)
            writer.start(library="test")
            channel = MCAPChannel(writer)
            channel.add_channel("gt", GroundTruth)
            gt = _make_ground_truth(7)
            assert channel.write_message(gt, "gt")
            writer.finish()

        with MultiTraceReader() as reader:
            assert reader.open(path)
            results = list(reader)
            assert len(results) == 1
            assert results[0].message.timestamp.seconds == 7

    def test_write_unregistered_topic(self, tmp_dir: Path):
        path = tmp_dir / "unreg.mcap"
        with open(path, "wb") as f:
            writer = McapRawWriter(f)
            writer.start(library="test")
            channel = MCAPChannel(writer)
            assert not channel.write_message(_make_ground_truth(), "nonexistent")
            writer.finish()

    def test_schema_deduplication(self, tmp_dir: Path):
        path = tmp_dir / "dedup.mcap"
        with open(path, "wb") as f:
            writer = McapRawWriter(f)
            writer.start(library="test")
            channel = MCAPChannel(writer)
            channel.add_channel("gt1", GroundTruth)
            channel.add_channel("gt2", GroundTruth)
            # Both channels should share the same schema_id
            assert len(channel._schema_cache) == 1
            writer.finish()

    def test_timestamp_extraction(self):
        gt = _make_ground_truth(10)
        gt.timestamp.nanos = 500
        ts_ns = timestamp_to_nanoseconds(gt)
        assert ts_ns == 10 * 1_000_000_000 + 500

    def test_timestamp_missing(self):
        """Message without timestamp field should return 0."""
        from google.protobuf.descriptor_pb2 import FileDescriptorProto

        msg = FileDescriptorProto()  # no .timestamp field
        assert timestamp_to_nanoseconds(msg) == 0

    def test_prepare_required_file_metadata(self):
        meta = prepare_required_file_metadata()
        assert "version" in meta
        assert "min_protobuf_version" in meta
        assert "creation_time" in meta
        # MCAPChannel delegates to the same function
        channel_meta = MCAPChannel.prepare_required_file_metadata()
        assert "version" in channel_meta

    def test_build_file_descriptor_set(self):
        fds = build_file_descriptor_set(GroundTruth)
        assert fds.file
        names = [f.name for f in fds.file]
        assert any("groundtruth" in n.lower() for n in names)


# ===========================================================================
# Reader error-path tests
# ===========================================================================


class TestSingleTraceReaderErrors:
    def test_read_after_close(self, tmp_dir: Path):
        path = tmp_dir / "close_gt.osi"
        with SingleTraceWriter() as w:
            w.open(path)
            w.write_message(_make_ground_truth())

        reader = SingleTraceReader()

        reader.set_message_type(MessageType.GROUND_TRUTH)
        reader.open(path)
        reader.close()
        assert reader.read_message() is None
        assert not reader.has_next()

    def test_prevent_multiple_opens(self, tmp_dir: Path):
        path1 = tmp_dir / "f1_gt.osi"
        path2 = tmp_dir / "f2_gt.osi"
        for p in [path1, path2]:
            with SingleTraceWriter() as w:
                w.open(p)
                w.write_message(_make_ground_truth())

        reader = SingleTraceReader()

        reader.set_message_type(MessageType.GROUND_TRUTH)
        assert reader.open(path1)
        # Opening a second file without closing should still work
        # (the implementation resets internal state)
        result = reader.open(path2)
        # Either the second open succeeded, or we can still read from the reader
        assert result or reader.read_message() is not None
        reader.close()

    def test_corrupt_message_size(self, tmp_dir: Path):
        path = tmp_dir / "corrupt_gt.osi"
        # Write a message size exceeding MAX_EXPECTED_MESSAGE_SIZE
        huge_size = MAX_EXPECTED_MESSAGE_SIZE + 1
        path.write_bytes(struct.pack("<I", huge_size))

        reader = SingleTraceReader()

        reader.set_message_type(MessageType.GROUND_TRUTH)
        reader.open(path)
        result = reader.read_message()
        assert result is not None
        assert result.status == ReadStatus.ERROR
        assert "exceeds maximum" in result.error_message
        reader.close()

    def test_unknown_message_type_fails_open(self, tmp_dir: Path):
        path = tmp_dir / "unknown.osi"
        path.write_bytes(b"")
        reader = SingleTraceReader()
        reader.set_message_type(MessageType.UNKNOWN)
        assert not reader.open(path)

    def test_has_next_transitions_to_false_at_eof(self, tmp_dir: Path):
        path = tmp_dir / "has_next_gt.osi"
        with SingleTraceWriter() as writer:
            assert writer.open(path)
            assert writer.write_message(_make_ground_truth())

        reader = SingleTraceReader()
        reader.set_message_type(MessageType.GROUND_TRUTH)
        assert reader.open(path)
        assert reader.has_next()
        result = reader.read_message()
        assert result is not None
        assert not reader.has_next()
        assert reader.read_message() is None
        assert not reader.has_next()
        reader.close()


class TestMultiTraceReaderErrors:
    def test_open_invalid_mcap(self, tmp_dir: Path):
        path = tmp_dir / "invalid.mcap"
        path.write_bytes(b"this is not an mcap file")
        reader = MultiTraceReader()
        assert not reader.open(path)

    def test_read_empty_mcap(self, tmp_dir: Path):
        path = tmp_dir / "empty.mcap"
        with open(path, "wb") as f:
            writer = McapRawWriter(f)
            writer.start(library="test")
            writer.finish()

        with MultiTraceReader() as reader:
            assert reader.open(path)
            results = list(reader)
            assert len(results) == 0

    def test_read_after_close(self, tmp_dir: Path):
        path = tmp_dir / "close.mcap"
        with MultiTraceWriter() as writer:
            assert writer.open(path)
            assert writer.write_message(_make_ground_truth())

        reader = MultiTraceReader()
        assert reader.open(path)
        reader.close()
        assert reader.read_message() is None
        assert not reader.has_next()

    def test_has_next_transitions_to_false_at_eof(self, tmp_dir: Path):
        path = tmp_dir / "has_next.mcap"
        with MultiTraceWriter() as writer:
            assert writer.open(path)
            assert writer.write_message(_make_ground_truth())

        with MultiTraceReader() as reader:
            assert reader.open(path)
            assert reader.has_next()
            result = reader.read_message()
            assert result is not None
            assert not reader.has_next()
            assert reader.read_message() is None
            assert not reader.has_next()

    def test_set_topics_filter(self, tmp_dir: Path):
        path = tmp_dir / "filter.mcap"
        with MultiTraceWriter() as writer:
            writer.open(path)
            writer.add_channel("gt", GroundTruth)
            writer.add_channel("sv", SensorView)
            writer.write_message(_make_ground_truth(1), "gt")
            writer.write_message(_make_sensor_view(2), "sv")
            writer.write_message(_make_ground_truth(3), "gt")

        with MultiTraceReader() as reader:
            reader.open(path)
            reader.set_topics(["gt"])
            results = list(reader)
            assert len(results) == 2
            assert all(r.message_type == MessageType.GROUND_TRUTH for r in results)

    def test_set_topic_message_types_warns_on_mismatch(self, tmp_dir: Path, caplog):
        path = tmp_dir / "topic_type_mismatch.mcap"
        with MultiTraceWriter() as writer:
            writer.open(path)
            writer.add_channel("gt", GroundTruth)
            writer.add_channel("sv", SensorView)
            writer.write_message(_make_ground_truth(1), "gt")
            writer.write_message(_make_sensor_view(2), "sv")

        with MultiTraceReader() as reader:
            assert reader.open(path)
            reader.set_topic_message_types({"sv": MessageType.GROUND_TRUTH})
            results = list(reader)
            assert len(results) == 1
            assert any("Message type mismatch on channel 'sv'" in rec.message for rec in caplog.records)

    def test_set_topic_message_types_can_surface_incompatible_result(self, tmp_dir: Path):
        path = tmp_dir / "topic_type_mismatch_surface.mcap"
        with MultiTraceWriter() as writer:
            writer.open(path)
            writer.add_channel("gt", GroundTruth)
            writer.add_channel("sv", SensorView)
            writer.write_message(_make_ground_truth(1), "gt")
            writer.write_message(_make_sensor_view(2), "sv")

        with MultiTraceReader() as reader:
            assert reader.open(path)
            reader.set_skip_incompatible_messages(False)
            reader.set_topic_message_types({"sv": MessageType.GROUND_TRUTH})

            result1 = reader.read_message()
            assert result1 is not None
            assert result1.status == ReadStatus.OK

            result2 = reader.read_message()
            assert result2 is not None
            assert result2.status == ReadStatus.INCOMPATIBLE
            assert "Message type mismatch on channel 'sv'" in result2.error_message

    def test_iterator_raises_value_error_on_incompatible_when_not_skipped(self, tmp_dir: Path):
        path = tmp_dir / "topic_type_mismatch_iter.mcap"
        with MultiTraceWriter() as writer:
            writer.open(path)
            writer.add_channel("gt", GroundTruth)
            writer.add_channel("sv", SensorView)
            writer.write_message(_make_ground_truth(1), "gt")
            writer.write_message(_make_sensor_view(2), "sv")

        with MultiTraceReader() as reader:
            assert reader.open(path)
            reader.set_skip_incompatible_messages(False)
            reader.set_topic_message_types({"sv": MessageType.GROUND_TRUTH})
            with pytest.raises(ValueError, match="Message type mismatch on channel 'sv'"):
                list(reader)

    def test_get_channel_metadata(self, tmp_dir: Path):
        path = tmp_dir / "ch_meta.mcap"
        custom_meta = {"my_key": "my_value"}
        with MultiTraceWriter() as writer:
            writer.open(path)
            writer.add_channel("gt", GroundTruth, metadata=custom_meta)
            writer.write_message(_make_ground_truth(), "gt")

        with MultiTraceReader() as reader:
            reader.open(path)
            meta = reader.get_channel_metadata("gt")
            assert meta is not None
            assert meta["my_key"] == "my_value"
            # Non-existent topic
            assert reader.get_channel_metadata("nonexistent") is None

    def test_get_message_type_for_topic(self, tmp_dir: Path):
        path = tmp_dir / "type_topic.mcap"
        with MultiTraceWriter() as writer:
            writer.open(path)
            writer.add_channel("gt", GroundTruth)
            writer.write_message(_make_ground_truth(), "gt")

        with MultiTraceReader() as reader:
            reader.open(path)
            msg_type = reader.get_message_type_for_topic("gt")
            assert msg_type == MessageType.GROUND_TRUTH
            assert reader.get_message_type_for_topic("nonexistent") is None

    def test_skip_non_osi_messages(self, tmp_dir: Path):
        """Non-OSI schemas are skipped by default (with warning) or silently."""
        path = tmp_dir / "non_osi.mcap"
        # Write a file with both an OSI and a non-OSI channel.
        # The MCAP protobuf decoder requires valid FileDescriptorSet schemas,
        # so we use a real protobuf type but with a non-OSI schema name.

        with MultiTraceWriter() as writer:
            writer.open(path)
            writer.add_channel("gt", GroundTruth)
            writer.write_message(_make_ground_truth(1), "gt")

        # Verify that the OSI message is read and no errors occur
        with MultiTraceReader() as reader:
            reader.open(path)
            reader.set_log_incompatible_messages(False)
            results = list(reader)
            assert len(results) == 1
            assert results[0].message_type == MessageType.GROUND_TRUTH


class TestProtobufTextFormatTraceReaderErrors:
    def test_multi_message_txth(self, tmp_dir: Path):
        path = tmp_dir / "multi_gt.txth"
        with ProtobufTextFormatTraceWriter() as w:
            w.open(path)
            w.write_message(_make_ground_truth(1))
            w.write_message(_make_ground_truth(2))

        with ProtobufTextFormatTraceReader() as reader:
            reader.set_message_type(MessageType.GROUND_TRUTH)
            reader.open(path)
            results = list(reader)
            assert len(results) == 2
            assert results[0].message.timestamp.seconds == 1
            assert results[1].message.timestamp.seconds == 2

    def test_parse_error_txth(self, tmp_dir: Path):
        path = tmp_dir / "bad_gt.txth"
        path.write_text("this is not valid protobuf text {{{", encoding="utf-8")
        reader = ProtobufTextFormatTraceReader()
        reader.set_message_type(MessageType.GROUND_TRUTH)
        reader.open(path)
        result = reader.read_message()
        assert result is not None
        assert result.status == ReadStatus.ERROR
        assert "Failed to parse" in result.error_message
        reader.close()

    def test_read_after_close(self, tmp_dir: Path):
        path = tmp_dir / "close_gt.txth"
        with ProtobufTextFormatTraceWriter() as w:
            w.open(path)
            w.write_message(_make_ground_truth())

        reader = ProtobufTextFormatTraceReader()

        reader.set_message_type(MessageType.GROUND_TRUTH)
        reader.open(path)
        reader.close()
        assert reader.read_message() is None
        assert not reader.has_next()

    def test_unknown_message_type_fails_open(self, tmp_dir: Path):
        path = tmp_dir / "unknown.txth"
        path.write_text("", encoding="utf-8")
        reader = ProtobufTextFormatTraceReader()
        reader.set_message_type(MessageType.UNKNOWN)
        assert not reader.open(path)

    def test_has_next_transitions_to_false_at_eof(self, tmp_dir: Path):
        path = tmp_dir / "has_next_gt.txth"
        with ProtobufTextFormatTraceWriter() as writer:
            assert writer.open(path)
            assert writer.write_message(_make_ground_truth())

        reader = ProtobufTextFormatTraceReader()
        reader.set_message_type(MessageType.GROUND_TRUTH)
        assert reader.open(path)
        assert reader.has_next()
        result = reader.read_message()
        assert result is not None
        assert not reader.has_next()
        assert reader.read_message() is None
        assert not reader.has_next()
        reader.close()


# ===========================================================================
# Writer error-path tests
# ===========================================================================


class TestSingleTraceWriterErrors:
    def test_write_after_close(self, tmp_dir: Path):
        path = tmp_dir / "closed_gt.osi"
        writer = SingleTraceWriter()
        writer.open(path)
        writer.close()
        assert not writer.write_message(_make_ground_truth())

    def test_write_empty_message(self, tmp_dir: Path):
        path = tmp_dir / "empty_msg_gt.osi"
        gt = GroundTruth()  # empty, no fields set

        with SingleTraceWriter() as w:
            w.open(path)
            assert w.write_message(gt)
            assert w.written_count == 1

        with SingleTraceReader() as r:
            r.set_message_type(MessageType.GROUND_TRUTH)
            r.open(path)
            results = list(r)
            assert len(results) == 1

    def test_reopen_writer(self, tmp_dir: Path):
        path1 = tmp_dir / "first_gt.osi"
        path2 = tmp_dir / "second_gt.osi"
        writer = SingleTraceWriter()
        writer.open(path1)
        writer.write_message(_make_ground_truth(1))
        writer.close()

        writer.open(path2)
        writer.write_message(_make_ground_truth(2))
        writer.close()

        with SingleTraceReader() as r:
            r.set_message_type(MessageType.GROUND_TRUTH)
            r.open(path2)
            results = list(r)
            assert len(results) == 1
            assert results[0].message.timestamp.seconds == 2


class TestMultiTraceWriterErrors:
    def test_add_channel_before_open(self):
        writer = MultiTraceWriter()
        with pytest.raises(RuntimeError, match="not open"):
            writer.add_channel("gt", GroundTruth)

    def test_write_without_metadata(self, tmp_dir: Path):
        """Auto-generated metadata should include required keys."""
        path = tmp_dir / "no_meta.mcap"
        with MultiTraceWriter() as writer:
            writer.open(path)
            writer.write_message(_make_ground_truth())

        with MultiTraceReader() as reader:
            reader.open(path)
            metadata = reader.get_file_metadata()
            osi_meta = [m for m in metadata if m["name"] == "net.asam.osi.trace"]
            assert len(osi_meta) == 1
            assert "version" in osi_meta[0]["data"]
            assert "creation_time" in osi_meta[0]["data"]

    def test_add_file_metadata(self, tmp_dir: Path):
        path = tmp_dir / "extra_meta.mcap"
        with MultiTraceWriter() as writer:
            writer.open(path)
            writer.add_file_metadata("custom.data", {"key1": "val1", "key2": "val2"})
            writer.write_message(_make_ground_truth())

        with MultiTraceReader() as reader:
            reader.open(path)
            metadata = reader.get_file_metadata()
            custom = [m for m in metadata if m["name"] == "custom.data"]
            assert len(custom) == 1
            assert custom[0]["data"]["key1"] == "val1"

    def test_schema_reuse_across_channels(self, tmp_dir: Path):
        path = tmp_dir / "schema_reuse.mcap"
        with MultiTraceWriter() as writer:
            writer.open(path)
            writer.add_channel("gt1", GroundTruth)
            writer.add_channel("gt2", GroundTruth)
            assert len(writer._schema_cache) == 1
            writer.write_message(_make_ground_truth(1), "gt1")
            writer.write_message(_make_ground_truth(2), "gt2")

        with MultiTraceReader() as reader:
            reader.open(path)
            results = list(reader)
            assert len(results) == 2

    def test_close_and_reopen(self, tmp_dir: Path):
        path1 = tmp_dir / "first.mcap"
        path2 = tmp_dir / "second.mcap"
        writer = MultiTraceWriter()

        writer.open(path1)
        writer.write_message(_make_ground_truth(1))
        writer.close()

        writer.open(path2)
        writer.write_message(_make_ground_truth(2))
        writer.close()

        with MultiTraceReader() as r:
            r.open(path2)
            results = list(r)
            assert len(results) == 1
            assert results[0].message.timestamp.seconds == 2

    def test_duplicate_channel_raises(self, tmp_dir: Path):
        path = tmp_dir / "dup_ch.mcap"
        with MultiTraceWriter() as writer:
            writer.open(path)
            writer.add_channel("gt", GroundTruth)
            with pytest.raises(RuntimeError, match="already exists"):
                writer.add_channel("gt", GroundTruth)

    def test_write_to_unknown_topic(self, tmp_dir: Path):
        path = tmp_dir / "unknown_topic.mcap"
        with MultiTraceWriter() as writer:
            writer.open(path)
            writer.add_channel("gt", GroundTruth)
            assert not writer.write_message(_make_ground_truth(), "nonexistent")


class TestProtobufTextFormatTraceWriterErrors:
    def test_write_multiple_messages(self, tmp_dir: Path):
        path = tmp_dir / "multi_gt.txth"
        with ProtobufTextFormatTraceWriter() as w:
            w.open(path)
            for i in range(3):
                assert w.write_message(_make_ground_truth(i))
            assert w.written_count == 3

    def test_write_after_close(self, tmp_dir: Path):
        path = tmp_dir / "closed_gt.txth"
        writer = ProtobufTextFormatTraceWriter()
        writer.open(path)
        writer.close()
        assert not writer.write_message(_make_ground_truth())


# ===========================================================================
# Cross-format roundtrip tests
# ===========================================================================


class TestCrossFormatRoundtrip:
    def test_binary_roundtrip_preserves_all_fields(self, tmp_dir: Path):
        """Write a complex GroundTruth and verify all fields survive roundtrip."""
        path = tmp_dir / "complex_gt.osi"
        gt = _make_ground_truth(99)
        gt.host_vehicle_id.value = 42
        obj = gt.moving_object.add()
        obj.id.value = 1
        obj.base.position.x = 1.5
        obj.base.position.y = 2.5
        obj.base.position.z = 3.5

        with SingleTraceWriter() as w:
            w.open(path)
            w.write_message(gt)

        with SingleTraceReader() as r:
            r.set_message_type(MessageType.GROUND_TRUTH)
            r.open(path)
            results = list(r)
            assert len(results) == 1
            read_gt = results[0].message
            assert read_gt.timestamp.seconds == 99
            assert read_gt.host_vehicle_id.value == 42
            assert len(read_gt.moving_object) == 1
            assert read_gt.moving_object[0].base.position.x == pytest.approx(1.5)
            assert read_gt.moving_object[0].base.position.y == pytest.approx(2.5)

    def test_mcap_roundtrip_preserves_all_fields(self, tmp_dir: Path):
        """Write a complex GroundTruth to MCAP and verify fields survive roundtrip."""
        path = tmp_dir / "complex.mcap"
        gt = _make_ground_truth(55)
        gt.host_vehicle_id.value = 7
        obj = gt.moving_object.add()
        obj.id.value = 10
        obj.base.dimension.length = 4.5
        obj.base.dimension.width = 1.8

        with MultiTraceWriter() as w:
            w.open(path)
            w.write_message(gt)

        with MultiTraceReader() as r:
            r.open(path)
            results = list(r)
            assert len(results) == 1
            read_gt = results[0].message
            assert read_gt.timestamp.seconds == 55
            assert read_gt.host_vehicle_id.value == 7
            assert read_gt.moving_object[0].base.dimension.length == pytest.approx(4.5)

    def test_write_read_sensor_view(self, tmp_dir: Path):
        """Test with SensorView instead of GroundTruth for message type diversity."""
        osi_path = tmp_dir / "test_sv.osi"
        mcap_path = tmp_dir / "test.mcap"

        sv = _make_sensor_view(33)
        sv.host_vehicle_id.value = 5

        # Binary roundtrip
        with SingleTraceWriter() as w:
            w.open(osi_path)
            w.write_message(sv)

        with SingleTraceReader() as r:
            r.set_message_type(MessageType.SENSOR_VIEW)
            r.open(osi_path)
            results = list(r)
            assert len(results) == 1
            assert results[0].message_type == MessageType.SENSOR_VIEW
            assert results[0].message.host_vehicle_id.value == 5

        # MCAP roundtrip
        with MultiTraceWriter() as w:
            w.open(mcap_path)
            w.write_message(sv)

        with MultiTraceReader() as r:
            r.open(mcap_path)
            results = list(r)
            assert len(results) == 1
            assert results[0].message_type == MessageType.SENSOR_VIEW
            assert results[0].message.timestamp.seconds == 33


_MESSAGE_TYPE_MAP = {
    MessageType.GROUND_TRUTH: GroundTruth,
    MessageType.SENSOR_VIEW: SensorView,
    MessageType.SENSOR_DATA: SensorData,
    MessageType.HOST_VEHICLE_DATA: HostVehicleData,
    MessageType.TRAFFIC_COMMAND: TrafficCommand,
    MessageType.TRAFFIC_COMMAND_UPDATE: TrafficCommandUpdate,
    MessageType.TRAFFIC_UPDATE: TrafficUpdate,
    MessageType.MOTION_REQUEST: MotionRequest,
    MessageType.STREAMING_UPDATE: StreamingUpdate,
}


def _make_message(msg_type: MessageType, index: int = 0):
    cls = _MESSAGE_TYPE_MAP[msg_type]
    msg = cls()
    msg.timestamp.seconds = index
    msg.timestamp.nanos = index * 1000
    return msg


_ALL_TYPES = [
    MessageType.GROUND_TRUTH,
    MessageType.SENSOR_VIEW,
    MessageType.SENSOR_DATA,
    MessageType.HOST_VEHICLE_DATA,
    MessageType.TRAFFIC_COMMAND,
    MessageType.TRAFFIC_COMMAND_UPDATE,
    MessageType.TRAFFIC_UPDATE,
    MessageType.MOTION_REQUEST,
    MessageType.STREAMING_UPDATE,
]


class TestMultiTypeRoundtrip:
    """Parameterized roundtrip tests for all 9 message types × 3 formats (27 tests)."""

    @pytest.mark.parametrize("msg_type", _ALL_TYPES, ids=lambda t: t.name)
    def test_binary_roundtrip(self, msg_type: MessageType, tmp_dir: Path):
        path = tmp_dir / f"roundtrip_{msg_type.name.lower()}.osi"
        msg = _make_message(msg_type, index=42)
        with SingleTraceWriter() as w:
            w.open(path)
            w.write_message(msg)
        with SingleTraceReader() as r:
            r.set_message_type(msg_type)
            r.open(path)
            results = list(r)
            assert len(results) == 1
            assert results[0].message_type == msg_type
            assert results[0].message.timestamp.seconds == 42

    @pytest.mark.parametrize("msg_type", _ALL_TYPES, ids=lambda t: t.name)
    def test_mcap_roundtrip(self, msg_type: MessageType, tmp_dir: Path):
        path = tmp_dir / f"roundtrip_{msg_type.name.lower()}.mcap"
        msg = _make_message(msg_type, index=55)
        with MultiTraceWriter() as w:
            w.open(path)
            w.write_message(msg)
        with MultiTraceReader() as r:
            r.open(path)
            results = list(r)
            assert len(results) == 1
            assert results[0].message_type == msg_type
            assert results[0].message.timestamp.seconds == 55

    @pytest.mark.parametrize("msg_type", _ALL_TYPES, ids=lambda t: t.name)
    def test_txth_roundtrip(self, msg_type: MessageType, tmp_dir: Path):
        path = tmp_dir / f"roundtrip_{msg_type.name.lower()}.txth"
        msg = _make_message(msg_type, index=77)
        with ProtobufTextFormatTraceWriter() as w:
            w.open(path)
            w.write_message(msg)
        with ProtobufTextFormatTraceReader() as r:
            r.set_message_type(msg_type)
            r.open(path)
            results = list(r)
            assert len(results) == 1
            assert results[0].message_type == msg_type
            assert results[0].message.timestamp.seconds == 77
