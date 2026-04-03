# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Abstract base class for trace file readers and factory for reader creation."""

from __future__ import annotations

import logging
from abc import ABC, abstractmethod
from collections.abc import Iterator
from pathlib import Path

from osi_utilities.tracefile._types import MessageType, ReadResult

logger = logging.getLogger(__name__)


class TraceReader(ABC):
    """Abstract base class for reading OSI trace files.

    Supports context manager protocol and iteration.

    Usage::

        with TraceReaderFactory.create_reader("trace.mcap") as reader:
            for result in reader:
                print(result.message_type, result.message)
    """

    @abstractmethod
    def open(self, path: Path) -> bool:
        """Open a trace file for reading.

        Args:
            path: Path to the trace file.

        Returns:
            True if the file was opened successfully, False otherwise.
        """

    @abstractmethod
    def read_message(self) -> ReadResult | None:
        """Read the next message from the trace file.

        Returns:
            A ReadResult containing the deserialized message, or None if no more messages.

        Raises:
            RuntimeError: If deserialization fails.
        """

    @abstractmethod
    def has_next(self) -> bool:
        """Check if there are more messages to read."""

    @abstractmethod
    def close(self) -> None:
        """Close the trace file and release resources."""

    def __enter__(self) -> TraceReader:
        return self

    def __exit__(self, exc_type: object, exc_val: object, exc_tb: object) -> None:
        self.close()

    def __iter__(self) -> Iterator[ReadResult]:
        return self

    def __next__(self) -> ReadResult:
        result = self.read_message()
        if result is None:
            raise StopIteration
        return result


class TraceReaderFactory:
    """Factory for creating trace file readers based on file extension."""

    @staticmethod
    def create_reader(
        path: str | Path,
        *,
        message_type: MessageType | None = None,
    ) -> TraceReader:
        """Create a trace file reader appropriate for the given file.

        Args:
            path: Path to the trace file. Extension determines the reader type.
            message_type: Explicit message type for binary/txth files. If
                provided, overrides filename-based inference. Validated against
                the overrides filename-based inference. Validated against the
                schema given in MCAP files.

        Returns:
            An opened TraceReader instance.

        Raises:
            ValueError: If the file extension is not supported. RuntimeError: If
            the file cannot be opened.
        """
        path = Path(path)
        suffix = path.suffix.lower()

        if suffix == ".mcap":
            from osi_utilities.tracefile.mcap_reader import MultiTraceReader

            reader = MultiTraceReader(message_type=message_type)
        elif suffix == ".osi":
            from osi_utilities.tracefile.binary_reader import SingleTraceReader

            if message_type is not None:
                reader = SingleTraceReader(message_type=message_type)
            else:
                reader = SingleTraceReader()
        elif suffix == ".txth":
            logger.warning("The .txth format is not reliably deserializable. Use .osi or .mcap instead.")

            from osi_utilities.tracefile.txth_reader import ProtobufTextFormatTraceReader

            reader = ProtobufTextFormatTraceReader(message_type=message_type)
        else:
            raise ValueError(f"Unsupported trace file extension: '{suffix}'")

        if not reader.open(path):
            raise RuntimeError(f"Failed to open trace file: {path}")
        return reader


def open_trace_file(
    path: str | Path,
    *,
    message_type: MessageType | None = None,
) -> TraceReader:
    """Convenience function to open a trace file for reading.

    Equivalent to ``TraceReaderFactory.create_reader(path)``.

    Args:
        path: Path to the trace file.
        message_type: Explicit message type for binary/txth files.

    Returns:
        An opened TraceReader instance (use as context manager).
    """
    return TraceReaderFactory.create_reader(path, message_type=message_type)
