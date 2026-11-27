# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""OSI version helpers for trace file metadata.

Every OSI channel carries an OSI version label (``net.asam.osi.trace.channel.osi_version``).
The file's ``min_osi_version`` / ``max_osi_version`` span exactly these labels, so a channel
can never fall outside the file range.
"""

from __future__ import annotations

import logging
import re

from google.protobuf.message import Message

from osi_utilities.tracefile._config import OSI_CHANNEL_OSI_VERSION_KEY

logger = logging.getLogger(__name__)

OsiVersion = tuple[int, int, int]

_SEMVER_CORE_RE = re.compile(r"^\s*(\d+)\.(\d+)\.(\d+)")


def parse_osi_version(version: str) -> OsiVersion | None:
    """Parse the numeric ``major.minor.patch`` core of a version string, ignoring any suffix."""
    match = _SEMVER_CORE_RE.match(version)
    return (int(match.group(1)), int(match.group(2)), int(match.group(3))) if match else None


def format_osi_version(version: OsiVersion) -> str:
    """Format an OSI version triple as ``major.minor.patch``."""
    return "{}.{}.{}".format(*version)


def normalize_osi_version_string(version: str) -> str:
    """Return the strict ``major.minor.patch`` core of a version string.

    The OSI trace metadata version fields must be ``major.minor.patch``; any
    pre-release/build suffix (e.g. ``"3.8.0-rc1"`` -> ``"3.8.0"``) is stripped so the
    emitted value stays spec-conformant across release-candidate cycles. The input is
    returned unchanged if it does not start with a numeric ``major.minor.patch``.
    """
    parsed = parse_osi_version(version)
    return format_osi_version(parsed) if parsed is not None else version


def get_osi_library_version() -> str | None:
    """Best-effort OSI interface (schema) version from the linked ``osi3`` library.

    Returns ``None`` if ``osi3`` is not importable (the import is intentionally lazy so the
    writers do not hard-depend on it).
    """
    try:
        from osi3 import osi_version_pb2
    except (ImportError, ModuleNotFoundError):
        logger.warning("osi3 is not importable; cannot determine the OSI library version for trace metadata")
        return None
    interface_version = osi_version_pb2.DESCRIPTOR.GetOptions().Extensions[osi_version_pb2.current_interface_version]
    return f"{interface_version.version_major}.{interface_version.version_minor}.{interface_version.version_patch}"


def get_message_osi_version(message: Message) -> OsiVersion | None:
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


class ChannelOsiVersions:
    """Tracks the OSI version label of each OSI channel of a trace file."""

    def __init__(self) -> None:
        self._labels: dict[str, OsiVersion | None] = {}
        self._mismatch_reported: set[str] = set()

    @staticmethod
    def apply_default_label(channel_metadata: dict[str, str]) -> None:
        """Default the channel's OSI version label to the linked OSI library version (in-place)."""
        if OSI_CHANNEL_OSI_VERSION_KEY not in channel_metadata:
            library_version = get_osi_library_version()
            if library_version is not None:
                channel_metadata[OSI_CHANNEL_OSI_VERSION_KEY] = library_version

    def register(self, topic: str, channel_metadata: dict[str, str]) -> None:
        """Record the OSI version label of a successfully registered channel."""
        label = channel_metadata.get(OSI_CHANNEL_OSI_VERSION_KEY)
        self._labels[topic] = parse_osi_version(label) if label else None

    def check_message(self, topic: str, message: Message) -> None:
        """Warn (once per channel) if the message's ``InterfaceVersion`` differs from the channel label."""
        label = self._labels.get(topic)
        if label is None or topic in self._mismatch_reported:
            return
        message_version = get_message_osi_version(message)
        if message_version is not None and message_version != label:
            logger.warning(
                "Message on topic '%s' has OSI version %s but the channel is labelled %s",
                topic,
                format_osi_version(message_version),
                format_osi_version(label),
            )
            self._mismatch_reported.add(topic)

    def fill_range(self, metadata: dict[str, str]) -> None:
        """Set ``min/max_osi_version`` to the range of the channel labels.

        Leaves ``metadata`` unchanged if no channel has a parseable label.
        """
        labels = [label for label in self._labels.values() if label is not None]
        if labels:
            metadata["min_osi_version"] = format_osi_version(min(labels))
            metadata["max_osi_version"] = format_osi_version(max(labels))

    def clear(self) -> None:
        self._labels.clear()
        self._mismatch_reported.clear()
