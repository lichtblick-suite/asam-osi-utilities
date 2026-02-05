# CI/CD Pipeline

This document describes the Continuous Integration and Continuous Deployment (CI/CD) pipeline for ASAM OSI Utilities.

## Overview

The CI/CD pipeline is split into modular workflow files for clarity and maintainability:

| File                | Type         | Description                             |
| ------------------- | ------------ | --------------------------------------- |
| `ci.yml`            | Orchestrator | Main workflow that triggers all CI jobs |
| `ci_format.yml`     | CI           | Code formatting checks                  |
| `ci_lint.yml`       | CI           | Static analysis with clang-tidy         |
| `ci_build.yml`      | CI           | Build and test on all platforms         |
| `ci_compliance.yml` | CI           | DCO and Conventional Commits checks     |
| `cd_docs.yml`       | CD           | Build and deploy documentation          |

## Naming Convention

- **`ci_*`** - Continuous Integration workflows (build, test, lint)
- **`cd_*`** - Continuous Deployment workflows (documentation, releases)

## Workflow Triggers

### On Pull Request

When a PR is opened or updated against `main` or `develop`:

1. **Format Check** - Verify code follows clang-format style
2. **Lint** - Run clang-tidy static analysis
3. **Build** - Compile on Ubuntu, Windows, and macOS
4. **Test** - Run unit tests on all platforms
5. **DCO Check** - Verify all commits have DCO sign-off
6. **Conventional Commits** - Verify commit message format

### On Push to Main

When code is pushed to `main`:

1. All CI checks run
2. **Documentation Deployment** - Build and deploy Doxygen docs to GitHub Pages

## Workflow Details

### ci_format.yml - Format Check

Verifies C++ code formatting using clang-format-18.

```yaml
# Checks files in: src/, include/, tests/, examples/
find src include tests examples -name "*.cpp" -o -name "*.h" | \
xargs clang-format-18 --dry-run --Werror
```

**Fails if:** Any file is not properly formatted.

### ci_lint.yml - Static Analysis

Runs clang-tidy via the pre-commit hook (clang-tidy is opt-in by default in hooks).

```yaml
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
bash scripts/setup-dev.sh
.git/hooks/pre-commit --all-files --run-tidy --skip-format
```

**Fails if:** Any clang-tidy warning is emitted.

### ci_build.yml - Build Matrix

Builds the project on multiple platforms and configurations:

| Platform         | Compiler    | Method      |
| ---------------- | ----------- | ----------- |
| Ubuntu 22.04     | GCC         | Manual deps |
| Ubuntu 22.04     | Clang       | Manual deps |
| Ubuntu 24.04     | GCC         | Manual deps |
| Ubuntu 24.04     | Clang       | Manual deps |
| Ubuntu Latest    | -           | vcpkg       |
| Windows Latest   | MSVC        | vcpkg       |
| macOS 15 (x64)   | Apple Clang | vcpkg       |
| macOS 14 (arm64) | Apple Clang | vcpkg       |

### Unit Tests (in ci_build.yml)

Tests are run as part of the build workflow using CTest:

```yaml
ctest --output-on-failure --parallel $(nproc)
```

On Ubuntu 24.04 with GCC, code coverage is collected and uploaded to Codecov.

### ci_compliance.yml - Compliance Checks

**DCO Check:**

- Verifies all commits have `Signed-off-by:` line
- Required for all pull requests

**Conventional Commits Check:**

- Verifies commit messages follow `<type>(<scope>): <description>` format
- Skips merge and revert commits
- Required for all pull requests

### cd_docs.yml - Documentation Deployment

Builds Doxygen documentation and deploys to GitHub Pages:

1. Configure CMake with `-DOSIUTILITIES_DOCS_ONLY=ON` (docs-only configuration)
2. Build `library_api_doc` target
3. Deploy `doc/html/` to GitHub Pages
4. Available at: `https://lichtblick-suite.github.io/asam-osi-utilities/`

**Triggers:** Push to `main` branch only.

## Concurrency

All workflows use concurrency groups to cancel in-progress runs when new commits are pushed:

```yaml
concurrency:
  group: ${{ github.workflow }}-${{ github.ref }}
  cancel-in-progress: true
```

This saves CI resources and provides faster feedback.

## Required Status Checks

For PRs to be mergeable, the following checks must pass:

- [ ] Format Check
- [ ] Clang-Tidy
- [ ] Build (all platforms)
- [ ] Tests (all platforms)
- [ ] DCO Sign-off Check
- [ ] Conventional Commits Check

## Badge Status

Add these badges to track CI status:

```markdown
[![CI](https://github.com/Lichtblick-Suite/asam-osi-utilities/actions/workflows/ci.yml/badge.svg)](https://github.com/Lichtblick-Suite/asam-osi-utilities/actions/workflows/ci.yml)
```

## Local CI Validation

Before pushing, you can run most CI checks locally:

### Format Check

```bash
find src include tests examples -name "*.cpp" -o -name "*.h" | \
  xargs clang-format --dry-run --Werror
```

### Build and Test

```bash
cmake --preset vcpkg-linux
cmake --build build -j
ctest --test-dir build --output-on-failure
```

## Troubleshooting

### CI Fails but Local Passes

- Ensure you're using the same clang-format version (18)
- Check that all submodules are initialized
- Verify you're building with the same CMake preset

### DCO Check Fails

Add sign-off to existing commits:

```bash
git rebase --signoff HEAD~N
git push --force-with-lease
```

### Conventional Commits Fails

Reword commit messages:

```bash
git rebase -i HEAD~N
# Change 'pick' to 'reword' for commits to fix
```

## Adding New Workflows

When adding new workflows:

1. Use the naming convention (`ci_*` or `cd_*`)
2. Add appropriate triggers
3. Include concurrency settings
4. Update this documentation
5. Add to the orchestrator workflow if needed
