# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Shared MCAP utility functions for OSI trace file writing.

Internal helpers used by both MultiTraceWriter and MCAPChannel
to avoid code duplication.
"""

from __future__ import annotations

from google.protobuf.descriptor import FileDescriptor
from google.protobuf.descriptor_pb2 import FileDescriptorSet
from google.protobuf.message import Message


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
    """Extract timestamp in nanoseconds from an OSI message.

    .. deprecated::
        Use :func:`osi_utilities.timestamp.timestamp_to_nanoseconds` instead.
    """
    from osi_utilities.timestamp import timestamp_to_nanoseconds

    return timestamp_to_nanoseconds(message)
