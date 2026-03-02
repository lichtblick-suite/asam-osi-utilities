# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Sphinx configuration for osi-utilities unified documentation."""

import json
import sys
from pathlib import Path

# -- Path setup --------------------------------------------------------------
# Add Python SDK to sys.path so autodoc can import it
sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "python"))

# -- Project information -----------------------------------------------------
# Read version from vcpkg.json (single source of truth)
_vcpkg = json.loads((Path(__file__).resolve().parent.parent / "vcpkg.json").read_text())
project = "osi-utilities"
copyright = "2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)"
author = "BMW AG"
version = _vcpkg.get("version-string", "0.0.0")
release = version

# -- General configuration ---------------------------------------------------
extensions = [
    "breathe",  # Doxygen XML → Sphinx bridge (C++ API)
    "sphinx.ext.autodoc",  # Python docstring extraction
    "sphinx.ext.napoleon",  # Google/NumPy docstring style support
    "sphinx.ext.viewcode",  # Links to highlighted source code
    "sphinx.ext.intersphinx",  # Cross-project references
    "myst_parser",  # Include existing Markdown files
    "sphinx_copybutton",  # Copy button on code blocks
]

# -- Breathe configuration (C++ API via Doxygen XML) -------------------------
breathe_projects = {"osi-utilities": str(Path(__file__).resolve().parent / "xml")}
breathe_default_project = "osi-utilities"
breathe_default_members = ("members", "undoc-members")

# -- Autodoc configuration (Python API) --------------------------------------
autodoc_default_options = {
    "members": True,
    "undoc-members": True,
    "show-inheritance": True,
    "imported-members": False,
}
autodoc_member_order = "bysource"
autodoc_typehints = "description"

# -- Napoleon configuration (Google-style docstrings) -------------------------
napoleon_google_docstring = True
napoleon_numpy_docstring = False
napoleon_include_init_with_doc = False

# -- MyST configuration (Markdown support) ------------------------------------
myst_enable_extensions = [
    "colon_fence",
    "fieldlist",
]
myst_heading_anchors = 3
myst_url_schemes = ("http", "https", "mailto")

# -- Intersphinx configuration -----------------------------------------------
# Optional: gracefully degrade when offline or behind a proxy
intersphinx_mapping = {
    "python": ("https://docs.python.org/3", None),
    "protobuf": ("https://googleapis.dev/python/protobuf/latest/", None),
}
intersphinx_timeout = 5

# -- Warning suppression -----------------------------------------------------
# Breathe warnings are expected when Doxygen XML hasn't been generated yet
# (e.g. local dev without CMake build). Suppress to avoid false negatives.
suppress_warnings = [
    "myst.xref_missing",
]

# -- HTML output configuration -----------------------------------------------
html_theme = "furo"
html_title = "OSI Utilities"
html_theme_options = {
    "source_repository": "https://github.com/lichtblick-suite/asam-osi-utilities",
    "source_branch": "main",
    "source_directory": "doc/",
}

# -- Source file configuration ------------------------------------------------
source_suffix = {
    ".rst": "restructuredtext",
    ".md": "markdown",
}
exclude_patterns = ["_build", "xml", "html", "Doxyfile.in"]

# -- Template and static paths -----------------------------------------------
templates_path = []
html_static_path = []
