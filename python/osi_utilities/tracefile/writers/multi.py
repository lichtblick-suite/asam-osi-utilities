# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""MCAP trace file writer with OSI-compliant metadata support.

Writes OSI trace files in the MCAP container format with multi-channel support,
schema registration via FileDescriptorSet, and OSI metadata validation.
"""

from __future__ import annotations

import logging
import re
from datetime import datetime, timezone
from pathlib import Path
from typing import IO

import google.protobuf
from google.protobuf.message import EncodeError, Message
from mcap.exceptions import McapError
from mcap.well_known import MessageEncoding
from mcap.writer import CompressionType
from mcap.writer import Writer as McapRawWriter

from osi_utilities.timestamp import timestamp_to_nanoseconds
from osi_utilities.tracefile._config import (
    DEFAULT_CHUNK_SIZE,
    MAX_CHUNK_SIZE,
    MIN_CHUNK_SIZE,
    OSI_CHANNEL_RECOMMENDED_METADATA_KEYS,
    OSI_CHANNEL_REQUIRED_METADATA_KEYS,
    OSI_TRACE_FILE_SPEC_VERSION,
    OSI_TRACE_METADATA_NAME,
    OSI_TRACE_RECOMMENDED_METADATA_KEYS,
    OSI_TRACE_REQUIRED_METADATA_KEYS,
)
from osi_utilities.tracefile._mcap_utils import build_file_descriptor_set
from osi_utilities.tracefile.writers.base import TraceWriter

logger = logging.getLogger(__name__)

_COMPRESSION_MAP: dict[str, CompressionType] = {
    "none": CompressionType.NONE,
    "lz4": CompressionType.LZ4,
    "zstd": CompressionType.ZSTD,
}


_SEMVER_CORE_RE = re.compile(r"^\s*(\d+)\.(\d+)\.(\d+)")


def _normalize_osi_version_string(version: str) -> str:
    """Return the strict ``major.minor.patch`` core of a version string.

    The OSI trace metadata version fields must be ``major.minor.patch``; any
    pre-release/build suffix (e.g. ``"3.8.0-rc1"`` -> ``"3.8.0"``) is stripped so the
    emitted value stays spec-conformant across release-candidate cycles. The input is
    returned unchanged if it does not start with a numeric ``major.minor.patch``.
    """
    match = _SEMVER_CORE_RE.match(version)
    return f"{match.group(1)}.{match.group(2)}.{match.group(3)}" if match else version


def _get_osi_library_version() -> str | None:
    """Best-effort OSI interface (schema) version from the linked ``osi3`` library.

    Used only as a fallback for ``min_osi_version`` / ``max_osi_version`` when no written
    message carries an embedded ``InterfaceVersion``. Returns ``None`` if ``osi3`` is not
    importable (the import is intentionally lazy so the writer does not hard-depend on it).
    """
    try:
        from osi3 import osi_version_pb2
    except (ImportError, ModuleNotFoundError):
        logger.warning("osi3 is not importable; cannot determine a fallback OSI version for trace metadata")
        return None
    interface_version = osi_version_pb2.DESCRIPTOR.GetOptions().Extensions[osi_version_pb2.current_interface_version]
    return f"{interface_version.version_major}.{interface_version.version_minor}.{interface_version.version_patch}"


def _get_message_osi_version(message: Message) -> tuple[int, int, int] | None:
    """Extract the embedded OSI ``InterfaceVersion`` from a top-level OSI message.

    Returns ``(major, minor, patch)`` if the message carries a populated, OSI-shaped
    ``version`` field, otherwise ``None`` (no ``version`` field, it is not a singular
    ``InterfaceVersion`` submessage, it is unset, or it is the meaningless all-zero
    default). Mirrors the C++ ``GetMessageOsiVersion`` guards so that writing a non-OSI
    message that merely happens to carry a differently-shaped ``version`` field cannot
    crash the writer.
    """
    field = message.DESCRIPTOR.fields_by_name.get("version")
    if field is None or field.is_repeated or field.message_type is None:
        return None
    try:
        if not message.HasField("version"):
            return None
    except ValueError:
        return None
    version = message.version
    components = (
        getattr(version, "version_major", 0),
        getattr(version, "version_minor", 0),
        getattr(version, "version_patch", 0),
    )
    if not all(isinstance(component, int) for component in components):
        return None
    return None if components == (0, 0, 0) else components


def prepare_required_file_metadata() -> dict[str, str]:
    """Prepare the required 'net.asam.osi.trace' metadata with default values.

    ``version`` is the OSI trace-file *format* version implemented by this library
    (:data:`OSI_TRACE_FILE_SPEC_VERSION`, normalized to ``major.minor.patch``).
    ``min_osi_version`` / ``max_osi_version`` are intentionally left empty here: when the
    metadata is written by :class:`MultiTraceWriter`, they are filled from the embedded
    ``InterfaceVersion`` of the messages actually written (falling back to the linked OSI
    library version). Callers using an external writer may fill them in themselves.
    """
    return {
        "version": _normalize_osi_version_string(OSI_TRACE_FILE_SPEC_VERSION),
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


class MultiTraceWriter(TraceWriter):
    """Writer for multi-channel OSI trace files (.mcap).

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
        # The net.asam.osi.trace record is buffered at open() and written at close(),
        # once the OSI schema version(s) of the written messages are known.
        self._pending_file_metadata: dict[str, str] | None = None
        self._osi_version_min: tuple[int, int, int] | None = None
        self._osi_version_max: tuple[int, int, int] | None = None

    def open(  # type: ignore[override]
        self,
        path: Path,
        metadata: dict[str, str] | None = None,
        *,
        compression: str | None = None,
        chunk_size: int | None = None,
    ) -> bool:
        """Open an MCAP file for writing.

        Args:
            path: Path to the output file. Must have .mcap extension.
            metadata: Optional net.asam.osi.trace file metadata. If None, default metadata is used.
            compression: Compression algorithm — ``"none"``, ``"lz4"``, or ``"zstd"``.
                If *None*, the mcap library default is used.
            chunk_size: Chunk size in bytes. Must be between ``MIN_CHUNK_SIZE`` and ``MAX_CHUNK_SIZE``.
                If *None*, ``DEFAULT_CHUNK_SIZE`` is used.

        Returns:
            True on success, False on failure.
        """
        if self._file is not None:
            logger.error("Opening file '%s', writer has already a file opened", path)
            return False

        if path.suffix.lower() != ".mcap":
            logger.error("MCAP files must have .mcap extension, got '%s'", path.suffix)
            return False

        if compression is not None:
            compression_lower = compression.lower()
            if compression_lower not in _COMPRESSION_MAP:
                logger.error(
                    "Invalid compression '%s'. Must be one of: %s",
                    compression,
                    ", ".join(_COMPRESSION_MAP),
                )
                return False
            mcap_compression = _COMPRESSION_MAP[compression_lower]
        else:
            mcap_compression = None

        effective_chunk_size = chunk_size if chunk_size is not None else DEFAULT_CHUNK_SIZE
        if effective_chunk_size < MIN_CHUNK_SIZE or effective_chunk_size > MAX_CHUNK_SIZE:
            logger.error(
                "chunk_size %d out of range [%d, %d]",
                effective_chunk_size,
                MIN_CHUNK_SIZE,
                MAX_CHUNK_SIZE,
            )
            return False

        try:
            self._file = open(path, "wb")  # noqa: SIM115
            writer_kwargs: dict[str, object] = {"chunk_size": effective_chunk_size}
            if mcap_compression is not None:
                writer_kwargs["compression"] = mcap_compression
            self._mcap_writer = McapRawWriter(self._file, **writer_kwargs)  # type: ignore[arg-type]
            self._mcap_writer.start(library="osi-utilities-python")
            self._path = path

            file_metadata = metadata if metadata is not None else prepare_required_file_metadata()
            _validate_file_metadata(file_metadata)
            # Defer writing the net.asam.osi.trace record until close(): min/max_osi_version
            # are filled from the embedded InterfaceVersion of the messages actually written.
            self._pending_file_metadata = dict(file_metadata)
            # "version" is the OSI trace-file format version and must be major.minor.patch;
            # normalize any caller-supplied pre-release/build suffix (e.g. "3.8.0-rc1" -> "3.8.0").
            if self._pending_file_metadata.get("version"):
                self._pending_file_metadata["version"] = _normalize_osi_version_string(
                    self._pending_file_metadata["version"]
                )
            self._osi_version_min = None
            self._osi_version_max = None

            self._written_count = 0
            return True
        except (OSError, McapError) as e:
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

        channel_meta = dict(metadata) if metadata is not None else {}
        _validate_channel_metadata(channel_meta)

        # Auto-fill protobuf version if not set
        if "net.asam.osi.trace.channel.protobuf_version" not in channel_meta:
            channel_meta["net.asam.osi.trace.channel.protobuf_version"] = google.protobuf.__version__

        schema_name = f"osi3.{message_class.DESCRIPTOR.name}"

        # Reuse schema if already registered
        if schema_name not in self._schema_cache:
            fds = build_file_descriptor_set(message_class)
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
            log_time = timestamp_to_nanoseconds(message)
            self._mcap_writer.add_message(
                channel_id=self._active_channels[topic],
                log_time=log_time,
                data=data,
                publish_time=log_time,
            )
            self._track_osi_version(message)
            self._written_count += 1
            return True
        except (OSError, EncodeError, McapError) as e:
            logger.error("Failed to write message to topic '%s': %s", topic, e)
            return False

    def _track_osi_version(self, message: Message) -> None:
        """Update the running min/max embedded OSI ``InterfaceVersion`` from a message."""
        version = _get_message_osi_version(message)
        if version is None:
            return
        if self._osi_version_min is None or version < self._osi_version_min:
            self._osi_version_min = version
        if self._osi_version_max is None or version > self._osi_version_max:
            self._osi_version_max = version

    def _finalize_file_metadata(self) -> None:
        """Write the buffered net.asam.osi.trace record, filling min/max_osi_version.

        ``min_osi_version`` / ``max_osi_version`` are derived from the embedded
        ``InterfaceVersion`` of the written messages, falling back to the linked OSI
        library version when no message carried one. Non-empty user-provided values are
        respected and never overwritten.
        """
        if self._mcap_writer is None or self._pending_file_metadata is None:
            return
        metadata = self._pending_file_metadata
        min_str = "{}.{}.{}".format(*self._osi_version_min) if self._osi_version_min is not None else None
        max_str = "{}.{}.{}".format(*self._osi_version_max) if self._osi_version_max is not None else None
        if min_str is None or max_str is None:
            fallback = _get_osi_library_version()
            if fallback is not None:
                min_str = min_str or fallback
                max_str = max_str or fallback
        # Only fill fields the caller left empty; respect explicit user values.
        if not metadata.get("min_osi_version") and min_str:
            metadata["min_osi_version"] = min_str
        if not metadata.get("max_osi_version") and max_str:
            metadata["max_osi_version"] = max_str
        self._mcap_writer.add_metadata(name=OSI_TRACE_METADATA_NAME, data=metadata)
        self._pending_file_metadata = None

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
        try:
            if self._mcap_writer is not None:
                self._finalize_file_metadata()
                self._mcap_writer.finish()
                logger.info(
                    "Wrote %d messages to channels [%s] in '%s'",
                    self._written_count,
                    ", ".join(self._active_channels.keys()),
                    self._path,
                )
        finally:
            self._mcap_writer = None
            if self._file is not None:
                try:
                    self._file.close()
                except OSError:
                    logger.debug("Error closing MCAP file handle", exc_info=True)
                self._file = None
            self._active_channels.clear()
            self._channel_metadata.clear()
            self._schema_cache.clear()
            self._pending_file_metadata = None
            self._osi_version_min = None
            self._osi_version_max = None

    @property
    def written_count(self) -> int:
        return self._written_count
