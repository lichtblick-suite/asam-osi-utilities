# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Trace file reader/writer implementations for MCAP, binary .osi, and text .txth formats."""

from osi_utilities.timestamp import (
    nanoseconds_to_seconds,
    seconds_to_nanoseconds,
    timestamp_to_nanoseconds,
    timestamp_to_seconds,
)
from osi_utilities.tracefile._config import (
    DEFAULT_CHUNK_SIZE,
    MAX_CHUNK_SIZE,
    MIN_CHUNK_SIZE,
)
from osi_utilities.tracefile.configure import create_reader
from osi_utilities.tracefile.mcap_channel import MCAPChannel
from osi_utilities.tracefile.readers import (
    MultiTraceReader,
    ProtobufTextFormatTraceReader,
    SingleTraceReader,
    TraceReader,
)
from osi_utilities.tracefile.writers import (
    MultiTraceWriter,
    ProtobufTextFormatTraceWriter,
    SingleTraceWriter,
    TraceWriter,
    prepare_required_file_metadata,
)

__all__ = [
    "DEFAULT_CHUNK_SIZE",
    "MAX_CHUNK_SIZE",
    "MIN_CHUNK_SIZE",
    "SingleTraceReader",
    "SingleTraceWriter",
    "MCAPChannel",
    "MultiTraceReader",
    "MultiTraceWriter",
    "ProtobufTextFormatTraceReader",
    "ProtobufTextFormatTraceWriter",
    "TraceReader",
    "create_reader",
    "TraceWriter",
    "nanoseconds_to_seconds",
    "prepare_required_file_metadata",
    "seconds_to_nanoseconds",
    "timestamp_to_nanoseconds",
    "timestamp_to_seconds",
]
