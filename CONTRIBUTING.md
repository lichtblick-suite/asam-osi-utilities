<!--
SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
SPDX-License-Identifier: MPL-2.0
-->

# Contributing

See the full [Contributing Guidelines](doc/contributing.md) for detailed information.

## Quick Reference

### Commit Requirements

All commits must:

1. **Follow Conventional Commits** format:

   ```text
   <type>(<scope>): <description>

   Signed-off-by: Your Name <your.email@example.com>
   ```

2. **Include DCO sign-off**: `git commit -s -m "message"`

3. **Be GPG signed**: `git commit -S -s -m "message"`

### Commit Types

| Type       | Description            |
| ---------- | ---------------------- |
| `feat`     | New feature            |
| `fix`      | Bug fix                |
| `docs`     | Documentation changes  |
| `refactor` | Code restructuring     |
| `test`     | Adding or fixing tests |
| `chore`    | Maintenance tasks      |
| `ci`       | CI/CD changes          |

### Getting Started

```bash
# Clone
git clone --recurse-submodules https://github.com/lichtblick-suite/asam-osi-utilities.git
cd asam-osi-utilities

# Setup dev environment
./scripts/setup-dev.sh  # All platforms (use Git Bash on Windows)

# Generate test fixtures (optional — tests skip gracefully without them)
./scripts/generate_test_traces.sh

# Make changes, then commit
git commit -S -s -m "feat: your feature description"
```

### Root Makefile Commands

```bash
# From repository root (recommended)
make setup dev       # Create venv, install dependencies, and install Git hooks
make lint cpp        # Run C++ format checks
make format cpp      # Auto-format C++ code
make test cpp        # Run C++ tests
make lint python     # Run Python lint + format checks
make format python   # Auto-format Python code
make test python     # Run Python tests
make run docs        # Build and serve docs locally
```

The root Makefile is the primary entry point for all tasks (C++ and Python). Run `make help` or `make <group> help` for the available subcommands.

### Python Development

```bash
# Or from python/ directory
cd python
make setup      # Create venv and install dependencies
make test       # Run tests
make lint       # Run linter
make format     # Format code
make typecheck  # Run type checker
```

The `python/Makefile` provides additional Python-specific targets.

## Documentation

- [Full Contributing Guide](doc/contributing.md) - Complete guidelines
- [Development Setup](doc/development.md) - Dev environment setup
- [Building](doc/building.md) - Build instructions
