<!--
SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
SPDX-License-Identifier: MPL-2.0
-->

# Development Setup

This page covers environment setup only. For day-to-day commands, see [Developer Workflow](workflow.md).

## Quick Setup

Run the setup script to configure hooks and check tools:

```bash
./scripts/setup-dev.sh
```

> **Windows:** Run from Git Bash, or use `bash scripts/setup-dev.sh` from PowerShell/CMD.

The setup script installs native Git hooks (format checks, Python lint, and commit message validation).

> **Pinned formatter:** C++ formatting is pinned to `clang-format` major version `18`. The setup script and hooks reject other major versions so local formatting matches CI.

## Required Tools

All package names are defined in [`.github/dependencies.yml`](../.github/dependencies.yml).

### Linux (Debian/Ubuntu)

```bash
sudo apt update
sudo apt install -y yq
yq -r '(.dependencies.dev.apt + .dependencies.lint.apt + .dependencies.docs.apt) | unique | .[]' .github/dependencies.yml | \
  xargs sudo apt install -y
```

### Linux (Fedora/RHEL)

```bash
sudo dnf install -y yq
yq -r '(.dependencies.dev.dnf + .dependencies.lint.dnf + .dependencies.docs.dnf) | unique | .[]' .github/dependencies.yml | \
  xargs sudo dnf install -y
```

Then verify the pinned formatter version:

```bash
clang-format --version
# expected: clang-format version 18.x
```

### Windows (winget)

```powershell
# yq (used to read .github/dependencies.yml)
winget install --id MikeFarah.yq --source winget

# Install from .github/dependencies.yml (excluding LLVM; clang-format is pinned separately)
yq -r '(.dependencies.dev.winget + .dependencies.docs.winget) | unique | .[]' .github/dependencies.yml |
  Where-Object { $_ -ne "LLVM.LLVM" } |
  ForEach-Object { winget install --id $_ --source winget }

# Install pinned LLVM / clang-format 18.x
winget show LLVM.LLVM --versions
winget install --id LLVM.LLVM --source winget --version <latest-18.x>

# Ensure the LLVM bin directory is on PATH, then verify:
clang-format.exe --version
```

> If a specific `18.x` package is no longer listed by `winget show`, install the latest available LLVM `18.x` release and keep that `bin` directory on `PATH`.

### macOS (vcpkg only)

macOS builds are supported only via vcpkg. Install the host tools first:

```bash
brew install yq
yq -r '.dependencies.vcpkg_host.brew | unique | .[]' .github/dependencies.yml | \
  xargs brew install
```

Optional tooling (format/lint/docs):

```bash
yq -r '(.dependencies.dev.brew + .dependencies.lint.brew + .dependencies.docs.brew) | unique | .[]' .github/dependencies.yml | \
  xargs brew install

# Add to PATH (llvm@18 is keg-only)
export PATH="$(brew --prefix llvm@18)/bin:$PATH"

# Verify pinned formatter version
clang-format --version
```

## Git Hooks

Hooks are generated into Git's hook directory (`git rev-parse --git-path hooks`) by the setup scripts. Use them in the workflow page:

- Run checks: `$(git rev-parse --git-path hooks)/pre-commit`
- Run checks on all files: `$(git rev-parse --git-path hooks)/pre-commit --all-files`
- Run clang-tidy: `$(git rev-parse --git-path hooks)/pre-commit --run-tidy`
- Run docs build: `$(git rev-parse --git-path hooks)/pre-commit --run-docs`
- Auto-fix C++ formatting: `$(git rev-parse --git-path hooks)/pre-commit --fix-format`
- Auto-fix clang-tidy: `$(git rev-parse --git-path hooks)/pre-commit --fix-tidy`
- Auto-fix Python formatting: `$(git rev-parse --git-path hooks)/pre-commit --fix-python`
- Skip C++ formatting: `$(git rev-parse --git-path hooks)/pre-commit --skip-format`
- Skip clang-tidy: `$(git rev-parse --git-path hooks)/pre-commit --skip-tidy`
- Skip Python checks: `$(git rev-parse --git-path hooks)/pre-commit --skip-python`
- Skip docs build: `$(git rev-parse --git-path hooks)/pre-commit --skip-docs`

Notes:

- clang-tidy and docs build are opt-in. Use `--run-tidy` or `--run-docs` to enable them.
- Python ruff checks run by default when a venv with ruff is detected.
- clang-tidy requires `compile_commands.json` (see [Developer Workflow](workflow.md)).
- The hook resolves `clang-format` major version `18` via `scripts/resolve-clang-format.sh`. Set `OSIUTILITIES_CLANG_FORMAT` if the pinned binary lives in a non-standard location.
