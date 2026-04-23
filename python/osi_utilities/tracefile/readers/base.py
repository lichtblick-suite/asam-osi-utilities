# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Abstract base class for trace file readers and factory for reader creation."""

from __future__ import annotations

import logging
from abc import ABC, abstractmethod
from collections.abc import Iterator
from pathlib import Path

from osi_utilities._types import ReadResult, ReadStatus

logger = logging.getLogger(__name__)


class TraceReader(ABC):
    """Abstract base class for reading OSI trace files.

    Supports context manager protocol and iteration.

    Read contract:
    - ``read_message()`` returns ``None`` on EOF.
    - ``read_message()`` returns ``ReadResult(status=ReadStatus.OK, ...)`` for valid messages.
    - ``read_message()`` returns ``ReadResult(status=ReadStatus.INCOMPATIBLE, ...)`` for
      incompatible schema/encoding/message-type cases.
    - ``read_message()`` returns ``ReadResult(status=ReadStatus.ERROR, ...)`` for actual
      runtime read/decode/parse failures.

    Iterator contract (``for ... in reader``):
    - raises ``ValueError`` for ``ReadStatus.INCOMPATIBLE``
    - raises ``RuntimeError`` for ``ReadStatus.ERROR``
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
            A ``ReadResult`` for message/status outcomes, or ``None`` if no more messages.
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
        if result.status == ReadStatus.INCOMPATIBLE:
            raise ValueError(result.error_message or "Trace reader encountered an incompatible message.")
        if result.status == ReadStatus.ERROR:
            raise RuntimeError(result.error_message or "Trace reader encountered an error.")
        return result
