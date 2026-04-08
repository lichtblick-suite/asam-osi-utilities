# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Configuration constants for OSI trace file handling.

Mirrors C++ TraceFileConfig.h constants.
"""

# MCAP chunk size configuration (bytes)
DEFAULT_CHUNK_SIZE: int = 16 * 1024 * 1024  # 16 MiB
MIN_CHUNK_SIZE: int = 1 * 1024 * 1024  # 1 MiB
MAX_CHUNK_SIZE: int = 32 * 1024 * 1024  # 32 MiB

# Binary format constants
BINARY_MESSAGE_LENGTH_PREFIX_SIZE: int = 4  # uint32 little-endian
MAX_EXPECTED_MESSAGE_SIZE: int = 512 * 1024 * 1024  # 512 MiB sanity check

# Time constants
NANOSECONDS_PER_SECOND: int = 1_000_000_000

# Text format constants
TXTH_READ_BUFFER_RESERVE_SIZE: int = 4096

# OSI MCAP metadata keys
OSI_TRACE_METADATA_NAME: str = "net.asam.osi.trace"
OSI_CHANNEL_OSI_VERSION_KEY: str = "net.asam.osi.trace.channel.osi_version"
OSI_CHANNEL_PROTOBUF_VERSION_KEY: str = "net.asam.osi.trace.channel.protobuf_version"

# Required metadata keys for net.asam.osi.trace (per OSI MCAP spec)
OSI_TRACE_REQUIRED_METADATA_KEYS: frozenset[str] = frozenset(
    {
        "version",
        "min_osi_version",
        "max_osi_version",
        "min_protobuf_version",
        "max_protobuf_version",
    }
)

# Recommended metadata keys for net.asam.osi.trace
OSI_TRACE_RECOMMENDED_METADATA_KEYS: frozenset[str] = frozenset(
    {
        "zero_time",
        "creation_time",
        "description",
        "authors",
        "data_sources",
    }
)

# Required channel metadata keys
OSI_CHANNEL_REQUIRED_METADATA_KEYS: frozenset[str] = frozenset(
    {
        OSI_CHANNEL_OSI_VERSION_KEY,
        OSI_CHANNEL_PROTOBUF_VERSION_KEY,
    }
)

# Recommended channel metadata keys
OSI_CHANNEL_RECOMMENDED_METADATA_KEYS: frozenset[str] = frozenset(
    {
        "net.asam.osi.trace.channel.description",
    }
)
