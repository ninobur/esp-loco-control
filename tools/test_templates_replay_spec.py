#!/usr/bin/env python3
"""Synthetic self-tests for templates_replay_spec.py (fast, no capture files
needed). Run before trusting the tool against the real ~2.8-3.7M row
captures -- see the task return summary for the real-capture self-test.

    python3 tools/test_templates_replay_spec.py
"""
import os
import sys
import unittest

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from quorum_map import QuorumMap
import templates_replay_spec as T


# DNA[2..13] is engineered so that, from base_mm=0/direction=+1, offset o=1's
# target (dna[2]) is the ONLY one of dna[2..13] holding a 1 -- every other
# offset o=2..12 targets dna[3..13], all engineered to 0. This makes the
# offset-hypothesis recovery tests deterministic by construction rather than
# by luck: repeating an observation equal to dna[2] gives offset 1 a
# perfect, ever-growing lead over every other offset (verified, not just
# asserted, in RecoveryTests.setUp). dna[1]=0 (opposite of dna[2]) so the
# very first candidate at expected mm=1 is a guaranteed contradiction.
DNA = [0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1]
SPACING = [300] * len(DNA)

# A second fixture for the "genuinely unresolvable" recovery tests: dna[1]=0
# so a constant observed=N candidate always contradicts, but dna[2..7]=1 and
# dna[8..13]=0 means offsets 1-6 all predict N and offsets 7-12 all predict
# S -- a constant-N stream ties all of offsets {1..6} together forever
# (every repeat grows all six by the same amount, margin stuck at 0), so it
# can never accumulate a clean MARGIN>=2 leader. This is deliberately a
# DIFFERENT fixture from DNA above, which is engineered for the opposite
# property (a single unique, fast-converging offset) -- no single fixture
# can honestly exercise both a clean win and a guaranteed non-convergence.
DNA_NOISY = [0, 0, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0]
SPACING_NOISY = [300] * len(DNA_NOISY)


class FakeManifest:
    def __init__(self, start_mm, start_t_ms, direction="CW"):
        self.path = "<test>"
        self.start_mm = start_mm
        self.start_t_ms = start_t_ms
        self.start_direction = T._dir_to_sign(direction)
        self.anchors = []
        self.known_context = []
        self.uncertainty_notes = []


def new_session(qmap, start_mm=0, start_t_ms=0, direction="CW", session="TEST"):
    m = FakeManifest(start_mm, start_t_ms, direction)
    return T.ReplaySession(qmap, m, capture_label="<test>", session_label=session)


def feed_sample(s, t_ms, raw, baseline, pwm_a=90, pwm_c=90, dir_str="CW"):
    row = {"t_ms": t_ms, "phys_raw": raw, "phys_baseline": baseline,
          "ctl_pwm_actual": pwm_a, "ctl_pwm_commanded": pwm_c, "ctl_dir": dir_str}
    s.process_row("SAMPLE", lambda name: row.get(name))


def emit_passage(s, start_t, sign, peak=200, rise_ms=15, plateau_ms=100, fall_ms=15,
                 tail_ms=40, baseline=1800, pwm_a=90, pwm_c=90, dir_str="CW", step=5):
    """A clean, floor-clearing trapezoidal passage -- comfortably above
    DURATION_FLOOR_MS/FLUX_FLOOR by construction (matches the doctrine's own
    reported clean-passage shape: ~160ms, ~190 peak)."""
    t = start_t
    pts = [(0, 0), (rise_ms, peak), (rise_ms + plateau_ms, peak),
          (rise_ms + plateau_ms + fall_ms, 0), (rise_ms + plateau_ms + fall_ms + tail_ms, 0)]
    last_t = start_t
    prev_off, prev_v = pts[0]
    for off, v in pts[1:]:
        n = max(1, (off - prev_off) // step)
        for i in range(1, n + 1):
            frac = i / n
            off_i = prev_off + frac * (off - prev_off)
            v_i = prev_v + frac * (v - prev_v)
            tt = start_t + int(round(off_i))
            d = int(round(v_i)) * sign
            feed_sample(s, tt, baseline + d, baseline, pwm_a, pwm_c, dir_str)
            last_t = tt
        prev_off, prev_v = off, v
    return last_t


def emit_spike(s, start_t, sign, peak=45, duration_ms=8, baseline=1800, pwm_a=90,
              dir_str="CW"):
    """A short artifact spike -- clears ENTRY_COUNTS but stays under
    DURATION_FLOOR_MS."""
    feed_sample(s, start_t, baseline + sign * peak, baseline, pwm_a, pwm_a, dir_str)
    feed_sample(s, start_t + duration_ms // 2, baseline + sign * peak, baseline, pwm_a, pwm_a, dir_str)
    feed_sample(s, start_t + duration_ms, baseline + sign * 10, baseline, pwm_a, pwm_a, dir_str)
    feed_sample(s, start_t + duration_ms + T.EXIT_HOLD_MS, baseline + sign * 10, baseline, pwm_a, pwm_a, dir_str)


def target_pol01(qmap, base_mm, direction, o):
    pos = qmap.route_mod(base_mm + direction * o)
    nxt = qmap.next_mm(pos, direction)
    return qmap.dna_at(nxt)


class AcquisitionAndArtifactTests(unittest.TestCase):
    def setUp(self):
        self.qmap = QuorumMap.from_data(DNA, SPACING)

    def test_short_spike_is_artifact_duration_floor(self):
        s = new_session(self.qmap)
        emit_spike(s, 1000, sign=1)
        s.finalize()
        self.assertEqual(len(s.rows_out), 1)
        row = s.rows_out[0]
        self.assertEqual(row["disposition"], "ARTIFACT")
        self.assertEqual(row["disposition_rule_id"], "ARTIFACT_DURATION_FLOOR")
        self.assertFalse(row["position_ring_inserted"])
        self.assertFalse(row["contradiction_ring_inserted"])
        # declared at start_t_ms=0 <= open_t=1000; an ARTIFACT never mutates position.
        self.assertEqual(s.primary_position, 0)

    def test_excessive_open_is_artifact(self):
        s = new_session(self.qmap)
        # Frozen-baseline pathology (Sec 1.3/2.5): opens, then just sits above
        # ENTRY_COUNTS without ever completing an exit hold, past MAX_OPEN_MS.
        t = 1000
        feed_sample(s, t, 1800 + 60, 1800)
        while t - 1000 < T.MAX_OPEN_MS + 200:
            t += 200
            feed_sample(s, t, 1800 + 60, 1800)
        s.finalize()
        artifacts = [r for r in s.rows_out if r["disposition_rule_id"] == "ARTIFACT_EXCESSIVE_OPEN"]
        self.assertEqual(len(artifacts), 1)
        self.assertLessEqual(artifacts[0]["evidence_duration_ms"], T.MAX_OPEN_MS + 200)
        self.assertGreaterEqual(artifacts[0]["evidence_duration_ms"], T.MAX_OPEN_MS)

    def test_flux_floor_gate_direct(self):
        """Constants ENTRY_COUNTS=38/EXIT_COUNTS=25 make a duration>=40ms
        candidate with abs_flux<900 mechanically awkward to build sample-by-
        sample (sustaining "open, not yet closing" requires staying above
        EXIT_COUNTS=25 for most of the window, which alone contributes most
        of the way to FLUX_FLOOR=900 over 40ms) -- consistent with the
        spec's own Sec 3.2 framing of flux as a backstop, duration as the
        cleaner discriminator. This test isolates the flux gate directly
        against a hand-built candidate dict rather than fighting that
        mechanical coupling, to prove the gate itself is wired correctly."""
        s = new_session(self.qmap)
        cand = {
            "session": "TEST", "open_t_ms": 1000, "close_t_ms": 1045,
            "polarity_sign": 1, "peak_abs": 42, "peak_t_ms": 1010,
            "pwm_actual_at_open": 90, "pwm_commanded_at_open": 90, "direction_at_open": "CW",
            "pwm_actual_at_peak": 90, "pwm_commanded_at_peak": 90,
            "pwm_actual_at_close": 90, "pwm_commanded_at_close": 90,
            "abs_flux": 500.0, "signed_flux": 500.0,
            "gap_overlap": False, "force_closed_excessive": False,
        }
        s._maybe_declare_start(1000)
        s._finalize_candidate(cand)
        s.finalize()
        self.assertEqual(len(s.rows_out), 1)
        row = s.rows_out[0]
        self.assertEqual(row["disposition"], "ARTIFACT")
        self.assertEqual(row["disposition_rule_id"], "ARTIFACT_FLUX_FLOOR")


class MapValidationTests(unittest.TestCase):
    def setUp(self):
        self.qmap = QuorumMap.from_data(DNA, SPACING)

    def test_expected_passage_advances_once_and_no_prev_exempts_timing(self):
        s = new_session(self.qmap, start_mm=0, start_t_ms=0)
        emit_passage(s, 1000, sign=(1 if DNA[1] else -1))
        s.finalize()
        self.assertEqual(len(s.rows_out), 1)
        row = s.rows_out[0]
        self.assertEqual(row["disposition"], "EXPECTED_ADVANCE")
        self.assertEqual(row["disposition_rule_id"], "MAP_MATCH")
        self.assertTrue(row["position_ring_inserted"])
        self.assertFalse(row["contradiction_ring_inserted"])
        self.assertEqual(s.primary_position, 1)
        self.assertEqual(s.report["disposition_counts"]["EXPECTED_ADVANCE"], 1)

    def test_credible_contradiction_does_not_move_position_or_touch_position_ring(self):
        s = new_session(self.qmap, start_mm=0, start_t_ms=0)
        wrong_sign = 1 if not DNA[1] else -1   # deliberately opposite of dna[1]
        emit_passage(s, 1000, sign=wrong_sign)
        s.finalize()
        self.assertEqual(len(s.rows_out), 1)
        row = s.rows_out[0]
        self.assertEqual(row["disposition"], "CREDIBLE_CONTRADICTION")
        self.assertEqual(row["disposition_rule_id"], "MAP_MISMATCH")
        self.assertEqual(s.primary_position, 0)     # unchanged
        self.assertFalse(row["position_ring_inserted"])
        self.assertTrue(row["contradiction_ring_inserted"])
        self.assertEqual(len(s.position_ring), 0)
        self.assertEqual(len(s.contradiction_ring), 1)

    def test_arrival_gate_rejects_too_soon_candidate(self):
        s = new_session(self.qmap, start_mm=0, start_t_ms=0)
        emit_passage(s, 1000, sign=(1 if DNA[1] else -1))
        self.assertEqual(s.primary_position, 1)
        # spacing=300mm, velocity_mm_s(90)=3.90*90-99.2=251.8 -> contextual
        # bound ~1191ms. Arrive far too soon (50ms later) with matching
        # polarity for marker 2 -- must still be rejected on timing, not
        # silently accepted for having "the right polarity".
        second_open = emit_passage(s, 1000 + 1000 + 50, sign=(1 if DNA[2] else -1))
        s.finalize()
        self.assertEqual(len(s.rows_out), 2)
        row2 = s.rows_out[1]
        self.assertEqual(row2["disposition"], "ARTIFACT")
        self.assertIn(row2["disposition_rule_id"],
                      ("ARTIFACT_HARD_IMPOSSIBLE", "ARTIFACT_CONTEXTUAL_TOO_SOON"))
        self.assertEqual(s.primary_position, 1)   # unchanged by the artifact


class MergeTests(unittest.TestCase):
    def setUp(self):
        self.qmap = QuorumMap.from_data(DNA, SPACING)

    def test_companion_lobe_is_not_a_second_marker(self):
        s = new_session(self.qmap, start_mm=0, start_t_ms=0)
        primary_close = emit_passage(s, 1000, sign=(1 if DNA[1] else -1), peak=200)
        # companion: opposite polarity, well under AMPLITUDE_RATIO_MAX*200=100,
        # opening well inside MERGE_WINDOW_MS=350 after the primary's close.
        emit_passage(s, primary_close + 60, sign=(-1 if DNA[1] else 1), peak=60,
                    rise_ms=2, plateau_ms=3, fall_ms=2, tail_ms=25)
        s.finalize()
        self.assertEqual(s.primary_position, 1)
        dispositions = [r["disposition"] for r in s.rows_out]
        self.assertIn("MERGED_COMPANION", dispositions)
        self.assertEqual(dispositions.count("EXPECTED_ADVANCE"), 1)
        primary_row = [r for r in s.rows_out if r["disposition"] == "EXPECTED_ADVANCE"][0]
        self.assertEqual(primary_row["evidence_lobe_count"], 2)
        self.assertTrue(primary_row["evidence_companions_json"])
        self.assertEqual(s.report["disposition_counts"]["MERGED_COMPANION"], 1)

    def test_companion_never_enters_either_ring(self):
        s = new_session(self.qmap, start_mm=0, start_t_ms=0)
        primary_close = emit_passage(s, 1000, sign=(1 if DNA[1] else -1), peak=200)
        emit_passage(s, primary_close + 60, sign=(-1 if DNA[1] else 1), peak=60,
                    rise_ms=2, plateau_ms=3, fall_ms=2, tail_ms=25)
        s.finalize()
        companion_row = [r for r in s.rows_out if r["disposition"] == "MERGED_COMPANION"][0]
        self.assertFalse(companion_row["position_ring_inserted"])
        self.assertFalse(companion_row["contradiction_ring_inserted"])


class RecoveryTests(unittest.TestCase):
    def setUp(self):
        self.qmap = QuorumMap.from_data(DNA, SPACING)
        # Precondition (verified, not assumed): for base_mm=0, direction=+1,
        # offset 1's predicted polarity must differ from every other offset
        # 2..MAX_OMITTED's predicted polarity. Without this, repeating a
        # single observation could tie offset 1 with a coincidentally-
        # agreeing offset forever (score(o)-score(o') both grow by 1 per
        # repeat, margin never changes) -- see the DNA fixture's own comment
        # for how this is engineered rather than left to chance.
        preds = {o: target_pol01(self.qmap, 0, 1, o) for o in range(1, T.MAX_OMITTED + 1)}
        others = set(o for o, p in preds.items() if o != 1 and p == preds[1])
        self.assertEqual(others, set(),
                         "offset 1 ties with offsets %s at mm=0/dir=+1 -- adjust the DNA "
                         "fixture (repetition can never break such a tie)" % others)

    def test_resync_adopts_unique_offset_and_clears_ring(self):
        s = new_session(self.qmap, start_mm=0, start_t_ms=0)
        direction = 1
        target_pol = target_pol01(self.qmap, 0, direction, 1)   # true offset = 1
        t = 1000
        rows_before = 0
        # Exactly MIN_SUPPORT observations: this fixture's offset 1 is a
        # unique, undiluted leader (see the DNA comment above), so WIN fires
        # as soon as MARGIN/MIN_SUPPORT are both met -- which, by
        # construction, is exactly at MIN_SUPPORT observations, not later.
        for i in range(T.MIN_SUPPORT):
            t = emit_passage(s, t + 1500, sign=(1 if target_pol else -1), peak=200)
            rows_before += 1
        s.finalize()
        resync_rows = [r for r in s.rows_out if r["disposition"] == "RESYNC_ADOPTED"]
        self.assertEqual(len(resync_rows), 1, "expected exactly one RESYNC_ADOPTED row: %s" %
                         [(r["disposition"], r["disposition_rule_id"]) for r in s.rows_out])
        self.assertEqual(resync_rows[0]["resync_offset"], 1)
        # primary was 0, next_mm(routeMod(0+1*1),1) = next_mm(1,1) = 2
        self.assertEqual(s.primary_position, 2)
        self.assertEqual(len(s.contradiction_ring), 0)
        self.assertEqual(len(s.report["recovery_events"]), 1)
        self.assertGreaterEqual(s.report["recovery_events"][0]["latency_ms"], 0)
        contradiction_rows = [r for r in s.rows_out if r["disposition"] == "CREDIBLE_CONTRADICTION"]
        self.assertEqual(len(contradiction_rows), rows_before - 1)

    def test_unresolvable_contradiction_stream_reaches_position_unresolved_and_stops(self):
        qmap_noisy = QuorumMap.from_data(DNA_NOISY, SPACING_NOISY)
        s = new_session(qmap_noisy, start_mm=0, start_t_ms=0)
        t = 1000
        # Constant observed=N: always contradicts dna_noisy[1]=S, but ties
        # offsets {1..6} together forever (see DNA_NOISY's comment) -- a
        # self-consistent-looking but genuinely unresolvable stream.
        for i in range(T.CONTRADICTION_RING_SIZE + 1):
            t = emit_passage(s, t + 1500, sign=1, peak=200)
        s.finalize()
        unresolved = [r for r in s.rows_out if r["disposition"] == "POSITION_UNRESOLVED"]
        self.assertGreaterEqual(len(unresolved), 1)
        self.assertTrue(s.is_stopped)
        self.assertEqual(s.stop_reason, "POSITION_UNRESOLVED")
        self.assertEqual(s.primary_position, 0)   # never moved
        self.assertGreaterEqual(len(s.report["terminal_stops"]), 1)

    def test_stopped_state_freezes_further_candidates_until_reversal(self):
        qmap_noisy = QuorumMap.from_data(DNA_NOISY, SPACING_NOISY)
        s = new_session(qmap_noisy, start_mm=0, start_t_ms=0)
        t = 1000
        for i in range(T.CONTRADICTION_RING_SIZE + 1):
            t = emit_passage(s, t + 1500, sign=1, peak=200)
        self.assertTrue(s.is_stopped)
        pos_before = s.primary_position
        # A further credible candidate while stopped must not mutate position.
        t = emit_passage(s, t + 1500, sign=1, peak=200)
        self.assertEqual(s.primary_position, pos_before)
        # A direction reversal (observed live via ctl_dir) is the documented
        # resume condition (Sec 6.4(a); see module docstring deviation note).
        t = emit_passage(s, t + 1500, sign=1, peak=200, dir_str="CCW")
        self.assertFalse(s.is_stopped)


class SessionAndDeclarationTests(unittest.TestCase):
    def setUp(self):
        self.qmap = QuorumMap.from_data(DNA, SPACING)

    def test_pre_declaration_window_is_diagnostic_only(self):
        s = new_session(self.qmap, start_mm=5, start_t_ms=5000)
        emit_passage(s, 1000, sign=(1 if DNA[6] else -1))   # well before start_t_ms
        s.finalize()
        self.assertEqual(len(s.rows_out), 1)
        self.assertEqual(s.rows_out[0]["disposition"], "PRE_DECLARATION")
        self.assertIsNone(s.primary_position)

    def test_session_row_resets_state(self):
        s = new_session(self.qmap, start_mm=0, start_t_ms=0)
        emit_passage(s, 1000, sign=(1 if DNA[1] else -1))
        self.assertEqual(s.primary_position, 1)
        row = {"session": "NEWSESS"}
        s.process_row("SESSION", lambda name: row.get(name))
        self.assertIsNone(s.primary_position)
        self.assertEqual(len(s.position_ring), 0)
        # No fresh declaration exists for this second "session" in the same
        # object (single-manifest limitation, documented) -- so a further
        # candidate is PRE_DECLARATION, not silently re-declared.
        before = len(s.rows_out)
        emit_passage(s, 50000, sign=(1 if DNA[1] else -1))
        s.finalize()
        self.assertEqual(s.rows_out[-1]["disposition"], "PRE_DECLARATION")


class AuditTests(unittest.TestCase):
    def setUp(self):
        self.qmap = QuorumMap.from_data(DNA, SPACING)

    def test_false_inclusion_audit_clean_on_normal_run(self):
        s = new_session(self.qmap, start_mm=0, start_t_ms=0)
        emit_passage(s, 1000, sign=(1 if DNA[1] else -1))
        s.finalize()
        audit = T.audit_false_inclusion(s.rows_out)
        self.assertEqual(audit["count"], 0)

    def test_ring_contamination_audit_clean_on_normal_run(self):
        s = new_session(self.qmap, start_mm=0, start_t_ms=0)
        emit_passage(s, 1000, sign=(1 if DNA[1] else -1))
        emit_passage(s, 1000 + 1500, sign=(1 if not DNA[2] else -1))  # contradiction
        s.finalize()
        audit = T.audit_ring_contamination(s.rows_out)
        self.assertEqual(audit["count"], 0, audit["violations"])


if __name__ == "__main__":
    unittest.main()
