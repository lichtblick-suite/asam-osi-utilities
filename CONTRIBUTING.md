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
git clone --recurse-submodules https://github.com/Lichtblick-Suite/asam-osi-utilities.git
cd asam-osi-utilities

# Setup dev environment
./scripts/setup-dev.sh  # Linux/macOS
.\scripts\setup-dev.ps1  # Windows

# Make changes, then commit
git commit -S -s -m "feat: your feature description"
```

## Documentation

- [Full Contributing Guide](doc/contributing.md) - Complete guidelines
- [Development Setup](doc/development.md) - Dev environment setup
- [Building](doc/building.md) - Build instructions
