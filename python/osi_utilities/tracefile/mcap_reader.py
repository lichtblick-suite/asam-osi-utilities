# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""MCAP trace file reader.

Reads OSI trace files in the MCAP container format with multi-channel support.
"""

from __future__ import annotations

import logging
from pathlib import Path
from typing import IO

from mcap.reader import make_reader
from mcap_protobuf.decoder import DecoderFactory

from osi_utilities.tracefile._types import (
    SCHEMA_NAME_TO_MESSAGE_TYPE,
    MessageType,
    ReadResult,
)
from osi_utilities.tracefile.reader import TraceFileReader

logger = logging.getLogger(__name__)


class MCAPTraceFileReader(TraceFileReader):
    """Reader for MCAP-format OSI trace files (.mcap).

    Supports multi-channel reading with topic-based filtering,
    schema-based message type detection, and metadata access.
    """

    def __init__(self) -> None:
        self._file: IO[bytes] | None = None
        self._reader = None
        self._message_iterator = None
        self._summary = None
        self._skip_non_osi_msgs = False
        self._next_result: ReadResult | None = None
        self._topics: list[str] | None = None

    def open(self, path: Path) -> bool:
        """Open an MCAP trace file.

        Args:
            path: Path to the .mcap file.

        Returns:
            True on success, False on failure.
        """
        try:
            self._file = open(path, "rb")  # noqa: SIM115
            self._reader = make_reader(self._file, decoder_factories=[DecoderFactory()])
            self._summary = self._reader.get_summary()
            self._start_iteration()
            return True
        except Exception as e:
            logger.error("Failed to open MCAP file '%s': %s", path, e)
            if self._file is not None:
                self._file.close()
                self._file = None
            return False

    def set_topics(self, topics: list[str]) -> None:
        """Filter reading to specific topics.

        Args:
            topics: List of topic names to read. If empty, reads all topics.
        """
        self._topics = topics if topics else None
        self._start_iteration()

    def set_skip_non_osi_msgs(self, skip: bool) -> None:
        """Configure whether to skip non-OSI messages.

        Args:
            skip: If True, silently skip messages with unrecognized schemas.
                  If False (default), they are skipped with a warning.
        """
        self._skip_non_osi_msgs = skip

    def read_message(self) -> ReadResult | None:
        """Read the next message from the MCAP file.

        Returns:
            ReadResult on success, None if no more messages.
        """
        if self._message_iterator is None:
            return None

        while True:
            try:
                schema, channel, _message, proto_msg = next(self._message_iterator)
            except StopIteration:
                return None

            msg_type = SCHEMA_NAME_TO_MESSAGE_TYPE.get(schema.name, MessageType.UNKNOWN)
            if msg_type == MessageType.UNKNOWN:
                if not self._skip_non_osi_msgs:
                    logger.warning("Skipping non-OSI message with schema '%s'", schema.name)
                continue

            return ReadResult(
                message=proto_msg,
                message_type=msg_type,
                channel_name=channel.topic,
            )

    def has_next(self) -> bool:
        """Check if there are more messages.

        Note: May return True even if only non-OSI messages remain.
        """
        return self._message_iterator is not None

    def close(self) -> None:
        """Close the MCAP file."""
        self._message_iterator = None
        self._summary = None
        self._reader = None
        if self._file is not None:
            self._file.close()
            self._file = None

    def get_available_topics(self) -> list[str]:
        """Return list of available topic names in the file."""
        if self._summary is None:
            return []
        return [channel.topic for channel in self._summary.channels.values()]

    def get_file_metadata(self) -> list[dict]:
        """Return file-level metadata entries."""
        if self._reader is None:
            return []
        return [{"name": m.name, "data": dict(m.metadata)} for m in self._reader.iter_metadata()]

    def get_channel_metadata(self, topic: str) -> dict[str, str] | None:
        """Return metadata for a specific channel/topic."""
        if self._summary is None:
            return None
        for channel in self._summary.channels.values():
            if channel.topic == topic:
                return dict(channel.metadata)
        return None

    def get_message_type_for_topic(self, topic: str) -> MessageType | None:
        """Return the MessageType for a given topic based on its schema."""
        if self._summary is None:
            return None
        for channel in self._summary.channels.values():
            if channel.topic == topic:
                schema = self._summary.schemas.get(channel.schema_id)
                if schema is not None:
                    return SCHEMA_NAME_TO_MESSAGE_TYPE.get(schema.name)
        return None

    def _start_iteration(self) -> None:
        """(Re)start the decoded message iterator."""
        if self._reader is not None:
            self._message_iterator = iter(self._reader.iter_decoded_messages(topics=self._topics))
