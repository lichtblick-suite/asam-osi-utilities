# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Tracefile writer implementations."""

from osi_utilities.tracefile.writers.base import TraceWriter
from osi_utilities.tracefile.writers.multi import MultiTraceWriter, prepare_required_file_metadata
from osi_utilities.tracefile.writers.single import SingleTraceWriter
from osi_utilities.tracefile.writers.textformat import ProtobufTextFormatTraceWriter

__all__ = [
    "MultiTraceWriter",
    "ProtobufTextFormatTraceWriter",
    "SingleTraceWriter",
    "TraceWriter",
    "prepare_required_file_metadata",
]
