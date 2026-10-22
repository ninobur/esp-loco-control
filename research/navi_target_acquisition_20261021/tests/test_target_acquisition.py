"""Synthetic unit tests for the non-IR target-acquisition recognizer.

These are synthetic, not validation against recordings -- that is
replay.py's job, run separately against the real Otto PWM-90 stream and
reported in results/. Nothing here uses real recorded data.
"""
from __future__ import annotations

import math
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

import numpy as np

import route_map
from target_acquisition import TargetAcquisition, HALL_DEPARTURE_ADC, MIN_ACQUIRE_DURATION_S


BASELINE_LEVEL = 1800.0
DT = 0.02


def quiet_run(n, level=BASELINE_LEVEL, noise=1.0, t0=0.0, pwm=90, seed=0):
    rng = np.random.default_rng(seed)
    return [(t0 + i * DT, level + rng.normal(0, noise), pwm) for i in range(n)]


def curve_run(n, t0, amplitude, width_s=0.15, level=BASELINE_LEVEL, pwm=90, seed=1):
    rng = np.random.default_rng(seed)
    out = []
    mu = t0 + n * DT / 2
    for i in range(n):
        t = t0 + i * DT
        y = level + amplitude * math.exp(-0.5 * ((t - mu) / width_s) ** 2) + rng.normal(0, 1.0)
        out.append((t, y, pwm))
    return out


def feed(ta: TargetAcquisition, obs):
    completed = []
    for t, y, pwm in obs:
        completed += ta.observe(t, y, pwm)
    return completed


class TargetAcquisitionTests(unittest.TestCase):
    def make(self, last_confirmed=45, direction=1):
        return TargetAcquisition(last_confirmed_mm=last_confirmed, direction=direction)

    def test_target_is_deterministic_next_marker(self):
        ta = self.make(last_confirmed=45, direction=1)
        self.assertEqual(ta.target_mm, 46)
        self.assertEqual(ta.target_polarity, route_map.polarity_at(46))

    def test_baseline_not_trusted_before_minimum_span(self):
        ta = self.make()
        feed(ta, quiet_run(5))  # well under MIN_BASELINE_BINS / span
        self.assertIsNone(ta.baseline_fit)

    def test_baseline_estimation_from_quiet_samples(self):
        ta = self.make()
        feed(ta, quiet_run(40))  # 0.8 s, > MIN_BASELINE_SPAN_S
        self.assertIsNotNone(ta.baseline_fit)
        level, slope, noise = ta.baseline_fit
        self.assertAlmostEqual(level, BASELINE_LEVEL, delta=5.0)

    def test_correct_polarity_curve_is_acquired_and_closes(self):
        ta = self.make(last_confirmed=45, direction=1)  # target MM46, polarity South(0)
        self.assertEqual(ta.target_polarity, 0)
        feed(ta, quiet_run(40))
        sign = route_map.polarity_at(ta.target_mm)
        amp = -140.0 if sign == 0 else 140.0  # South departs below baseline
        obs = curve_run(40, t0=0.8, amplitude=amp, width_s=0.2)
        obs += quiet_run(20, t0=0.8 + 40 * DT, seed=2)
        completed = feed(ta, obs)
        kinds = [e.kind for e in ta.events]
        self.assertIn("OPEN_CANDIDATE", kinds)
        self.assertIn("ACQUIRED", kinds)
        self.assertIn("CLOSE_AND_ADVANCE", kinds)
        self.assertEqual(len(completed), 1)
        self.assertEqual(ta.last_confirmed_mm, 46)
        self.assertEqual(ta.target_mm, 47)

    def test_wrong_polarity_disturbance_is_preserved_not_advanced(self):
        ta = self.make(last_confirmed=45, direction=1)  # target South(0): expect departure below
        feed(ta, quiet_run(40))
        obs = curve_run(40, t0=0.8, amplitude=140.0, width_s=0.2)  # wrong sign: North-shaped
        obs += quiet_run(20, t0=0.8 + 40 * DT, seed=3)
        feed(ta, obs)
        kinds = [e.kind for e in ta.events]
        self.assertIn("CONTRADICTION", kinds)
        self.assertNotIn("CLOSE_AND_ADVANCE", kinds)
        # Target is unchanged -- the disposed candidate did not relocate NAVI.
        self.assertEqual(ta.last_confirmed_mm, 45)
        self.assertEqual(ta.target_mm, 46)
        contradiction = next(e for e in ta.events if e.kind == "CONTRADICTION")
        self.assertIn("resolved_sign", contradiction.detail)
        self.assertIn("peak_departure", contradiction.detail)

    def test_short_noise_excursion_below_duration_is_disposed_not_contradiction(self):
        ta = self.make(last_confirmed=45, direction=1)
        feed(ta, quiet_run(40))
        # Correct sign, but far too brief to ever reach MIN_ACQUIRE_DURATION_S.
        obs = curve_run(4, t0=0.8, amplitude=-140.0, width_s=0.04)
        obs += quiet_run(20, t0=0.8 + 4 * DT, seed=4)
        feed(ta, obs)
        kinds = [e.kind for e in ta.events]
        self.assertNotIn("ACQUIRED", kinds)
        self.assertNotIn("CLOSE_AND_ADVANCE", kinds)
        self.assertEqual(ta.target_mm, 46)

    def test_no_advancement_before_closure(self):
        ta = self.make(last_confirmed=45, direction=1)
        feed(ta, quiet_run(40))
        obs = curve_run(40, t0=0.8, amplitude=-140.0, width_s=0.2)
        feed(ta, obs)  # curve still open -- no quiet return fed yet
        self.assertEqual(ta.last_confirmed_mm, 45, "must not advance before observed RTB")
        self.assertIsNotNone(ta.candidate)

    def test_extended_acquisition_has_no_automatic_timeout(self):
        ta = self.make(last_confirmed=45, direction=1)
        feed(ta, quiet_run(40))
        # A candidate open far longer than any plausible magnet, still with
        # correct polarity, must still eventually close and advance -- no
        # elapsed-time limit should have fired a LOST/rejection in between.
        obs = curve_run(400, t0=0.8, amplitude=-140.0, width_s=2.0)  # 8 s open
        obs += quiet_run(20, t0=0.8 + 400 * DT, seed=5)
        completed = feed(ta, obs)
        self.assertEqual(len(completed), 1)
        self.assertEqual(ta.last_confirmed_mm, 46)
        self.assertFalse(any(e.kind in ("LOST", "TIMEOUT") for e in ta.events))

    def test_stationary_samples_do_not_invent_progression(self):
        ta = self.make(last_confirmed=45, direction=1)
        feed(ta, quiet_run(40))
        # PWM = 0 throughout a long stationary stretch with drifting ADC --
        # must not be read as baseline, candidate, or movement evidence.
        stall = [(0.8 + i * DT, BASELINE_LEVEL + 500.0, 0) for i in range(300)]
        feed(ta, stall)
        self.assertEqual(ta.target_mm, 46, "stationary samples must not advance the target")
        self.assertIsNone(ta.candidate, "stationary samples must not open a candidate")

    def test_stop_mid_candidate_preserves_evidence(self):
        ta = self.make(last_confirmed=45, direction=1)
        feed(ta, quiet_run(40))
        obs = curve_run(20, t0=0.8, amplitude=-140.0, width_s=0.3)
        feed(ta, obs)
        self.assertIsNotNone(ta.candidate)
        samples_before = len(ta.candidate.samples)
        stall = [(0.8 + (20 + i) * DT, 9999.0, 0) for i in range(100)]  # garbage while PWM=0
        feed(ta, stall)
        self.assertEqual(len(ta.candidate.samples), samples_before, "PWM=0 bins must not be fed in")
        # Resume moving: candidate still open with its prior evidence intact.
        obs2 = curve_run(20, t0=0.8 + 20 * DT, amplitude=-140.0, width_s=0.3, seed=9)
        obs2 += quiet_run(20, t0=0.8 + 40 * DT, seed=6)
        feed(ta, obs2)
        self.assertEqual(ta.last_confirmed_mm, 46)

    def test_contradicting_evidence_is_never_relabeled(self):
        ta = self.make(last_confirmed=45, direction=1)
        feed(ta, quiet_run(40))
        obs = curve_run(40, t0=0.8, amplitude=140.0, width_s=0.2)
        obs += quiet_run(20, t0=0.8 + 40 * DT, seed=7)
        feed(ta, obs)
        contradiction = next(e for e in ta.events if e.kind == "CONTRADICTION")
        # The disposed candidate is reported against the target it failed
        # to match, not silently reassigned to some other mapped magnet.
        self.assertEqual(contradiction.target_mm, 46)

    def test_causal_no_future_sample_access(self):
        ta = self.make(last_confirmed=45, direction=1)
        feed(ta, quiet_run(40))
        obs = curve_run(40, t0=0.8, amplitude=-140.0, width_s=0.2)
        obs += quiet_run(20, t0=0.8 + 40 * DT, seed=8)
        t_seen = 0.0
        for t, y, pwm in obs:
            for e in ta.observe(t, y, pwm):
                pass
            t_seen = t
            for e in ta.events:
                self.assertLessEqual(e.t, t_seen + 1e-9,
                                      "an event must never be timestamped beyond samples seen so far")


if __name__ == "__main__":
    unittest.main()
