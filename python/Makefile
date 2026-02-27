# Makefile for osi-utilities Python SDK
# Build command center for common development tasks

VENV ?= .venv
BOOTSTRAP_PYTHON ?= python3

# Cross-platform venv paths
ifeq ($(OS),Windows_NT)
PYTHON ?= $(VENV)/Scripts/python
PIP ?= $(PYTHON) -m pip
PYTEST ?= $(PYTHON) -m pytest
VENV_MARKER := $(VENV)/Scripts/python.exe
VENV_ACTIVATE := source $(VENV)/Scripts/activate
else
PYTHON ?= $(VENV)/bin/python3
PIP ?= $(PYTHON) -m pip
PYTEST ?= $(PYTHON) -m pytest
VENV_MARKER := $(VENV)/bin/python3
VENV_ACTIVATE := source $(VENV)/bin/activate
endif

# Check if dev environment is set up
define check_dev_setup
	@if [ ! -x "$(PYTHON)" ]; then \
		echo ""; \
		echo "[ERR] Development environment not set up."; \
		echo ""; \
		echo "Please run first:"; \
		echo "  make setup"; \
		echo ""; \
		exit 1; \
	fi
	@if ! $(PYTHON) -c "import mcap, google.protobuf" 2>/dev/null; then \
		echo ""; \
		echo "[ERR] Dev dependencies not installed."; \
		echo ""; \
		echo "Please run:"; \
		echo "  make setup"; \
		echo ""; \
		exit 1; \
	fi
endef

.PHONY: all setup install install-dev lint format test test-cov clean help

# Default target
all: lint test

# Setup: create venv and install dev dependencies
setup:
	@echo "[INFO] Setting up osi-utilities Python SDK..."
	@echo "[INFO] Checking Python virtual environment and dependencies..."
	@set -e; \
	if [ ! -x "$(PYTHON)" ]; then \
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

# Use a stamp file for dependency tracking
.venv/.installed: $(VENV_MARKER) pyproject.toml
	@echo "[INFO] Installing osi-python from submodule..."
	@$(PIP) install ../submodules/osi-python
	@echo "[INFO] Installing osi-utilities Python SDK dependencies..."
	@$(PIP) install -e ".[dev]"
	@touch .venv/.installed
	@echo "[OK] Python dependencies installed"

# Installation targets
install: $(VENV_MARKER)
	@echo "[INFO] Installing osi-utilities package..."
	@$(PIP) install ../submodules/osi-python
	@$(PIP) install -e .
	@echo "[OK] Package installation complete"

install-dev: .venv/.installed

# Linting and formatting
lint:
	$(call check_dev_setup)
	@echo "[INFO] Running ruff linter..."
	@$(PYTHON) -m ruff check .
	@echo "[OK] Lint checks passed"

format:
	$(call check_dev_setup)
	@echo "[INFO] Formatting Python code..."
	@$(PYTHON) -m ruff format .
	@$(PYTHON) -m ruff check --fix .
	@echo "[OK] Formatting complete"

# Type checking
typecheck:
	$(call check_dev_setup)
	@echo "[INFO] Running mypy type checker..."
	@$(PYTHON) -m mypy osi_utilities/
	@echo "[OK] Type checking complete"

# Testing targets
test:
	$(call check_dev_setup)
	@echo "[INFO] Running osi-utilities Python test suite..."
	@$(PYTEST) tests/ -v --tb=short
	@echo "[OK] Test suite complete"

test-cov:
	$(call check_dev_setup)
	@echo "[INFO] Running tests with coverage..."
	@$(PYTEST) tests/ --cov=osi_utilities --cov-report=html --cov-report=term
	@echo "[OK] Coverage run complete"

# Cleaning
clean:
	@echo "[INFO] Cleaning generated files and caches..."
	@rm -rf build/ dist/ *.egg-info/
	@rm -rf .pytest_cache/ .mypy_cache/ .ruff_cache/
	@rm -rf htmlcov/
	@find . -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true
	@find . -type f -name "*.pyc" -delete
	@echo "[OK] Cleaned"

# Help
help:
	@echo "osi-utilities Python SDK - Available Commands"
	@echo ""
	@echo "Installation:"
	@echo "  make setup          - Create venv and install dev dependencies"
	@echo "  make install        - Install package (user mode)"
	@echo "  make install-dev    - Install with dev dependencies"
	@echo ""
	@echo "Linting:"
	@echo "  make lint           - Run ruff linter"
	@echo "  make format         - Format code with ruff"
	@echo "  make typecheck      - Run mypy type checker"
	@echo ""
	@echo "Testing:"
	@echo "  make test           - Run all tests"
	@echo "  make test-cov       - Run tests with coverage report"
	@echo ""
	@echo "Cleaning:"
	@echo "  make clean          - Remove build artifacts and caches"
