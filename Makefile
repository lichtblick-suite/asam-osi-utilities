# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

# Makefile for asam-osi-utilities
# Build command center for all development tasks (C++ and Python).
#
# Local development:  make setup dev && make lint && make test
# CI:                 make setup && make lint python / make test python / ...
#
# NOTE: Recipes use POSIX shell syntax.  On Windows, run make from
#       Git Bash, MSYS2, or WSL — not from cmd.exe or PowerShell.

# Force POSIX shell for recipe execution on all platforms.
SHELL := /bin/sh

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
CMAKE_TEST_PRESET ?= vcpkg
CTEST_TEST_DIR ?= build-vcpkg
CTEST_CONFIG ?= Release

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

.PHONY: all help setup lint format test docs run clean \
	build cpp dev python serve \
	_help_main _help_setup _help_lint _help_format _help_test _help_docs _help_run _help_clean \
	_setup _setup_dev _setup_docs \
	_lint _lint_cpp _lint_python _format _format_cpp _format_python \
	_test _test_cpp _test_python \
	_docs_build _run_docs \
	_clean _clean_docs \
	setup-dev setup-docs lint-cpp lint-python format-cpp format-python test-cpp test-python docs-build docs-serve clean-docs

PUBLIC_GROUPS := help setup lint format test docs run clean
FIRST_GOAL := $(firstword $(MAKECMDGOALS))
SUBCOMMAND := $(word 2,$(MAKECMDGOALS))
EXTRA_SUBCOMMANDS := $(wordlist 3,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))
DISPATCH_CONTEXT := $(filter $(FIRST_GOAL),$(PUBLIC_GROUPS))

# These are placeholders so grouped commands such as `make lint cpp` work as intended.
build cpp dev python serve:
	@if [ "$(FIRST_GOAL)" = "$@" ]; then \
		echo "[ERR] '$@' is a subcommand placeholder. Use a grouped command such as 'make lint cpp' or 'make docs build'."; \
		exit 1; \
	fi

# Default target
all: _lint _test

# ===========================================================================
# Setup targets
# ===========================================================================

# Full setup: venv + dev + docs Python dependencies
_setup: $(ACTIVATE_SCRIPT)
	@if ! "$(PYTHON)" -c "import sphinx, breathe, ruff, pytest" >/dev/null 2>&1; then \
		echo "[INFO] Dependencies missing; reinstalling..."; \
		"$(PYTHON)" -m pip install --upgrade pip && \
		"$(PYTHON)" -m pip install submodules/osi-python && \
		mkdir -p python/osi3 && touch python/osi3/__init__.py && \
		"$(PYTHON)" -m pip install -e "python/[dev,docs]" && \
		rm -rf python/osi3; \
	fi
	@echo "[OK] Python virtual environment and dependencies are ready at $(VENV)"
ifeq ($(OS),Windows_NT)
	@echo ""
	@echo "Activate with: $(VENV_BIN)\\activate"
else
	@echo ""
	@echo "Activate with: source $(ACTIVATE_SCRIPT)"
endif

ifeq ($(OS),Windows_NT)
    CTEST_CONFIG_ARG := -C $(CTEST_CONFIG)
else
    CTEST_CONFIG_ARG :=
endif

setup:
	@if [ -n "$(DISPATCH_CONTEXT)" ] && [ "$(FIRST_GOAL)" != "setup" ]; then \
		:; \
	elif [ -n "$(strip $(EXTRA_SUBCOMMANDS))" ]; then \
		echo "[ERR] Too many setup subcommands: $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))"; \
		"$(MAKE)" --no-print-directory _help_setup; \
		exit 1; \
	elif [ -z "$(SUBCOMMAND)" ]; then \
		"$(MAKE)" --no-print-directory _setup; \
	elif [ "$(SUBCOMMAND)" = "dev" ]; then \
		"$(MAKE)" --no-print-directory _setup_dev; \
	elif [ "$(SUBCOMMAND)" = "docs" ]; then \
		"$(MAKE)" --no-print-directory _setup_docs; \
	elif [ "$(SUBCOMMAND)" = "help" ]; then \
		"$(MAKE)" --no-print-directory _help_setup; \
	else \
		echo "[ERR] Unknown setup subcommand: $(SUBCOMMAND)"; \
		"$(MAKE)" --no-print-directory _help_setup; \
		exit 1; \
	fi

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
	@"$(PYTHON)" -m pip install submodules/osi-python && \
		mkdir -p python/osi3 && touch python/osi3/__init__.py && \
		"$(PYTHON)" -m pip install -e "python/[dev,docs]" && \
		rm -rf python/osi3
	@touch "$(ACTIVATE_SCRIPT)"
	@echo "[OK] Python dependencies installed"

# Dev setup: venv + dev deps + git hooks
_setup_dev: _setup
	@echo "[INFO] Installing Git hooks..."
	@bash scripts/setup-dev.sh
	@echo "[OK] Development environment ready"

# Docs-only setup: venv + docs deps (lighter than full setup)
_setup_docs: $(PYTHON)
	@if ! "$(PYTHON)" -c "import sphinx, breathe" >/dev/null 2>&1; then \
		echo "[INFO] Installing docs dependencies..."; \
		"$(PYTHON)" -m pip install submodules/osi-python && \
		mkdir -p python/osi3 && touch python/osi3/__init__.py && \
		"$(PYTHON)" -m pip install -e "python/[docs]" && \
		rm -rf python/osi3; \
	fi
	@echo "[OK] Documentation dependencies ready"

# ===========================================================================
# Linting and formatting
# ===========================================================================

_lint: _lint_cpp _lint_python

lint:
	@if [ -n "$(DISPATCH_CONTEXT)" ] && [ "$(FIRST_GOAL)" != "lint" ]; then \
		:; \
	elif [ -n "$(strip $(EXTRA_SUBCOMMANDS))" ]; then \
		echo "[ERR] Too many lint subcommands: $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))"; \
		"$(MAKE)" --no-print-directory _help_lint; \
		exit 1; \
	elif [ -z "$(SUBCOMMAND)" ]; then \
		"$(MAKE)" --no-print-directory _lint; \
	elif [ "$(SUBCOMMAND)" = "cpp" ]; then \
		"$(MAKE)" --no-print-directory _lint_cpp; \
	elif [ "$(SUBCOMMAND)" = "python" ]; then \
		"$(MAKE)" --no-print-directory _lint_python; \
	elif [ "$(SUBCOMMAND)" = "help" ]; then \
		"$(MAKE)" --no-print-directory _help_lint; \
	else \
		echo "[ERR] Unknown lint subcommand: $(SUBCOMMAND)"; \
		"$(MAKE)" --no-print-directory _help_lint; \
		exit 1; \
	fi

_lint_cpp:
	@echo "[INFO] Running C++ format checks..."
	@bash scripts/setup-dev.sh >/dev/null 2>&1
	@HOOK_PATH="$$(git rev-parse --git-path hooks)/pre-commit"; "$$HOOK_PATH" --all-files --skip-tidy --skip-python --skip-docs
	@echo "[OK] C++ format checks passed"

_lint_python:
	$(call check_venv)
	@echo "[INFO] Running Python lint + format checks..."
	@"$(PYTHON)" -m ruff check python/
	@"$(PYTHON)" -m ruff format --check python/
	@echo "[OK] Python lint + format checks passed"

_format: _format_cpp _format_python

format:
	@if [ -n "$(DISPATCH_CONTEXT)" ] && [ "$(FIRST_GOAL)" != "format" ]; then \
		:; \
	elif [ -n "$(strip $(EXTRA_SUBCOMMANDS))" ]; then \
		echo "[ERR] Too many format subcommands: $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))"; \
		"$(MAKE)" --no-print-directory _help_format; \
		exit 1; \
	elif [ -z "$(SUBCOMMAND)" ]; then \
		"$(MAKE)" --no-print-directory _format; \
	elif [ "$(SUBCOMMAND)" = "cpp" ]; then \
		"$(MAKE)" --no-print-directory _format_cpp; \
	elif [ "$(SUBCOMMAND)" = "python" ]; then \
		"$(MAKE)" --no-print-directory _format_python; \
	elif [ "$(SUBCOMMAND)" = "help" ]; then \
		"$(MAKE)" --no-print-directory _help_format; \
	else \
		echo "[ERR] Unknown format subcommand: $(SUBCOMMAND)"; \
		"$(MAKE)" --no-print-directory _help_format; \
		exit 1; \
	fi

_format_cpp:
	@echo "[INFO] Formatting C++ code..."
	@bash scripts/setup-dev.sh >/dev/null 2>&1
	@HOOK_PATH="$$(git rev-parse --git-path hooks)/pre-commit"; "$$HOOK_PATH" --all-files --fix-format --skip-tidy --skip-python --skip-docs
	@echo "[OK] C++ formatting complete"

_format_python:
	$(call check_venv)
	@echo "[INFO] Formatting Python code..."
	@"$(PYTHON)" -m ruff format python/
	@"$(PYTHON)" -m ruff check --fix python/
	@echo "[OK] Python formatting complete"

# ===========================================================================
# Testing
# ===========================================================================

_test: _test_cpp _test_python

test:
	@if [ -n "$(DISPATCH_CONTEXT)" ] && [ "$(FIRST_GOAL)" != "test" ]; then \
		:; \
	elif [ -n "$(strip $(EXTRA_SUBCOMMANDS))" ]; then \
		echo "[ERR] Too many test subcommands: $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))"; \
		"$(MAKE)" --no-print-directory _help_test; \
		exit 1; \
	elif [ -z "$(SUBCOMMAND)" ]; then \
		"$(MAKE)" --no-print-directory _test; \
	elif [ "$(SUBCOMMAND)" = "cpp" ]; then \
		"$(MAKE)" --no-print-directory _test_cpp; \
	elif [ "$(SUBCOMMAND)" = "python" ]; then \
		"$(MAKE)" --no-print-directory _test_python; \
	elif [ "$(SUBCOMMAND)" = "help" ]; then \
		"$(MAKE)" --no-print-directory _help_test; \
	else \
		echo "[ERR] Unknown test subcommand: $(SUBCOMMAND)"; \
		"$(MAKE)" --no-print-directory _help_test; \
		exit 1; \
	fi

_test_cpp:
	@echo "[INFO] Running C++ test suite..."
	@cmake --build --preset $(CMAKE_TEST_PRESET) --parallel && \
		ctest --test-dir $(CTEST_TEST_DIR) $(CTEST_CONFIG_ARG) --output-on-failure
	@echo "[OK] C++ tests passed"

_test_python:
	$(call check_venv)
	@echo "[INFO] Running Python test suite..."
	@cd python && "$(abspath $(PYTHON))" -m pytest tests/ -v --tb=short --junitxml=test-results.xml
	@echo "[OK] Python tests passed"

# ===========================================================================
# Documentation
# ===========================================================================

# Build unified documentation (Doxygen XML → Sphinx HTML)
_docs_build: _setup_docs
	@echo "[INFO] Building documentation (Doxygen XML → Sphinx HTML)..."
	@cmake --preset $(CMAKE_DOCS_PRESET) -DOSIUTILITIES_DOCS_ONLY=ON -DPython3_EXECUTABLE="$(abspath $(PYTHON))" && \
		cmake --build --preset $(CMAKE_DOCS_PRESET) --target library_api_doc
	@echo "[OK] Documentation built at doc/html/"

docs:
	@if [ -n "$(DISPATCH_CONTEXT)" ] && [ "$(FIRST_GOAL)" != "docs" ]; then \
		:; \
	elif [ -n "$(strip $(EXTRA_SUBCOMMANDS))" ]; then \
		echo "[ERR] Too many docs subcommands: $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))"; \
		"$(MAKE)" --no-print-directory _help_docs; \
		exit 1; \
	elif [ -z "$(SUBCOMMAND)" ] || [ "$(SUBCOMMAND)" = "build" ]; then \
		"$(MAKE)" --no-print-directory _docs_build; \
	elif [ "$(SUBCOMMAND)" = "help" ]; then \
		"$(MAKE)" --no-print-directory _help_docs; \
	else \
		echo "[ERR] Unknown docs subcommand: $(SUBCOMMAND)"; \
		"$(MAKE)" --no-print-directory _help_docs; \
		exit 1; \
	fi

# Serve docs locally for preview
_run_docs: _docs_build
	@echo "[INFO] Starting local documentation server at http://localhost:8000 ..."
	@"$(PYTHON)" -m http.server 8000 --directory doc/html

run:
	@if [ -n "$(DISPATCH_CONTEXT)" ] && [ "$(FIRST_GOAL)" != "run" ]; then \
		:; \
	elif [ -n "$(strip $(EXTRA_SUBCOMMANDS))" ]; then \
		echo "[ERR] Too many run subcommands: $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))"; \
		"$(MAKE)" --no-print-directory _help_run; \
		exit 1; \
	elif [ -z "$(SUBCOMMAND)" ] || [ "$(SUBCOMMAND)" = "help" ]; then \
		"$(MAKE)" --no-print-directory _help_run; \
	elif [ "$(SUBCOMMAND)" = "docs" ]; then \
		"$(MAKE)" --no-print-directory _run_docs; \
	else \
		echo "[ERR] Unknown run subcommand: $(SUBCOMMAND)"; \
		"$(MAKE)" --no-print-directory _help_run; \
		exit 1; \
	fi

# ===========================================================================
# Cleaning
# ===========================================================================

_clean:
	@echo "[INFO] Cleaning generated files and caches..."
	@rm -rf build/ build-vcpkg/
	@rm -rf doc/html/ doc/xml/ doc/_build/
	@rm -rf python/.pytest_cache/ python/.mypy_cache/ python/.ruff_cache/
	@rm -rf python/build/ python/dist/ python/*.egg-info/
	@find . -type d -name __pycache__ -exec rm -rf {} + 2>/dev/null || true
	@echo "[OK] Cleaned"

_clean_docs:
	@echo "[INFO] Cleaning documentation artifacts..."
	@rm -rf doc/html/ doc/xml/ doc/_build/
	@echo "[OK] Documentation artifacts cleaned"

clean:
	@if [ -n "$(DISPATCH_CONTEXT)" ] && [ "$(FIRST_GOAL)" != "clean" ]; then \
		:; \
	elif [ -n "$(strip $(EXTRA_SUBCOMMANDS))" ]; then \
		echo "[ERR] Too many clean subcommands: $(wordlist 2,$(words $(MAKECMDGOALS)),$(MAKECMDGOALS))"; \
		"$(MAKE)" --no-print-directory _help_clean; \
		exit 1; \
	elif [ -z "$(SUBCOMMAND)" ]; then \
		"$(MAKE)" --no-print-directory _clean; \
	elif [ "$(SUBCOMMAND)" = "docs" ]; then \
		"$(MAKE)" --no-print-directory _clean_docs; \
	elif [ "$(SUBCOMMAND)" = "help" ]; then \
		"$(MAKE)" --no-print-directory _help_clean; \
	else \
		echo "[ERR] Unknown clean subcommand: $(SUBCOMMAND)"; \
		"$(MAKE)" --no-print-directory _help_clean; \
		exit 1; \
	fi

# ===========================================================================
# Help
# ===========================================================================

help:
	@if [ -n "$(DISPATCH_CONTEXT)" ] && [ "$(FIRST_GOAL)" != "help" ]; then \
		:; \
	else \
		"$(MAKE)" --no-print-directory _help_main; \
	fi

_help_main:
	@echo "asam-osi-utilities - Available Commands"
	@echo ""
	@echo "Use grouped commands with spaces. Run \`make <group> help\` to see subgroup details."
	@echo ""
	@echo "Setup:"
	@echo "  make setup          - Create the Python environment and install dev + docs dependencies"
	@echo "  make setup dev      - Install Git hooks after setup"
	@echo "  make setup docs     - Install docs-only Python dependencies"
	@echo "  make setup help     - Show setup subcommands"
	@echo ""
	@echo "Lint:"
	@echo "  make lint           - Run all linters (C++ format + Python ruff)"
	@echo "  make lint cpp       - Run C++ format checks"
	@echo "  make lint python    - Run Python ruff lint + format checks"
	@echo "  make lint help      - Show lint subcommands"
	@echo ""
	@echo "Format:"
	@echo "  make format         - Auto-format C++ and Python code"
	@echo "  make format cpp     - Auto-format C++ code"
	@echo "  make format python  - Auto-format Python code"
	@echo "  make format help    - Show format subcommands"
	@echo ""
	@echo "Test:"
	@echo "  make test           - Run all tests (C++ + Python)"
	@echo "  make test cpp       - Run C++ tests (vcpkg preset)"
	@echo "  make test python    - Run Python tests"
	@echo "  make test help      - Show test subcommands"
	@echo ""
	@echo "Docs:"
	@echo "  make docs           - Build unified docs (Doxygen XML -> Sphinx HTML)"
	@echo "  make docs build     - Build unified docs"
	@echo "  make docs help      - Show docs subcommands"
	@echo ""
	@echo "Run:"
	@echo "  make run docs       - Build and serve docs locally at http://localhost:8000"
	@echo "  make run help       - Show run subcommands"
	@echo ""
	@echo "Clean:"
	@echo "  make clean          - Remove all build artifacts and caches"
	@echo "  make clean docs     - Remove documentation artifacts only"
	@echo "  make clean help     - Show clean subcommands"
	@echo ""
	@echo "Legacy hyphenated targets remain available for compatibility but are intentionally hidden from this help output."

_help_setup:
	@echo "Setup subcommands:"
	@echo "  make setup          - Create the Python environment and install dev + docs dependencies"
	@echo "  make setup dev      - Install Git hooks after setup"
	@echo "  make setup docs     - Install docs-only Python dependencies"
	@echo "  make setup help     - Show this help"

_help_lint:
	@echo "Lint subcommands:"
	@echo "  make lint           - Run all linters"
	@echo "  make lint cpp       - Run C++ format checks"
	@echo "  make lint python    - Run Python lint + format checks"
	@echo "  make lint help      - Show this help"

_help_format:
	@echo "Format subcommands:"
	@echo "  make format         - Auto-format C++ and Python code"
	@echo "  make format cpp     - Auto-format C++ code"
	@echo "  make format python  - Auto-format Python code"
	@echo "  make format help    - Show this help"

_help_test:
	@echo "Test subcommands:"
	@echo "  make test           - Run all tests"
	@echo "  make test cpp       - Run C++ tests"
	@echo "  make test python    - Run Python tests"
	@echo "  make test help      - Show this help"

_help_docs:
	@echo "Docs subcommands:"
	@echo "  make docs           - Build unified docs"
	@echo "  make docs build     - Build unified docs"
	@echo "  make docs help      - Show this help"

_help_run:
	@echo "Run subcommands:"
	@echo "  make run docs       - Build and serve docs locally"
	@echo "  make run help       - Show this help"

_help_clean:
	@echo "Clean subcommands:"
	@echo "  make clean          - Remove all build artifacts and caches"
	@echo "  make clean docs     - Remove documentation artifacts only"
	@echo "  make clean help     - Show this help"

# Backward-compatible aliases for CI and existing automation.
setup-dev: _setup_dev
setup-docs: _setup_docs
lint-cpp: _lint_cpp
lint-python: _lint_python
format-cpp: _format_cpp
format-python: _format_python
test-cpp: _test_cpp
test-python: _test_python
docs-build: _docs_build
docs-serve: _run_docs
clean-docs: _clean_docs
