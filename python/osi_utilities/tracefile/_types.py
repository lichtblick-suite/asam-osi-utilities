# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Core types, enumerations, and constants for OSI trace file handling."""

from __future__ import annotations

import logging
import re
from collections.abc import Mapping
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from pathlib import Path
from types import MappingProxyType

from google.protobuf.message import Message

logger = logging.getLogger(__name__)


class MessageType(str, Enum):
    """OSI top-level message types.

    String-backed enum so values can be used where string class names are expected.
    """

    UNKNOWN = "Unknown"  # Unknown should only be used when message type inference fails
    GROUND_TRUTH = "GroundTruth"
    SENSOR_DATA = "SensorData"
    SENSOR_VIEW = "SensorView"
    SENSOR_VIEW_CONFIGURATION = "SensorViewConfiguration"
    HOST_VEHICLE_DATA = "HostVehicleData"
    TRAFFIC_COMMAND = "TrafficCommand"
    TRAFFIC_COMMAND_UPDATE = "TrafficCommandUpdate"
    TRAFFIC_UPDATE = "TrafficUpdate"
    MOTION_REQUEST = "MotionRequest"
    STREAMING_UPDATE = "StreamingUpdate"


class TraceFileFormat(Enum):
    """Trace file format categories."""

    SINGLE_CHANNEL = 1
    MULTI_CHANNEL = 2


# Short code to message type (for OSI naming convention parser)
_SHORT_CODE_TO_MESSAGE_TYPE: dict[str, MessageType] = {
    "sv": MessageType.SENSOR_VIEW,
    "svc": MessageType.SENSOR_VIEW_CONFIGURATION,
    "gt": MessageType.GROUND_TRUTH,
    "hvd": MessageType.HOST_VEHICLE_DATA,
    "sd": MessageType.SENSOR_DATA,
    "tc": MessageType.TRAFFIC_COMMAND,
    "tcu": MessageType.TRAFFIC_COMMAND_UPDATE,
    "tu": MessageType.TRAFFIC_UPDATE,
    "mr": MessageType.MOTION_REQUEST,
    "su": MessageType.STREAMING_UPDATE,
}

# Longest-first to avoid prefix ambiguity (e.g. svc before sv)
_SHORT_CODE_PATTERN = "|".join(
    sorted(_SHORT_CODE_TO_MESSAGE_TYPE.keys(), key=len, reverse=True)
)

# Maps filename pattern substrings to MessageType (matching C++ kFileNameMessageTypeMap)
_FILENAME_MESSAGE_TYPE_MAP: dict[str, MessageType] = {
    f"_{short_code}_": message_type
    for short_code, message_type in _SHORT_CODE_TO_MESSAGE_TYPE.items()
}

_MESSAGE_NAME_TO_TYPE: dict[str, MessageType] = {
    message_type.value: message_type
    for message_type in MessageType
    if message_type is not MessageType.UNKNOWN
}

_EXTENSION_TO_FORMAT: dict[str, TraceFileFormat] = {
    ".osi": TraceFileFormat.SINGLE_CHANNEL,
    ".txth": TraceFileFormat.SINGLE_CHANNEL,
    ".mcap": TraceFileFormat.MULTI_CHANNEL,
}

_FORMAT_TO_EXTENSION: dict[TraceFileFormat, str] = {
    TraceFileFormat.SINGLE_CHANNEL: ".osi",
    TraceFileFormat.MULTI_CHANNEL: ".mcap",
}


def get_trace_file_format(path: Path) -> TraceFileFormat:
    """Determine the trace file format from the file extension."""
    format = _EXTENSION_TO_FORMAT.get(path.suffix.lower())
    if format is None:
        raise ValueError(f"Unsupported trace file extension: '{path.suffix}'")
    return format


def message_type_to_class_name(message_type: MessageType) -> str | None:
    """Return protobuf class name for the message type."""
    if message_type is MessageType.UNKNOWN:
        return None
    return message_type.value


def message_type_to_schema_name(message_type: MessageType) -> str | None:
    """Return MCAP schema name (e.g. ``osi3.SensorView``) for a message type."""
    class_name = message_type_to_class_name(message_type)
    if class_name is None:
        return None
    return f"osi3.{class_name}"


def schema_name_to_message_type(schema_name: str | None) -> MessageType:
    """Convert an MCAP schema name to MessageType."""
    if not schema_name or not schema_name.startswith("osi3."):
        return MessageType.UNKNOWN
    class_name = schema_name.split(".", 1)[1]
    return _MESSAGE_NAME_TO_TYPE.get(class_name, MessageType.UNKNOWN)


def get_message_class(message_type: MessageType) -> type[Message]:
    """Dynamically import and return the protobuf class for the given MessageType."""
    class_name = message_type_to_class_name(message_type)
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

    module_name = module_name_map.get(class_name)
    if module_name is None:
        raise ValueError(f"No module mapping for protobuf class: {class_name}")
    module = importlib.import_module(f"osi3.{module_name}")
    return getattr(module, class_name)


def infer_message_type_from_filename(filename: str) -> MessageType:
    """Infer message type from filename patterns.

    Matches the C++ kFileNameMessageTypeMap patterns (e.g. ``_gt_``) and also
    supports the common convention where the type code appears at the end of
    the stem before the extension (e.g. ``trace_gt.osi``, ``output_sv.mcap``).
    """
    lower_name = filename.lower()
    # First try the strict C++ patterns (e.g. "_gt_")
    for pattern, msg_type in _FILENAME_MESSAGE_TYPE_MAP.items():
        if pattern in lower_name:
            return msg_type

    # Fallback: match type code at end of stem before extension (e.g. "_gt.osi")
    stem = Path(filename).stem.lower()
    for pattern, msg_type in _FILENAME_MESSAGE_TYPE_MAP.items():
        code = pattern.strip("_")  # "_gt_" -> "gt"
        if stem.endswith(f"_{code}") or stem == code:
            return msg_type

    return MessageType.UNKNOWN


def coerce_message_type(value: MessageType | str | None) -> MessageType | None:
    """Normalize user input to ``MessageType``.

    Args:
        value: Message type input as ``MessageType``, class-name string
            (e.g. ``"SensorView"``), enum-name string
            (e.g. ``"SENSOR_VIEW"``), or ``None``.

    Returns:
        A normalized ``MessageType`` instance, or ``None`` if ``value`` is ``None``.

    Raises:
        ValueError: If a string value is not a supported OSI message type.
        TypeError: If ``value`` is not ``MessageType``, ``str``, or ``None``.

    Note:
        Use :func:`require_message_type` when a concrete non-``None`` message type
        is required.
    """
    if value is None:
        return None
    if isinstance(value, MessageType):
        return value
    if isinstance(value, str):
        by_class_name = _MESSAGE_NAME_TO_TYPE.get(value)
        if by_class_name is not None:
            return by_class_name
        try:
            return MessageType[value]
        except KeyError as exc:
            allowed = sorted(
                [m.value for m in MessageType if m is not MessageType.UNKNOWN]
            )
            raise ValueError(
                f"Unsupported OSI message type: {value!r}. Allowed values: {allowed}"
            ) from exc

    raise TypeError(
        f"Unsupported type for message_type: {type(value).__name__}. "
        "Expected MessageType, str, or None."
    )


def require_message_type(value: MessageType | str | None) -> MessageType:
    """Normalize and require a concrete MessageType.

    Raises:
        ValueError: If value is None or MessageType.UNKNOWN.
        TypeError: If value type is unsupported.
    """
    message_type = coerce_message_type(value)
    if message_type is None or message_type is MessageType.UNKNOWN:
        raise ValueError(f"A concrete OSI message type is required, got: {value!r}")
    return message_type


def parse_osi_trace_filename(filename: str) -> dict:
    """Parse an OSI trace filename according to OSI naming convention 2.2.6.2.

    Format: <timestamp>_<type>_<osi-version>_<protobuf-version>_<number-of-frames>_<custom-trace-name>.osi

    Returns a dict with parsed fields, or empty dict if parsing fails.
    """
    pattern = re.compile(
        r"^(?P<timestamp>\d{8}T\d{6}Z)"
        rf"_(?P<message_type>{_SHORT_CODE_PATTERN})"
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
            "message_type": _SHORT_CODE_TO_MESSAGE_TYPE.get(
                match.group("message_type")
            ),
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


@dataclass(frozen=True)
class ChannelSpecification:
    """Immutable specification for an OSI channel within a trace file."""

    #: Path to the trace file.
    path: Path
    #: OSI message type (normalized to MessageType if provided).
    message_type: MessageType | str | None = None
    #: Channel topic name (MCAP only).
    topic: str | None = None
    #: Additional channel metadata.
    metadata: Mapping[str, str] = field(default_factory=dict)

    def __post_init__(self) -> None:
        """Normalize runtime input values to canonical internal types."""
        object.__setattr__(self, "path", Path(self.path))
        object.__setattr__(self, "message_type", coerce_message_type(self.message_type))
        object.__setattr__(
            self,
            "metadata",
            MappingProxyType(dict(self.metadata or {})),  # Ensure metadata is immutable and non-None
        )

    @property
    def trace_file_format(self) -> TraceFileFormat:
        """Determine the trace file format from the file extension."""
        return get_trace_file_format(self.path)

    @property
    def autodetected_message_type(self) -> MessageType | None:
        """Best-effort inferred message type from filename, without mutating this instance."""
        parsed = parse_osi_trace_filename(self.path.name)
        detected = parsed.get("message_type")
        if detected is not None:
            return detected

        msg_type = infer_message_type_from_filename(self.path.name)
        if msg_type != MessageType.UNKNOWN:
            return msg_type
        return None

    def with_autodetected_message_type(self) -> ChannelSpecification:
        """Return a specification with auto-detected message type when possible.

        If message_type is already set or cannot be inferred, returns self unchanged.
        """
        if self.message_type is not None:
            return self

        detected = self.autodetected_message_type
        if detected is None:
            return self
        return self.with_message_type(detected)

    def with_autofilled_topic(self) -> ChannelSpecification:
        """Return a specification with topic set to filename stem if missing."""
        if self.topic is None:
            return self.with_topic(self.path.stem)
        return self

    def exists(self) -> bool:
        """Check if the file at path exists."""
        return self.path.exists() and self.path.is_file()

    def with_message_type(
        self, message_type: MessageType | str
    ) -> ChannelSpecification:
        """Return a new ChannelSpecification with a different message type."""
        return ChannelSpecification(
            path=self.path,
            message_type=message_type,
            topic=self.topic,
            metadata=dict(self.metadata),
        )

    def with_topic(self, topic: str) -> ChannelSpecification:
        """Return a new ChannelSpecification with a different topic."""
        return ChannelSpecification(
            path=self.path,
            message_type=self.message_type,
            topic=topic,
            metadata=dict(self.metadata),
        )

    def with_path(self, path: Path | str) -> ChannelSpecification:
        """Return a new ChannelSpecification with a different path."""
        return ChannelSpecification(
            path=path,
            message_type=self.message_type,
            topic=self.topic,
            metadata=dict(self.metadata),
        )

    def with_metadata(self, metadata: Mapping[str, str]) -> ChannelSpecification:
        """Return a new ChannelSpecification with replaced metadata."""
        return ChannelSpecification(
            path=self.path,
            message_type=self.message_type,
            topic=self.topic,
            metadata=dict(metadata),
        )

    def with_metadata_updates(
        self, updates: Mapping[str, str]
    ) -> ChannelSpecification:
        """Return a new ChannelSpecification with metadata updated by key."""
        merged = dict(self.metadata)
        merged.update(dict(updates))
        return ChannelSpecification(
            path=self.path,
            message_type=self.message_type,
            topic=self.topic,
            metadata=merged,
        )

    def with_metadata_entry(self, key: str, value: str) -> ChannelSpecification:
        """Return a new ChannelSpecification with one metadata key updated."""
        return self.with_metadata_updates({key: value})

    def with_trace_file_format(self, format: TraceFileFormat) -> ChannelSpecification:
        """Return a new ChannelSpecification with a different file extension."""
        return ChannelSpecification(
            path=self.path.with_suffix(_FORMAT_TO_EXTENSION[format]),
            message_type=self.message_type,
            topic=self.topic,
            metadata=dict(self.metadata),
        )
