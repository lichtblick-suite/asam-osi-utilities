# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Unified single-channel reader API across OSI trace container formats.

This module introduces a format-agnostic channel reader abstraction:
- Single-channel files (.osi, .txth) are exposed as one logical channel
- Multi-channel files (.mcap) are narrowed to one selected topic/channel
"""

from __future__ import annotations

from abc import ABC, abstractmethod
from collections.abc import Iterator
from pathlib import Path

from google.protobuf.message import Message

from osi_utilities.tracefile._types import (
    ChannelSpecification,
    MessageType,
    TraceFileFormat,
    message_type_to_class_name,
    require_message_type,
)
from osi_utilities.tracefile.mcap_reader import MCAPTraceFileReader
from osi_utilities.tracefile.reader import TraceFileReader, TraceFileReaderFactory


class ChannelReader(ABC):
    """Read exactly one logical OSI channel from any supported trace format."""

    @abstractmethod
    def read_message(self) -> Message | None:
        """Read the next protobuf message from the selected channel."""

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
    """Adapter exposing a selected channel on top of TraceFileReader implementations."""

    def __init__(
        self,
        reader: TraceFileReader,
        channel_spec: ChannelSpecification,
    ) -> None:
        self._reader = reader
        self._channel_spec = channel_spec

    def read_message(self) -> Message | None:
        result = self._reader.read_message()
        if result is None:
            return None
        return result.message

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
    """Open a unified single-channel reader from a ChannelSpecification.

    For .mcap files, selects a topic (specified topic or first available topic).
    For single-channel files, resolves a synthetic topic name if none is provided.
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

        reader = TraceFileReaderFactory.create_reader(
            channel_spec.path, message_type=message_type
        )
        return _TraceFileChannelReader(reader=reader, channel_spec=channel_spec)
    elif channel_spec.trace_file_format == TraceFileFormat.MULTI_CHANNEL:
        reader = TraceFileReaderFactory.create_reader(channel_spec.path)
        if not isinstance(reader, MCAPTraceFileReader):
            reader.close()
            raise RuntimeError(
                f"Expected MCAPTraceFileReader for '{channel_spec.path}', got {type(reader).__name__}."
            )

        available_topics = reader.get_available_topics()
        if not available_topics:
            reader.close()
            raise ValueError(f"No topics found in MCAP file '{channel_spec.path}'.")

        topic = channel_spec.topic or available_topics[0]
        if topic not in available_topics:
            reader.close()
            raise ValueError(
                f"Topic '{topic}' not found in MCAP file '{channel_spec.path}'. Available topics: {available_topics}"
            )

        detected_message_type = reader.get_message_type_for_topic(topic)
        if (
            channel_spec.message_type is not None
            and channel_spec.message_type != detected_message_type
        ):
            reader.close()
            raise ValueError(
                f"Specified message type '{message_type_to_class_name(channel_spec.message_type)}' does not match detected message type "
                f"'{message_type_to_class_name(detected_message_type)}'."
            )

        reader.set_topics([topic])
        resolved_spec = ChannelSpecification(
            path=channel_spec.path,
            message_type=detected_message_type,
            topic=topic,
            metadata=dict(reader.get_channel_metadata(topic) or channel_spec.metadata),
        )
        return _TraceFileChannelReader(reader=reader, channel_spec=resolved_spec)

    raise ValueError(
        f"Unsupported trace file format: {channel_spec.trace_file_format} for '{channel_spec.path}'."
    )
