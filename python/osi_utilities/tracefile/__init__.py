# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Trace file reader/writer implementations for MCAP, binary .osi, and text .txth formats."""

from osi_utilities.tracefile.binary_reader import SingleTraceReader
from osi_utilities.tracefile.binary_writer import SingleTraceWriter
from osi_utilities.tracefile.channel_reader import ChannelReader, open_channel
from osi_utilities.tracefile.mcap_channel import MCAPChannel
from osi_utilities.tracefile.mcap_reader import MultiTraceReader
from osi_utilities.tracefile.mcap_writer import MultiTraceWriter
from osi_utilities.tracefile.reader import TraceReader, TraceReaderFactory, open_trace_file
from osi_utilities.tracefile.timestamp import (
    nanoseconds_to_seconds,
    seconds_to_nanoseconds,
    timestamp_to_nanoseconds,
    timestamp_to_seconds,
)
from osi_utilities.tracefile.txth_reader import ProtobufTextFormatTraceReader
from osi_utilities.tracefile.txth_writer import ProtobufTextFormatTraceWriter
from osi_utilities.tracefile.writer import TraceWriter

__all__ = [
    "SingleTraceReader",
    "SingleTraceWriter",
    "ChannelReader",
    "MCAPChannel",
    "MultiTraceReader",
    "MultiTraceWriter",
    "ProtobufTextFormatTraceReader",
    "ProtobufTextFormatTraceWriter",
    "TraceReader",
    "TraceReaderFactory",
    "TraceWriter",
    "nanoseconds_to_seconds",
    "open_trace_file",
    "open_channel",
    "seconds_to_nanoseconds",
    "timestamp_to_nanoseconds",
    "timestamp_to_seconds",
]
