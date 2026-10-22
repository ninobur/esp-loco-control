"""Regression tests for the seed-timing defect confirmed in the
2026-10-22 corrective investigation (independent review finding #2):
geographic progression must not begin before the seed annotation's own
encounter has ended. Synthetic trace only -- no recorded data needed.
"""
from __future__ import annotations

import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from annotations import Opening
from replay import seed_established_index

DT = 0.02


def make_trace(t0, n):
    return [(round(t0 + i * DT, 3), 1890.0, 90) for i in range(n)]


class SeedTimingTests(unittest.TestCase):
    def test_progress_starts_after_seed_encounter_ends_not_at_trial_start(self):
        # Trial spans [0.0, 2.0); the seed annotation's own encounter is
        # [0.40, 0.60) -- well after the trial's raw start.
        trace = make_trace(0.0, 100)  # t in [0.00, 1.98]
        seed = Opening(enc_id="1", start_s=0.40, end_s=0.60, mm=10, direction=1, polarity=0)
        idx = seed_established_index(trace, 0, len(trace), seed)
        self.assertIsNotNone(idx)
        # The returned index's time must be >= the seed's own end, never
        # the trial's raw start (0.0) -- that was the confirmed defect.
        self.assertGreaterEqual(trace[idx][0], seed.end_s)
        self.assertLess(trace[idx - 1][0], seed.end_s if idx > 0 else trace[idx][0])

    def test_seed_encounter_not_closed_in_trial_returns_none(self):
        # Trial ends before the seed's own encounter ends.
        trace = make_trace(0.0, 10)  # t in [0.00, 0.18]
        seed = Opening(enc_id="1", start_s=0.10, end_s=5.00, mm=10, direction=1, polarity=0)
        idx = seed_established_index(trace, 0, len(trace), seed)
        self.assertIsNone(idx)

    def test_seed_at_exact_trial_start_still_requires_its_own_end(self):
        trace = make_trace(0.0, 50)
        seed = Opening(enc_id="1", start_s=0.0, end_s=0.3, mm=10, direction=1, polarity=0)
        idx = seed_established_index(trace, 0, len(trace), seed)
        self.assertIsNotNone(idx)
        self.assertGreaterEqual(trace[idx][0], 0.3)
        self.assertGreater(idx, 0, "must not start at the raw trial beginning when the seed "
                                    "encounter has not yet ended there")


if __name__ == "__main__":
    unittest.main()
