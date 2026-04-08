# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Unified single-channel writer API across OSI trace container formats.

This module introduces a format-agnostic channel writer abstraction:
- Single-channel files (.osi, .txth, .osi.xz) are exposed as one logical channel
- Multi-channel files (.mcap) are written to one selected topic/channel
"""

from __future__ import annotations

import logging
from abc import ABC, abstractmethod

import google.protobuf
from google.protobuf.message import Message

from osi_utilities._types import MessageType, TraceFileFormat
from osi_utilities.api.types import ChannelSpecification
from osi_utilities.message_types import require_message_type
from osi_utilities.tracefile._config import OSI_CHANNEL_OSI_VERSION_KEY, OSI_CHANNEL_PROTOBUF_VERSION_KEY
from osi_utilities.tracefile.writers.multi import (
    MultiTraceWriter,
    prepare_required_file_metadata,
)
from osi_utilities.tracefile.writers.single import SingleTraceWriter
from osi_utilities.tracefile.writers.textformat import ProtobufTextFormatTraceWriter

logger = logging.getLogger(__name__)


class ChannelWriter(ABC):
    """Write exactly one logical OSI channel to any supported trace format."""

    @abstractmethod
    def write_message(self, message: Message) -> bool:
        """Write one protobuf message to the selected channel."""

    @abstractmethod
    def get_channel_specification(self) -> ChannelSpecification:
        """Return the resolved channel specification in use by this writer."""

    @abstractmethod
    def close(self) -> None:
        """Close underlying resources."""

    def __enter__(self) -> ChannelWriter:
        return self

    def __exit__(self, exc_type: object, exc_val: object, exc_tb: object) -> None:
        self.close()


class _TraceFileChannelWriter(ChannelWriter):
    """Adapter exposing one logical channel on top of trace writer implementations."""

    def __init__(
        self,
        writer: SingleTraceWriter | MultiTraceWriter | ProtobufTextFormatTraceWriter,
        channel_spec: ChannelSpecification,
    ) -> None:
        self._writer = writer
        self._channel_spec = channel_spec
        self._channel_metadata = dict(channel_spec.metadata)
        self._channel_registered = False

    def _ensure_message_type(self, message: Message) -> None:
        actual_message_type = require_message_type(message.DESCRIPTOR.name)

        if self._channel_spec.message_type is None:
            self._channel_spec = self._channel_spec.with_message_type(actual_message_type)
            return

        configured_message_type = require_message_type(self._channel_spec.message_type)
        if configured_message_type != actual_message_type:
            raise ValueError(
                f"Configured message type '{configured_message_type.value}' does not "
                f"match protobuf message type '{actual_message_type.value}'."
            )

    def _ensure_mcap_channel(self, message: Message) -> None:
        if not isinstance(self._writer, MultiTraceWriter) or self._channel_registered:
            return

        if self._channel_spec.topic is None:
            raise RuntimeError("Resolved MCAP channel specification has no topic.")

        if hasattr(message, "version"):
            message_version = (
                f"{message.version.version_major}."
                f"{message.version.version_minor}."
                f"{message.version.version_patch}"
            )
            configured_version = self._channel_metadata.get(OSI_CHANNEL_OSI_VERSION_KEY)
            if configured_version is None:
                self._channel_metadata[OSI_CHANNEL_OSI_VERSION_KEY] = message_version
            elif configured_version != message_version:
                raise ValueError(
                    f"Configured OSI version '{configured_version}' does not match "
                    f"message version '{message_version}'."
                )

        configured_pb_version = self._channel_metadata.get(OSI_CHANNEL_PROTOBUF_VERSION_KEY)
        if configured_pb_version is None:
            self._channel_metadata[OSI_CHANNEL_PROTOBUF_VERSION_KEY] = google.protobuf.__version__
        elif configured_pb_version != google.protobuf.__version__:
            logger.warning(
                "Configured protobuf version '%s' does not match installed protobuf version '%s'.",
                configured_pb_version,
                google.protobuf.__version__,
            )

        self._writer.add_channel(
            self._channel_spec.topic,
            type(message),
            self._channel_metadata,
        )
        self._channel_registered = True

    def write_message(self, message: Message) -> bool:
        self._ensure_message_type(message)
        self._ensure_mcap_channel(message)
        success = self._writer.write_message(message, self._channel_spec.topic or "")
        if not success:
            raise RuntimeError(
                f"Failed to write message to '{self._channel_spec.path}'."
            )
        return True

    def write_message(self, message: Message, topic: str = "") -> bool:
        return self.write_message(message)

    def close(self) -> None:
        self._writer.close()

    def get_channel_specification(self) -> ChannelSpecification:
        return ChannelSpecification(
            path=self._channel_spec.path,
            message_type=self._channel_spec.message_type,
            topic=self._channel_spec.topic,
            metadata=dict(self._channel_metadata),
        )


def open_channel_writer(channel_spec: ChannelSpecification) -> ChannelWriter:
    """Open a single-channel writer from a channel specification.

    Behavior by trace format:
    - ``SINGLE_CHANNEL`` (``.osi``, ``.txth``, ``.osi.xz``): writes one logical channel.
      If ``message_type`` is missing, it is inferred from the first written message.
    - ``MULTI_CHANNEL`` (``.mcap``): resolves one topic/channel and registers it lazily
      on first write with message class and channel metadata.

    Args:
        channel_spec: Requested channel specification.

    Returns:
        A ``ChannelWriter`` bound to one logical channel.

    Raises:
        ValueError: If format/suffix is unsupported or writer creation fails.
        RuntimeError: If underlying writer cannot be opened.
    """

    suffixes = "".join(channel_spec.path.suffixes).lower()

    if channel_spec.trace_file_format == TraceFileFormat.SINGLE_CHANNEL:

        if channel_spec.path.suffix.lower() == ".osi":
            writer = SingleTraceWriter()
            if not writer.open(channel_spec.path):
                raise RuntimeError(f"Failed to open trace file for writing: {channel_spec.path}")
            resolved_spec = ChannelSpecification(
                path=channel_spec.path,
                message_type=channel_spec.message_type,
                topic=channel_spec.topic or "",
                metadata=dict(channel_spec.metadata),
            )
            return _TraceFileChannelWriter(writer=writer, channel_spec=resolved_spec)

        if channel_spec.path.suffix.lower() == ".txth":
            writer = ProtobufTextFormatTraceWriter()
            if not writer.open(channel_spec.path):
                raise RuntimeError(f"Failed to open trace file for writing: {channel_spec.path}")
            resolved_spec = ChannelSpecification(
                path=channel_spec.path,
                message_type=channel_spec.message_type,
                topic=channel_spec.topic or "",
                metadata=dict(channel_spec.metadata),
            )
            return _TraceFileChannelWriter(writer=writer, channel_spec=resolved_spec)

        raise ValueError(
            f"Unsupported single-channel trace file extension for writing: '{suffixes}'."
        )

    if channel_spec.trace_file_format == TraceFileFormat.MULTI_CHANNEL:
        message_type: MessageType | None = None
        if channel_spec.message_type is not None:
            message_type = require_message_type(channel_spec.message_type)

        writer = MultiTraceWriter()
        if not writer.open(channel_spec.path, prepare_required_file_metadata()):
            raise RuntimeError(f"Failed to open trace file for writing: {channel_spec.path}")

        resolved_spec = ChannelSpecification(
            path=channel_spec.path,
            message_type=message_type,
            topic=channel_spec.topic or channel_spec.path.stem,
            metadata=dict(channel_spec.metadata),
        )
        return _TraceFileChannelWriter(writer=writer, channel_spec=resolved_spec)

    raise ValueError(
        f"Unsupported trace file format: {channel_spec.trace_file_format} for '{channel_spec.path}'."
    )
