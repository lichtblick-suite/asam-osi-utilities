# Development Setup

This page covers environment setup only. For day-to-day commands, see [Developer Workflow](workflow.md).

## Quick Setup

Run the setup script to configure hooks and check tools. **No Python required**.

**Linux/macOS:**

```bash
./scripts/setup-dev.sh
```

**Windows PowerShell:**

```powershell
.\scripts\setup-dev.ps1
```

The setup script installs native Git hooks (format checks and commit message validation).

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

### Windows (winget)

```powershell
# yq (used to read .github/dependencies.yml)
winget install --id MikeFarah.yq --source winget

# Install from .github/dependencies.yml
yq -r '(.dependencies.dev.winget + .dependencies.docs.winget) | unique | .[]' .github/dependencies.yml |
  ForEach-Object { winget install --id $_ --source winget }

# Ninja (recommended for clangd compile database on Windows)
winget install --id Ninja-build.Ninja --source winget
```

### macOS (Homebrew)

```bash
brew install yq
yq -r '(.dependencies.dev.brew + .dependencies.lint.brew + .dependencies.docs.brew) | unique | .[]' .github/dependencies.yml | \
  xargs brew install

# Add to PATH (llvm is keg-only)
export PATH="$(brew --prefix llvm)/bin:$PATH"
```

## Git Hooks

Hooks are generated into `.git/hooks/` by the setup scripts. Use them in the workflow page:

- Run checks: `.git/hooks/pre-commit`
- Run checks on all files: `.git/hooks/pre-commit --run-all`
- Auto-fix: `.git/hooks/pre-commit --fix`
