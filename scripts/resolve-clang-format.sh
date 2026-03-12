#!/bin/sh
#
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# SPDX-License-Identifier: MPL-2.0
#

set -eu

PINNED_CLANG_FORMAT_MAJOR=18
MODE="${1:-resolve}"

print_diagnostic() {
    cat >&2 <<EOF
ERROR: clang-format major version ${PINNED_CLANG_FORMAT_MAJOR} is required for this repository.

Install a pinned formatter for your platform:
  - Debian/Ubuntu: sudo apt install clang-format-${PINNED_CLANG_FORMAT_MAJOR}
  - Fedora/RHEL: install clang-format ${PINNED_CLANG_FORMAT_MAJOR}.x and ensure it is on PATH
  - macOS (Homebrew): brew install llvm@${PINNED_CLANG_FORMAT_MAJOR}
      export PATH="\$(brew --prefix llvm@${PINNED_CLANG_FORMAT_MAJOR})/bin:\$PATH"
  - Windows (winget): install LLVM ${PINNED_CLANG_FORMAT_MAJOR}.x and ensure clang-format.exe is on PATH
      winget show LLVM.LLVM --versions
      winget install --id LLVM.LLVM --source winget --version <latest-18.x>

Override automatic discovery with:
  OSIUTILITIES_CLANG_FORMAT=/absolute/path/to/clang-format
EOF
}

extract_major_version() {
    "$1" --version 2>/dev/null | sed -n 's/.*version \([0-9][0-9]*\)\(\.[0-9][^ ]*\)\?.*/\1/p' | head -n 1
}

resolve_candidate() {
    candidate="$1"
    [ -n "$candidate" ] || return 1

    if [ ! -x "$candidate" ]; then
        if ! command -v "$candidate" >/dev/null 2>&1; then
            return 1
        fi
        candidate="$(command -v "$candidate")"
    fi

    major_version="$(extract_major_version "$candidate")"
    [ "$major_version" = "$PINNED_CLANG_FORMAT_MAJOR" ] || return 1

    printf '%s\n' "$candidate"
    return 0
}

if [ -n "${OSIUTILITIES_CLANG_FORMAT:-}" ]; then
    if resolve_candidate "$OSIUTILITIES_CLANG_FORMAT"; then
        exit 0
    fi
fi

if resolve_candidate "clang-format-${PINNED_CLANG_FORMAT_MAJOR}"; then
    exit 0
fi

if resolve_candidate "clang-format"; then
    exit 0
fi

if command -v brew >/dev/null 2>&1; then
    BREW_LLVM_PREFIX="$(brew --prefix "llvm@${PINNED_CLANG_FORMAT_MAJOR}" 2>/dev/null || true)"
    if [ -n "$BREW_LLVM_PREFIX" ] && resolve_candidate "${BREW_LLVM_PREFIX}/bin/clang-format"; then
        exit 0
    fi
fi

for candidate in \
    "/c/Program Files/LLVM/bin/clang-format.exe" \
    "/c/Program Files/LLVM/bin/clang-format" \
    "/mnt/c/Program Files/LLVM/bin/clang-format.exe" \
    "/mnt/c/Program Files/LLVM/bin/clang-format"; do
    if resolve_candidate "$candidate"; then
        exit 0
    fi
done

if [ "$MODE" = "--diagnose" ]; then
    print_diagnostic
fi

exit 1
