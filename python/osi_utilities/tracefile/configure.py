# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
 
import logging
from pathlib import Path

from osi_utilities.api.types import ChannelSpecification, MessageType
from osi_utilities.message_types import require_message_type
from osi_utilities.tracefile.readers.base import TraceReader


logger = logging.getLogger(__name__)


def create_reader(path: str | Path) -> TraceReader:
    """Create a trace file reader appropriate for the given file.

    Args:
        path: Path to the trace file. Extension determines the reader type.

    Returns:
        An unopened TraceReader instance.

    Raises:
        ValueError: If the file extension is not supported.
    """
    path = Path(path)
    suffix = path.suffix.lower()

    if suffix == ".mcap":
        from osi_utilities.tracefile.readers.multi import MultiTraceReader

        reader = MultiTraceReader()
    elif suffix == ".osi":
        from osi_utilities.tracefile.readers.single import SingleTraceReader

        reader = SingleTraceReader()
    elif suffix == ".txth":
        logger.warning("The .txth format is not reliably deserializable. Use .osi or .mcap instead.")

        from osi_utilities.tracefile.readers.textformat import ProtobufTextFormatTraceReader

        reader = ProtobufTextFormatTraceReader()
    else:
        raise ValueError(f"Unsupported trace file extension: '{suffix}'")

    return reader


def configure_reader(reader: TraceReader, channel_spec: ChannelSpecification) -> None:
    """Configure a concrete reader from a channel specification."""
    # Single channel reader configuration
    if channel_spec.message_type is not None and hasattr(reader, "set_message_type"):
        reader.set_message_type(require_message_type(channel_spec.message_type))

    # Multi channel reader configuration
    if channel_spec.topic is not None and hasattr(reader, "set_topics"):
        reader.set_topics([channel_spec.topic])

    if (
        channel_spec.topic is not None
        and channel_spec.message_type is not None
        and hasattr(reader, "set_topic_message_types")
    ):
        reader.set_topic_message_types(
            {channel_spec.topic: require_message_type(channel_spec.message_type)}
        )


def configure_reader_for_channels(
    reader: TraceReader, channel_specs: list[ChannelSpecification]
) -> None:
    """Configure a reader from multiple channel specifications.

    Intended for multi-channel use-cases where several logical channels from the
    same source file should be read through one configured reader.

    Raises:
        ValueError: If ``channel_specs`` reference different paths or contain
            conflicting message type expectations.
    """
    if not channel_specs:
        return

    paths = {Path(spec.path) for spec in channel_specs}
    if len(paths) > 1:
        raise ValueError(
            "configure_reader_for_channels requires all channel specs to reference the same path."
        )

    # Configure topics and per-topic message type expectations (for multi readers).
    topics: list[str] = []
    topic_message_types: dict[str, MessageType] = {}

    for spec in channel_specs:
        if spec.topic is not None and spec.topic not in topics:
            topics.append(spec.topic)

        if spec.topic is not None and spec.message_type is not None:
            msg_type = require_message_type(spec.message_type)
            existing = topic_message_types.get(spec.topic)
            if existing is not None and existing != msg_type:
                raise ValueError(
                    f"Conflicting message type expectations for topic '{spec.topic}': "
                    f"{existing} vs {msg_type}."
                )
            topic_message_types[spec.topic] = msg_type

    if topics and hasattr(reader, "set_topics"):
        reader.set_topics(topics)

    if topic_message_types and hasattr(reader, "set_topic_message_types"):
        reader.set_topic_message_types(topic_message_types)

    # Configure single-channel readers when a single consistent type is specified.
    if hasattr(reader, "set_message_type"):
        single_types = {
            require_message_type(spec.message_type)
            for spec in channel_specs
            if spec.message_type is not None
        }
        if len(single_types) > 1:
            raise ValueError(
                f"Conflicting message type expectations for single reader: {single_types}."
            )
        if len(single_types) == 1:
            reader.set_message_type(next(iter(single_types)))
