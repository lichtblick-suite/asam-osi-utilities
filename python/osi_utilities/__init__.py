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
from osi_utilities.tracefile.binary_reader import SingleTraceReader
from osi_utilities.tracefile.binary_writer import SingleTraceWriter
from osi_utilities.tracefile.channel_reader import ChannelReader, open_channel
from osi_utilities.tracefile.mcap_channel import MCAPChannel
from osi_utilities.tracefile.mcap_reader import MultiTraceReader
from osi_utilities.tracefile.mcap_writer import MultiTraceWriter
from osi_utilities.tracefile.reader import TraceReader, TraceReaderFactory, open_trace_file
from osi_utilities.tracefile.txth_reader import ProtobufTextFormatTraceReader
from osi_utilities.tracefile.txth_writer import ProtobufTextFormatTraceWriter
from osi_utilities.tracefile.writer import TraceWriter

__all__ = [
    "SingleTraceReader",
    "SingleTraceWriter",
    "ChannelReader",
    "ChannelSpecification",
    "MCAPChannel",
    "MultiTraceReader",
    "MultiTraceWriter",
    "MessageType",
    "ReadResult",
    "ProtobufTextFormatTraceReader",
    "ProtobufTextFormatTraceWriter",
    "TraceFileFormat",
    "TraceReader",
    "TraceReaderFactory",
    "TraceWriter",
    "convert_gt2sv",
    "open_trace_file",
    "open_channel",
]
