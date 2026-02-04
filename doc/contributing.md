# Contributing Guidelines

Thank you for your interest in contributing to ASAM OSI Utilities! This document provides guidelines and information for contributors.

## Table of Contents

- [Code of Conduct](#code-of-conduct)
- [Conventional Commits](#conventional-commits)
- [Developer Certificate of Origin (DCO)](#developer-certificate-of-origin-dco)
- [DCO Sign-Off Methods](#dco-sign-off-methods)
- [GPG Signed Commits](#gpg-signed-commits)
- [Setup and Workflow](#setup-and-workflow)
- [Pull Request Process](#pull-request-process)

## Code of Conduct

Please be respectful and constructive in all interactions. We are committed to providing a welcoming and inclusive environment for everyone.

## Conventional Commits

All commits **must** follow the [Conventional Commits](https://www.conventionalcommits.org/) standard to ensure consistency and enable automatic changelog generation.

### Commit Message Format

```text
<type>(<scope>): <description>

[optional body]

[optional footer(s)]
Signed-off-by: Your Name <your.email@example.com>
```

### Examples

```bash
feat: add new trace file reader
fix(mcap): resolve memory leak in writer
docs: update README.md file
refactor(tracefile): restructure reader classes
```

### Types

| Type       | Description                                 |
| ---------- | ------------------------------------------- |
| `feat`     | New feature                                 |
| `fix`      | Bug fix                                     |
| `docs`     | Documentation changes                       |
| `style`    | Code style changes (formatting, no logic)   |
| `refactor` | Code restructuring without feature changes  |
| `test`     | Adding or fixing tests                      |
| `chore`    | Maintenance tasks                           |
| `ci`       | Continuous integration changes              |
| `perf`     | Performance improvements                    |
| `build`    | Build system or external dependency changes |

### Scope (Optional)

The scope provides additional context about what part of the codebase is affected:

- `tracefile` - Trace file reader/writer
- `mcap` - MCAP format handling
- `examples` - Example code
- `ci` - CI/CD workflows
- `docs` - Documentation

## Developer Certificate of Origin (DCO)

To make a good faith effort to ensure licensing criteria are met, this project requires the Developer Certificate of Origin (DCO) process to be followed. The DCO is an attestation attached to every contribution made by every developer. In the commit message, the developer simply adds a `Signed-off-by` statement and thereby agrees to the DCO.

When a developer submits a patch, it is a commitment that the contributor has the right to submit the patch per the license. The DCO agreement is shown below and available at [developercertificate.org](https://developercertificate.org/).

```text
Developer's Certificate of Origin 1.1

By making a contribution to this project, I certify that:

(a) The contribution was created in whole or in part by me and I
    have the right to submit it under the open source license
    indicated in the file; or

(b) The contribution is based upon previous work that, to the
    best of my knowledge, is covered under an appropriate open
    source license and I have the right under that license to
    submit that work with modifications, whether created in whole
    or in part by me, under the same open source license (unless
    I am permitted to submit under a different license), as
    indicated in the file; or

(c) The contribution was provided directly to me by some other
    person who certified (a), (b) or (c) and I have not modified
    it.

(d) I understand and agree that this project and the contribution
    are public and that a record of the contribution (including
    all personal information I submit with it, including my
    sign-off) is maintained indefinitely and may be redistributed
    consistent with this project or the open source license(s)
    involved.
```

## DCO Sign-Off Methods

The DCO requires a sign-off message in the following format appear on each commit in the pull request:

```text
Signed-off-by: Firstname Lastname <email@address.com>
```

The DCO text can either be manually added to your commit body, or you can add either `-s` or `--signoff` to your usual Git commit commands:

```bash
git commit -s -m "Your commit message"
```

If you forget to add the sign-off you can also amend a previous commit with the sign-off:

```bash
git commit --amend -s
```

You can add sign-offs to multiple commits using:

```bash
git rebase --signoff HEAD~<number_of_commits>
```

If you've pushed your changes to GitHub already you'll need to force push your branch after this:

```bash
git push --force-with-lease
```

## GPG Signed Commits

In addition to the DCO sign-off, we **require** all commits to be GPG signed. This provides cryptographic verification that commits actually come from the stated author.

### Setting Up GPG Signing

**1. Generate a GPG key** (if you don't have one):

```bash
gpg --full-generate-key
```

**2. List your GPG keys** to get the key ID:

```bash
gpg --list-secret-keys --keyid-format=long
```

**3. Configure Git to use your GPG key**:

```bash
git config --global user.signingkey YOUR_KEY_ID
git config --global commit.gpgsign true
```

**4. Add your GPG key to GitHub**:

- Export your public key: `gpg --armor --export YOUR_KEY_ID`
- Go to GitHub Settings → SSH and GPG keys → New GPG key
- Paste your public key

### Signing Commits

With `commit.gpgsign` set to `true`, all commits will be automatically signed. You can also explicitly sign:

```bash
git commit -S -s -m "Your signed and signed-off commit message"
```

## Setup and Workflow

Use the focused pages for everyday steps:

- [Development Setup](setup.md) — tools, hooks, and environment
- [Developer Workflow](workflow.md) — build, test, format, and commit

## Pull Request Process

### Before Submitting

- [ ] All commits are signed-off (DCO)
- [ ] All commits are GPG signed
- [ ] Code passes formatting checks
- [ ] All tests pass
- [ ] Commit messages follow Conventional Commits

### PR Title Convention

Use conventional commit prefixes in your PR title:

- `feat:` - New features
- `fix:` - Bug fixes
- `docs:` - Documentation changes
- `style:` - Code style changes (formatting)
- `refactor:` - Code refactoring
- `test:` - Adding/updating tests
- `chore:` - Maintenance tasks

### Review Process

- At least one maintainer approval is required
- All CI checks must pass
- Address review feedback promptly

### After Merge

- Delete your feature branch
- Pull the latest changes from upstream

## Version Updates

The project version is managed in a single `VERSION` file. When preparing a release:

1. Update the `VERSION` file with the new version
2. Run the sync script:

   ```bash
   # Linux/macOS
   ./scripts/sync_version.sh

   # Windows PowerShell
   .\scripts\sync_version.ps1
   ```

3. Commit the version changes

## Questions?

If you have questions, please open an issue on GitHub or reach out to the maintainers.

Thank you for contributing to ASAM OSI Utilities!
