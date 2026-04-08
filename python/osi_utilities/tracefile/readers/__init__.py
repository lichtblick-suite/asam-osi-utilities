# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Tracefile reader implementations."""

from osi_utilities.tracefile.readers.base import TraceReader
from osi_utilities.tracefile.readers.multi import MultiTraceReader
from osi_utilities.tracefile.readers.single import SingleTraceReader
from osi_utilities.tracefile.readers.textformat import ProtobufTextFormatTraceReader

__all__ = [
    "MultiTraceReader",
    "ProtobufTextFormatTraceReader",
    "SingleTraceReader",
    "TraceReader",
]
