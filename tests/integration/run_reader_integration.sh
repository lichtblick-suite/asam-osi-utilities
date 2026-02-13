#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# SPDX-License-Identifier: MPL-2.0
#
# Integration test: run a writer example, then feed its output to the corresponding reader example.
# Usage: run_reader_integration.sh <writer_exe> <reader_exe> <tmp_dir> [reader_extra_args...]

set -euo pipefail

WRITER_EXE="$1"
READER_EXE="$2"
TMP_DIR="$3"
shift 3
READER_EXTRA_ARGS=("$@")

mkdir -p "$TMP_DIR"

echo "=== Running writer: $WRITER_EXE ==="
WRITER_OUTPUT=$("$WRITER_EXE" 2>&1) || { echo "Writer failed"; echo "$WRITER_OUTPUT"; exit 1; }
echo "$WRITER_OUTPUT"

# Extract the output file path from writer stdout (looks for paths ending in .osi, .mcap, .txth)
# Uses grep -oE instead of grep -oP for portability (BSD grep on macOS lacks -P)
OUTPUT_FILE=$(echo "$WRITER_OUTPUT" | grep -oE '/[^ "]+\.(osi|mcap|txth)' | head -1)

if [ -z "$OUTPUT_FILE" ]; then
    echo "ERROR: Could not determine output file from writer output"
    exit 1
fi

if [ ! -f "$OUTPUT_FILE" ]; then
    echo "ERROR: Writer output file does not exist: $OUTPUT_FILE"
    exit 1
fi

echo "=== Running reader: $READER_EXE on $OUTPUT_FILE ${READER_EXTRA_ARGS[*]:-} ==="
READER_OUTPUT=$("$READER_EXE" "$OUTPUT_FILE" "${READER_EXTRA_ARGS[@]}" 2>&1) || { echo "Reader failed"; echo "$READER_OUTPUT"; exit 1; }
echo "$READER_OUTPUT"

# Verify reader produced meaningful output
if echo "$READER_OUTPUT" | grep -qi "Timestamp"; then
    echo "=== PASS: Reader produced timestamp output ==="
else
    echo "=== FAIL: Reader did not produce expected timestamp output ==="
    exit 1
fi

# Cleanup
rm -f "$OUTPUT_FILE"
echo "=== Integration test passed ==="
