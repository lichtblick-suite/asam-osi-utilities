# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

from __future__ import annotations

from collections.abc import Mapping
from dataclasses import dataclass, field
from pathlib import Path
from types import MappingProxyType

from osi_utilities._types import MessageType, TraceFileFormat


@dataclass(frozen=True)
class ChannelSpecification:
    """Immutable specification for an OSI channel within a trace file."""

    #: Path to the trace file.
    path: Path
    #: OSI message type (normalized to MessageType if provided).
    message_type: MessageType | str | None = None
    #: Channel topic name (MCAP only).
    topic: str | None = None
    #: Additional channel metadata.
    metadata: Mapping[str, str] = field(default_factory=dict)

    def __post_init__(self) -> None:
        """Normalize runtime input values to canonical internal types."""
        from osi_utilities.message_types import coerce_message_type

        object.__setattr__(self, "path", Path(self.path))
        object.__setattr__(self, "message_type", coerce_message_type(self.message_type))
        object.__setattr__(
            self,
            "metadata",
            MappingProxyType(dict(self.metadata or {})),  # Ensure metadata is immutable and non-None
        )

    @property
    def trace_file_format(self) -> TraceFileFormat:
        """Determine the trace file format from the file extension."""
        from osi_utilities.tracefile.format import get_trace_file_format

        return get_trace_file_format(self.path)

    @property
    def autodetected_message_type(self) -> MessageType | None:
        """Best-effort inferred message type from filename, without mutating this instance."""
        from osi_utilities.filename import infer_message_type_from_filename, parse_osi_trace_filename

        parsed = parse_osi_trace_filename(self.path.name)
        detected = parsed.get("message_type")
        if detected is not None:
            return detected

        msg_type = infer_message_type_from_filename(self.path.name)
        if msg_type != MessageType.UNKNOWN:
            return msg_type
        return None

    def with_autodetected_message_type(self) -> ChannelSpecification:
        """Return a specification with auto-detected message type when possible.

        If message_type is already set or cannot be inferred, returns self unchanged.
        """
        if self.message_type is not None:
            return self

        detected = self.autodetected_message_type
        if detected is None:
            return self
        return self.with_message_type(detected)

    def with_autofilled_topic(self) -> ChannelSpecification:
        """Return a specification with topic set to filename stem if missing."""
        if self.topic is None:
            return self.with_topic(self.path.stem)
        return self

    def exists(self) -> bool:
        """Check if the file at path exists."""
        return self.path.exists() and self.path.is_file()

    def with_message_type(self, message_type: MessageType | str) -> ChannelSpecification:
        """Return a new ChannelSpecification with a different message type."""
        return ChannelSpecification(
            path=self.path,
            message_type=message_type,
            topic=self.topic,
            metadata=dict(self.metadata),
        )

    def with_topic(self, topic: str) -> ChannelSpecification:
        """Return a new ChannelSpecification with a different topic."""
        return ChannelSpecification(
            path=self.path,
            message_type=self.message_type,
            topic=topic,
            metadata=dict(self.metadata),
        )

    def with_path(self, path: Path | str) -> ChannelSpecification:
        """Return a new ChannelSpecification with a different path."""
        return ChannelSpecification(
            path=path,
            message_type=self.message_type,
            topic=self.topic,
            metadata=dict(self.metadata),
        )

    def with_metadata(self, metadata: Mapping[str, str]) -> ChannelSpecification:
        """Return a new ChannelSpecification with replaced metadata."""
        return ChannelSpecification(
            path=self.path,
            message_type=self.message_type,
            topic=self.topic,
            metadata=dict(metadata),
        )

    def with_metadata_updates(self, updates: Mapping[str, str]) -> ChannelSpecification:
        """Return a new ChannelSpecification with metadata updated by key."""
        merged = dict(self.metadata)
        merged.update(dict(updates))
        return ChannelSpecification(
            path=self.path,
            message_type=self.message_type,
            topic=self.topic,
            metadata=merged,
        )

    def with_metadata_entry(self, key: str, value: str) -> ChannelSpecification:
        """Return a new ChannelSpecification with one metadata key updated."""
        return self.with_metadata_updates({key: value})

    def with_trace_file_format(self, format: TraceFileFormat) -> ChannelSpecification:
        """Return a new ChannelSpecification with a different file extension."""
        from osi_utilities.tracefile.format import _FORMAT_TO_EXTENSION

        return ChannelSpecification(
            path=self.path.with_suffix(_FORMAT_TO_EXTENSION[format]),
            message_type=self.message_type,
            topic=self.topic,
            metadata=dict(self.metadata),
        )
