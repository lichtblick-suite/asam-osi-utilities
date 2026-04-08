# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""MCAP channel helper for writing OSI messages to an external MCAP writer."""

from __future__ import annotations

import logging

import google.protobuf
from google.protobuf.message import EncodeError, Message
from mcap.well_known import MessageEncoding
from mcap.writer import Writer as McapRawWriter

from osi_utilities.tracefile._mcap_utils import build_file_descriptor_set
from osi_utilities.timestamp import timestamp_to_nanoseconds

logger = logging.getLogger(__name__)


class MCAPChannel:
    """Helper for writing OSI messages to an external MCAP writer.

    Use this when you already have a ``mcap.writer.Writer`` instance and want
    to add OSI channels/messages to it.

    Usage::

        from mcap.writer import Writer
        with open("output.mcap", "wb") as f:
            writer = Writer(f)
            writer.start()
            channel = MCAPChannel(writer)
            channel.add_channel("ground_truth", GroundTruth)
            channel.write_message(gt_msg, "ground_truth")
            writer.finish()
    """

    def __init__(self, mcap_writer: McapRawWriter) -> None:
        self._mcap_writer = mcap_writer
        self._channels: dict[str, int] = {}
        self._schema_cache: dict[str, int] = {}

    def add_channel(
        self,
        topic: str,
        message_class: type[Message],
        metadata: dict[str, str] | None = None,
    ) -> int:
        """Register an OSI channel with the external writer.

        Args:
            topic: Channel topic name.
            message_class: The protobuf message class.
            metadata: Optional channel metadata.

        Returns:
            The channel ID.
        """
        if topic in self._channels:
            raise RuntimeError(f"Channel '{topic}' already registered")

        schema_name = f"osi3.{message_class.DESCRIPTOR.name}"
        if schema_name not in self._schema_cache:
            fds = build_file_descriptor_set(message_class)
            schema_id = self._mcap_writer.register_schema(
                name=schema_name,
                encoding=MessageEncoding.Protobuf,
                data=fds.SerializeToString(),
            )
            self._schema_cache[schema_name] = schema_id

        channel_meta = metadata or {}
        if "net.asam.osi.trace.channel.protobuf_version" not in channel_meta:
            channel_meta["net.asam.osi.trace.channel.protobuf_version"] = google.protobuf.__version__

        channel_id = self._mcap_writer.register_channel(
            topic=topic,
            message_encoding=MessageEncoding.Protobuf,
            schema_id=self._schema_cache[schema_name],
            metadata=channel_meta,
        )
        self._channels[topic] = channel_id
        return channel_id

    def write_message(self, message: Message, topic: str) -> bool:
        """Write an OSI message to a registered channel.

        Args:
            message: The protobuf message.
            topic: The channel topic.

        Returns:
            True on success.
        """
        if topic not in self._channels:
            logger.error("Topic '%s' not registered", topic)
            return False

        try:
            data = message.SerializeToString()
            log_time = timestamp_to_nanoseconds(message)
            self._mcap_writer.add_message(
                channel_id=self._channels[topic],
                log_time=log_time,
                data=data,
                publish_time=log_time,
            )
            return True
        except (OSError, EncodeError) as e:
            logger.error("Failed to write message: %s", e)
            return False

    @staticmethod
    def prepare_required_file_metadata() -> dict[str, str]:
        """Prepare required net.asam.osi.trace metadata."""
        from osi_utilities.tracefile.writers.multi import prepare_required_file_metadata

        return prepare_required_file_metadata()
