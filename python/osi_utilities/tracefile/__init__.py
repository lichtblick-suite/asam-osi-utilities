# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Trace file reader/writer implementations for MCAP, binary .osi, and text .txth formats."""

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
)
from osi_utilities.tracefile.configure import create_reader
from osi_utilities.timestamp import (
    nanoseconds_to_seconds,
    seconds_to_nanoseconds,
    timestamp_to_nanoseconds,
    timestamp_to_seconds,
)

__all__ = [
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
    "seconds_to_nanoseconds",
    "timestamp_to_nanoseconds",
    "timestamp_to_seconds",
]
