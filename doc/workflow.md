<!--
SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
SPDX-License-Identifier: MPL-2.0
-->

# Developer Workflow

This page contains day-to-day commands. For environment setup, see [Development Setup](setup.md).

## Update Repository

```bash
git pull --recurse-submodules
# If needed:
git submodule update --init --recursive
```

## Build and Test

**vcpkg preset:**

```bash
cmake --preset vcpkg
cmake --build --preset vcpkg --parallel
ctest --test-dir build-vcpkg --output-on-failure
```

Notes:

- On Windows, prefer `vcpkg-windows-static-md` for static libraries with dynamic MSVC runtime.
- Tests are opt-in. Configure with `-DBUILD_TESTING=ON` and ensure GTest is available (for vcpkg: set `VCPKG_MANIFEST_FEATURES=tests`).

## Format and Lint

### Using make targets (recommended)

```bash
make lint           # All linters (C++ format + Python ruff)
make lint cpp       # C++ format checks only
make lint python    # Python ruff lint + format checks
make format         # Auto-format C++ and Python code
make format cpp     # Auto-format C++ code
make format python  # Auto-format Python code
make run help       # Show run subcommands
```

`make lint cpp`, `make format cpp`, and the generated Git hook all require `clang-format` major version `18` so local checks match CI.

### Using hooks directly

```bash
$(git rev-parse --git-path hooks)/pre-commit
$(git rev-parse --git-path hooks)/pre-commit --run-tidy
$(git rev-parse --git-path hooks)/pre-commit --fix-format
$(git rev-parse --git-path hooks)/pre-commit --fix-tidy
$(git rev-parse --git-path hooks)/pre-commit --fix-python
$(git rev-parse --git-path hooks)/pre-commit --all-files --run-tidy --fix-format --fix-tidy
$(git rev-parse --git-path hooks)/pre-commit --all-files --skip-python
$(git rev-parse --git-path hooks)/pre-commit --skip-format
$(git rev-parse --git-path hooks)/pre-commit --skip-tidy
```

Notes:

- clang-tidy is opt-in. Use `--run-tidy` (or `--fix-tidy`) to enable it.
- It requires `compile_commands.json`. Generate it using the ninja preset (run from Developer Command Prompt on Windows):
  ```bash
  cmake --preset ninja
  ```

## Commit

```bash
git commit -S -s -m "feat: short description"
```
