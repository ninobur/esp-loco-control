#!/usr/bin/env python3
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from quorum_map import QuorumMap
from templates_replay import Replay


DNA = [1, 1, 0, 1, 0, 0, 0, 1, 0, 1, 1, 1, 0, 0, 1, 0, 0]


def event(t, pol, duration=160, peak=180):
    return {"session": "TEST", "t_ms": t, "polarity": "N" if pol else "S",
            "duration_ms": duration, "peak": peak, "pwm": 90,
            "label": "SYNTHETIC", "nav_before": None, "nav_after": None,
            "nav_state": "NORMAL"}


class TemplatesReplayTest(unittest.TestCase):
    def setUp(self):
        self.m = QuorumMap.from_data(DNA, [300] * len(DNA))

    def test_artifact_has_no_standing(self):
        r = Replay(self.m, 1, 0)
        r.feed(event(1000, DNA[1], duration=43, peak=39))
        self.assertEqual(r.mm, 0)
        self.assertIsNone(r.recovery)
        self.assertEqual(r.rows[-1]["action"], "ARTIFACT")

    def test_expected_passage_advances_once(self):
        r = Replay(self.m, 1, 0)
        r.feed(event(1000, DNA[1]))
        self.assertEqual(r.mm, 1)
        self.assertEqual(r.metrics["primary_advances"], 1)

    def test_one_omission_recovers_from_perfect_sequence(self):
        r = Replay(self.m, 1, 0, recovery_observations=6)
        t = 1000
        # Marker 1 was deliberately omitted; observations begin at marker 2.
        for pos in range(2, 9):
            r.feed(event(t, DNA[pos]))
            t += 1000
        self.assertGreaterEqual(r.metrics["recoveries"], 1)
        self.assertEqual(r.metrics["recovery_omissions"], 1)
        self.assertFalse(r.safe_stopped)

    def test_unresolvable_credible_stream_stops_safely(self):
        r = Replay(self.m, 1, 0)
        t = 1000
        for _ in range(13):
            r.feed(event(t, 0))
            t += 1000
        self.assertTrue(r.safe_stopped)
        stopped_mm = r.mm
        r.feed(event(t, 1))
        self.assertEqual(r.mm, stopped_mm)

    def test_companion_lobe_is_not_second_marker(self):
        r = Replay(self.m, 1, 0)
        r.feed(event(1000, DNA[1]))
        r.feed(event(1200, not DNA[1]))
        self.assertEqual(r.mm, 1)
        self.assertEqual(r.rows[-1]["action"], "COMPANION_MERGED")


if __name__ == "__main__":
    unittest.main()
