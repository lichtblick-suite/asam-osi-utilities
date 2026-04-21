# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Core types, enumerations, and constants for OSI trace file handling."""

from __future__ import annotations

import logging

from google.protobuf.message import Message

from osi_utilities._types import MessageType

logger = logging.getLogger(__name__)


_MESSAGE_NAME_TO_TYPE: dict[str, MessageType] = {
    message_type.value: message_type for message_type in MessageType if message_type is not MessageType.UNKNOWN
}


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
            allowed = sorted([m.value for m in MessageType if m is not MessageType.UNKNOWN])
            raise ValueError(f"Unsupported OSI message type: {value!r}. Allowed values: {allowed}") from exc

    raise TypeError(f"Unsupported type for message_type: {type(value).__name__}. Expected MessageType, str, or None.")


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
