<!--
SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
SPDX-License-Identifier: MPL-2.0
-->

# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

C++17 library for reading/writing OSI (Open Simulation Interface) trace files in MCAP, binary `.osi`, and text `.txth` formats. Maintained by BMW AG under MPL-2.0.

## Build Commands

```bash
# Prerequisites: ensure submodules are initialized
git submodule update --init --recursive

# Configure and build (vcpkg preset — recommended)
cmake --preset vcpkg
cmake --build --preset vcpkg --parallel

# Configure and build with tests
cmake --preset vcpkg -DBUILD_TESTING=ON -DVCPKG_MANIFEST_FEATURES=tests
cmake --build --preset vcpkg --parallel

# Run all tests
ctest --test-dir build-vcpkg --output-on-failure

# Run a single test (by name regex)
ctest --test-dir build-vcpkg --output-on-failure -R <test_name>

# Base preset (no vcpkg, manual deps)
cmake --preset base -DBUILD_TESTING=ON
cmake --build --preset base --parallel
ctest --test-dir build --output-on-failure
```

Note: the `vcpkg` preset writes to `build-vcpkg/`, the `base` preset writes to `build/`.

## Formatting & Linting

```bash
# Install pre-commit hooks (run once after clone)
./scripts/setup-dev.sh          # All platforms (use Git Bash on Windows)

# Check formatting only
.git/hooks/pre-commit --all-files --skip-tidy

# Check linting only (needs compile_commands.json from a build)
.git/hooks/pre-commit --all-files --run-tidy --skip-format

# Both
.git/hooks/pre-commit --all-files --run-tidy
```

Style: Google-based clang-format with **4-space indentation** and **180-column limit** (see `.clang-format`). Static analysis via clang-tidy (see `.clang-tidy`).

## Architecture

**Reader/Writer abstraction with factory pattern:**

- `include/osi-utilities/tracefile/Reader.h` — base `TraceFileReader` with `TraceFileReaderFactory::createReader(path)` that picks the right reader by file extension
- `include/osi-utilities/tracefile/Writer.h` — base `TraceFileWriter`
- Three format implementations under `reader/` and `writer/`: `MCAPTraceFile*`, `SingleChannelBinaryTraceFile*`, `TXTHTraceFile*`

**Key types:**

- `ReaderTopLevelMessage` enum — 10 OSI message types (GroundTruth, SensorData, SensorView, etc.)
- `ReadResult` — parsed message + type + channel name
- `kFileNameMessageTypeMap` — maps filename patterns to message types

**Configuration:**

- `include/osi-utilities/tracefile/TraceFileConfig.h` — chunk size constants (default 16 MiB) and binary format constants

**Submodules in `lib/`:**

- `osi-cpp/` — OSI protobuf definitions (has nested `open-simulation-interface` submodule)
- `mcap/` — MCAP C++ library

## Commit & PR Requirements

All commits must use **Conventional Commits** format and be both **DCO signed-off** (`-s`) and **GPG signed** (`-S`):

```bash
git commit -S -s -m "feat(tracefile): add new capability"
```

Valid types: `feat`, `fix`, `docs`, `refactor`, `test`, `chore`, `ci`, `perf`, `build`, `style`

PR bodies must follow `.github/pull_request_template.md` with sections: Related Issue, Summary, Validation, Checklist.

## Naming Conventions (from .clang-tidy)

- Classes/Structs: `CamelCase`
- Variables: `lower_case`
- Member variables: `lower_case_` (trailing underscore)
- Constants/Enums: `kCamelCase` (k-prefix)
- Functions: flexible (matches existing patterns)

## Testing

GoogleTest framework. Tests in `tests/src/` following `*_test.cpp` pattern. Common helpers in `tests/src/TestUtilities.h`. Test target name: `unit_tests`.
