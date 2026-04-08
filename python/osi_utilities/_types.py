# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Shared core types used across api and tracefile modules."""

from dataclasses import dataclass
from enum import Enum

from google.protobuf.message import Message


class MessageType(str, Enum):
    """OSI top-level message types."""

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


@dataclass
class ReadResult:
    """Result of reading a single message from a trace file."""

    message: Message
    message_type: MessageType
    channel_name: str = ""
