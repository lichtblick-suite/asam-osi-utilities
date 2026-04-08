# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Abstract base class for trace file writers."""

from __future__ import annotations

import logging
from abc import ABC, abstractmethod
from pathlib import Path

from google.protobuf.message import Message

logger = logging.getLogger(__name__)


class TraceWriter(ABC):
    """Abstract base class for writing OSI trace files.

    Supports context manager protocol.

    Usage::

        with SingleTraceWriter() as writer:
            writer.open(Path("output.osi"))
            writer.write_message(ground_truth_msg)
    """

    @abstractmethod
    def open(self, path: Path) -> bool:
        """Open a trace file for writing.

        Args:
            path: Path to the output file.

        Returns:
            True if the file was opened successfully, False otherwise.
        """

    @abstractmethod
    def write_message(self, message: Message, topic: str = "") -> bool:
        """Write a protobuf message to the trace file.

        Args:
            message: The protobuf message to write.
            topic: Channel topic name (used by MCAP writer, ignored by single-channel writers).

        Returns:
            True if the message was written successfully, False otherwise.
        """

    @abstractmethod
    def close(self) -> None:
        """Close the trace file and finalize writing."""

    def __enter__(self) -> TraceWriter:
        return self

    def __exit__(self, exc_type: object, exc_val: object, exc_tb: object) -> None:
        self.close()
