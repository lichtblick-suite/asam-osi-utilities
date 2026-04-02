# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Python utilities for reading and writing ASAM OSI trace files."""

from osi_utilities.converters.gt2sv import convert_gt2sv
from osi_utilities.tracefile._types import (
    ChannelSpecification,
    MessageType,
    ReadResult,
    TraceFileFormat,
)
from osi_utilities.tracefile.binary_reader import BinaryTraceFileReader
from osi_utilities.tracefile.binary_writer import BinaryTraceFileWriter
from osi_utilities.tracefile.channel_reader import ChannelReader, open_channel
from osi_utilities.tracefile.mcap_channel import MCAPChannel
from osi_utilities.tracefile.mcap_reader import MCAPTraceFileReader
from osi_utilities.tracefile.mcap_writer import MCAPTraceFileWriter
from osi_utilities.tracefile.reader import TraceFileReader, TraceFileReaderFactory, open_trace_file
from osi_utilities.tracefile.txth_reader import TXTHTraceFileReader
from osi_utilities.tracefile.txth_writer import TXTHTraceFileWriter
from osi_utilities.tracefile.writer import TraceFileWriter

__all__ = [
    "BinaryTraceFileReader",
    "BinaryTraceFileWriter",
    "ChannelReader",
    "ChannelSpecification",
    "MCAPChannel",
    "MCAPTraceFileReader",
    "MCAPTraceFileWriter",
    "MessageType",
    "ReadResult",
    "TXTHTraceFileReader",
    "TXTHTraceFileWriter",
    "TraceFileFormat",
    "TraceFileReader",
    "TraceFileReaderFactory",
    "TraceFileWriter",
    "convert_gt2sv",
    "open_trace_file",
    "open_channel",
]
