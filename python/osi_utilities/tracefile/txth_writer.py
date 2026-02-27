# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Text human-readable (.txth) trace file writer.

Writes OSI trace files in Google protobuf TextFormat.
"""

from __future__ import annotations

import logging
from pathlib import Path
from typing import IO

from google.protobuf import text_format
from google.protobuf.message import Message

from osi_utilities.tracefile.writer import TraceFileWriter

logger = logging.getLogger(__name__)


class TXTHTraceFileWriter(TraceFileWriter):
    """Writer for text human-readable OSI trace files (.txth).

    Messages are stored in Google protobuf TextFormat, one after another.
    """

    def __init__(self) -> None:
        self._file: IO[str] | None = None
        self._path: Path | None = None
        self._written_count = 0

    def open(self, path: Path) -> bool:
        """Open a .txth trace file for writing.

        Args:
            path: Path to the output file. Must have .txth extension.

        Returns:
            True on success, False on failure.
        """
        if path.suffix.lower() != ".txth":
            logger.error("Text trace files must have .txth extension, got '%s'", path.suffix)
            return False

        try:
            self._file = open(path, "w", encoding="utf-8")  # noqa: SIM115
            self._path = path
            self._written_count = 0
            return True
        except OSError as e:
            logger.error("Failed to open file '%s' for writing: %s", path, e)
            return False

    def write_message(self, message: Message, topic: str = "") -> bool:
        """Write a protobuf message in TextFormat.

        Args:
            message: The protobuf message to write.
            topic: Ignored for single-channel text files.

        Returns:
            True on success, False on failure.
        """
        if self._file is None:
            logger.error("Writer is not open")
            return False

        try:
            text = text_format.MessageToString(message)
            self._file.write(text)
            self._written_count += 1
            return True
        except Exception as e:
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
        return self._written_count
