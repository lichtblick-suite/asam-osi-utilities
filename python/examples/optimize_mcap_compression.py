# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Optimize MCAP trace file compression for OSI data.

This script re-encodes an existing MCAP file with optimal compression settings,
targeting maximum space savings for OSI (Open Simulation Interface) protobuf data.

=============================================================================
WHY OPTIMIZE MCAP COMPRESSION FOR OSI DATA?
=============================================================================

OSI trace files store serialized protobuf messages that describe simulation
scenarios: ground truth, sensor views, traffic updates, etc. These messages
exhibit *high structural redundancy*:

  - Repeated protobuf field tags and wire types across messages
  - Nearly identical timestamps incrementing by small deltas
  - Object lists (vehicles, pedestrians) with stable IDs and slowly-changing fields
  - Large floating-point arrays (positions, velocities) with correlated values

This redundancy makes OSI data an excellent candidate for dictionary-based
compression algorithms like Zstandard (zstd), especially at higher levels
where the compressor invests more CPU cycles to find longer matches.

=============================================================================
KEY CONCEPTS
=============================================================================

MCAP Chunk Size
---------------
MCAP groups messages into "chunks" before compressing them (see MCAP spec:
https://mcap.dev/spec#chunk-op0x06). Larger chunks give the compressor more
context to find redundant patterns, but increase memory usage for readers.

  - Default in this library: 16 MiB (see TraceFileConfig.h / _config.py)
  - This script uses 32 MiB (MAX_CHUNK_SIZE) for maximum compression context
  - Trade-off: readers must decompress an entire chunk to seek within it,
    so very large chunks can hurt random-access performance in viewers like
    Lichtblick (https://github.com/lichtblick-suite/lichtblick)

Zstandard Compression Levels
-----------------------------
Zstd supports levels 1-22 (https://facebook.github.io/zstd/):

  Level 1-3:   Fast compression, moderate ratio (mcap library default: 3)
  Level 4-9:   Better ratio, still practical for interactive workloads
  Level 10-18: High compression, suitable for batch/offline processing
  Level 19:    Best *practical* level — highest ratio before extreme RAM usage
  Level 20-22: Ultra — requires --ultra flag, exponential memory cost with diminishing returns

For OSI data, level 19 typically achieves 5-15% better compression than the
default level 3, with the write penalty being acceptable for archival or
post-processing workflows.

MCAP Library Limitation
-----------------------
The mcap Python library (https://github.com/foxglove/mcap) does not expose
a compression_level parameter — it calls `zstandard.compress(data)` internally
with the default level (3). This script works around that limitation by
monkey-patching the `zstandard.compress` function within the mcap.writer
module namespace to use a higher level. This is safe because:

  1. The patch is scoped to a single process (this script)
  2. The original function is restored after writing completes
  3. The compressed output remains fully spec-compliant zstd

LZ4 vs Zstandard
-----------------
LZ4 (https://lz4.github.io/lz4/) prioritizes speed over ratio. Even at its
maximum level (16), LZ4 produces larger files than zstd at level 3. For OSI
archival/optimization, zstd is strictly superior. LZ4 is useful when
decompression speed is critical (e.g., real-time playback on constrained HW).

=============================================================================
REFERENCES
=============================================================================

- MCAP format specification: https://mcap.dev/spec
- MCAP chunk record: https://mcap.dev/spec#chunk-op0x06
- Zstandard manual: https://facebook.github.io/zstd/zstd_manual.html
- ASAM OSI MCAP trace file spec: https://opensimulationinterface.github.io/osi-antora-generator/asamosi/latest/interface/architecture/trace_file_formats.html
- asam-osi-utilities config: see osi_utilities/tracefile/_config.py

=============================================================================
USAGE
=============================================================================

  python optimize_mcap_compression.py <input.mcap> [output.mcap]

If output is omitted, the output filename is derived from the input with a
suffix indicating the applied settings (e.g., "_32MiB_zstd19.mcap").

Options:
  --chunk-size     Chunk size in MiB (default: 32, max allowed by library)
  --level          Zstd compression level 1-22 (default: 19)
  --compression    Algorithm: "zstd" or "lz4" (default: zstd)
  --dry-run        Analyze input file without writing output
"""

from __future__ import annotations

import argparse
import sys
import time
from pathlib import Path


def _format_size(size_bytes: int) -> str:
    """Format byte count as human-readable string."""
    if size_bytes >= 1024 * 1024 * 1024:
        return f"{size_bytes / (1024**3):.2f} GiB"
    if size_bytes >= 1024 * 1024:
        return f"{size_bytes / (1024**2):.2f} MiB"
    if size_bytes >= 1024:
        return f"{size_bytes / 1024:.2f} KiB"
    return f"{size_bytes} B"


def _build_output_path(input_path: Path, chunk_mib: int, compression: str, level: int) -> Path:
    """Generate output filename with compression settings suffix."""
    stem = input_path.stem
    suffix = f"_{chunk_mib}MiB_{compression}{level}"
    return input_path.with_name(f"{stem}{suffix}.mcap")


def _analyze_input(input_path: Path) -> dict:
    """Read MCAP summary to report current file stats."""
    from mcap.reader import make_reader

    stats: dict = {"path": input_path, "size": input_path.stat().st_size}
    with open(input_path, "rb") as f:
        reader = make_reader(f)
        summary = reader.get_summary()
        if summary and summary.statistics:
            stats["message_count"] = summary.statistics.message_count
            stats["chunk_count"] = summary.statistics.chunk_count
            stats["schema_count"] = summary.statistics.schema_count
            stats["channel_count"] = len(summary.channels) if summary.channels else 0
        else:
            stats["message_count"] = 0
            stats["chunk_count"] = 0
            stats["schema_count"] = 0
            stats["channel_count"] = 0

        # Determine current compression from chunk indexes
        if summary and summary.chunk_indexes:
            compressions = {ci.compression for ci in summary.chunk_indexes}
            stats["current_compression"] = ", ".join(sorted(compressions)) or "none"
            # Estimate average chunk size from uncompressed sizes
            total_uncompressed = sum(ci.uncompressed_size for ci in summary.chunk_indexes)
            stats["avg_chunk_size"] = total_uncompressed // len(summary.chunk_indexes) if summary.chunk_indexes else 0
            stats["total_uncompressed"] = total_uncompressed
            total_compressed = sum(ci.compressed_size for ci in summary.chunk_indexes)
            stats["total_compressed"] = total_compressed
        else:
            stats["current_compression"] = "unknown"
            stats["avg_chunk_size"] = 0
            stats["total_uncompressed"] = 0
            stats["total_compressed"] = 0

    return stats


def _recompress(
    input_path: Path,
    output_path: Path,
    chunk_size: int,
    compression: str,
    level: int,
) -> dict:
    """Read all records from input MCAP and write to output with new compression settings.

    Strategy:
      1. Read summary (schemas, channels) from the source file
      2. Monkey-patch zstandard.compress (or lz4.frame.compress) in the mcap.writer
         module to use the specified compression level
      3. Stream messages from reader to writer (no full materialization in RAM)
      4. Copy metadata and attachments
      5. Restore the original compression function

    This preserves all MCAP content (schemas, channels, metadata, messages,
    attachments) while only changing the physical chunk layout and compression.
    On failure, incomplete output files are removed to prevent confusion.
    """
    import mcap.writer as mcap_writer_module
    from mcap.reader import make_reader
    from mcap.writer import CompressionType, Writer

    # --- Monkey-patch compression to inject custom level ---
    # The mcap library calls zstandard.compress(data) without a level parameter,
    # defaulting to level 3. We intercept this call to use a higher level.
    # THREAD SAFETY: This modifies global module state. Do NOT run multiple
    # instances concurrently in the same Python process (e.g., via threading).
    # The patch is scoped to this function and restored in the finally block.
    original_zstd_compress = None
    original_lz4_compress = None

    if compression == "zstd":
        try:
            import zstandard
        except ImportError:
            raise SystemExit(
                "Error: The 'zstandard' package is required for zstd compression.\n"
                "Install it with: pip install zstandard"
            ) from None

        original_zstd_compress = mcap_writer_module.zstandard.compress  # type: ignore[attr-defined]
        compressor = zstandard.ZstdCompressor(level=level)

        def _zstd_compress_high(data: bytes) -> bytes:
            return compressor.compress(data)

        mcap_writer_module.zstandard.compress = _zstd_compress_high  # type: ignore[attr-defined]
        mcap_compression = CompressionType.ZSTD
    elif compression == "lz4":
        try:
            import lz4.frame
        except ImportError:
            raise SystemExit(
                "Error: The 'lz4' package is required for lz4 compression.\nInstall it with: pip install lz4"
            ) from None

        original_lz4_compress = mcap_writer_module.lz4.frame.compress  # type: ignore[attr-defined]

        def _lz4_compress_high(data: bytes) -> bytes:
            return lz4.frame.compress(data, compression_level=level)

        mcap_writer_module.lz4.frame.compress = _lz4_compress_high  # type: ignore[attr-defined]
        mcap_compression = CompressionType.LZ4
    else:
        mcap_compression = CompressionType.NONE

    output_complete = False
    message_count = 0
    t_start = t_read = t_write = 0.0

    try:
        t_start = time.perf_counter()

        with open(input_path, "rb") as f_in:
            # --- Phase 1: Read summary and header for schemas, channels, and profile ---
            reader = make_reader(f_in)
            header = reader.get_header()
            summary = reader.get_summary()

            schemas = summary.schemas if summary and summary.schemas else {}
            channels = summary.channels if summary and summary.channels else {}

            t_read = time.perf_counter()

            # --- Phase 2: Write output (streaming messages from input) ---
            with open(output_path, "wb") as f_out:
                writer = Writer(f_out, chunk_size=chunk_size, compression=mcap_compression)
                # Preserve the MCAP header profile (e.g., "osi2mcap") from input
                profile = header.profile if header else ""
                writer.start(profile=profile, library="osi-utilities-python/optimize_mcap_compression")

                # Re-register schemas (build old->new ID mapping)
                schema_id_map: dict[int, int] = {}
                for old_id, schema in schemas.items():
                    new_id = writer.register_schema(
                        name=schema.name,
                        encoding=schema.encoding,
                        data=schema.data,
                    )
                    schema_id_map[old_id] = new_id

                # Re-register channels (build old->new ID mapping)
                channel_id_map: dict[int, int] = {}
                for old_id, channel in channels.items():
                    if channel.schema_id != 0:
                        new_schema_id = schema_id_map.get(channel.schema_id)
                        if new_schema_id is None:
                            raise ValueError(
                                f"Channel '{channel.topic}' (id={old_id}) references unknown "
                                f"schema ID {channel.schema_id}. Input file may be malformed."
                            )
                    else:
                        new_schema_id = 0
                    new_id = writer.register_channel(
                        topic=channel.topic,
                        message_encoding=channel.message_encoding,
                        schema_id=new_schema_id,
                        metadata=channel.metadata,
                    )
                    channel_id_map[old_id] = new_id

                # Stream messages directly from reader to writer (no list materialization).
                # This avoids loading all message data into RAM simultaneously, which is
                # critical for multi-GB OSI trace files.
                f_in.seek(0)
                msg_reader = make_reader(f_in)
                for _schema, _channel, message in msg_reader.iter_messages():
                    new_channel_id = channel_id_map.get(message.channel_id)
                    if new_channel_id is None:
                        raise ValueError(
                            f"Message references unknown channel ID {message.channel_id}. "
                            f"Input file may be malformed or summary is incomplete."
                        )
                    writer.add_message(
                        channel_id=new_channel_id,
                        log_time=message.log_time,
                        publish_time=message.publish_time,
                        data=message.data,
                        sequence=message.sequence,
                    )
                    message_count += 1

                # Copy metadata records
                f_in.seek(0)
                meta_reader = make_reader(f_in)
                for metadata in meta_reader.iter_metadata():
                    writer.add_metadata(name=metadata.name, data=metadata.metadata)

                # Copy attachment records (images, calibration data, etc.)
                f_in.seek(0)
                attach_reader = make_reader(f_in)
                for attachment in attach_reader.iter_attachments():
                    writer.add_attachment(
                        create_time=attachment.create_time,
                        log_time=attachment.log_time,
                        name=attachment.name,
                        media_type=attachment.media_type,
                        data=attachment.data,
                    )

                writer.finish()
                output_complete = True

        t_write = time.perf_counter()

    finally:
        # --- Restore original compression functions ---
        if original_zstd_compress is not None:
            mcap_writer_module.zstandard.compress = original_zstd_compress  # type: ignore[attr-defined]
        if original_lz4_compress is not None:
            mcap_writer_module.lz4.frame.compress = original_lz4_compress  # type: ignore[attr-defined]

        # Remove incomplete output on failure to prevent confusion
        if not output_complete and output_path.exists():
            output_path.unlink()

    return {
        "read_time": t_read - t_start,
        "write_time": t_write - t_read,
        "total_time": t_write - t_start,
        "message_count": message_count,
        "output_size": output_path.stat().st_size,
    }


def _print_stats(input_stats: dict, output_stats: dict | None, args: argparse.Namespace) -> None:
    """Display before/after comparison in terminal."""
    print("\n" + "=" * 72)
    print("  MCAP COMPRESSION OPTIMIZATION REPORT")
    print("=" * 72)

    print(f"\n{'INPUT FILE':}")
    print(f"  Path:               {input_stats['path']}")
    print(f"  File size:          {_format_size(input_stats['size'])}")
    print(f"  Messages:           {input_stats['message_count']:,}")
    print(f"  Chunks:             {input_stats['chunk_count']:,}")
    print(f"  Channels:           {input_stats['channel_count']}")
    print(f"  Current compression: {input_stats['current_compression']}")
    print(f"  Avg chunk size:     {_format_size(input_stats['avg_chunk_size'])}")
    if input_stats["total_uncompressed"] > 0:
        current_ratio = input_stats["total_compressed"] / input_stats["total_uncompressed"]
        print(f"  Current ratio:      {current_ratio:.3f} ({(1 - current_ratio) * 100:.1f}% saved)")

    print(f"\n{'TARGET SETTINGS':}")
    print(f"  Chunk size:         {args.chunk_size} MiB ({args.chunk_size * 1024 * 1024:,} bytes)")
    print(f"  Compression:        {args.compression}")
    print(f"  Level:              {args.level}")

    if output_stats is not None:
        input_size = input_stats["size"]
        output_size = output_stats["output_size"]
        reduction = input_size - output_size
        ratio = output_size / input_size if input_size > 0 else 1.0

        print(f"\n{'OUTPUT FILE':}")
        print(f"  File size:          {_format_size(output_size)}")
        print(f"  Messages written:   {output_stats['message_count']:,}")

        print(f"\n{'IMPROVEMENT':}")
        print(f"  Size reduction:     {_format_size(reduction)} ({(1 - ratio) * 100:.1f}%)")
        print(f"  Compression ratio:  {ratio:.4f}")
        print(f"  Read time:          {output_stats['read_time']:.1f} s")
        print(f"  Write time:         {output_stats['write_time']:.1f} s")
        print(f"  Total time:         {output_stats['total_time']:.1f} s")

        if output_stats["write_time"] > 0:
            throughput = input_stats["total_uncompressed"] / (1024**2) / output_stats["write_time"]
            print(f"  Write throughput:   {throughput:.1f} MiB/s (uncompressed input)")

    print("\n" + "=" * 72)


def main() -> int:
    """Entry point for MCAP compression optimization."""
    parser = argparse.ArgumentParser(
        description=(
            "Optimize MCAP trace file compression for OSI data. "
            "Re-encodes with larger chunks and higher zstd level for maximum space savings."
        ),
        epilog=(
            "Example:\n"
            "  python optimize_mcap_compression.py recording.mcap\n"
            "  python optimize_mcap_compression.py recording.mcap optimized.mcap --level 19\n"
            "  python optimize_mcap_compression.py recording.mcap --dry-run\n"
        ),
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument("input", help="Path to input MCAP file")
    parser.add_argument("output", nargs="?", default=None, help="Path to output MCAP file (auto-generated if omitted)")
    parser.add_argument(
        "--chunk-size",
        type=int,
        default=32,
        help="Chunk size in MiB (default: 32, the maximum for optimal compression context)",
    )
    parser.add_argument(
        "--level",
        type=int,
        default=None,
        help="Compression level (zstd: 1-22, default: 19; lz4: 1-16, default: 16)",
    )
    parser.add_argument(
        "--compression",
        choices=["zstd", "lz4", "none"],
        default="zstd",
        help="Compression algorithm (default: zstd — best ratio for OSI data)",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Only analyze input file, do not write output",
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Overwrite output file if it already exists",
    )
    args = parser.parse_args()

    input_path = Path(args.input)
    if not input_path.exists():
        print(f"Error: Input file not found: {input_path}", file=sys.stderr)
        return 1
    if input_path.suffix.lower() != ".mcap":
        print(f"Error: Input must be an MCAP file (got {input_path.suffix})", file=sys.stderr)
        return 1

    # Apply compression-specific default level if not specified
    if args.level is None:
        if args.compression == "lz4":
            args.level = 16
        else:
            args.level = 19

    # Validate compression level
    if args.compression == "zstd" and not (1 <= args.level <= 22):
        print("Error: zstd level must be 1-22", file=sys.stderr)
        return 1
    if args.compression == "lz4" and not (1 <= args.level <= 16):
        print("Error: lz4 level must be 1-16", file=sys.stderr)
        return 1

    # Validate chunk size (convert MiB to bytes)
    chunk_size_bytes = args.chunk_size * 1024 * 1024
    from osi_utilities.tracefile import MAX_CHUNK_SIZE, MIN_CHUNK_SIZE

    if not (MIN_CHUNK_SIZE <= chunk_size_bytes <= MAX_CHUNK_SIZE):
        print(
            f"Error: chunk-size must be between "
            f"{MIN_CHUNK_SIZE // (1024 * 1024)} and {MAX_CHUNK_SIZE // (1024 * 1024)} MiB",
            file=sys.stderr,
        )
        return 1

    # Determine output path
    if args.output:
        output_path = Path(args.output)
    else:
        output_path = _build_output_path(input_path, args.chunk_size, args.compression, args.level)

    # Guard against input/output collision (would truncate input)
    if output_path.resolve() == input_path.resolve():
        print("Error: Output path cannot be the same as input path", file=sys.stderr)
        return 1

    # Guard against accidental overwrite
    if not args.dry_run and output_path.exists() and not args.force:
        print(f"Error: Output file already exists: {output_path}", file=sys.stderr)
        print("Use --force to overwrite.", file=sys.stderr)
        return 1

    # Analyze input
    print(f"Analyzing: {input_path}")
    input_stats = _analyze_input(input_path)

    if args.dry_run:
        _print_stats(input_stats, None, args)
        print("\n  [DRY RUN — no output written]")
        return 0

    # Perform optimization
    print(f"Optimizing: {input_path.name}")
    print(f"  -> {output_path.name}")
    print(f"  Chunk size: {args.chunk_size} MiB | Compression: {args.compression} level {args.level}")
    print("  Processing...", end="", flush=True)

    output_stats = _recompress(input_path, output_path, chunk_size_bytes, args.compression, args.level)
    print(" done.")

    _print_stats(input_stats, output_stats, args)
    print(f"\n  Output: {output_path}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
