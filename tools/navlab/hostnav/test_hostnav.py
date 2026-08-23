#!/usr/bin/env python3
"""Focused unit tests for the host navigator's internals.

The frozen acceptance suite measures behaviour end to end. These tests pin
the pieces it exercises only indirectly, so a regression names its own cause:
the map is read-only and the uniqueness lengths are computed, elapsed time is
branch-local and never double-counted, propagation over-approximates in both
directions, the reversal lane keeps a reversed truth, peer information can
only enlarge or invalidate a bound, and published occupancy never omits a
candidate.

Run:  python3 -m unittest tools.navlab.hostnav.test_hostnav -v
      python3 tools/navlab/hostnav/test_hostnav.py
"""
import pathlib
import re
import sys
import unittest

if __name__ == '__main__':                                  # pragma: no cover
    sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[3]))

from tools.navlab.acceptance import navapi as A
from tools.navlab.hostnav import params as P
from tools.navlab.hostnav import route as R
from tools.navlab.hostnav.branches import KNOWN, Lane, propagate
from tools.navlab.hostnav.envelopes import NominalEnvelope, PwmHistory
from tools.navlab.hostnav.navigator import Navigator
from tools.navlab.hostnav.occupancy import (covering_arcs, peer_occupancy,
                                            separated)

HERE = pathlib.Path(__file__).resolve().parent
#: The navigator itself, excluding this test module, which quotes the
#: names it forbids.
PKG = sorted(f for f in HERE.glob('*.py')
             if not f.name.startswith('test_'))


def detection(t, epoch, mm, pwm=60, elapsed=2300, peak=180, dur=200):
    return A.Detection(t_detect=t, clock_epoch=epoch,
                       polarity='N' if R.DNA[mm] == 1 else 'S',
                       peak=peak, duration_ms=dur,
                       pwm_actual_history=[(elapsed, pwm), (0, pwm)],
                       decoy_firmware_mm=(mm + 55) % R.DNA_N,
                       decoy_firmware_verdict='QUARANTINED',
                       decoy_mqtt_recv_ts=t + 2500)


class TestRoute(unittest.TestCase):
    """3.3: uniqueness is computed over the committed map, never asserted."""

    def test_map_matches_committed_firmware(self):
        self.assertEqual(R.DNA_N, 171)
        self.assertEqual(len(R.SPACING), R.DNA_N)
        self.assertEqual(R.CIRCUIT_MM, sum(R.SPACING))
        self.assertEqual(set(R.DNA), {0, 1})

    def test_uniqueness_lengths_are_computed(self):
        self.assertEqual((R.W_DIR, R.W_BOTH), (10, 12))
        # and are genuinely minimal: one shorter collides.
        short = {}
        for p in range(R.DNA_N):
            short.setdefault(R.window(p, R.CW, R.W_DIR - 1), []).append(p)
        self.assertTrue(any(len(v) > 1 for v in short.values()))

    def test_unique_tables_resolve_every_position(self):
        for step in R.DIRS:
            for p in range(R.DNA_N):
                hit = R.UNIQUE_DIR[step][R.window(p, step, R.W_DIR)]
                self.assertEqual(hit, [(p, step)])
                both = R.UNIQUE_BOTH[R.window(p, step, R.W_BOTH)]
                self.assertEqual(both, [(p, step)])

    def test_map_is_never_written(self):
        """A prerequisite failure records the failure; the map stands."""
        import hashlib
        before = hashlib.sha256(R._INO.read_bytes()).hexdigest()
        nav = Navigator()
        nav.start(A.MODE_LAUNCH_REGION, A.Policy(), direction=R.CW)
        mm = 40
        for i in range(12):
            mm = R.nxt(mm, R.CW)
            nav.observe(detection(3000 + 2300 * i, 1, mm))
        self.assertEqual(hashlib.sha256(R._INO.read_bytes()).hexdigest(), before)
        for f in PKG:
            src = f.read_text()
            for banned in ('write_text', 'write_bytes', 'unlink', "'w'", '"w"'):
                self.assertTrue(banned not in src,
                                '%s can write: %s' % (f.name, banned))


class TestBranchLocalTiming(unittest.TestCase):
    """3.6: the worked contrast, asserted rather than described."""

    def test_phantom_branch_does_not_advance_last_genuine(self):
        env = NominalEnvelope()
        hist = PwmHistory()
        lane = Lane(allow_reversal=False)
        lane.seed({(40, R.CW)}, 0, 1, origin=True)
        lane.branches[0].last_genuine = 1200
        # a ghost at 1250: weak and short, so GHOST_LIKE
        ghost = detection(1250, 1, 41, peak=40, dur=40, elapsed=50)
        hist.add_detection(ghost.t_detect, ghost.pwm_actual_history)
        lane.observe(_obs(ghost), env, hist, ghost_like=True)
        self.assertEqual([b.last_genuine for b in lane.branches], [1200],
                         'the phantom branch must keep the earlier origin')
        # the successor is then measured from 1200, once, on this branch
        b = lane.branches[0]
        d_lo, d_hi = env.distance_window(hist, b.last_genuine, 2400)
        self.assertEqual(d_lo, 0.0)
        self.assertAlmostEqual(d_hi, P.nominal_speed(60) * P.SPEED_BAND_HI * 1200)

    def test_no_precomputed_dt_is_read(self):
        self.assertFalse(hasattr(A.Detection, 'dt'))
        for f in PKG:
            src = f.read_text()
            for banned in ('decoy_firmware_mm', 'decoy_firmware_verdict',
                           'decoy_mqtt_recv_ts', 'decoy_claimed_mm'):
                self.assertTrue(
                    banned not in src,
                    'P4: %s reads %s, which is not evidence about position'
                    % (f.name, banned))

    def test_epoch_mismatch_is_unknown_time(self):
        nav = Navigator()
        nav.start(A.MODE_EXACT, A.Policy(), mm=40, direction=R.CW)
        t = 3000
        mm = 40
        for _ in range(4):
            mm = R.nxt(mm, R.CW)
            nav.observe(detection(t, 1, mm))
            t += 2300
        self.assertFalse(nav.status().gap_bearing)
        nav.observe(detection(120, 2, R.nxt(mm, R.CW)))     # new epoch
        st = nav.status()
        self.assertTrue(st.gap_bearing)
        self.assertFalse(st.confirmation_authority)


class TestPropagation(unittest.TestCase):
    """3.5.1: both ends over-approximate, so H keeps the truth (P5)."""

    def test_window_admits_the_true_successor(self):
        for mm in range(R.DNA_N):
            for step in R.DIRS:
                q = R.nxt(mm, step)
                out, skip = propagate({(mm, step)}, R.DNA[q], KNOWN, 0.0,
                                      R.step_mm(mm, step) * 1.3, False)
                self.assertIn((q, step), out)
                self.assertFalse(skip)

    def test_missed_markers_need_no_special_case(self):
        mm, step = 40, R.CW
        far = mm
        for _ in range(4):
            far = R.nxt(far, step)
        dist = sum(R.step_mm((mm + i) % R.DNA_N, step) for i in range(4))
        out, skip = propagate({(mm, step)}, R.DNA[far], KNOWN, 0.0, dist * 1.05,
                              False)
        self.assertIn((far, step), out)
        self.assertTrue(skip, 'an admitted skip must be flagged for 3.11')

    def test_standstill_only_when_pwm_leaves_it_possible(self):
        out, _ = propagate({(40, R.CW)}, R.DNA[40], KNOWN, 0.0, 0.0, False,
                           stopped=True)
        self.assertEqual(out, {(40, R.CW)})
        out, _ = propagate({(40, R.CW)}, R.DNA[40], KNOWN, 0.0, 400.0, False,
                           stopped=False)
        self.assertNotIn((40, R.CW), out)

    def test_reversal_lane_keeps_a_reversed_truth(self):
        """T4's property, at the component level."""
        collisions = 0
        for mm in range(R.DNA_N):
            back = R.nxt(mm, R.CCW)
            if R.DNA[back] != R.DNA[R.nxt(mm, R.CW)]:
                continue                       # forward chain dies on its own
            collisions += 1
            out, _ = propagate({(mm, R.CW)}, R.DNA[back], KNOWN, 0.0,
                               R.step_mm(mm, R.CCW) * 1.3, True)
            self.assertIn((back, R.CCW), out)
            out, _ = propagate({(mm, R.CW)}, R.DNA[back], KNOWN, 0.0,
                               R.step_mm(mm, R.CCW) * 1.3, False)
            self.assertNotIn((back, R.CCW), out)
        self.assertGreater(collisions, 0,
                           'the polarity-invisible reversal must exist on this '
                           'map, or the test proves nothing')


class TestPeerAndOccupancy(unittest.TestCase):
    """3.12.2: enlarge or invalidate, never create, never grant."""

    def test_no_region_yields_no_bound(self):
        rep = A.PeerReport(t_report=0, peer_id='p', commanded_stopped=True,
                           reported_speed_mm_per_ms=0.0, decoy_claimed_mm=100)
        self.assertIsNone(peer_occupancy(rep, 0))
        self.assertIsNone(peer_occupancy(rep, 10 ** 6))

    def test_staleness_enlarges_and_immobilised_does_not(self):
        moving = A.PeerReport(t_report=0, peer_id='p',
                              bounded_region=(100, 101))
        latched = A.PeerReport(t_report=0, peer_id='p',
                               bounded_region=(100, 101), immobilised=True)
        fresh = peer_occupancy(moving, 0)
        stale = peer_occupancy(moving, 30000)
        self.assertGreater(len(stale), len(fresh))
        self.assertEqual(len(peer_occupancy(latched, 600000)), len(fresh))

    def test_a_stale_enough_bound_becomes_unbounded(self):
        rep = A.PeerReport(t_report=0, peer_id='p', bounded_region=(100, 101))
        self.assertIsNone(peer_occupancy(rep, 10 ** 6))

    def test_separation_uses_the_worst_case_pair(self):
        self.assertFalse(separated({10}, {10 + P.CLEAR_GAP_MARKERS}))
        self.assertTrue(separated({10}, {10 + P.CLEAR_GAP_MARKERS + 1}))
        self.assertFalse(separated({0, 60}, {66}))

    def test_published_arcs_never_omit_a_candidate(self):
        import random
        rng = random.Random(9)
        for _ in range(2000):
            occ = R.occupancy_markers(
                {(rng.randrange(R.DNA_N), R.CW)
                 for _ in range(rng.randint(1, 12))},
                P.TRAIN_EXTENT_MARKERS)
            arcs = covering_arcs(occ)
            self.assertLessEqual(len(arcs), P.OCC_ARCS_MAX)
            covered = set()
            for a, b in arcs:
                mm = a % R.DNA_N
                for _ in range(R.DNA_N):
                    covered.add(mm)
                    if mm == b % R.DNA_N:
                        break
                    mm = (mm + 1) % R.DNA_N
            self.assertFalse(occ - covered)


class TestStructure(unittest.TestCase):
    """7.7 / 7.8, checked over the whole package, not just the factory module."""

    def test_no_prohibited_symbol_anywhere_in_the_package(self):
        for f in PKG:
            src = f.read_text()
            for sym in A.PROHIBITED_SYMBOLS:
                self.assertTrue(sym not in src,
                                '%s defines the prohibited symbol %r'
                                % (f.name, sym))
            holdish = set(re.findall(r"\b([A-Z][A-Z_]*HOLD[A-Z_]*)\b", src))
            self.assertFalse(holdish - {'CTO_FLEET_HOLD'}, f.name)

    def test_only_one_navigation_commanded_motion_order(self):
        nav = Navigator()
        nav.start(A.MODE_EXACT, A.Policy(), mm=40, direction=R.CW)
        self.assertEqual(A.ONLY_MOTION_ORDER, A.STOPPED_FOR_NAVIGATION_SAFETY)
        self.assertIn(nav.status().movement_state, A.MOVEMENT_STATES)

    def test_a_manual_declaration_is_never_demanded(self):
        nav = Navigator()
        nav.start(A.MODE_UNKNOWN, A.Policy())
        self.assertFalse(nav.status().manual_declaration_required)
        self.assertTrue(nav.operator('manual_throttle', pwm=60))
        self.assertFalse(nav.status().manual_declaration_required)


class TestOperatorBoundaries(unittest.TestCase):
    """7.5, and the rulings that are not configurable."""

    def test_declaration_requires_stationary_identification(self):
        nav = Navigator()
        nav.start(A.MODE_EXACT, A.Policy(), mm=40, direction=R.CW)
        self.assertFalse(nav.operator('declare_mm', mm=99, stationary=False))
        self.assertTrue(nav.operator('declare_mm', mm=99, stationary=True))
        self.assertEqual(nav.status().confirmed_mm, 99)
        self.assertEqual(len(nav.status().hypotheses), 1)

    def test_launch_region_is_never_presumed(self):
        nav = Navigator()
        nav.start(A.MODE_UNKNOWN, A.Policy())
        st = nav.status()
        self.assertEqual(st.nav_state, A.UNLOCATED)
        self.assertEqual(len(st.hypotheses), 2 * R.DNA_N)
        self.assertFalse({h[0] for h in st.hypotheses} <= set(R.LAUNCH_REGION))

    def test_launch_region_is_the_operator_ruling(self):
        self.assertEqual(R.LAUNCH_REGION, tuple(range(36, 46)))
        self.assertEqual(len(R.LAUNCH_REGION), 10)
        for p in R.LAUNCH_REGION:
            self.assertGreaterEqual(
                R.markers_ahead(p, R.STATIONS['Grillers'], R.CW),
                R.STATION_LOOKAHEAD_MARKERS)
            self.assertGreaterEqual(
                R.markers_ahead(p, R.STATIONS['Patio'], R.CCW),
                R.STATION_LOOKAHEAD_MARKERS)

    def test_exact_startup_costs_no_acquisition_sequence(self):
        for mm in (0, 40, 170):
            for step in R.DIRS:
                nav = Navigator()
                nav.start(A.MODE_EXACT, A.Policy(), mm=mm, direction=step)
                st = nav.status()
                self.assertEqual(st.nav_state, A.POSITIONED)
                self.assertEqual(st.hypotheses, {(mm, step)})
                self.assertEqual((st.confirmed_mm, st.confirmed_dir),
                                 (mm, step))


class _obs(object):
    __slots__ = ('t_detect', 'clock_epoch', 'pol_bit')

    def __init__(self, det):
        self.t_detect = det.t_detect
        self.clock_epoch = det.clock_epoch
        self.pol_bit = R.pol_bit(det.polarity)


if __name__ == '__main__':                                  # pragma: no cover
    unittest.main(verbosity=2)
