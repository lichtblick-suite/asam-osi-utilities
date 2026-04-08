# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Python utilities for reading and writing ASAM OSI trace files."""

from typing import Any

from osi_utilities.api import (
    ChannelReader,
    ChannelWriter,
    ChannelSpecification,
    MessageType,
    ReadResult,
    TraceFileFormat,
    open_channel,
    open_channel_writer,
)
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
from osi_utilities.tracefile.mcap_channel import MCAPChannel
from osi_utilities.tracefile.configure import create_reader


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
    "ProtobufTextFormatTraceReader",
    "ProtobufTextFormatTraceWriter",
    "TraceFileFormat",
    "TraceReader",
    "create_reader",
    "TraceWriter",
    "convert_gt2sv",
    "open_channel",
    "open_channel_writer",
]
