# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""MCAP channel helper for writing OSI messages to an external MCAP writer."""

from __future__ import annotations

import logging

import google.protobuf
from google.protobuf.message import EncodeError, Message
from mcap.well_known import MessageEncoding
from mcap.writer import Writer as McapRawWriter

from osi_utilities.timestamp import timestamp_to_nanoseconds
from osi_utilities.tracefile._mcap_utils import build_file_descriptor_set
from osi_utilities.tracefile._osi_version import ChannelOsiVersions

logger = logging.getLogger(__name__)


class MCAPChannel:
    """Helper for writing OSI messages to an external MCAP writer.

    Use this when you already have a ``mcap.writer.Writer`` instance and want
    to add OSI channels/messages to it. The caller writes the required
    ``net.asam.osi.trace`` file metadata itself.

    Usage::

        from mcap.writer import Writer
        with open("output.mcap", "wb") as f:
            writer = Writer(f)
            writer.start()
            channel = MCAPChannel(writer)
            channel.add_channel("ground_truth", GroundTruth)
            metadata = MCAPChannel.prepare_required_file_metadata()
            channel.fill_osi_version_range(metadata)
            writer.add_metadata("net.asam.osi.trace", metadata)
            channel.write_message(gt_msg, "ground_truth")
            writer.finish()
    """

    def __init__(self, mcap_writer: McapRawWriter) -> None:
        self._mcap_writer = mcap_writer
        self._channels: dict[str, int] = {}
        self._schema_cache: dict[str, int] = {}
        self._channel_osi_versions = ChannelOsiVersions()

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
            metadata: Optional channel metadata. ``net.asam.osi.trace.channel.osi_version``
                declares the OSI version of the channel's data; it defaults to the linked OSI
                library version.

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

        channel_meta = dict(metadata) if metadata else {}
        ChannelOsiVersions.apply_default_label(channel_meta)
        if "net.asam.osi.trace.channel.protobuf_version" not in channel_meta:
            channel_meta["net.asam.osi.trace.channel.protobuf_version"] = google.protobuf.__version__

        channel_id = self._mcap_writer.register_channel(
            topic=topic,
            message_encoding=MessageEncoding.Protobuf,
            schema_id=self._schema_cache[schema_name],
            metadata=channel_meta,
        )
        self._channels[topic] = channel_id
        self._channel_osi_versions.register(topic, channel_meta)
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
            self._channel_osi_versions.check_message(topic, message)
            return True
        except (OSError, EncodeError) as e:
            logger.error("Failed to write message: %s", e)
            return False

    def fill_osi_version_range(self, metadata: dict[str, str]) -> None:
        """Set ``min/max_osi_version`` in ``metadata`` to the range of the registered channels' OSI versions.

        Leaves ``metadata`` unchanged if no channel has a parseable OSI version label.
        """
        self._channel_osi_versions.fill_range(metadata)

    @staticmethod
    def prepare_required_file_metadata() -> dict[str, str]:
        """Prepare required net.asam.osi.trace metadata (min/max_osi_version default to the OSI library version)."""
        from osi_utilities.tracefile.writers.multi import prepare_required_file_metadata

        return prepare_required_file_metadata()
