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

| Tool         | Required    | Purpose                                     |
| ------------ | ----------- | ------------------------------------------- |
| Git          | Yes         | Version control                             |
| CMake        | Yes         | Build system                                |
| clang-format | Recommended | Code formatting                             |
| clang-tidy   | Optional    | Static analysis                             |
| Ninja        | Optional    | Compile database for clangd (Windows)       |
| Graphviz     | Optional    | Doxygen diagrams (dot)                      |

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

# Ninja (recommended for clangd compile database on Windows)
winget install --id Ninja-build.Ninja --source winget

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

> **Quick usage:**
>
> - Check staged files: `.git/hooks/pre-commit`
> - Check all files: `.git/hooks/pre-commit --run-all`
> - Fix staged files: `.git/hooks/pre-commit --fix`
> - Fix all files: `.git/hooks/pre-commit --run-all --fix`

### Running Checks Manually

Check formatting on all C++ files:

```bash
find src include tests examples -name "*.cpp" -o -name "*.h" | xargs clang-format --dry-run --Werror
```

Fix formatting on all C++ files:

```bash
find src include tests examples -name "*.cpp" -o -name "*.h" | xargs clang-format -i
```

Run the hook with auto-fix:

```bash
.git/hooks/pre-commit --fix
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

**clangd configuration note (recommended setup):**

1. Install the VS Code **CMake Tools** extension.
2. Configure a build directory named `build` (CMake Tools → Configure). On Windows, select the **Ninja** generator.
3. Build once (CMake Tools → Build).

This generates `build/compile_commands.json`, which clangd uses automatically. If you prefer manual setup, run:

```bash
cmake -B build -DCMAKE_EXPORT_COMPILE_COMMANDS=ON
```

Ensure clangd is pointed at `build/compile_commands.json` (this is automatic with CMake Tools). If you keep the compile database in the repo root, clangd will also pick it up.

If you prefer manual setup on Windows, run from a **x64 Native Tools Command Prompt for VS 2022** (or call `vcvars64.bat` first). In PowerShell, use this one-liner to preserve the VS environment:

```powershell
cmd /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" && set VCPKG_ROOT=C:\vcpkg && rmdir /s /q build-ninja && cmake -G Ninja -B build-ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows'
```

Then point clangd to `build-ninja/compile_commands.json`. If clangd still reports missing headers, set the compile commands directory explicitly (VS Code):

```json
{
  "clangd.arguments": ["--compile-commands-dir=build-ninja"]
}
```

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

To run the pre-commit hook and auto-fix all files:

```bash
.git/hooks/pre-commit --run-all --fix
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
