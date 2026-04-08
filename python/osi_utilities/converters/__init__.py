# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Converters for transforming between OSI message types and trace formats."""

from typing import Any


def convert_gt2sv(*args: Any, **kwargs: Any) -> Any:
    """Lazily import and call the GT->SV converter."""
    from osi_utilities.converters.gt2sv import convert_gt2sv as _convert_gt2sv

    return _convert_gt2sv(*args, **kwargs)

__all__ = [
    "convert_gt2sv",
]
