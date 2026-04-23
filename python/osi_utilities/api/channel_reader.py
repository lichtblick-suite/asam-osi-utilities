# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Unified single-channel reader API across OSI trace container formats.

This module introduces a format-agnostic channel reader abstraction:
- Single-channel files (.osi, .txth) are exposed as one logical channel
- Multi-channel files (.mcap) are narrowed to one selected topic/channel

Error handling is normalized at this layer: underlying reader errors
(``RuntimeError`` from binary/text readers) are caught, logged, and
surfaced as end-of-stream (``None``).  Callers can inspect
:pyattr:`ChannelReader.read_error` after iteration to detect whether the
stream ended due to an error.
"""

from __future__ import annotations

import logging
from abc import ABC, abstractmethod
from collections.abc import Iterator

from google.protobuf.message import Message

from osi_utilities._types import MessageType, TraceFileFormat
from osi_utilities.api.types import ChannelSpecification
from osi_utilities.message_types import (
    message_type_to_class_name,
    require_message_type,
)
from osi_utilities.tracefile.configure import configure_reader, create_reader
from osi_utilities.tracefile.readers.base import TraceReader
from osi_utilities.tracefile.readers.multi import MultiTraceReader

logger = logging.getLogger(__name__)


class ChannelReader(ABC):
    """Read exactly one logical OSI channel from any supported trace format.

    Iteration is safe across all formats: errors in the underlying reader
    are caught and surfaced as end-of-stream (``read_message()`` returns
    ``None``).  After iteration, :pyattr:`read_error` reports the last
    error message, if any.
    """

    @abstractmethod
    def read_message(self) -> Message | None:
        """Read the next protobuf message from the selected channel.

        Returns ``None`` when no more messages are available **or** when
        an unrecoverable read error occurs.  In the latter case,
        :pyattr:`read_error` is set with a description of the failure.
        """

    @property
    @abstractmethod
    def read_error(self) -> str | None:
        """Error description from the last failed ``read_message()`` call.

        ``None`` when the last read succeeded or the reader has not been used yet.
        """

    @abstractmethod
    def get_channel_specification(self) -> ChannelSpecification:
        """Return the resolved channel specification in use by this reader."""

    @abstractmethod
    def close(self) -> None:
        """Close underlying resources."""

    def __enter__(self) -> ChannelReader:
        return self

    def __exit__(self, exc_type: object, exc_val: object, exc_tb: object) -> None:
        self.close()

    def __iter__(self) -> Iterator[Message]:
        return self

    def __next__(self) -> Message:
        message = self.read_message()
        if message is None:
            raise StopIteration
        return message


class _TraceFileChannelReader(ChannelReader):
    """Adapter exposing a selected channel on top of TraceReader implementations.

    Normalizes error handling: ``RuntimeError`` raised by single-channel and
    text-format readers is caught, logged at WARNING level, and converted to
    ``None`` (end-of-stream).  The error description is stored in
    :pyattr:`read_error` so callers can distinguish normal EOF from failure.
    """

    def __init__(
        self,
        reader: TraceReader,
        channel_spec: ChannelSpecification,
    ) -> None:
        self._reader = reader
        self._channel_spec = channel_spec
        self._read_error: str | None = None

    def read_message(self) -> Message | None:
        self._read_error = None
        try:
            result = self._reader.read_message()
        except RuntimeError as exc:
            self._read_error = str(exc)
            logger.warning("Read error in '%s': %s", self._channel_spec.path, exc)
            return None
        if result is None:
            return None
        return result.message

    @property
    def read_error(self) -> str | None:
        return self._read_error

    def close(self) -> None:
        self._reader.close()

    def get_channel_specification(self) -> ChannelSpecification:
        return ChannelSpecification(
            path=self._channel_spec.path,
            message_type=self._channel_spec.message_type,
            topic=self._channel_spec.topic,
            metadata=dict(self._channel_spec.metadata),
        )


def open_channel(channel_spec: ChannelSpecification) -> ChannelReader:
    """Open a single-channel reader from a channel specification.

    Behavior by trace format:
    - ``SINGLE_CHANNEL`` (``.osi``): opens the file as one logical channel.
      If ``message_type`` is missing, it is auto-detected from the filename when possible.
    - ``MULTI_CHANNEL`` (``.mcap``): resolves exactly one topic
      (explicit ``channel_spec.topic`` or the first available topic), validates
      optional ``channel_spec.message_type``, and narrows the underlying reader
      to that topic.

    Args:
        channel_spec: Requested channel specification.

    Returns:
        A ``ChannelReader`` bound to one logical channel.

    Raises:
        FileNotFoundError: If the trace file does not exist.
        ValueError: If no topics are available, topic/message type resolution fails,
            or the trace file format is unsupported.
        RuntimeError: If a multi-channel spec does not yield an MCAP reader.
    """

    if not channel_spec.exists():
        raise FileNotFoundError(f"OSI trace file '{channel_spec.path}' does not exist.")

    if channel_spec.trace_file_format == TraceFileFormat.SINGLE_CHANNEL:
        message_type: MessageType | None = None
        if channel_spec.message_type is not None:
            message_type = require_message_type(channel_spec.message_type)
        else:
            channel_spec = channel_spec.with_autodetected_message_type()
            if channel_spec.message_type is not None:
                message_type = require_message_type(channel_spec.message_type)

        reader = create_reader(channel_spec.path)
        if message_type is not None:
            channel_spec = channel_spec.with_message_type(message_type)
        configure_reader(reader, channel_spec)
        if not reader.open(channel_spec.path):
            raise RuntimeError(f"Failed to open trace file: {channel_spec.path}")
        return _TraceFileChannelReader(reader=reader, channel_spec=channel_spec)
    elif channel_spec.trace_file_format == TraceFileFormat.MULTI_CHANNEL:
        reader = create_reader(channel_spec.path)
        configure_reader(reader, channel_spec)
        if not reader.open(channel_spec.path):
            raise RuntimeError(f"Failed to open trace file: {channel_spec.path}")
        try:
            if not isinstance(reader, MultiTraceReader):
                raise RuntimeError(f"Expected MultiTraceReader for '{channel_spec.path}', got {type(reader).__name__}.")

            available_topics = reader.get_available_topics()
            if not available_topics:
                raise ValueError(f"No topics found in MCAP file '{channel_spec.path}'.")

            topic = channel_spec.topic or available_topics[0]
            if topic not in available_topics:
                raise ValueError(
                    f"Topic '{topic}' not found in MCAP file '{channel_spec.path}'. "
                    f"Available topics: {available_topics}"
                )

            detected_channel_spec = reader.get_channel_specification(topic)
            detected_message_type = detected_channel_spec.message_type if detected_channel_spec is not None else None
            if detected_channel_spec is None:
                raise ValueError(
                    f"Topic '{topic}' is not an OSI-compatible channel in MCAP file '{channel_spec.path}'."
                )
            if channel_spec.message_type is not None and channel_spec.message_type != detected_message_type:
                specified = message_type_to_class_name(channel_spec.message_type)
                detected = message_type_to_class_name(detected_message_type)
                raise ValueError(
                    f"Specified message type '{specified}' does not match detected message type '{detected}'."
                )

            resolved_topic = detected_channel_spec.topic
            if resolved_topic is None:
                raise RuntimeError(f"Resolved channel specification for '{channel_spec.path}' has no topic.")

            reader.set_topics([resolved_topic])
            detected_metadata = (
                dict(detected_channel_spec.metadata)
                if detected_channel_spec is not None and detected_channel_spec.metadata
                else dict(channel_spec.metadata)
            )
            resolved_spec = ChannelSpecification(
                path=channel_spec.path,
                message_type=detected_message_type,
                topic=resolved_topic,
                metadata=detected_metadata,
            )
            return _TraceFileChannelReader(reader=reader, channel_spec=resolved_spec)
        except Exception:
            reader.close()
            raise

    raise ValueError(f"Unsupported trace file format: {channel_spec.trace_file_format} for '{channel_spec.path}'.")
