# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Trace file reader/writer implementations for MCAP, binary .osi, and text .txth formats."""

from osi_utilities.tracefile.binary_reader import BinaryTraceFileReader
from osi_utilities.tracefile.binary_writer import BinaryTraceFileWriter
from osi_utilities.tracefile.channel_reader import ChannelReader, open_channel
from osi_utilities.tracefile.mcap_channel import MCAPChannel
from osi_utilities.tracefile.mcap_reader import MCAPTraceFileReader
from osi_utilities.tracefile.mcap_writer import MCAPTraceFileWriter
from osi_utilities.tracefile.reader import TraceFileReader, TraceFileReaderFactory, open_trace_file
from osi_utilities.tracefile.timestamp import (
    nanoseconds_to_seconds,
    seconds_to_nanoseconds,
    timestamp_to_nanoseconds,
    timestamp_to_seconds,
)
from osi_utilities.tracefile.txth_reader import TXTHTraceFileReader
from osi_utilities.tracefile.txth_writer import TXTHTraceFileWriter
from osi_utilities.tracefile.writer import TraceFileWriter

__all__ = [
    "BinaryTraceFileReader",
    "BinaryTraceFileWriter",
    "ChannelReader",
    "MCAPChannel",
    "MCAPTraceFileReader",
    "MCAPTraceFileWriter",
    "TXTHTraceFileReader",
    "TXTHTraceFileWriter",
    "TraceFileReader",
    "TraceFileReaderFactory",
    "TraceFileWriter",
    "nanoseconds_to_seconds",
    "open_trace_file",
    "open_channel",
    "seconds_to_nanoseconds",
    "timestamp_to_nanoseconds",
    "timestamp_to_seconds",
]
