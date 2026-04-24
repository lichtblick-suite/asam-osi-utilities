# Makefile for osi-utilities Python SDK
# Single point of truth for all build, lint, test, and CI commands.
#
# Local development:  make setup && make lint test
# CI:                 Use root Makefile (make setup && make lint-python test-python)
#                     or set up venv from root and call targets here directly.
#
# NOTE: Recipes use POSIX shell syntax.  On Windows, run make from
#       Git Bash, MSYS2, or WSL — not from cmd.exe or PowerShell.

# Force POSIX shell for recipe execution on all platforms.
SHELL := /bin/sh

VENV ?= .venv
BOOTSTRAP_PYTHON ?= python3

# Cross-platform venv paths
ifeq ($(OS),Windows_NT)
PYTHON ?= $(VENV)/Scripts/python
PIP ?= $(PYTHON) -m pip
PYTEST ?= $(PYTHON) -m pytest
VENV_MARKER := $(VENV)/Scripts/python.exe
VENV_ACTIVATE := $(VENV)/Scripts/activate
else
PYTHON ?= $(VENV)/bin/python3
PIP ?= $(PYTHON) -m pip
PYTEST ?= $(PYTHON) -m pytest
VENV_MARKER := $(VENV)/bin/python3
VENV_ACTIVATE := source $(VENV)/bin/activate
endif

# Check if dev environment is set up
define check_dev_setup
	@if [ ! -f "$(PYTHON)" ]; then \
		echo ""; \
		echo "[ERR] Development environment not set up."; \
		echo ""; \
		echo "Please run first:"; \
		echo "  make setup"; \
		echo ""; \
		exit 1; \
	fi; \
	if ! $(PYTHON) -c "import mcap, google.protobuf" 2>/dev/null; then \
		echo ""; \
		echo "[ERR] Dev dependencies not installed."; \
		echo ""; \
		echo "Please run:"; \
		echo "  make setup"; \
		echo ""; \
		exit 1; \
	fi
endef

.PHONY: all setup install install-dev lint format format-check typecheck test test-ci test-cov package package-verify vendor-osi3 clean help

# Default target
all: lint test

# ---------------------------------------------------------------------------
# Local development setup (creates venv)
# ---------------------------------------------------------------------------
setup:
	@echo "[INFO] Setting up osi-utilities Python SDK..."
	@echo "[INFO] Checking Python virtual environment and dependencies..."
	@if [ ! -f "$(PYTHON)" ]; then \
		echo "[INFO] Python virtual environment not found; bootstrapping..."; \
		$(MAKE) --no-print-directory .venv/.installed; \
	elif $(PYTHON) -c "import mcap, google.protobuf, pytest, ruff" >/dev/null 2>&1; then \
		echo "[OK] Python virtual environment and dependencies are ready at $(VENV)"; \
	else \
		echo "[INFO] Python virtual environment found but dependencies are missing; bootstrapping..."; \
		$(MAKE) --no-print-directory -B .venv/.installed; \
	fi
	@echo ""
	@echo "[OK] osi-utilities Python SDK setup complete. Activate with: $(VENV_ACTIVATE)"

$(VENV_MARKER):
	@echo "[INFO] Creating Python virtual environment at $(VENV)..."
	@$(BOOTSTRAP_PYTHON) -m venv $(VENV)
	@$(PIP) install --upgrade pip
	@echo "[OK] Python virtual environment ready"

.venv/.installed: $(VENV_MARKER) pyproject.toml
	@echo "[INFO] Installing osi-python from submodule..."
	@$(PIP) install ../submodules/osi-python
	@echo "[INFO] Installing osi-utilities Python SDK dependencies..."
	@mkdir -p osi3 && touch osi3/__init__.py && \
		$(PIP) install -e ".[dev]" && \
		rm -rf osi3
	@touch .venv/.installed
	@echo "[OK] Python dependencies installed"

# ---------------------------------------------------------------------------
# Installation targets
# ---------------------------------------------------------------------------
install:
	$(MAKE) $(VENV_MARKER)
	@echo "[INFO] Installing osi-utilities package..."
	@$(PIP) install ../submodules/osi-python
	@$(PIP) install -e .
	@echo "[OK] Package installation complete"

install-dev:
	$(MAKE) .venv/.installed

# ---------------------------------------------------------------------------
# Linting and formatting
# ---------------------------------------------------------------------------
lint:
	$(call check_dev_setup)
	@echo "[INFO] Running ruff linter..."
	$(PYTHON) -m ruff check .
	@echo "[OK] Lint checks passed"

format:
	$(call check_dev_setup)
	@echo "[INFO] Formatting Python code..."
	$(PYTHON) -m ruff format .
	$(PYTHON) -m ruff check --fix .
	@echo "[OK] Formatting complete"

format-check:
	$(call check_dev_setup)
	@echo "[INFO] Checking Python code formatting..."
	$(PYTHON) -m ruff format --check .
	$(PYTHON) -m ruff check .
	@echo "[OK] Format and lint checks passed"

# ---------------------------------------------------------------------------
# Type checking
# ---------------------------------------------------------------------------
typecheck:
	$(call check_dev_setup)
	@echo "[INFO] Running mypy type checker..."
	$(PYTHON) -m mypy osi_utilities/
	@echo "[OK] Type checking complete"

# ---------------------------------------------------------------------------
# Testing
# ---------------------------------------------------------------------------
test:
	$(call check_dev_setup)
	@echo "[INFO] Running osi-utilities Python test suite..."
	$(PYTEST) tests/ -v --tb=short
	@echo "[OK] Test suite complete"

test-ci:
	$(call check_dev_setup)
	@echo "[INFO] Running osi-utilities Python test suite (CI mode)..."
	$(PYTEST) tests/ -v --tb=short --junitxml=test-results.xml
	@echo "[OK] Test suite complete"

test-cov:
	$(call check_dev_setup)
	@echo "[INFO] Running tests with coverage..."
	$(PYTEST) tests/ --cov=osi_utilities --cov-report=html --cov-report=term
	@echo "[OK] Coverage run complete"

# ---------------------------------------------------------------------------
# Vendoring: copy osi3 bindings from submodule into package source
# ---------------------------------------------------------------------------
vendor-osi3:
	@echo "[INFO] Vendoring osi3 protobuf bindings from submodule..."
	@rm -rf osi3/
	@mkdir -p osi3
	@cp ../submodules/osi-python/osi3/__init__.py osi3/
	@cp ../submodules/osi-python/osi3/*_pb2.py osi3/
	@cp ../submodules/osi-python/osi3/*_pb2.pyi osi3/ 2>/dev/null || true
	@echo "[OK] Vendored osi3 (version: $$(grep __version__ osi3/__init__.py))"

# ---------------------------------------------------------------------------
# Package build
# ---------------------------------------------------------------------------
package: vendor-osi3
	@echo "[INFO] Building Python package..."
	$(PIP) install build
	$(PYTHON) -m build
	@echo "[OK] Package built in dist/"

package-verify: package
	@echo "[INFO] Verifying package installation..."
	$(PIP) install dist/*.whl
	$(PYTHON) -c "from osi_utilities import MessageType, create_reader; print('[OK] Package import OK')"
	$(PYTHON) -c "import osi3; print('[OK] Bundled osi3 version:', osi3.__version__)"

# ---------------------------------------------------------------------------
# Cleaning
# ---------------------------------------------------------------------------
clean:
	@echo "[INFO] Cleaning generated files and caches..."
	@rm -rf build/ dist/ *.egg-info/
	@rm -rf osi3/
	@rm -rf .pytest_cache/ .mypy_cache/ .ruff_cache/
	@rm -rf htmlcov/ test-results.xml
	@find . -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true
	@find . -type f -name "*.pyc" -delete
	@echo "[OK] Cleaned"

# ---------------------------------------------------------------------------
# Help
# ---------------------------------------------------------------------------
help:
	@echo "osi-utilities Python SDK - Available Commands"
	@echo ""
	@echo "Setup:"
	@echo "  make setup          - Create venv and install dev dependencies"
	@echo "  make install        - Install package (user mode)"
	@echo "  make install-dev    - Install with dev dependencies"
	@echo ""
	@echo "Linting:"
	@echo "  make lint           - Run ruff linter"
	@echo "  make format         - Format code with ruff"
	@echo "  make format-check   - Check formatting without changes"
	@echo "  make typecheck      - Run mypy type checker"
	@echo ""
	@echo "Testing:"
	@echo "  make test           - Run all tests"
	@echo "  make test-ci        - Run tests with JUnit XML output"
	@echo "  make test-cov       - Run tests with coverage report"
	@echo ""
	@echo "Packaging:"
	@echo "  make package        - Build sdist and wheel"
	@echo "  make package-verify - Build and verify package installs"
	@echo ""
	@echo "Cleaning:"
	@echo "  make clean          - Remove build artifacts and caches"
