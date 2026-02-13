#!/usr/bin/env bash
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# SPDX-License-Identifier: MPL-2.0
#
# Generate test trace fixtures using esmini + Euro NCAP scenarios.
#
# Phase A: Legacy 5-frame GroundTruth fixture from cut-in.xosc
# Phase B: NCAP scenarios (4 representative Euro NCAP variations)
#          with GT->SV conversion pipeline
#
# Downloads esmini demo release (includes binaries + resources), runs headless
# simulations to produce binary .osi GroundTruth traces, converts GT->SV using
# convert_gt2sv, and produces MCAP files via convert_osi2mcap.
#
# Supported platforms: Linux x64, macOS x64/arm64, Windows (Git Bash / MSYS2)
#
# Prerequisites:
#   - Build the project first with -DBUILD_EXAMPLES=ON:
#     cmake --preset vcpkg -DBUILD_TESTING=ON -DBUILD_EXAMPLES=ON -DVCPKG_MANIFEST_FEATURES=tests
#     cmake --build --preset vcpkg --parallel
#   - curl, unzip, git, python3
#
# Usage:
#   ./scripts/generate_test_traces.sh [--esmini-version <version>]
#
# Output (11 files):
#   tests/data/5frames_gt_esmini.osi       # Phase A - legacy
#   tests/data/5frames_gt_esmini.mcap      # Phase A - legacy
#   tests/data/ccrs_gt_ncap.osi            # Phase B - NCAP GroundTruth
#   tests/data/ccrs_sv_ncap.osi            # Phase B - NCAP SensorView
#   tests/data/ccftap_gt_ncap.osi
#   tests/data/ccftap_sv_ncap.osi
#   tests/data/cpna_gt_ncap.osi
#   tests/data/cpna_sv_ncap.osi
#   tests/data/cbla_gt_ncap.osi
#   tests/data/cbla_sv_ncap.osi
#   tests/data/ccrs_gt_ncap.mcap           # Phase B - one MCAP

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
OUTPUT_DIR="$PROJECT_ROOT/tests/data"
TMP_DIR=$(mktemp -d)

ESMINI_VERSION="2.59.0"
NCAP_REPO="https://github.com/vectorgrp/OSC-NCAP-scenarios"

# Parse arguments
while [[ $# -gt 0 ]]; do
    case "$1" in
        --esmini-version)
            ESMINI_VERSION="$2"
            shift 2
            ;;
        *)
            echo "Unknown argument: $1"
            echo "Usage: $0 [--esmini-version <version>]"
            exit 1
            ;;
    esac
done

# ============================================================================
# Platform detection
# ============================================================================
case "$(uname -s)" in
    Linux*)
        ESMINI_PLATFORM="Linux"
        ESMINI_EXE_NAME="esmini"
        ;;
    Darwin*)
        ESMINI_PLATFORM="macOS"
        ESMINI_EXE_NAME="esmini"
        ;;
    MINGW*|MSYS*|CYGWIN*|Windows_NT)
        ESMINI_PLATFORM="Windows"
        ESMINI_EXE_NAME="esmini.exe"
        ;;
    *)
        echo "ERROR: Unsupported platform: $(uname -s)"
        exit 1
        ;;
esac

# Use the demo package — it includes both binaries and resources (xosc, xodr, models)
ESMINI_ARCHIVE="esmini-demo_${ESMINI_PLATFORM}.zip"
ESMINI_URL="https://github.com/esmini/esmini/releases/download/v${ESMINI_VERSION}/${ESMINI_ARCHIVE}"

# ============================================================================
# Find built tool executables (try vcpkg build first, then base build)
# ============================================================================
find_executable() {
    local name="$1"
    for candidate in \
        "$PROJECT_ROOT/build-vcpkg/examples/${name}" \
        "$PROJECT_ROOT/build-vcpkg/${name}" \
        "$PROJECT_ROOT/build/examples/${name}" \
        "$PROJECT_ROOT/build/${name}"; do
        if [ -x "$candidate" ]; then
            echo "$candidate"
            return 0
        fi
    done
    return 1
}

CONVERTER_OSI2MCAP=$(find_executable "convert_osi2mcap") || CONVERTER_OSI2MCAP=""
CONVERTER_GT2SV=$(find_executable "convert_gt2sv") || CONVERTER_GT2SV=""

if [ -z "$CONVERTER_OSI2MCAP" ]; then
    echo "WARNING: convert_osi2mcap not found. MCAP conversion will be skipped."
    echo "         Build with -DBUILD_EXAMPLES=ON first."
fi
if [ -z "$CONVERTER_GT2SV" ]; then
    echo "WARNING: convert_gt2sv not found. SensorView conversion will be skipped."
    echo "         Build with -DBUILD_EXAMPLES=ON first."
fi

# ============================================================================
# Cleanup — all downloads and intermediates go to TMP_DIR (auto-cleaned)
# ============================================================================
cleanup() {
    rm -rf "$TMP_DIR"
}
trap cleanup EXIT

mkdir -p "$OUTPUT_DIR"

echo "Working directory: $TMP_DIR"
echo ""

# ============================================================================
# Download esmini demo package
# ============================================================================
echo "=== Downloading esmini demo v${ESMINI_VERSION} (${ESMINI_PLATFORM}) ==="
curl -fsSL -o "$TMP_DIR/$ESMINI_ARCHIVE" "$ESMINI_URL"
unzip -q "$TMP_DIR/$ESMINI_ARCHIVE" -d "$TMP_DIR/esmini-extract"

# The demo archive extracts to esmini-demo/ inside the extraction target
ESMINI_DIR="$TMP_DIR/esmini-extract/esmini-demo"
if [ ! -d "$ESMINI_DIR" ]; then
    # Fallback: try to find the extracted directory
    ESMINI_DIR=$(find "$TMP_DIR/esmini-extract" -maxdepth 1 -type d ! -name esmini-extract | head -1)
fi
ESMINI_EXE="$ESMINI_DIR/bin/$ESMINI_EXE_NAME"

if [ ! -f "$ESMINI_EXE" ]; then
    echo "ERROR: esmini executable not found at: $ESMINI_EXE"
    echo "Archive contents:"
    find "$TMP_DIR/esmini-extract" -maxdepth 3 -type d
    exit 1
fi
chmod +x "$ESMINI_EXE" 2>/dev/null || true

echo "esmini binary: $ESMINI_EXE"
echo "esmini resources: $ESMINI_DIR/resources/"

# ============================================================================
# Phase A: Legacy 5-frame fixture
# ============================================================================
echo ""
echo "=========================================="
echo "  Phase A: Legacy 5-frame fixture"
echo "=========================================="

CUTIN_XOSC="$ESMINI_DIR/resources/xosc/cut-in.xosc"
if [ ! -f "$CUTIN_XOSC" ]; then
    echo "ERROR: cut-in.xosc not found at: $CUTIN_XOSC"
    exit 1
fi

echo "=== Running esmini headless simulation (5 frames) ==="
OSI_RAW="$TMP_DIR/gt_output.osi"
"$ESMINI_EXE" \
    --osc "$CUTIN_XOSC" \
    --headless \
    --fixed_timestep 0.1 \
    --osi_file "$OSI_RAW" \
    --osi_freq 1 \
    --disable_stdout \
    --quit_at_end \
    --duration 0.5 \
    || true  # esmini may exit non-zero at end of scenario

if [ ! -f "$OSI_RAW" ]; then
    echo "ERROR: esmini did not produce output file: $OSI_RAW"
    exit 1
fi

echo "=== Trimming to 5 frames ==="
python3 -c "
import struct
with open('$OSI_RAW', 'rb') as fin, open('$TMP_DIR/5frames.osi', 'wb') as fout:
    for _ in range(5):
        hdr = fin.read(4)
        if len(hdr) < 4:
            break
        size = struct.unpack('<I', hdr)[0]
        data = fin.read(size)
        if len(data) < size:
            break
        fout.write(hdr + data)
print('Trimmed to 5 frames')
"

cp "$TMP_DIR/5frames.osi" "$OUTPUT_DIR/5frames_gt_esmini.osi"
echo "  -> $OUTPUT_DIR/5frames_gt_esmini.osi"

if [ -n "$CONVERTER_OSI2MCAP" ]; then
    echo "=== Converting to MCAP ==="
    "$CONVERTER_OSI2MCAP" "$OUTPUT_DIR/5frames_gt_esmini.osi" "$OUTPUT_DIR/5frames_gt_esmini.mcap" \
        --input-type GroundTruth
    echo "  -> $OUTPUT_DIR/5frames_gt_esmini.mcap"
fi

echo "Phase A complete."

# ============================================================================
# Phase B: NCAP scenarios
# ============================================================================
echo ""
echo "=========================================="
echo "  Phase B: NCAP scenarios"
echo "=========================================="

# Clone NCAP scenarios repository into temp dir
NCAP_DIR="$TMP_DIR/ncap-scenarios"
echo "=== Cloning OSC-NCAP-scenarios ==="
git clone --depth 1 "$NCAP_REPO" "$NCAP_DIR"

# Define scenarios as "code:variation_file" pairs (no associative arrays — macOS ships bash 3.2)
NCAP_SCENARIOS="
ccrs:OpenSCENARIO/NCAP/AEB_C2C_2023/Variations/NCAP_AEB_C2C_CCRs_50kph_2023.xosc
ccftap:OpenSCENARIO/NCAP/AEB_C2C_2023/Variations/NCAP_AEB_C2C_CCFtap_10kph_30kph_2023.xosc
cpna:OpenSCENARIO/NCAP/AEB_VRU_2023/Variations/NCAP_AEB_VRU_CPNA-25_50kph_2023.xosc
cbla:OpenSCENARIO/NCAP/AEB_VRU_2023/Variations/NCAP_AEB_VRU_CBLA-50_50kph_2023.xosc
"

for entry in $NCAP_SCENARIOS; do
    code="${entry%%:*}"
    variation="${entry#*:}"
    xosc_path="$NCAP_DIR/$variation"

    if [ ! -f "$xosc_path" ]; then
        echo "WARNING: Variation file not found: $xosc_path"
        echo "         Skipping $code"
        continue
    fi

    echo ""
    echo "--- Running NCAP scenario: $code ---"
    echo "    Variation: $variation"

    OSI_TMP="$TMP_DIR/${code}_raw.osi"
    "$ESMINI_EXE" \
        --osc "$xosc_path" \
        --headless \
        --fixed_timestep 0.1 \
        --osi_file "$OSI_TMP" \
        --osi_freq 1 \
        --disable_stdout \
        --quit_at_end \
        || true  # esmini may exit non-zero

    # esmini appends a suffix like _1_of_1 for parameterized scenarios
    if [ ! -f "$OSI_TMP" ]; then
        OSI_ACTUAL=$(find "$TMP_DIR" -maxdepth 1 -name "${code}_raw*.osi" -type f | head -1)
        if [ -n "$OSI_ACTUAL" ]; then
            mv "$OSI_ACTUAL" "$OSI_TMP"
        fi
    fi

    if [ ! -f "$OSI_TMP" ]; then
        echo "WARNING: esmini did not produce output for $code — skipping."
        continue
    fi

    # Copy GT trace to output
    GT_OUT="$OUTPUT_DIR/${code}_gt_ncap.osi"
    cp "$OSI_TMP" "$GT_OUT"
    echo "    GT: $GT_OUT ($(du -h "$GT_OUT" | cut -f1))"

    # Convert GT -> SV
    SV_OUT="$OUTPUT_DIR/${code}_sv_ncap.osi"
    if [ -n "$CONVERTER_GT2SV" ]; then
        "$CONVERTER_GT2SV" "$GT_OUT" "$SV_OUT"
        echo "    SV: $SV_OUT ($(du -h "$SV_OUT" | cut -f1))"
    else
        echo "    SKIP: convert_gt2sv not available"
    fi
done

# Convert one GT trace (ccrs) to MCAP
if [ -n "$CONVERTER_OSI2MCAP" ] && [ -f "$OUTPUT_DIR/ccrs_gt_ncap.osi" ]; then
    echo ""
    echo "=== Converting ccrs GT to MCAP ==="
    "$CONVERTER_OSI2MCAP" "$OUTPUT_DIR/ccrs_gt_ncap.osi" "$OUTPUT_DIR/ccrs_gt_ncap.mcap" \
        --input-type GroundTruth
    echo "    MCAP: $OUTPUT_DIR/ccrs_gt_ncap.mcap"
fi

# ============================================================================
# Summary
# ============================================================================
echo ""
echo "=========================================="
echo "  Generation complete"
echo "=========================================="
echo "Generated fixtures:"
ls -lh "$OUTPUT_DIR"/ 2>/dev/null || true
