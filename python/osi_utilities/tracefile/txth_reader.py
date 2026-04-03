# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Text human-readable (.txth) trace file reader.

Reads OSI trace files in Google protobuf TextFormat where messages
are separated by a known top-level field boundary.
"""

from __future__ import annotations

import logging
from pathlib import Path

from google.protobuf import text_format

from osi_utilities.tracefile._types import (
    MessageType,
    ReadResult,
    get_message_class,
    infer_message_type_from_filename,
)
from osi_utilities.tracefile.reader import TraceReader

logger = logging.getLogger(__name__)


class ProtobufTextFormatTraceReader(TraceReader):
    """
    .. deprecated::
    The `.txth` text format is not reliably deserializable. The OSI
    specification states that it is "not unambiguously deserializable" —
    protobuf text format output is not guaranteed to be stable across
    library versions, field ordering may change, and float/double precision
    varies. Round-tripping (write then read) can silently lose data. Prefer
    `.osi` (binary) for single-channel or `.mcap` for multi-channel trace
    files.

    Reader for text human-readable OSI trace files (.txth).

    Messages are stored in Google protobuf TextFormat. Each message is
    delimited by reading until the text can be parsed as a complete message.

    See the
    `Protocol Buffers Text Format Language Specification <https://protobuf.dev/reference/protobuf/textformat-spec/>`_.
    """

    def __init__(self, message_type: MessageType = MessageType.UNKNOWN) -> None:
        self._message_type = message_type
        self._message_class: type | None = None
        self._has_next = False
        self._buffer = ""

    def open(self, path: Path) -> bool:
        """Open a .txth trace file.

        Args:
            path: Path to the .txth file.

        Returns:
            True on success, False on failure.
        """
        if self._message_type == MessageType.UNKNOWN:
            self._message_type = infer_message_type_from_filename(path.name)

        if self._message_type == MessageType.UNKNOWN:
            logger.error(
                "Cannot determine message type for '%s'. Specify it explicitly.", path
            )
            return False

        try:
            self._message_class = get_message_class(self._message_type)
        except ValueError as e:
            logger.error("Failed to get message class: %s", e)
            return False

        try:
            self._buffer = Path(path).read_text(encoding="utf-8")
        except OSError as e:
            logger.error("Failed to open file '%s': %s", path, e)
            return False
        self._has_next = len(self._buffer.strip()) > 0
        return True

    def read_message(self) -> ReadResult | None:
        """Read the next message from the text trace file.

        Returns:
            ReadResult on success, None if no more messages.

        Raises:
            RuntimeError: If parsing fails.
        """
        if self._message_class is None or not self._buffer.strip():
            self._has_next = False
            return None

        # Try to parse the entire remaining buffer as one message.
        # If it contains multiple messages, we parse greedily: TextFormat
        # will consume as much as it can for one message.
        message = self._message_class()
        try:
            text_format.Parse(self._buffer, message)
            self._buffer = ""
            self._has_next = False
            return ReadResult(message=message, message_type=self._message_type)
        except text_format.ParseError:
            # If full buffer fails, the file may have multiple concatenated messages.
            # Try splitting on the first top-level field name appearing again.
            logger.debug(
                "Buffer contains multiple messages, splitting at field boundary."
            )

        # Multi-message: find the boundary by looking for a repeated top-level field
        # The C++ implementation reads line by line and tries parsing.
        lines = self._buffer.split("\n")
        if not lines:
            self._has_next = False
            return None

        # Detect the first top-level field name (not indented, contains ':' or '{')
        first_field = None
        for line in lines:
            stripped = line.strip()
            if stripped and not stripped.startswith("#"):
                # Extract field name
                if ":" in stripped:
                    first_field = stripped.split(":")[0].strip()
                elif "{" in stripped:
                    first_field = stripped.split("{")[0].strip()
                break

        if first_field is None:
            self._has_next = False
            return None

        # Find the next occurrence of this field after the first message
        split_idx = None
        in_first_message = True
        for i, line in enumerate(lines):
            if i == 0:
                continue
            stripped = line.strip()
            if stripped.startswith(first_field) and not line[0].isspace():
                # This is a new top-level field with the same name = new message boundary
                if in_first_message:
                    split_idx = i
                    break

        if split_idx is not None:
            msg_text = "\n".join(lines[:split_idx])
            self._buffer = "\n".join(lines[split_idx:])
        else:
            msg_text = self._buffer
            self._buffer = ""

        message = self._message_class()
        try:
            text_format.Parse(msg_text, message)
        except text_format.ParseError as e:
            raise RuntimeError(f"Failed to parse text format message: {e}") from e

        self._has_next = len(self._buffer.strip()) > 0
        return ReadResult(message=message, message_type=self._message_type)

    def has_next(self) -> bool:
        return self._has_next

    def close(self) -> None:
        self._buffer = ""
        self._has_next = False

    @property
    def message_type(self) -> MessageType:
        return self._message_type
