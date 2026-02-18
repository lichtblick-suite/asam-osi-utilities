#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# SPDX-License-Identifier: MPL-2.0
#
# Integration test: run binary writer -> convert to mcap -> read mcap.
# Usage: run_converter_integration.sh <binary_writer_exe> <converter_exe> <mcap_reader_exe> <tmp_dir>

set -euo pipefail

BINARY_WRITER_EXE="$1"
CONVERTER_EXE="$2"
MCAP_READER_EXE="$3"
TMP_DIR="$4"
shift 4
CONVERTER_EXTRA_ARGS=("$@")

mkdir -p "$TMP_DIR"

echo "=== Step 1: Running binary writer ==="
WRITER_OUTPUT=$("$BINARY_WRITER_EXE" 2>&1) || { echo "Binary writer failed"; echo "$WRITER_OUTPUT"; exit 1; }
echo "$WRITER_OUTPUT"

# Extract the output file path from writer stdout
# Uses grep -oE instead of grep -oP for portability (BSD grep on macOS lacks -P)
OSI_FILE=$(echo "$WRITER_OUTPUT" | grep -oE '([A-Za-z]:[/\\]|/)[^ "]+\.osi' | head -1)
if [ -z "$OSI_FILE" ] || [ ! -f "$OSI_FILE" ]; then
    echo "ERROR: Could not find binary writer output file"
    exit 1
fi
echo "Binary file: $OSI_FILE"

MCAP_FILE="$TMP_DIR/converted_integration_test.mcap"

echo "=== Step 2: Running converter ==="
CONVERTER_OUTPUT=$("$CONVERTER_EXE" "$OSI_FILE" "$MCAP_FILE" "${CONVERTER_EXTRA_ARGS[@]}" 2>&1) || { echo "Converter failed"; echo "$CONVERTER_OUTPUT"; exit 1; }
echo "$CONVERTER_OUTPUT"

if [ ! -f "$MCAP_FILE" ]; then
    echo "ERROR: Converter output file does not exist: $MCAP_FILE"
    exit 1
fi

echo "=== Step 3: Running MCAP reader ==="
READER_OUTPUT=$("$MCAP_READER_EXE" "$MCAP_FILE" 2>&1) || { echo "MCAP reader failed"; echo "$READER_OUTPUT"; exit 1; }
echo "$READER_OUTPUT"

# Verify reader produced meaningful output
if echo "$READER_OUTPUT" | grep -qi "Timestamp"; then
    echo "=== PASS: Converter integration test passed ==="
else
    echo "=== FAIL: Reader did not produce expected timestamp output ==="
    exit 1
fi

# Cleanup
rm -f "$OSI_FILE" "$MCAP_FILE"
echo "=== Integration test passed ==="
