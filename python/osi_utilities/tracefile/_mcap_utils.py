# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Shared MCAP utility functions for OSI trace file writing.

Internal helpers used by both MCAPTraceFileWriter and MCAPChannel
to avoid code duplication.
"""

from __future__ import annotations

from google.protobuf.descriptor import FileDescriptor
from google.protobuf.descriptor_pb2 import FileDescriptorSet
from google.protobuf.message import Message

from osi_utilities.tracefile._config import NANOSECONDS_PER_SECOND


def build_file_descriptor_set(message_class: type[Message]) -> FileDescriptorSet:
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


def extract_timestamp_ns(message: Message) -> int:
    """Extract timestamp in nanoseconds from an OSI message."""
    if hasattr(message, "timestamp"):
        ts = message.timestamp
        return int(ts.seconds * NANOSECONDS_PER_SECOND + ts.nanos)
    return 0
