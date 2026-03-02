# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Core types, enumerations, and constants for OSI trace file handling."""

from __future__ import annotations

import logging
import re
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from pathlib import Path

from google.protobuf.message import Message

logger = logging.getLogger(__name__)


class MessageType(Enum):
    """OSI top-level message types. Mirrors C++ ReaderTopLevelMessage enum."""

    UNKNOWN = 0
    GROUND_TRUTH = 1
    SENSOR_DATA = 2
    SENSOR_VIEW = 3
    SENSOR_VIEW_CONFIGURATION = 4
    HOST_VEHICLE_DATA = 5
    TRAFFIC_COMMAND = 6
    TRAFFIC_COMMAND_UPDATE = 7
    TRAFFIC_UPDATE = 8
    MOTION_REQUEST = 9
    STREAMING_UPDATE = 10


class TraceFileFormat(Enum):
    """Trace file format categories."""

    SINGLE_CHANNEL = 1
    MULTI_CHANNEL = 2


# Maps filename pattern substrings to MessageType (matching C++ kFileNameMessageTypeMap)
FILENAME_MESSAGE_TYPE_MAP: dict[str, MessageType] = {
    "_gt_": MessageType.GROUND_TRUTH,
    "_sd_": MessageType.SENSOR_DATA,
    "_sv_": MessageType.SENSOR_VIEW,
    "_svc_": MessageType.SENSOR_VIEW_CONFIGURATION,
    "_hvd_": MessageType.HOST_VEHICLE_DATA,
    "_tc_": MessageType.TRAFFIC_COMMAND,
    "_tcu_": MessageType.TRAFFIC_COMMAND_UPDATE,
    "_tu_": MessageType.TRAFFIC_UPDATE,
    "_mr_": MessageType.MOTION_REQUEST,
    "_su_": MessageType.STREAMING_UPDATE,
}

# Short code to message type name (for OSI naming convention parser)
_SHORT_CODE_TO_MESSAGE_TYPE: dict[str, str] = {
    "sv": "SensorView",
    "svc": "SensorViewConfiguration",
    "gt": "GroundTruth",
    "hvd": "HostVehicleData",
    "sd": "SensorData",
    "tc": "TrafficCommand",
    "tcu": "TrafficCommandUpdate",
    "tu": "TrafficUpdate",
    "mr": "MotionRequest",
    "su": "StreamingUpdate",
}

# Maps MCAP schema names to MessageType (matching C++ MCAPTraceFileReader logic)
SCHEMA_NAME_TO_MESSAGE_TYPE: dict[str, MessageType] = {
    "osi3.GroundTruth": MessageType.GROUND_TRUTH,
    "osi3.SensorData": MessageType.SENSOR_DATA,
    "osi3.SensorView": MessageType.SENSOR_VIEW,
    "osi3.SensorViewConfiguration": MessageType.SENSOR_VIEW_CONFIGURATION,
    "osi3.HostVehicleData": MessageType.HOST_VEHICLE_DATA,
    "osi3.TrafficCommand": MessageType.TRAFFIC_COMMAND,
    "osi3.TrafficCommandUpdate": MessageType.TRAFFIC_COMMAND_UPDATE,
    "osi3.TrafficUpdate": MessageType.TRAFFIC_UPDATE,
    "osi3.MotionRequest": MessageType.MOTION_REQUEST,
    "osi3.StreamingUpdate": MessageType.STREAMING_UPDATE,
}

# Maps MessageType to protobuf class name (for dynamic class lookup)
MESSAGE_TYPE_TO_CLASS_NAME: dict[MessageType, str] = {
    MessageType.GROUND_TRUTH: "GroundTruth",
    MessageType.SENSOR_DATA: "SensorData",
    MessageType.SENSOR_VIEW: "SensorView",
    MessageType.SENSOR_VIEW_CONFIGURATION: "SensorViewConfiguration",
    MessageType.HOST_VEHICLE_DATA: "HostVehicleData",
    MessageType.TRAFFIC_COMMAND: "TrafficCommand",
    MessageType.TRAFFIC_COMMAND_UPDATE: "TrafficCommandUpdate",
    MessageType.TRAFFIC_UPDATE: "TrafficUpdate",
    MessageType.MOTION_REQUEST: "MotionRequest",
    MessageType.STREAMING_UPDATE: "StreamingUpdate",
}

# Extension to format mapping
_EXT_TO_FORMAT: dict[str, TraceFileFormat] = {
    ".osi": TraceFileFormat.SINGLE_CHANNEL,
    ".xz": TraceFileFormat.SINGLE_CHANNEL,
    ".lzma": TraceFileFormat.SINGLE_CHANNEL,
    ".txth": TraceFileFormat.SINGLE_CHANNEL,
    ".mcap": TraceFileFormat.MULTI_CHANNEL,
}


def get_trace_file_format(path: Path) -> TraceFileFormat:
    """Determine the trace file format from the file extension."""
    fmt = _EXT_TO_FORMAT.get(path.suffix.lower())
    if fmt is None:
        raise ValueError(f"Unsupported trace file extension: '{path.suffix}'")
    return fmt


def _get_message_class(message_type: MessageType) -> type[Message]:
    """Dynamically import and return the protobuf class for the given MessageType."""
    class_name = MESSAGE_TYPE_TO_CLASS_NAME.get(message_type)
    if class_name is None:
        raise ValueError(f"No protobuf class for message type: {message_type}")

    # Import from the osi3 package
    module_name_map: dict[str, str] = {
        "GroundTruth": "osi_groundtruth_pb2",
        "SensorData": "osi_sensordata_pb2",
        "SensorView": "osi_sensorview_pb2",
        "SensorViewConfiguration": "osi_sensorviewconfiguration_pb2",
        "HostVehicleData": "osi_hostvehicledata_pb2",
        "TrafficCommand": "osi_trafficcommand_pb2",
        "TrafficCommandUpdate": "osi_trafficcommandupdate_pb2",
        "TrafficUpdate": "osi_trafficupdate_pb2",
        "MotionRequest": "osi_motionrequest_pb2",
        "StreamingUpdate": "osi_streamingupdate_pb2",
    }
    import importlib

    module = importlib.import_module(f"osi3.{module_name_map[class_name]}")
    return getattr(module, class_name)


def infer_message_type_from_filename(filename: str) -> MessageType:
    """Infer message type from filename patterns.

    Matches the C++ kFileNameMessageTypeMap patterns (e.g. ``_gt_``) and also
    supports the common convention where the type code appears at the end of
    the stem before the extension (e.g. ``trace_gt.osi``, ``output_sv.mcap``).
    """
    lower_name = filename.lower()
    # First try the strict C++ patterns (e.g. "_gt_")
    for pattern, msg_type in FILENAME_MESSAGE_TYPE_MAP.items():
        if pattern in lower_name:
            return msg_type

    # Fallback: match type code at end of stem before extension (e.g. "_gt.osi")
    stem = Path(filename).stem.lower()
    for pattern, msg_type in FILENAME_MESSAGE_TYPE_MAP.items():
        code = pattern.strip("_")  # "_gt_" -> "gt"
        if stem.endswith(f"_{code}") or stem == code:
            return msg_type

    return MessageType.UNKNOWN


def parse_osi_trace_filename(filename: str) -> dict:
    """Parse an OSI trace filename according to OSI naming convention 2.2.6.2.

    Format: <timestamp>_<type>_<osi-version>_<protobuf-version>_<number-of-frames>_<custom-trace-name>.osi

    Returns a dict with parsed fields, or empty dict if parsing fails.
    """
    pattern = re.compile(
        r"^(?P<timestamp>\d{8}T\d{6}Z)"
        r"_(?P<message_type>sv|svc|gt|hvd|sd|tc|tcu|tu|mr|su)"
        r"_(?P<osi_version>[^_]+)"
        r"_(?P<protobuf_version>[^_]+)"
        r"_(?P<number_of_frames>\d+)"
        r"_(?P<custom_trace_name>[^.]+)"
        r"\.osi$",
    )
    match = pattern.match(filename)
    if not match:
        return {}

    try:
        timestamp = datetime.strptime(match.group("timestamp"), "%Y%m%dT%H%M%SZ")
        return {
            "timestamp": timestamp,
            "message_type": _SHORT_CODE_TO_MESSAGE_TYPE.get(match.group("message_type")),
            "osi_version": match.group("osi_version"),
            "protobuf_version": match.group("protobuf_version"),
            "number_of_frames": int(match.group("number_of_frames")),
            "custom_trace_name": match.group("custom_trace_name"),
        }
    except (ValueError, IndexError) as e:
        logger.warning("Error parsing filename %s: %s", filename, e)
        return {}


@dataclass
class ReadResult:
    """Result of reading a single message from a trace file."""

    #: The deserialized protobuf message.
    message: Message
    #: The OSI message type.
    message_type: MessageType
    #: The channel/topic name (MCAP only, empty for single-channel formats).
    channel_name: str = ""


@dataclass
class ChannelSpecification:
    """Specification for an OSI channel within a trace file.

    Adopted from PMSFIT's OSIChannelSpecification with builder pattern.
    """

    #: Path to the trace file.
    path: Path
    #: OSI message type name (e.g. "GroundTruth").
    message_type: str | None = None
    #: Channel topic name (MCAP only).
    topic: str | None = None
    #: Additional channel metadata.
    metadata: dict[str, str] = field(default_factory=dict)

    @property
    def trace_file_format(self) -> TraceFileFormat:
        """Determine the trace file format from the file extension."""
        return get_trace_file_format(self.path)

    def try_autodetect_message_type(self) -> bool:
        """Attempt to detect message type from filename.

        Returns True if detection succeeded and sets self.message_type.
        """
        if self.message_type is not None:
            return True

        parsed = parse_osi_trace_filename(self.path.name)
        detected = parsed.get("message_type")
        if detected is not None:
            self.message_type = detected
            return True

        msg_type = infer_message_type_from_filename(self.path.name)
        if msg_type != MessageType.UNKNOWN:
            self.message_type = MESSAGE_TYPE_TO_CLASS_NAME[msg_type]
            return True

        return False

    def autofill_topic(self) -> None:
        """Set topic to filename stem if not already set."""
        if self.topic is None:
            self.topic = self.path.stem

    def exists(self) -> bool:
        """Check if the file at path exists."""
        return self.path.exists() and self.path.is_file()

    def with_message_type(self, message_type: str) -> ChannelSpecification:
        """Return a new ChannelSpecification with a different message type."""
        return ChannelSpecification(
            path=self.path, message_type=message_type, topic=self.topic, metadata=dict(self.metadata)
        )

    def with_topic(self, topic: str) -> ChannelSpecification:
        """Return a new ChannelSpecification with a different topic."""
        return ChannelSpecification(
            path=self.path, message_type=self.message_type, topic=topic, metadata=dict(self.metadata)
        )

    def with_trace_file_format(self, fmt: TraceFileFormat) -> ChannelSpecification:
        """Return a new ChannelSpecification with a different file extension."""
        ext_map = {TraceFileFormat.SINGLE_CHANNEL: ".osi", TraceFileFormat.MULTI_CHANNEL: ".mcap"}
        return ChannelSpecification(
            path=self.path.with_suffix(ext_map[fmt]),
            message_type=self.message_type,
            topic=self.topic,
            metadata=dict(self.metadata),
        )
