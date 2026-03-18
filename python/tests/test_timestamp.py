# SPDX-License-Identifier: MPL-2.0
# SPDX-FileCopyrightText: Copyright (c) 2026, Bayerische Motoren Werke Aktiengesellschaft (BMW AG)

"""Tests for public timestamp conversion utilities."""

from __future__ import annotations

import pytest
from google.protobuf.descriptor_pb2 import FileDescriptorProto
from osi3.osi_groundtruth_pb2 import GroundTruth

from osi_utilities.tracefile.timestamp import (
    nanoseconds_to_seconds,
    seconds_to_nanoseconds,
    timestamp_to_nanoseconds,
    timestamp_to_seconds,
)


def _make_message(seconds: int, nanos: int) -> GroundTruth:
    gt = GroundTruth()
    gt.timestamp.seconds = seconds
    gt.timestamp.nanos = nanos
    return gt


class TestTimestampToNanoseconds:
    def test_basic(self):
        msg = _make_message(10, 500)
        assert timestamp_to_nanoseconds(msg) == 10_000_000_500

    def test_zero(self):
        msg = _make_message(0, 0)
        assert timestamp_to_nanoseconds(msg) == 0

    def test_nanos_only(self):
        msg = _make_message(0, 123_456_789)
        assert timestamp_to_nanoseconds(msg) == 123_456_789

    def test_seconds_only(self):
        msg = _make_message(42, 0)
        assert timestamp_to_nanoseconds(msg) == 42_000_000_000

    def test_no_timestamp_field(self):
        msg = FileDescriptorProto()
        assert timestamp_to_nanoseconds(msg) == 0


class TestTimestampToSeconds:
    def test_basic(self):
        msg = _make_message(1, 500_000_000)
        assert timestamp_to_seconds(msg) == pytest.approx(1.5)

    def test_zero(self):
        msg = _make_message(0, 0)
        assert timestamp_to_seconds(msg) == 0.0

    def test_subsecond(self):
        msg = _make_message(0, 100_000_000)
        assert timestamp_to_seconds(msg) == pytest.approx(0.1)

    def test_no_timestamp_field(self):
        msg = FileDescriptorProto()
        assert timestamp_to_seconds(msg) == 0.0


class TestNanosecondsToSeconds:
    def test_basic(self):
        assert nanoseconds_to_seconds(1_500_000_000) == pytest.approx(1.5)

    def test_zero(self):
        assert nanoseconds_to_seconds(0) == 0.0


class TestSecondsToNanoseconds:
    def test_basic(self):
        assert seconds_to_nanoseconds(1.5) == 1_500_000_000

    def test_zero(self):
        assert seconds_to_nanoseconds(0.0) == 0

    def test_roundtrip(self):
        original_s = 3.141592653
        ns = seconds_to_nanoseconds(original_s)
        back = nanoseconds_to_seconds(ns)
        assert back == pytest.approx(original_s, abs=1e-9)
