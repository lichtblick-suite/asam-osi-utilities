# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Binary (.osi) trace file reader.

Reads OSI trace files in the single-channel binary format where each message
is prefixed with a 4-byte little-endian uint32 length.
"""

from __future__ import annotations

import logging
import struct
from pathlib import Path
from typing import IO

from osi_utilities.tracefile._config import (
    BINARY_MESSAGE_LENGTH_PREFIX_SIZE,
    MAX_EXPECTED_MESSAGE_SIZE,
)
from osi_utilities.tracefile._types import (
    MessageType,
    ReadResult,
    get_message_class,
    infer_message_type_from_filename,
)
from osi_utilities.tracefile.reader import TraceFileReader

logger = logging.getLogger(__name__)


class BinaryTraceFileReader(TraceFileReader):
    """Reader for single-channel binary OSI trace files (.osi).

    Each message is stored as: [4-byte LE length][serialized protobuf bytes]

    The message type can be specified explicitly or inferred from the filename.
    """

    def __init__(self, message_type: MessageType = MessageType.UNKNOWN) -> None:
        """Initialize the binary trace file reader.

        Args:
            message_type: The expected message type. If UNKNOWN, will be inferred from filename.
        """
        self._message_type = message_type
        self._file: IO[bytes] | None = None
        self._message_class: type | None = None
        self._has_next = False

    def open(self, path: Path) -> bool:
        """Open a binary .osi trace file.

        Args:
            path: Path to the .osi file.

        Returns:
            True on success, False on failure.
        """
        if self._message_type == MessageType.UNKNOWN:
            self._message_type = infer_message_type_from_filename(path.name)

        if self._message_type == MessageType.UNKNOWN:
            logger.error("Cannot determine message type for '%s'. Specify it explicitly.", path)
            return False

        try:
            self._message_class = get_message_class(self._message_type)
        except ValueError as e:
            logger.error("Failed to get message class: %s", e)
            return False

        try:
            self._file = open(path, "rb")  # noqa: SIM115
        except OSError as e:
            logger.error("Failed to open file '%s': %s", path, e)
            return False

        self._has_next = self._peek_has_data()
        return True

    def open_with_type(self, path: Path, message_type: MessageType) -> bool:
        """Open a binary .osi trace file with an explicit message type.

        Args:
            path: Path to the .osi file.
            message_type: The message type to use.

        Returns:
            True on success, False on failure.
        """
        self._message_type = message_type
        return self.open(path)

    def read_message(self) -> ReadResult | None:
        """Read the next message from the binary trace file.

        Returns:
            ReadResult on success, None if no more messages.

        Raises:
            RuntimeError: If the message is truncated or deserialization fails.
        """
        if self._file is None or self._message_class is None:
            return None

        length_bytes = self._file.read(BINARY_MESSAGE_LENGTH_PREFIX_SIZE)
        if not length_bytes:
            self._has_next = False
            return None
        if len(length_bytes) < BINARY_MESSAGE_LENGTH_PREFIX_SIZE:
            raise RuntimeError("Truncated length header in binary trace file")

        (msg_len,) = struct.unpack("<I", length_bytes)
        if msg_len > MAX_EXPECTED_MESSAGE_SIZE:
            raise RuntimeError(f"Message size {msg_len} exceeds maximum expected size {MAX_EXPECTED_MESSAGE_SIZE}")

        data = self._file.read(msg_len)
        if len(data) < msg_len:
            raise RuntimeError(f"Truncated message body: expected {msg_len} bytes, got {len(data)}")

        message = self._message_class()
        try:
            message.ParseFromString(data)
        except Exception as e:
            raise RuntimeError(f"Failed to deserialize protobuf message ({msg_len} bytes): {e}") from e

        self._has_next = self._peek_has_data()
        return ReadResult(message=message, message_type=self._message_type)

    def has_next(self) -> bool:
        """Check if there are more messages to read."""
        return self._has_next

    def close(self) -> None:
        """Close the trace file."""
        if self._file is not None:
            self._file.close()
            self._file = None
        self._has_next = False

    @property
    def message_type(self) -> MessageType:
        """The message type being read."""
        return self._message_type

    def _peek_has_data(self) -> bool:
        """Check if there is more data without consuming it."""
        if self._file is None:
            return False
        pos = self._file.tell()
        data = self._file.read(1)
        if data:
            self._file.seek(pos)
            return True
        return False
