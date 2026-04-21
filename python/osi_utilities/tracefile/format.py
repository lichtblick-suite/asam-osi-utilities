# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

from pathlib import Path

from osi_utilities._types import TraceFileFormat

_EXTENSION_TO_FORMAT: dict[str, TraceFileFormat] = {
    ".osi": TraceFileFormat.SINGLE_CHANNEL,
    ".txth": TraceFileFormat.SINGLE_CHANNEL,
    ".mcap": TraceFileFormat.MULTI_CHANNEL,
}

_FORMAT_TO_EXTENSION: dict[TraceFileFormat, str] = {
    TraceFileFormat.SINGLE_CHANNEL: ".osi",
    TraceFileFormat.MULTI_CHANNEL: ".mcap",
}


def get_trace_file_format(path: Path) -> TraceFileFormat:
    """Determine the trace file format from the file extension."""
    trace_format = _EXTENSION_TO_FORMAT.get(path.suffix.lower())
    if trace_format is None:
        raise ValueError(f"Unsupported trace file extension: '{path.suffix}'")
    return trace_format
