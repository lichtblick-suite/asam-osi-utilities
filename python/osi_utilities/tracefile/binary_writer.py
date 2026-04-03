# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Binary (.osi) trace file writer.

Writes OSI trace files in the single-channel binary format where each message
is prefixed with a 4-byte little-endian uint32 length.
"""

from __future__ import annotations

import logging
import struct
from pathlib import Path
from typing import IO

from google.protobuf.message import EncodeError, Message

from osi_utilities.tracefile.writer import TraceWriter

logger = logging.getLogger(__name__)


class SingleTraceWriter(TraceWriter):
    """Writer for single-channel binary OSI trace files (.osi).

    Each message is stored as: [4-byte LE length][serialized protobuf bytes]
    """

    def __init__(self) -> None:
        self._file: IO[bytes] | None = None
        self._path: Path | None = None
        self._written_count = 0

    def open(self, path: Path) -> bool:
        """Open a binary .osi trace file for writing.

        Args:
            path: Path to the output file. Must have .osi extension.

        Returns:
            True on success, False on failure.
        """
        if self._file is not None:
            logger.error("Opening file '%s', writer has already a file opened", path)
            return False

        if path.suffix.lower() != ".osi":
            logger.error("Binary trace files must have .osi extension, got '%s'", path.suffix)
            return False

        try:
            self._file = open(path, "wb")  # noqa: SIM115
            self._path = path
            self._written_count = 0
            return True
        except OSError as e:
            logger.error("Failed to open file '%s' for writing: %s", path, e)
            return False

    def write_message(self, message: Message, topic: str = "") -> bool:
        """Write a protobuf message to the binary trace file.

        Args:
            message: The protobuf message to serialize and write.
            topic: Ignored for single-channel binary files.

        Returns:
            True on success, False on failure.
        """
        if self._file is None:
            logger.error("Writer is not open")
            return False

        try:
            data = message.SerializeToString()
            self._file.write(struct.pack("<I", len(data)))
            self._file.write(data)
            self._written_count += 1
            return True
        except (OSError, EncodeError) as e:
            logger.error("Failed to write message: %s", e)
            return False

    def close(self) -> None:
        """Close the trace file."""
        if self._file is not None:
            self._file.close()
            logger.info("Wrote %d messages to '%s'", self._written_count, self._path)
            self._file = None

    @property
    def written_count(self) -> int:
        """Number of messages written so far."""
        return self._written_count
