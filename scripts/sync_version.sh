#!/bin/bash
#
# Copyright (c) 2024, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# SPDX-License-Identifier: MPL-2.0
#
# Synchronize version from VERSION file to vcpkg.json and Doxyfile.in
# This script ensures all version references stay in sync.
# Run this script when updating the version.

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

VERSION_FILE="$ROOT_DIR/VERSION"
VCPKG_FILE="$ROOT_DIR/vcpkg.json"
DOXYFILE="$ROOT_DIR/doc/Doxyfile.in"

# Read version from VERSION file
if [ ! -f "$VERSION_FILE" ]; then
    echo "ERROR: VERSION file not found at $VERSION_FILE"
    exit 1
fi

VERSION=$(cat "$VERSION_FILE" | tr -d '[:space:]')

# Validate version format (MAJOR.MINOR.PATCH)
if ! echo "$VERSION" | grep -qE '^[0-9]+\.[0-9]+\.[0-9]+$'; then
    echo "ERROR: Invalid version format '$VERSION'. Expected MAJOR.MINOR.PATCH"
    exit 1
fi

echo "Version from VERSION file: $VERSION"

# Update vcpkg.json
if [ -f "$VCPKG_FILE" ]; then
    # Extract current version
    OLD_VERSION=$(grep -o '"version-string"[[:space:]]*:[[:space:]]*"[^"]*"' "$VCPKG_FILE" | sed 's/.*"\([^"]*\)"$/\1/')
    
    # Update version-string using sed
    if [ "$(uname)" = "Darwin" ]; then
        # macOS sed requires different syntax
        sed -i '' "s/\"version-string\"[[:space:]]*:[[:space:]]*\"[^\"]*\"/\"version-string\": \"$VERSION\"/" "$VCPKG_FILE"
    else
        sed -i "s/\"version-string\"[[:space:]]*:[[:space:]]*\"[^\"]*\"/\"version-string\": \"$VERSION\"/" "$VCPKG_FILE"
    fi
    
    if [ "$OLD_VERSION" != "$VERSION" ]; then
        echo "Updated vcpkg.json: $OLD_VERSION -> $VERSION"
    else
        echo "vcpkg.json already at version $VERSION"
    fi
else
    echo "WARNING: vcpkg.json not found at $VCPKG_FILE"
fi

# Update Doxyfile.in
if [ -f "$DOXYFILE" ]; then
    # Extract current version
    OLD_DOXY_VERSION=$(grep '^PROJECT_NUMBER' "$DOXYFILE" | sed 's/PROJECT_NUMBER[[:space:]]*=[[:space:]]*//')
    
    # Update PROJECT_NUMBER
    if [ "$(uname)" = "Darwin" ]; then
        sed -i '' "s/^PROJECT_NUMBER[[:space:]]*=.*/PROJECT_NUMBER         = $VERSION/" "$DOXYFILE"
    else
        sed -i "s/^PROJECT_NUMBER[[:space:]]*=.*/PROJECT_NUMBER         = $VERSION/" "$DOXYFILE"
    fi
    
    if [ "$OLD_DOXY_VERSION" != "$VERSION" ]; then
        echo "Updated Doxyfile.in PROJECT_NUMBER to $VERSION"
    else
        echo "Doxyfile.in already at version $VERSION"
    fi
else
    echo "WARNING: Doxyfile.in not found at $DOXYFILE"
fi

echo "Version synchronization complete!"
