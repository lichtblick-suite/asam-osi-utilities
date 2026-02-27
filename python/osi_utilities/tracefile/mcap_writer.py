# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""MCAP trace file writer with OSI-compliant metadata support.

Writes OSI trace files in the MCAP container format with multi-channel support,
schema registration via FileDescriptorSet, and OSI metadata validation.
"""

from __future__ import annotations

import logging
from datetime import datetime, timezone
from pathlib import Path
from typing import IO

import google.protobuf
from google.protobuf.descriptor import FileDescriptor
from google.protobuf.descriptor_pb2 import FileDescriptorSet
from google.protobuf.message import Message
from mcap.well_known import MessageEncoding
from mcap.writer import Writer as McapRawWriter

from osi_utilities.tracefile._config import (
    NANOSECONDS_PER_SECOND,
    OSI_CHANNEL_RECOMMENDED_METADATA_KEYS,
    OSI_CHANNEL_REQUIRED_METADATA_KEYS,
    OSI_TRACE_METADATA_NAME,
    OSI_TRACE_RECOMMENDED_METADATA_KEYS,
    OSI_TRACE_REQUIRED_METADATA_KEYS,
)
from osi_utilities.tracefile.writer import TraceFileWriter

logger = logging.getLogger(__name__)


def _get_package_version() -> str:
    """Get the package version from importlib.metadata, falling back to vcpkg.json."""
    try:
        from importlib.metadata import version

        return version("asam-osi-utilities")
    except Exception:
        return "0.0.0"


def _build_file_descriptor_set(message_class: type[Message]) -> FileDescriptorSet:
    """Build a FileDescriptorSet for a protobuf message class, including all dependencies."""
    fds = FileDescriptorSet()
    seen: set[str] = set()

    def _append(fd: FileDescriptor) -> None:
        for dep in fd.dependencies:
            if dep.name not in seen:
                seen.add(dep.name)
                _append(dep)
        fd.CopyToProto(fds.file.add())

    _append(message_class.DESCRIPTOR.file)
    return fds


def prepare_required_file_metadata() -> dict[str, str]:
    """Prepare the required 'net.asam.osi.trace' metadata with default values.

    Returns a dict with required keys populated with placeholder/current values.
    """
    return {
        "version": _get_package_version(),
        "min_osi_version": "",
        "max_osi_version": "",
        "min_protobuf_version": google.protobuf.__version__,
        "max_protobuf_version": google.protobuf.__version__,
        "creation_time": datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
    }


def _validate_file_metadata(metadata: dict[str, str]) -> None:
    """Validate net.asam.osi.trace metadata completeness."""
    missing_required = OSI_TRACE_REQUIRED_METADATA_KEYS - metadata.keys()
    missing_recommended = OSI_TRACE_RECOMMENDED_METADATA_KEYS - metadata.keys()
    if missing_required:
        logger.warning("Missing required 'net.asam.osi.trace' metadata: %s", ", ".join(missing_required))
    if missing_recommended:
        logger.info("Missing recommended 'net.asam.osi.trace' metadata: %s", ", ".join(missing_recommended))


def _validate_channel_metadata(metadata: dict[str, str]) -> None:
    """Validate net.asam.osi.trace.channel metadata completeness."""
    missing_required = OSI_CHANNEL_REQUIRED_METADATA_KEYS - metadata.keys()
    missing_recommended = OSI_CHANNEL_RECOMMENDED_METADATA_KEYS - metadata.keys()
    if missing_required:
        logger.warning("Missing required channel metadata: %s", ", ".join(missing_required))
    if missing_recommended:
        logger.info("Missing recommended channel metadata: %s", ", ".join(missing_recommended))


class MCAPTraceFileWriter(TraceFileWriter):
    """Writer for MCAP-format OSI trace files (.mcap).

    Supports multi-channel writing with schema registration,
    OSI-compliant file/channel metadata, and FileDescriptorSet-based schemas.
    """

    def __init__(self) -> None:
        self._file: IO[bytes] | None = None
        self._mcap_writer: McapRawWriter | None = None
        self._path: Path | None = None
        self._active_channels: dict[str, int] = {}  # topic -> channel_id
        self._channel_metadata: dict[str, dict[str, str]] = {}
        self._schema_cache: dict[str, int] = {}  # schema_name -> schema_id
        self._written_count = 0

    def open(self, path: Path, metadata: dict[str, str] | None = None) -> bool:
        """Open an MCAP file for writing.

        Args:
            path: Path to the output file. Must have .mcap extension.
            metadata: Optional net.asam.osi.trace file metadata. If None, default metadata is used.

        Returns:
            True on success, False on failure.
        """
        if path.suffix.lower() != ".mcap":
            logger.error("MCAP files must have .mcap extension, got '%s'", path.suffix)
            return False

        try:
            self._file = open(path, "wb")  # noqa: SIM115
            self._mcap_writer = McapRawWriter(self._file)
            self._mcap_writer.start(library="osi-utilities-python")
            self._path = path

            file_metadata = metadata if metadata is not None else prepare_required_file_metadata()
            _validate_file_metadata(file_metadata)
            self._mcap_writer.add_metadata(name=OSI_TRACE_METADATA_NAME, data=file_metadata)

            self._written_count = 0
            return True
        except Exception as e:
            logger.error("Failed to open MCAP file '%s' for writing: %s", path, e)
            if self._file is not None:
                self._file.close()
                self._file = None
            return False

    def add_channel(
        self,
        topic: str,
        message_class: type[Message],
        metadata: dict[str, str] | None = None,
    ) -> int:
        """Register an OSI channel with schema.

        Args:
            topic: Channel topic name.
            message_class: The protobuf message class for this channel.
            metadata: Optional channel metadata dict.

        Returns:
            The channel ID.

        Raises:
            RuntimeError: If writer is not open or topic already exists.
        """
        if self._mcap_writer is None:
            raise RuntimeError("Writer is not open")
        if topic in self._active_channels:
            raise RuntimeError(f"Channel with topic '{topic}' already exists")

        channel_meta = metadata if metadata is not None else {}
        _validate_channel_metadata(channel_meta)

        # Auto-fill protobuf version if not set
        if "net.asam.osi.trace.channel.protobuf_version" not in channel_meta:
            channel_meta["net.asam.osi.trace.channel.protobuf_version"] = google.protobuf.__version__

        schema_name = f"osi3.{message_class.DESCRIPTOR.name}"

        # Reuse schema if already registered
        if schema_name not in self._schema_cache:
            fds = _build_file_descriptor_set(message_class)
            schema_id = self._mcap_writer.register_schema(
                name=schema_name,
                encoding=MessageEncoding.Protobuf,
                data=fds.SerializeToString(),
            )
            self._schema_cache[schema_name] = schema_id

        channel_id = self._mcap_writer.register_channel(
            topic=topic,
            message_encoding=MessageEncoding.Protobuf,
            schema_id=self._schema_cache[schema_name],
            metadata=channel_meta,
        )
        self._active_channels[topic] = channel_id
        self._channel_metadata[topic] = channel_meta
        return channel_id

    def write_message(self, message: Message, topic: str = "") -> bool:
        """Write a protobuf message to the specified topic channel.

        If no channels are registered, auto-creates one using the message type.

        Args:
            message: The protobuf message to write.
            topic: The channel topic. If empty and only one channel exists, uses that channel.

        Returns:
            True on success, False on failure.
        """
        if self._mcap_writer is None:
            logger.error("Writer is not open")
            return False

        # Auto-create channel if none exist
        if not self._active_channels:
            auto_topic = topic or message.DESCRIPTOR.name
            self.add_channel(auto_topic, type(message))
            topic = auto_topic

        # Default to single channel if topic not specified
        if not topic and len(self._active_channels) == 1:
            topic = next(iter(self._active_channels))

        if topic not in self._active_channels:
            logger.error("Topic '%s' not found. Available: %s", topic, list(self._active_channels.keys()))
            return False

        try:
            data = message.SerializeToString()
            log_time = self._extract_timestamp_ns(message)
            self._mcap_writer.add_message(
                channel_id=self._active_channels[topic],
                log_time=log_time,
                data=data,
                publish_time=log_time,
            )
            self._written_count += 1
            return True
        except Exception as e:
            logger.error("Failed to write message to topic '%s': %s", topic, e)
            return False

    def add_file_metadata(self, name: str, data: dict[str, str]) -> bool:
        """Add additional file-level metadata.

        Args:
            name: Metadata entry name.
            data: Key-value metadata pairs.

        Returns:
            True on success.
        """
        if self._mcap_writer is None:
            return False
        self._mcap_writer.add_metadata(name=name, data=data)
        return True

    def close(self) -> None:
        """Finalize and close the MCAP file."""
        if self._mcap_writer is not None:
            self._mcap_writer.finish()
            logger.info(
                "Wrote %d messages to channels [%s] in '%s'",
                self._written_count,
                ", ".join(self._active_channels.keys()),
                self._path,
            )
            self._mcap_writer = None
        if self._file is not None:
            self._file.close()
            self._file = None
        self._active_channels.clear()
        self._channel_metadata.clear()
        self._schema_cache.clear()

    @property
    def written_count(self) -> int:
        return self._written_count

    @staticmethod
    def _extract_timestamp_ns(message: Message) -> int:
        """Extract timestamp in nanoseconds from an OSI message."""
        if hasattr(message, "timestamp"):
            ts = message.timestamp
            return int(ts.seconds * NANOSECONDS_PER_SECOND + ts.nanos)
        return 0
