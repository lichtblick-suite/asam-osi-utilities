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

from osi_utilities._types import MessageType, ReadResult, ReadStatus
from osi_utilities.api.types import ChannelSpecification
from osi_utilities.message_types import (
    coerce_message_type,
    get_message_class,
    schema_name_to_message_type,
)
from osi_utilities.tracefile.readers.base import TraceReader

logger = logging.getLogger(__name__)


class MultiTraceReader(TraceReader):
    """Reader for multi-channel OSI trace files (.mcap).

    Supports multi-channel reading with topic-based filtering,
    schema-based message type detection, and metadata access.

    Incompatible message handling:

    - Controlled by ``set_skip_incompatible_messages(...)``.
    - If enabled (default), incompatible messages are skipped.
    - If disabled, incompatible messages are surfaced as
      ``ReadResult(status=ReadStatus.INCOMPATIBLE, ...)``.

    Logging behavior for incompatible messages is controlled independently by
    ``set_log_incompatible_messages(...)``.
    """

    def __init__(
        self,
        decoder_mode: str = "precompiled",
        skip_incompatible_messages: bool = True,
        log_incompatible_messages: bool = True,
    ) -> None:
        self._file: IO[bytes] | None = None
        self._path: Path | None = None
        self._reader = None
        self._message_iterator = None
        self._summary = None
        self._log_incompatible_messages = log_incompatible_messages
        self._skip_incompatible_messages = skip_incompatible_messages
        self._topics: list[str] | None = None
        self._topic_message_types: dict[str, MessageType] = {}
        self._decoder_mode = "precompiled"
        self._set_decoder_mode(decoder_mode)
        self._peeked_result: ReadResult | None = None
        self._peek_exhausted = False

        self._decoder_factory = DecoderFactory()
        self._mcap_contained_decoders: dict[int, object] = {}
        self._precompiled_message_classes: dict[int, type] = {}

    def _set_decoder_mode(self, decoder_mode: str) -> None:
        mode = decoder_mode.lower().replace("_", "-")
        if mode not in {"precompiled", "mcap-contained"}:
            raise ValueError(
                f"Unsupported decoder_mode '{decoder_mode}'. Expected one of: precompiled, mcap-contained."
            )
        self._decoder_mode = mode

    def set_decoder_mode(self, decoder_mode: str) -> None:
        """Set protobuf decoding strategy.

        Supported values:
        - ``precompiled``: decode OSI schemas via precompiled ``osi3`` classes
        - ``mcap-contained``: decode via schema descriptor sets embedded in MCAP
        """
        self._set_decoder_mode(decoder_mode)

    def open(self, path: Path) -> bool:
        """Open an MCAP trace file.

        Args:
            path: Path to the .mcap file.

        Returns:
            True on success, False on failure.
        """
        if self._file is not None:
            logger.error("Reader is already open. Call close() before re-opening.")
            return False

        try:
            self._file = open(path, "rb")  # noqa: SIM115
            self._path = path
            if self._decoder_mode == "precompiled":
                self._reader = make_reader(self._file)
            else:
                self._reader = make_reader(self._file, decoder_factories=[self._decoder_factory])
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

    def set_topic_message_types(self, topic_message_types: dict[str, MessageType | str | None]) -> None:
        """Set expected message types per topic.

        Args:
            topic_message_types: Mapping of topic name to expected message type.
                Entries with ``None`` remove expectations for that topic.
        """
        normalized: dict[str, MessageType] = {}
        for topic, message_type in topic_message_types.items():
            coerced = coerce_message_type(message_type)
            if coerced is not None:
                normalized[topic] = coerced
        self._topic_message_types = normalized

    def set_log_incompatible_messages(self, log_messages: bool) -> None:
        """Configure whether incompatible messages should be logged.

        Args:
            log_messages: If True (default), incompatible messages are logged.
                If False, incompatible messages are not logged.
        """
        self._log_incompatible_messages = log_messages

    def set_skip_incompatible_messages(self, skip: bool) -> None:
        """Configure whether incompatible messages should be skipped.

        Incompatible messages are non-matching encoding/schema/message-type cases.
        Actual decode/read errors are always surfaced as ``ReadStatus.ERROR`` and are
        never skipped by this setting.
        """
        self._skip_incompatible_messages = skip

    def read_message(self) -> ReadResult | None:
        """Read the next message from the MCAP file.

        Returns:
            ``None`` on EOF; otherwise a ``ReadResult`` with status:
            - ``ReadStatus.OK`` for a valid decoded message
            - ``ReadStatus.INCOMPATIBLE`` for incompatible messages when
              ``set_skip_incompatible_messages(False)`` is configured
            - ``ReadStatus.ERROR`` for runtime read/decode failures
        """
        if self._peeked_result is not None:
            result = self._peeked_result
            self._peeked_result = None
            self._peek_exhausted = False
            return result

        if self._peek_exhausted:
            return None

        if self._message_iterator is None:
            return None

        while True:
            try:
                schema, channel, raw_message = next(self._message_iterator)
            except StopIteration:
                self._message_iterator = None
                return None
            except Exception as exc:  # noqa: BLE001 - upstream reader may raise broad exceptions
                self._message_iterator = None
                return ReadResult(
                    message=None,
                    message_type=MessageType.UNKNOWN,
                    status=ReadStatus.ERROR,
                    error_message=f"Failed while reading MCAP message stream: {exc}",
                )

            # Skip non-protobuf messages (e.g. JSON channels in mixed files)
            if channel.message_encoding != MessageEncoding.Protobuf:
                msg = f"Non-protobuf message on channel '{channel.topic}' (encoding: {channel.message_encoding})"
                result = self._handle_incompatible(
                    channel_name=channel.topic,
                    message_type=MessageType.UNKNOWN,
                    error_message=msg,
                )
                if result is None:
                    continue
                return result

            if schema is None:
                result = self._handle_incompatible(
                    channel_name=channel.topic,
                    message_type=MessageType.UNKNOWN,
                    error_message=f"Protobuf message without schema on channel '{channel.topic}'.",
                )
                if result is None:
                    continue
                return result

            msg_type = schema_name_to_message_type(schema.name)
            if msg_type == MessageType.UNKNOWN:
                result = self._handle_incompatible(
                    channel_name=channel.topic,
                    message_type=MessageType.UNKNOWN,
                    error_message=f"Non-OSI message with schema '{schema.name}' on channel '{channel.topic}'.",
                )
                if result is None:
                    continue
                return result

            expected_message_type = self._topic_message_types.get(channel.topic)
            if expected_message_type is not None and msg_type != expected_message_type:
                msg = (
                    f"Message type mismatch on channel '{channel.topic}'"
                    f" (expected: '{expected_message_type}', actual: '{msg_type}')."
                )
                result = self._handle_incompatible(
                    channel_name=channel.topic,
                    message_type=msg_type,
                    error_message=msg,
                )
                if result is None:
                    continue
                return result

            decode_result = self._decode_message(
                topic=channel.topic,
                channel_id=raw_message.channel_id,
                message_encoding=channel.message_encoding,
                schema=schema,
                payload=raw_message.data,
                msg_type=msg_type,
            )
            if decode_result is None:
                continue
            if decode_result.status == ReadStatus.INCOMPATIBLE:
                result = self._handle_incompatible(
                    channel_name=channel.topic,
                    message_type=msg_type,
                    error_message=decode_result.error_message,
                )
                if result is None:
                    continue
                return result
            if decode_result.status == ReadStatus.ERROR:
                return ReadResult(
                    message=None,
                    message_type=msg_type,
                    channel_name=channel.topic,
                    status=ReadStatus.ERROR,
                    error_message=decode_result.error_message,
                )

            return ReadResult(
                message=decode_result.message,
                message_type=msg_type,
                channel_name=channel.topic,
                status=ReadStatus.OK,
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
                    return ReadResult(
                        message=None,
                        message_type=msg_type,
                        status=ReadStatus.ERROR,
                        error_message=f"Failed to resolve precompiled class for schema '{schema.name}': {exc}",
                    )
                else:
                    self._precompiled_message_classes[channel_id] = message_class

            try:
                message = message_class()
                message.ParseFromString(payload)
                return ReadResult(message=message, message_type=msg_type, status=ReadStatus.OK)
            except Exception as exc:  # noqa: BLE001 - protobuf parser raises broad exceptions
                return ReadResult(
                    message=None,
                    message_type=msg_type,
                    status=ReadStatus.ERROR,
                    error_message=f"Failed to decode precompiled protobuf message on channel '{topic}': {exc}",
                )

        if self._decoder_mode == "mcap-contained":
            decoder = self._mcap_contained_decoders.get(channel_id)
            if decoder is None:
                decoder = self._decoder_factory.decoder_for(message_encoding, schema)
                if decoder is None:
                    schema_name = schema.name if schema else "None"
                    return ReadResult(
                        message=None,
                        message_type=msg_type,
                        status=ReadStatus.INCOMPATIBLE,
                        error_message=f"No MCAP-contained decoder for schema '{schema_name}'.",
                    )
                self._mcap_contained_decoders[channel_id] = decoder

            try:
                return ReadResult(
                    message=decoder(payload),
                    message_type=msg_type,
                    status=ReadStatus.OK,
                )
            except Exception as exc:  # noqa: BLE001 - decoder raises broad exceptions
                return ReadResult(
                    message=None,
                    message_type=msg_type,
                    status=ReadStatus.ERROR,
                    error_message=f"Failed to decode MCAP-contained protobuf message on channel '{topic}': {exc}",
                )

        return None

    def _handle_incompatible(
        self,
        *,
        channel_name: str,
        message_type: MessageType,
        error_message: str,
    ) -> ReadResult | None:
        if self._log_incompatible_messages:
            logger.warning("%s", error_message)
        if self._skip_incompatible_messages:
            return None
        return ReadResult(
            message=None,
            message_type=message_type,
            channel_name=channel_name,
            status=ReadStatus.INCOMPATIBLE,
            error_message=error_message,
        )

    def has_next(self) -> bool:
        """Check if there are more messages to read.

        Peeks ahead in the iterator to give an accurate answer.
        """
        if self._peeked_result is not None:
            return True
        if self._peek_exhausted:
            return False
        peeked = self.read_message()
        if peeked is None:
            self._peek_exhausted = True
            return False
        self._peeked_result = peeked
        return True

    def close(self) -> None:
        """Close the MCAP file."""
        self._message_iterator = None
        self._peeked_result = None
        self._peek_exhausted = False
        self._summary = None
        self._reader = None
        self._path = None
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

    def get_channel_specification(self, topic: str) -> ChannelSpecification | None:
        """Return ChannelSpecification for a specific OSI-compatible topic.

        Returns None when the topic is missing or not a supported OSI schema.
        """
        # TODO: Optionally enforce required OSI channel metadata keys during
        #       channel validation (e.g. osi_version, protobuf_version).
        if self._summary is None or self._path is None:
            return None

        for channel in self._summary.channels.values():
            if channel.topic != topic:
                continue
            schema = self._summary.schemas.get(channel.schema_id)
            if schema is None:
                return None
            message_type = schema_name_to_message_type(schema.name)
            if message_type is MessageType.UNKNOWN:
                return None
            return ChannelSpecification(
                path=self._path,
                message_type=message_type,
                topic=channel.topic,
                metadata=dict(channel.metadata),
            )
        return None

    def list_channel_specifications(self) -> list[ChannelSpecification]:
        """Return ChannelSpecification objects for all OSI-compatible topics."""
        # TODO: Optionally enforce required OSI channel metadata keys during
        #       channel validation (e.g. osi_version, protobuf_version).
        if self._summary is None or self._path is None:
            return []
        channel_specs: list[ChannelSpecification] = []
        for channel in self._summary.channels.values():
            schema = self._summary.schemas.get(channel.schema_id)
            if schema is None:
                continue
            message_type = schema_name_to_message_type(schema.name)
            if message_type is MessageType.UNKNOWN:
                continue
            channel_specs.append(
                ChannelSpecification(
                    path=self._path,
                    message_type=message_type,
                    topic=channel.topic,
                    metadata=dict(channel.metadata),
                )
            )
        return channel_specs

    def get_message_type_for_topic(self, topic: str) -> MessageType | None:
        """Return the MessageType for a given topic based on its schema."""
        channel_spec = self.get_channel_specification(topic)
        return channel_spec.message_type if channel_spec is not None else None

    def get_channel_metadata(self, topic: str) -> dict[str, str] | None:
        """Return metadata for a specific channel/topic."""
        if self._summary is None:
            return None
        for channel in self._summary.channels.values():
            if channel.topic == topic:
                return dict(channel.metadata)
        return None

    def _start_iteration(self) -> None:
        """(Re)start the raw message iterator."""
        self._peeked_result = None
        self._peek_exhausted = False
        if self._reader is not None:
            self._message_iterator = iter(self._reader.iter_messages(topics=self._topics))
