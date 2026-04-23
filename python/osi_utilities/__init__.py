# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Python utilities for reading and writing ASAM OSI trace files."""

from typing import Any

from osi_utilities.api import (
    ChannelReader,
    ChannelSpecification,
    ChannelWriter,
    MessageType,
    ReadResult,
    ReadStatus,
    TraceFileFormat,
    open_channel,
    open_channel_writer,
)
from osi_utilities.filename import infer_message_type_from_filename, parse_osi_trace_filename
from osi_utilities.timestamp import (
    nanoseconds_to_seconds,
    seconds_to_nanoseconds,
    timestamp_to_nanoseconds,
    timestamp_to_seconds,
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
)


def convert_gt2sv(*args: Any, **kwargs: Any) -> Any:
    """Lazily import and call the GT->SV converter."""
    from osi_utilities.converters.gt2sv import convert_gt2sv as _convert_gt2sv

    return _convert_gt2sv(*args, **kwargs)


__all__ = [
    "SingleTraceReader",
    "SingleTraceWriter",
    "ChannelReader",
    "ChannelSpecification",
    "ChannelWriter",
    "MCAPChannel",
    "MultiTraceReader",
    "MultiTraceWriter",
    "MessageType",
    "ReadResult",
    "ReadStatus",
    "ProtobufTextFormatTraceReader",
    "ProtobufTextFormatTraceWriter",
    "TraceFileFormat",
    "TraceReader",
    "create_reader",
    "TraceWriter",
    "convert_gt2sv",
    "infer_message_type_from_filename",
    "nanoseconds_to_seconds",
    "open_channel",
    "open_channel_writer",
    "parse_osi_trace_filename",
    "seconds_to_nanoseconds",
    "timestamp_to_nanoseconds",
    "timestamp_to_seconds",
]
