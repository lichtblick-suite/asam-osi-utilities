# Development Setup

This guide covers setting up your development environment for contributing to ASAM OSI Utilities.

## Quick Setup

Run the setup script to configure your development environment. **No Python required** - the setup uses native Git hooks.

**Linux/macOS:**

```bash
./scripts/setup-dev.sh
```

**Windows PowerShell:**

```powershell
.\scripts\setup-dev.ps1
```

The setup script will:

- Install native Git hooks for code formatting checks
- Configure commit message hooks for DCO sign-off and Conventional Commits
- Check for required C++ tools (clang-format, CMake)
- Provide guidance for GPG signing setup

## Required Tools

| Tool         | Required    | Purpose                |
| ------------ | ----------- | ---------------------- |
| Git          | Yes         | Version control        |
| CMake        | Yes         | Build system           |
| clang-format | Recommended | Code formatting        |
| clang-tidy   | Optional    | Static analysis        |
| Graphviz     | Optional    | Doxygen diagrams (dot) |

### Installing Development Tools

**Linux (Debian/Ubuntu):**

```bash
sudo apt update
sudo apt install clang-format clang-tidy graphviz
```

**Linux (Fedora/RHEL):**

```bash
sudo dnf install clang-tools-extra graphviz
```

**Windows (winget):**

```powershell
# LLVM includes clang-format and clang-tidy
winget install --id LLVM.LLVM --source winget

# Graphviz provides the 'dot' tool for Doxygen diagrams
winget install --id Graphviz.Graphviz --source winget
```

> **Note:** If `winget` fails while searching the `msstore` source (e.g. certificate/proxy issues),
> force the community repository with `--source winget` or disable the store source:
> `winget source disable msstore`.

> **Graphviz on Windows (troubleshooting):** verify Graphviz works in PowerShell with `dot -V`.
> If `dot` fails in Git Bash/MSYS with `error while loading shared libraries` and a missing
> `libltdl-7.dll`, install MSYS2 and the missing runtime:
>
> - `winget install --id MSYS2.MSYS2 --source winget`
> - In the **MSYS2 MinGW x64** shell: `pacman -Syu` then `pacman -S --needed mingw-w64-x86_64-libtool`
> - Add `C:\msys64\mingw64\bin` to PATH (or copy `libltdl-7.dll` next to `dot.exe`).

> **Note:** After installing LLVM on Windows, add `C:\Program Files\LLVM\bin` to your PATH if not done automatically.

**macOS (Homebrew):**

```bash
brew install llvm graphviz
# Add to PATH (llvm is keg-only)
export PATH="$(brew --prefix llvm)/bin:$PATH"
```

## Manual Setup

If you prefer to set up hooks manually, run the setup script which generates the hooks:

**Linux/macOS:**

```bash
./scripts/setup-dev.sh
```

**Windows PowerShell:**

```powershell
.\scripts\setup-dev.ps1
```

The hooks are generated directly in `.git/hooks/` by the setup scripts.

## Git Hooks

This project uses native Git hooks to ensure code quality before commits:

| Hook         | Trigger       | Description                                            |
| ------------ | ------------- | ------------------------------------------------------ |
| `pre-commit` | Before commit | Checks C++ code formatting with clang-format           |
| `commit-msg` | After message | Validates DCO sign-off and Conventional Commits format |

### Running Checks Manually

Check formatting on all C++ files:

```bash
find src include tests examples -name "*.cpp" -o -name "*.h" | xargs clang-format --dry-run --Werror
```

Fix formatting on all C++ files:

```bash
find src include tests examples -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

## Code Style

This project uses:

- **clang-format** for C++ code formatting (Google style base with modifications)
- **clang-tidy** for static analysis

Configuration files:

- `.clang-format` - Code formatting rules
- `.clang-tidy` - Static analysis checks

### Running Format Checks Manually

Check formatting:

```bash
find src include tests examples -name "*.cpp" -o -name "*.h" | xargs clang-format --dry-run --Werror
```

Apply formatting:

```bash
find src include tests examples -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

### Running Clang-Tidy Manually

First, configure CMake with compile commands export:

```bash
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
cmake --build build
```

Then run clang-tidy:

```bash
clang-tidy -p build src/**/*.cpp
```

## License Headers

All source files must include the MPL-2.0 license header. The pre-commit hook checks for this automatically.

Example header for `.cpp` and `.h` files:

```cpp
// SPDX-License-Identifier: MPL-2.0
// Copyright (c) 2024 Your Name or Organization
```

## GPG Signing

GPG signed commits are required for all contributions. See the [Contributing Guide](contributing.md#gpg-signed-commits) for setup instructions.

Quick setup:

```bash
# Generate key (if needed)
gpg --full-generate-key

# Get key ID
gpg --list-secret-keys --keyid-format=long

# Configure Git
git config --global user.signingkey YOUR_KEY_ID
git config --global commit.gpgsign true
```

## Editor Integration

### VS Code

Recommended extensions:

- **C/C++** - IntelliSense and debugging
- **CMake Tools** - CMake integration
- **clangd** - Better C++ language server
- **EditorConfig** - Consistent editor settings

Settings for VS Code (`.vscode/settings.json`):

```json
{
  "editor.formatOnSave": true,
  "C_Cpp.formatting": "clangFormat",
  "C_Cpp.clang_format_style": "file"
}
```

### CLion

CLion automatically detects `.clang-format` and `.clang-tidy` configuration files.

Enable format on save:

- Settings → Editor → Code Style → Enable "Reformat code on file save"

## Running Tests

Build and run tests:

```bash
cmake --preset vcpkg-linux
cmake --build build -j
ctest --test-dir build --output-on-failure
```

Run a specific test:

```bash
ctest --test-dir build -R "test_name_pattern"
```

Run tests with verbose output:

```bash
ctest --test-dir build -V
```

## Code Coverage

Enable code coverage (GCC only):

```bash
cmake -B build -DCODE_COVERAGE=ON
cmake --build build
ctest --test-dir build
gcovr -r . build --xml -o coverage.xml
```

## Building Documentation

Build Doxygen documentation:

```bash
cmake -B build
cmake --build build --target doc
```

View the generated documentation:

```bash
open doc/html/index.html  # macOS
xdg-open doc/html/index.html  # Linux
start doc\html\index.html  # Windows
```

## Debugging

### VS Code

Create `.vscode/launch.json`:

```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Debug Example",
      "type": "cppdbg",
      "request": "launch",
      "program": "${workspaceFolder}/build/examples/example_mcap_reader",
      "args": ["input.mcap"],
      "cwd": "${workspaceFolder}",
      "environment": [],
      "externalConsole": false,
      "MIMode": "gdb"
    }
  ]
}
```

### GDB

```bash
gdb ./build/examples/example_mcap_reader
(gdb) run input.mcap
```

### LLDB

```bash
lldb ./build/examples/example_mcap_reader
(lldb) run input.mcap
```

## Troubleshooting

### Pre-commit Hook Fails

If pre-commit hooks fail:

1. Check the error message for formatting issues
2. Fix the issue by running: `clang-format -i <file>`
3. Stage changes and commit again

To run the pre-commit hook manually on all files:

```bash
.git/hooks/pre-commit --run-all
```

To skip hooks temporarily (not recommended):

```bash
git commit --no-verify -m "message"
```

### clang-format Version Differences

Different versions of clang-format may produce slightly different output even with the same configuration file. This project uses clang-format **18.x** as the reference version.

**Recommended versions:**

- **clang-format 18.x** (preferred)
- clang-format 17.x or 19.x (mostly compatible)

Check your version:

```bash
clang-format --version
```

If you encounter formatting inconsistencies between your local version and CI:

1. The CI pipeline uses a specific clang-format version
2. Install the matching version locally, or
3. Let CI auto-fix formatting and pull the changes

> **Note:** The `.clang-format` configuration is designed to minimize version-specific differences. Most formatting rules are stable across versions 14+.

### GPG Signing Fails

If GPG signing fails:

1. Ensure GPG agent is running: `gpg-agent --daemon`
2. Test signing: `echo "test" | gpg --clear-sign`
3. Check Git config: `git config --global user.signingkey`

## Next Steps

- [Contributing Guidelines](contributing.md) - How to submit changes
- [CI Pipeline](ci-pipeline.md) - Understand the CI/CD process
