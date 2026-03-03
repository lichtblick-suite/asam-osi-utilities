# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Public timestamp conversion utilities for OSI trace files.

Provides conversions between OSI protobuf Timestamp messages,
float seconds, and integer nanoseconds.
"""

from __future__ import annotations

from google.protobuf.message import Message

from osi_utilities.tracefile._config import NANOSECONDS_PER_SECOND


def timestamp_to_nanoseconds(message: Message) -> int:
    """Extract timestamp from an OSI message as integer nanoseconds.

    Args:
        message: Any OSI protobuf message with a ``timestamp`` field
            (e.g. GroundTruth, SensorView, SensorData).

    Returns:
        Timestamp in nanoseconds, or 0 if the message has no timestamp field.
    """
    if hasattr(message, "timestamp"):
        ts = message.timestamp
        return int(ts.seconds * NANOSECONDS_PER_SECOND + ts.nanos)
    return 0


def timestamp_to_seconds(message: Message) -> float:
    """Extract timestamp from an OSI message as float seconds.

    Args:
        message: Any OSI protobuf message with a ``timestamp`` field.

    Returns:
        Timestamp in seconds, or 0.0 if the message has no timestamp field.
    """
    ns = timestamp_to_nanoseconds(message)
    return ns / NANOSECONDS_PER_SECOND


def nanoseconds_to_seconds(nanoseconds: int) -> float:
    """Convert nanoseconds to float seconds.

    Args:
        nanoseconds: Timestamp in nanoseconds.

    Returns:
        Timestamp in seconds as float.
    """
    return nanoseconds / NANOSECONDS_PER_SECOND


def seconds_to_nanoseconds(seconds: float) -> int:
    """Convert float seconds to integer nanoseconds.

    Args:
        seconds: Timestamp in seconds.

    Returns:
        Timestamp in nanoseconds as integer.
    """
    return int(seconds * NANOSECONDS_PER_SECOND)
