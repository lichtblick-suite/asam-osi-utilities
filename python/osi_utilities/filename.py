# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

import logging
from pathlib import Path
import re
from datetime import datetime

from osi_utilities._types import MessageType

logger = logging.getLogger(__name__)


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
