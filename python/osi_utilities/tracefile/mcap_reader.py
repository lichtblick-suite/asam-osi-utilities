# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""MCAP trace file reader.

Reads OSI trace files in the MCAP container format with multi-channel support.
"""

from __future__ import annotations

import logging
from pathlib import Path
from typing import IO

from mcap.exceptions import McapError
from mcap.reader import make_reader
from mcap.well_known import MessageEncoding
from mcap_protobuf.decoder import DecoderFactory

from osi_utilities.tracefile._types import (
    MessageType,
    ReadResult,
    coerce_message_type,
    schema_name_to_message_type,
    get_message_class,
)
from osi_utilities.tracefile.reader import TraceFileReader

logger = logging.getLogger(__name__)


class MCAPTraceFileReader(TraceFileReader):
    """Reader for MCAP-format OSI trace files (.mcap).

    Supports multi-channel reading with topic-based filtering,
    schema-based message type detection, and metadata access.
    """

    def __init__(
        self,
        expected_message_type: MessageType | str | None = None,
        decoder_mode: str = "precompiled",
    ) -> None:
        self._file: IO[bytes] | None = None
        self._reader = None
        self._message_iterator = None
        self._summary = None
        self._silence_incompatible_topic_warnings = False
        self._topics: list[str] | None = None
        self._expected_message_type = coerce_message_type(expected_message_type)
        self._decoder_mode = "precompiled"
        self._set_decoder_mode(decoder_mode)

        self._decoder_factory = DecoderFactory()
        self._mcap_contained_decoders: dict[int, object] = {}
        self._precompiled_message_classes: dict[int, type] = {}

    def _set_decoder_mode(self, decoder_mode: str) -> None:
        mode = decoder_mode.lower().replace("_", "-")
        if mode not in {"precompiled", "mcap-contained"}:
            raise ValueError(
                f"Unsupported decoder_mode '{decoder_mode}'. "
                "Expected one of: precompiled, mcap-contained."
            )
        self._decoder_mode = mode

    def set_decoder_mode(self, decoder_mode: str) -> None:
        """Set protobuf decoding strategy.

        Supported values:
        - ``precompiled``: decode OSI schemas via precompiled ``osi3`` classes
        - ``mcap-contained``: decode via schema descriptor sets embedded in MCAP
        """
        self._set_decoder_mode(decoder_mode)

    def set_expected_message_type(
        self, expected_message_type: MessageType | str | None
    ) -> None:
        """Set optional expected message type for filtering.

        When set, only messages with this type are yielded.
        """
        self._expected_message_type = coerce_message_type(expected_message_type)

    def get_expected_message_type(self) -> MessageType | None:
        """Return the currently configured expected message type filter."""
        return self._expected_message_type

    def open(self, path: Path) -> bool:
        """Open an MCAP trace file.

        Args:
            path: Path to the .mcap file.

        Returns:
            True on success, False on failure.
        """
        try:
            self._file = open(path, "rb")  # noqa: SIM115
            if self._decoder_mode == "precompiled":
                self._reader = make_reader(self._file)
            else:
                self._reader = make_reader(
                    self._file, decoder_factories=[self._decoder_factory]
                )
            self._summary = self._reader.get_summary()
            self._mcap_contained_decoders.clear()
            self._precompiled_message_classes.clear()
            self._start_iteration()
            return True
        except (OSError, McapError) as e:
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

    def set_silence_incompatible_topic_warnings(self, silence: bool) -> None:
        """Configure whether to skip incompatible messages with or without warning.

        Args:
            silence: If True, silently skip messages with unrecognized schemas.
                     If False (default), they are skipped with a warning.
        """
        self._silence_incompatible_topic_warnings = silence

    def read_message(self) -> ReadResult | None:
        """Read the next message from the MCAP file.

        Returns:
            ReadResult on success, None if no more messages.
        """
        if self._message_iterator is None:
            return None

        while True:
            try:
                schema, channel, raw_message = next(self._message_iterator)
            except StopIteration:
                return None

            # Skip non-protobuf messages (e.g. JSON channels in mixed files)
            if channel.message_encoding != MessageEncoding.Protobuf:
                if not self._silence_incompatible_topic_warnings:
                    logger.warning(
                        "Skipping non-protobuf message on channel '%s' (encoding: %s)",
                        channel.topic,
                        channel.message_encoding,
                    )
                continue

            if schema is None:
                if not self._silence_incompatible_topic_warnings:
                    logger.warning(
                        "Skipping protobuf message without schema on channel '%s'.",
                        channel.topic,
                    )
                continue

            msg_type = schema_name_to_message_type(schema.name)
            if msg_type == MessageType.UNKNOWN:
                if not self._silence_incompatible_topic_warnings:
                    logger.warning(
                        "Skipping non-OSI message with schema '%s'", schema.name
                    )
                continue

            if (
                self._expected_message_type is not None
                and msg_type != self._expected_message_type
            ):
                if not self._silence_incompatible_topic_warnings:
                    logger.warning(
                        "Skipping message of type '%s' on channel '%s' due to message type mismatch (expected: '%s', actual: '%s')",
                        msg_type,
                        channel.topic,
                        self._expected_message_type,
                        msg_type,
                    )
                continue

            proto_msg = self._decode_message(
                topic=channel.topic,
                channel_id=raw_message.channel_id,
                message_encoding=channel.message_encoding,
                schema=schema,
                payload=raw_message.data,
                msg_type=msg_type,
            )
            if proto_msg is None:
                continue

            return ReadResult(
                message=proto_msg,
                message_type=msg_type,
                channel_name=channel.topic,
            )

    def _decode_message(
        self,
        *,
        topic: str,
        channel_id: int,
        message_encoding: str,
        schema: object,
        payload: bytes,
        msg_type: MessageType,
    ):
        if self._decoder_mode == "precompiled":
            message_class = self._precompiled_message_classes.get(channel_id)
            if message_class is None:
                try:
                    message_class = get_message_class(msg_type)
                except ValueError as exc:
                    if not self._silence_incompatible_topic_warnings:
                        logger.warning(
                            "Failed to resolve precompiled class for schema '%s': %s",
                            schema.name,
                            exc,
                        )
                    return None
                else:
                    self._precompiled_message_classes[channel_id] = message_class

            try:
                message = message_class()
                message.ParseFromString(payload)
                return message
            except (
                Exception
            ) as exc:  # noqa: BLE001 - protobuf parser raises broad exceptions
                logger.warning(
                    "Failed to decode precompiled protobuf message on channel '%s': %s",
                    topic,
                    exc,
                )
                return None

        if self._decoder_mode == "mcap-contained":
            decoder = self._mcap_contained_decoders.get(channel_id)
            if decoder is None:
                decoder = self._decoder_factory.decoder_for(message_encoding, schema)
                if decoder is None:
                    if not self._silence_incompatible_topic_warnings:
                        logger.warning(
                            "Skipping message with no MCAP-contained decoder for schema '%s'",
                            schema.name if schema else "None",
                        )
                    return None
                self._mcap_contained_decoders[channel_id] = decoder

            try:
                return decoder(payload)
            except Exception as exc:  # noqa: BLE001 - decoder raises broad exceptions
                logger.warning(
                    "Failed to decode MCAP-contained protobuf message on channel '%s': %s",
                    topic,
                    exc,
                )
                return None

        return None

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
        return [
            {"name": m.name, "data": dict(m.metadata)}
            for m in self._reader.iter_metadata()
        ]

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
                    msg_type = schema_name_to_message_type(schema.name)
                    return msg_type if msg_type is not MessageType.UNKNOWN else None
        return None

    def _start_iteration(self) -> None:
        """(Re)start the raw message iterator."""
        if self._reader is not None:
            self._message_iterator = iter(
                self._reader.iter_messages(topics=self._topics)
            )
