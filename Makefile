# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

# Makefile for asam-osi-utilities
# Build command center for all development tasks (C++ and Python).
#
# Local development:  make setup && make lint test
# CI:                 make setup && make docs / make test-python / ...

# Allow parent makefiles or CI to override the venv path.
VENV ?= python/.venv

# OS detection for cross-platform support (Windows vs Unix)
ifeq ($(OS),Windows_NT)
    VENV_BIN := $(VENV)/Scripts
    PYTHON ?= $(VENV_BIN)/python.exe
    BOOTSTRAP_PYTHON ?= python
    ACTIVATE_SCRIPT := $(VENV_BIN)/activate
else
    VENV_BIN := $(VENV)/bin
    PYTHON ?= $(VENV_BIN)/python3
    BOOTSTRAP_PYTHON ?= python3
    ACTIVATE_SCRIPT := $(VENV_BIN)/activate
endif

# CMake preset for docs-only builds (no library/test compilation needed)
CMAKE_DOCS_PRESET ?= base

# Check if dev environment is set up
define check_venv
	@if [ ! -f "$(PYTHON)" ]; then \
		echo ""; \
		echo "[ERR] Development environment not set up."; \
		echo ""; \
		echo "Please run first:"; \
		echo "  make setup"; \
		echo ""; \
		exit 1; \
	fi
endef

.PHONY: all setup setup-dev setup-docs lint lint-cpp lint-python format-python test test-cpp test-python docs docs-serve clean clean-docs help

# Default target
all: lint test

# ===========================================================================
# Setup targets
# ===========================================================================

# Full setup: venv + dev + docs dependencies + git hooks
setup: $(ACTIVATE_SCRIPT)
	@if ! "$(PYTHON)" -c "import sphinx, breathe, ruff, pytest" >/dev/null 2>&1; then \
		echo "[INFO] Dependencies missing; reinstalling..."; \
		"$(PYTHON)" -m pip install --upgrade pip; \
		"$(PYTHON)" -m pip install submodules/osi-python; \
		mkdir -p python/osi3 && touch python/osi3/__init__.py; \
		"$(PYTHON)" -m pip install -e "python/[dev,docs]"; \
		rm -rf python/osi3; \
	fi
	@echo "[OK] Python virtual environment and dependencies are ready at $(VENV)"
	@echo ""
	@echo "Activate with: source $(ACTIVATE_SCRIPT)"

# Create the virtual environment
$(PYTHON):
	@echo "[INFO] Creating Python virtual environment at $(VENV)..."
	@"$(BOOTSTRAP_PYTHON)" -m venv "$(VENV)"
	@"$(PYTHON)" -m pip install --upgrade pip
	@echo "[OK] Python virtual environment ready"

# Install all dependencies into the venv
# Note: hatchling force-include requires osi3/ to exist during metadata
# generation, so we create a placeholder before the editable install.
$(ACTIVATE_SCRIPT): $(PYTHON) python/pyproject.toml
	@echo "[INFO] Installing Python dependencies (dev + docs)..."
	@"$(PYTHON)" -m pip install submodules/osi-python
	@mkdir -p python/osi3 && touch python/osi3/__init__.py
	@"$(PYTHON)" -m pip install -e "python/[dev,docs]"
	@rm -rf python/osi3
	@touch "$(ACTIVATE_SCRIPT)"
	@echo "[OK] Python dependencies installed"

# Dev setup: venv + dev deps + git hooks
setup-dev: setup
	@echo "[INFO] Installing Git hooks..."
	@bash scripts/setup-dev.sh
	@echo "[OK] Development environment ready"

# Docs-only setup: venv + docs deps (lighter than full setup)
setup-docs: $(PYTHON)
	@if ! "$(PYTHON)" -c "import sphinx, breathe" >/dev/null 2>&1; then \
		echo "[INFO] Installing docs dependencies..."; \
		"$(PYTHON)" -m pip install submodules/osi-python; \
		mkdir -p python/osi3 && touch python/osi3/__init__.py; \
		"$(PYTHON)" -m pip install -e "python/[docs]"; \
		rm -rf python/osi3; \
	fi
	@echo "[OK] Documentation dependencies ready"

# ===========================================================================
# Linting and formatting
# ===========================================================================

lint: lint-cpp lint-python

lint-cpp:
	@echo "[INFO] Running C++ format checks..."
	@bash scripts/setup-dev.sh >/dev/null 2>&1
	@.git/hooks/pre-commit --all-files --skip-tidy --skip-python --skip-docs
	@echo "[OK] C++ format checks passed"

lint-python:
	$(call check_venv)
	@echo "[INFO] Running Python lint + format checks..."
	@"$(PYTHON)" -m ruff check python/
	@"$(PYTHON)" -m ruff format --check python/
	@echo "[OK] Python lint + format checks passed"

format-python:
	$(call check_venv)
	@echo "[INFO] Formatting Python code..."
	@"$(PYTHON)" -m ruff format python/
	@"$(PYTHON)" -m ruff check --fix python/
	@echo "[OK] Python formatting complete"

# ===========================================================================
# Testing
# ===========================================================================

test: test-cpp test-python

test-cpp:
	@echo "[INFO] Running C++ test suite..."
	@cmake --build --preset vcpkg --parallel
	@ctest --test-dir build-vcpkg --output-on-failure
	@echo "[OK] C++ tests passed"

test-python:
	$(call check_venv)
	@echo "[INFO] Running Python test suite..."
	@cd python && "$(abspath $(PYTHON))" -m pytest tests/ -v --tb=short --junitxml=test-results.xml
	@echo "[OK] Python tests passed"

# ===========================================================================
# Documentation
# ===========================================================================

# Build unified documentation (Doxygen XML → Sphinx HTML)
docs: setup-docs
	@echo "[INFO] Building documentation (Doxygen XML → Sphinx HTML)..."
	@cmake --preset $(CMAKE_DOCS_PRESET) -DOSIUTILITIES_DOCS_ONLY=ON -DPython3_EXECUTABLE=$(abspath $(PYTHON))
	@cmake --build --preset $(CMAKE_DOCS_PRESET) --target library_api_doc
	@echo "[OK] Documentation built at doc/html/"

# Serve docs locally for preview
docs-serve: docs
	@echo "[INFO] Starting local documentation server..."
	@"$(PYTHON)" -m http.server 8000 --directory doc/html

# ===========================================================================
# Cleaning
# ===========================================================================

clean:
	@echo "[INFO] Cleaning generated files and caches..."
	@rm -rf build/ build-vcpkg/
	@rm -rf doc/html/ doc/xml/ doc/_build/
	@rm -rf python/.pytest_cache/ python/.mypy_cache/ python/.ruff_cache/
	@rm -rf python/build/ python/dist/ python/*.egg-info/
	@find . -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true
	@echo "[OK] Cleaned"

clean-docs:
	@echo "[INFO] Cleaning documentation artifacts..."
	@rm -rf doc/html/ doc/xml/ doc/_build/
	@echo "[OK] Documentation artifacts cleaned"

# ===========================================================================
# Help
# ===========================================================================

help:
	@echo "asam-osi-utilities - Available Commands"
	@echo ""
	@echo "Setup:"
	@echo "  make setup          - Create venv and install all dependencies (dev + docs)"
	@echo "  make setup-dev      - Full setup + install Git hooks"
	@echo "  make setup-docs     - Minimal setup for documentation building only"
	@echo ""
	@echo "Linting:"
	@echo "  make lint           - Run all linters (C++ format + Python ruff)"
	@echo "  make lint-cpp       - Run C++ clang-format checks"
	@echo "  make lint-python    - Run Python ruff lint + format checks"
	@echo "  make format-python  - Auto-format Python code"
	@echo ""
	@echo "Testing:"
	@echo "  make test           - Run all tests (C++ + Python)"
	@echo "  make test-cpp       - Run C++ tests (vcpkg preset)"
	@echo "  make test-python    - Run Python tests"
	@echo ""
	@echo "Documentation:"
	@echo "  make docs           - Build unified docs (Doxygen XML → Sphinx HTML)"
	@echo "  make docs-serve     - Build and serve docs locally on port 8000"
	@echo ""
	@echo "Cleaning:"
	@echo "  make clean          - Remove all build artifacts and caches"
	@echo "  make clean-docs     - Remove documentation artifacts only"
