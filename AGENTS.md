<!--
SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
SPDX-License-Identifier: MPL-2.0
-->

# Repository Guidelines

## Project Structure & Module Organization

The library implementation lives in `src/`, with public headers in `include/osi-utilities/`. Unit tests are in `tests/src/`, and example executables live in `examples/`. Documentation is in `doc/`, CMake helpers in `cmake/`, and developer scripts in `scripts/`. Third-party code is tracked as submodules in `lib/` (`mcap/`, `osi-cpp/`). Build artifacts should go in `build/` (generated). Dependency manifests are `vcpkg.json` and `vcpkg-configuration.json`.

## Build, Test, and Development Commands

- Update submodules: `git pull --recurse-submodules` (or `git submodule update --init --recursive`).
- Install hooks/tools: `./scripts/setup-dev.sh` (all platforms; use Git Bash on Windows).
- Configure with vcpkg: `cmake --preset vcpkg`.
- Build: `cmake --build --preset vcpkg --parallel`.
- Run tests: `ctest --test-dir build --output-on-failure`.
- For linting, ensure `build/compile_commands.json` exists (e.g., `cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON`).

## Coding Style & Naming Conventions

Formatting is enforced by `clang-format` with a Google-based style (`.clang-format`): 4-space indentation, no tabs, and a 180-column limit. Static analysis uses `clang-tidy` when available. Prefer running `.git/hooks/pre-commit` (or `--fix`) after the setup script installs hooks. Keep public APIs in `include/osi-utilities/` with matching implementations in `src/`, and follow existing naming patterns in nearby files.

## Testing Guidelines

Tests are written with GoogleTest and registered via CTest (`tests/CMakeLists.txt` uses `gtest_discover_tests`). Test files follow the `*_test.cpp` naming pattern under `tests/src/`. Run the full suite with `ctest --test-dir build --output-on-failure`. CI collects coverage on Ubuntu for validation; add tests alongside new behavior.

## Commit & Pull Request Guidelines

Recent history uses Conventional Commits (e.g., `feat: ...`, `fix: ...`, `ci(fix): ...`), and the project requires that format for all new commits. Use `<type>(<scope>): <description>` with optional scopes. All commits must be DCO signed-off and GPG signed: `git commit -S -s -m "feat: short description"`. PR titles should use the same conventional prefix, include a clear description, and pass format, lint, and test checks. At least one maintainer approval is required before merge.

PR bodies must follow `.github/pull_request_template.md` and include all sections:

- `Related Issue` with a valid reference (`closes #...`, `fixes #...`, or `refs #...`).
- `Summary` describing what changed and why.
- `Validation` listing local verification commands and outcomes.
- `Checklist` completed accurately. If an item is not applicable, mark as N/A and provide a short reason in `Summary` or `Validation`.

PR checklist:

- Conventional Commit title and message.
- DCO sign-off and GPG signature on every commit.
- `.git/hooks/pre-commit` clean (format + lint).
- `ctest --test-dir build --output-on-failure` passes.
- Docs/tests updated when behavior changes.

## Agent Workflow Guardrails

When creating or updating PRs, agents should follow this sequence:

1. Sync with `main` using rebase (avoid merge commits in PR branches).
2. Ensure every commit is Conventional Commit compliant.
3. Ensure every commit is signed (`-s` for DCO and `-S` for GPG).
4. Fill the PR body using `.github/pull_request_template.md` headings verbatim.
5. Verify repository links use the canonical path `https://github.com/lichtblick-suite/asam-osi-utilities`.

Recommended pre-push checks for agents:

- `git log --format=%H%x09%s origin/main..HEAD` (confirm commit subjects).
- `git log --format=%H%x09%B origin/main..HEAD` (confirm `Signed-off-by` trailers).
- `git rebase origin/main` (remove accidental merge commits before pushing).

## Versioning & Releases

Release versions are stored in `vcpkg.json` (`version-string`). The project follows Semantic Versioning. Releases are triggered via the **Release** workflow (manual dispatch). See `doc/releasing.md` for full details.

API docs are generated via the `library_api_doc` CMake target and published from `doc/`.
