#!/usr/bin/env python3
"""templates_replay_spec.py -- offline replay implementing
docs/TEMPLATES_REPLAY_DESIGN_SPEC.md end to end.

INVESTIGATORY / DESIGN EVIDENCE ONLY. Not firmware. Read-only against a
decoded QUORUM TRACE CSV (tools/qt_decode.py output) and a JSON ground-truth
manifest; writes a per-candidate event stream and a JSON summary report.

NOTE ON THE FILENAME: the design spec and task both name this tool
"tools/templates_replay.py". That exact path already exists in this repo as
a COMMITTED, tracked file (git commit d05a8b4, "TEMPLATES: add provisional
offline admission replay") implementing a materially different, earlier
design -- it reads QUORUM's own DECISION records (EVENT_CLOSED peak/duration/
polarity, AGREE/DISAGREE) as classifier INPUT, which the operator's brief for
*this* build explicitly forbids ("det_*/dec_* columns ... never as input to
the new classifier's logic, or the exercise is circular"), and it implements
a two-way accept/reject disposition, not the three-way taxonomy this file
implements. Under this task's hard constraint "do NOT edit any existing
tracked file", that path could not be reused, so this implementation lives
here instead. See the task return summary for the full explanation; a human
should reconcile the two files.

WHAT THIS FILE READS: from SAMPLE rows, only phys_raw, phys_baseline,
ctl_pwm_actual, ctl_pwm_commanded, ctl_dir, t_ms, session (spec Sec 1.1).
From ANCHOR rows, only op_* fields (ground-truth checkpoints, never fed into
the classifier). GAP and SESSION rows are used structurally (spec Sec 1.1,
2.6). DECISION and STATUS rows -- QUORUM's OWN interpretation and
decisions -- are never read as classifier input anywhere in this file; they
do not even reach this tool, since this tool never opens them for anything
but the fact that they exist as other rows to skip.

Usage:
    python3 tools/templates_replay.py <decoded.csv> --manifest <manifest.json> \\
        --out-events <events.csv> --out-report <report.json> [--session <id>]

(invoked as templates_replay.py above because that is the spec's own CLI
shape; the actual file is tools/templates_replay_spec.py -- see the note
above.)
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from quorum_map import QuorumMap  # noqa: E402


# ===========================================================================
# Sec 10 -- consolidated constants table. Every value here is named and
# justified in the design spec section cited in the comment; none is
# re-derived independently in this file. See docs/TEMPLATES_REPLAY_DESIGN_SPEC.md.
# ===========================================================================
ENTRY_COUNTS = 38                  # Sec 2.2 -- validated by direct replay
EXIT_COUNTS = 25                   # Sec 2.2 -- validated by direct replay
EXIT_HOLD_MS = 20                  # Sec 2.2 -- reused from QUORUM's EVENT_EXIT_HOLD_MS
SUSPEND_PWM_FLOOR = 20             # Sec 2.4 -- reused from MOTOR_DEAD_ZONE_PWM
MAX_OPEN_MS = 3000                 # Sec 2.5
RAMP_DELTA = 10                    # Sec 2.3 -- diagnostic only here
DURATION_FLOOR_MS = 40             # Sec 3.1
FLUX_FLOOR = 900                   # Sec 3.2 -- tightest margin in the spec
MERGE_WINDOW_MS = 350              # Sec 4.1
AMPLITUDE_RATIO_MAX = 0.5          # Sec 4.1
MAX_LOBES = 5                      # Sec 4.1 -- defensive cap
PWM_MODEL_VALID_FLOOR = 40         # Sec 5.3 -- reused from GATE_LOW_PWM_FLOOR
VEL_MODEL_SLOPE = 3.90             # Sec 5.2 -- reused from QUORUM's own calibration
VEL_MODEL_INTERCEPT = -99.2        # Sec 5.2
MAX_OMITTED = 12                   # Sec 7a / 8.2 -- inherited from QUORUM_MAX, flagged for revalidation
POSITION_RING_SIZE = 12            # Sec 6.4
CONTRADICTION_RING_SIZE = 12       # Sec 6.4
MARGIN = 2                         # Sec 7a -- reused from QUORUM_MARGIN
MIN_SUPPORT = 2                    # Sec 7a -- new
N_LIVENESS = 3                     # Sec 7b -- new
BOUNDARY_AMBIGUOUS_LO_MS = 35      # Sec 3.1
BOUNDARY_AMBIGUOUS_HI_MS = 50      # Sec 3.1
PWM_VALIDATED_CEILING = 130        # Sec 11 item 10 -- real coverage extends to ~120

# Sec 3.5 -- ADC saturation rail. The spec is explicit this is NOT
# independently confirmed against the firmware's ADC configuration and is
# "not expected to fire under the operating conditions seen so far". Left as
# a structural no-op (None disables the check) rather than guessing a rail
# value -- guessing here would be exactly the "invented precision" the
# doctrine's Consequence #10 forbids.
ADC_MIN_RAIL = None
ADC_MAX_RAIL = None

# Positive deviation (phys_raw - phys_baseline > 0) is treated as North,
# matching QUORUM's own detectorSample() convention (evOpenPole = raw >=
# northEnter ? 1 : 0) -- see CLAUDE.md: HALL_POLARITY_INVERTED is dead
# config no firmware reads, so this convention is uniform across both locos.
POSITIVE_DEVIATION_IS_N = True

# The spec's Sec 9 disposition enum is exactly these six values. This
# implementation adds exactly one more, PRE_DECLARATION, documented in the
# module-level DEVIATIONS note below and in the task return summary.
DISPOSITIONS = (
    "ARTIFACT", "MERGED_COMPANION", "EXPECTED_ADVANCE",
    "CREDIBLE_CONTRADICTION", "RESYNC_ADOPTED", "POSITION_UNRESOLVED",
    "PRE_DECLARATION",
)

EVENT_COLUMNS = [
    "event_id", "capture", "session",
    "candidate_open_t_ms", "candidate_close_t_ms",
    "evidence_duration_ms", "evidence_peak", "evidence_polarity",
    "evidence_abs_flux", "evidence_signed_flux",
    "evidence_pwm_actual_at_open", "evidence_pwm_commanded_at_open",
    "evidence_pwm_actual_at_peak", "evidence_pwm_commanded_at_peak",
    "evidence_pwm_actual_at_close", "evidence_pwm_commanded_at_close",
    "evidence_pwm_for_gate", "evidence_direction",
    "evidence_lobe_count", "evidence_companions_json",
    "evidence_spans_pwm_transition", "evidence_boundary_ambiguous",
    "evidence_pwm_regime_unvalidated",
    "map_position_before", "map_direction_before", "map_expected_polarity",
    "map_position_after",
    "disposition", "disposition_rule_id", "disposition_reasoning",
    "contradiction_streak_index",
    "resync_offset", "resync_score_vector",
    "position_ring_inserted", "contradiction_ring_inserted",
]


def polchar(pol01):
    return "N" if pol01 else "S"


def sgn_to_pol01(sign):
    """+1/-1 candidate polarity sign -> QuorumMap's 1(N)/0(S) convention."""
    is_n = (sign > 0) if POSITIVE_DEVIATION_IS_N else (sign < 0)
    return 1 if is_n else 0


def pol01_char(sign):
    return polchar(sgn_to_pol01(sign))


# ===========================================================================
# Manifest
# ===========================================================================
class Manifest:
    """Ground truth this tool's --manifest reads: session to filter to
    (optional -- --session on the CLI overrides), a declared start position/
    direction/time, and operator anchors for drift reporting only. Anchors
    are NEVER used to mutate primary_position mid-replay (see the task
    return summary for why: the report's own required "drift at each
    anchor" metric only means something if anchors are a measurement
    checkpoint, not a reset)."""

    def __init__(self, path):
        with open(path) as fh:
            data = json.load(fh)
        self.path = path
        self.raw = data
        self.capture_label = data.get("capture", "")
        self.session = data.get("session")
        start = data.get("start", {})
        self.start_mm = start.get("mm")
        self.start_t_ms = start.get("t_ms")
        self.start_direction = _dir_to_sign(start.get("direction"))
        self.start_confirmed = bool(start.get("confirmed", False))
        self.start_note = start.get("note", "")
        end = data.get("end", {})
        self.end_mm = end.get("mm")
        self.end_t_ms = end.get("t_ms")
        self.anchors = data.get("anchors", [])
        self.known_context = data.get("known_context", [])
        self.uncertainty_notes = data.get("uncertainty_notes", [])
        if self.start_mm is None or self.start_t_ms is None:
            raise ValueError(
                "manifest %s must declare start.mm and start.t_ms -- this "
                "tool never guesses a starting position (Sec 5.5/2.6 require "
                "an explicit declaration before any position-dependent "
                "disposition can be produced)" % path)


def _dir_to_sign(label):
    if label == "CW":
        return 1
    if label == "CCW":
        return -1
    return None


# ===========================================================================
# Replay engine
# ===========================================================================
class ReplaySession:
    """One session's worth of state. Sec 2.1: at most one open RawCandidate
    per session; all other state (rings, primary position, merge anchor,
    liveness/interval-reference tracking) is likewise per-session and is
    fully reset on a SESSION boundary row (Sec 2.6)."""

    def __init__(self, qmap, manifest, capture_label, session_label):
        self.qmap = qmap
        self.manifest = manifest
        self.capture_label = capture_label
        self.session_label = session_label

        self.open_candidate = None
        self.pending_anchor = None

        self.primary_position = None
        self.primary_direction = None
        self._declared_once = False

        self.position_ring = []       # list of {mm,t_ms,close_t_ms,event_id}
        self.contradiction_ring = []  # list of ContradictionObservation dicts
        self.prev_marker = None       # most recent PositionRing entry
        self.contradiction_streak_next = 1

        self.since_prev_max_pwm = None
        self.since_prev_push_close_t_ms = None
        self.moving_ms_since_prev_push = 0.0
        self._last_sample_t_ms = None
        self.interval_ref_invalidated_pending = False
        self.suspend_prev_state = None
        self.reversal_pending_singleshot = False

        self.is_stopped = False
        self.stop_reason = None
        self.stop_t_ms = None
        self.liveness_stage1_fired = False
        self.liveness_stage2_fired = False

        self._next_event_id = 1
        self._next_observation_id = 1

        self.rows_out = []
        self.anchor_events = []

        # report accumulators
        self.report = {
            "disposition_counts": {d: 0 for d in DISPOSITIONS},
            "rule_id_counts": {},
            "recovery_events": [],
            "terminal_stops": [],
            "liveness_warnings": [],
        }

    # -- ids -----------------------------------------------------------
    def new_event_id(self):
        eid = "%s:%05d" % (self.session_label, self._next_event_id)
        self._next_event_id += 1
        return eid

    def new_observation_id(self):
        oid = self._next_observation_id
        self._next_observation_id += 1
        return oid

    # -- top-level row dispatch -----------------------------------------
    def process_row(self, row_type, get):
        """get(col) returns a field by qt_decode.py column name."""
        if row_type == "SESSION":
            self._flush_pending_anchor()
            self._reset_session_state()
            new_sid = get("session")
            if new_sid:
                # Keep future candidates' own "session" field (set from
                # self.session_label at open time) correct across a boundary
                # -- relevant only to an unfiltered multi-session run; every
                # manifest this task built filters to one session, so this
                # path is not exercised by the self-test, but it is cheap
                # and clearly correct to fix rather than leave stale.
                self.session_label = new_sid
            return
        if row_type == "GAP":
            info = get("info") or ""
            # Sec 1.1's GAP gotcha: only a SAMPLE-stream gap matters here.
            if "SAMPLE" in info and self.open_candidate is not None:
                self.open_candidate["gap_overlap"] = True
            return
        if row_type == "ANCHOR":
            self._on_anchor(get)
            return
        if row_type != "SAMPLE":
            # DECISION / STATUS / BAD rows -- QUORUM's own interpretation.
            # Never read as classifier input (operator's hard input contract).
            return
        self._on_sample(get)

    def finalize(self):
        self._flush_pending_anchor()

    # -- SESSION boundary (Sec 2.6) --------------------------------------
    def _reset_session_state(self):
        self.open_candidate = None          # discarded, no disposition record
        self.pending_anchor = None
        self.primary_position = None
        self.primary_direction = None
        self.position_ring = []
        self.contradiction_ring = []
        self.prev_marker = None
        self.contradiction_streak_next = 1
        self.since_prev_max_pwm = None
        self.since_prev_push_close_t_ms = None
        self.moving_ms_since_prev_push = 0.0
        self._last_sample_t_ms = None
        self.interval_ref_invalidated_pending = False
        self.suspend_prev_state = None
        self.reversal_pending_singleshot = False
        self.is_stopped = False
        self.stop_reason = None
        self.stop_t_ms = None
        self.liveness_stage1_fired = False
        self.liveness_stage2_fired = False
        # NOTE: self._declared_once is intentionally NOT reset -- a manifest
        # declares exactly one start, for the first session it applies to.
        # A second session in an unfiltered multi-session run gets no
        # declaration and everything in it reports PRE_DECLARATION. All five
        # manifests this task built are session-filtered, so this path is
        # not exercised by the self-test; it is documented, not silently
        # dropped. See task return summary.

    # -- ANCHOR rows (ground truth checkpoints only, never fed to the state
    # machine) -----------------------------------------------------------
    def _on_anchor(self, get):
        self.anchor_events.append({
            "t_ms": _num(get("t_ms")),
            "anchor_id": get("op_anchor_id"),
            "text": get("op_text"),
            "ctl_dir": get("ctl_dir"),
            "ctl_pwm_actual": _num(get("ctl_pwm_actual")),
            "primary_position_at_anchor": self.primary_position,
            "primary_direction_at_anchor": _dir_label(self.primary_direction),
        })

    # -- SAMPLE rows -------------------------------------------------------
    def _on_sample(self, get):
        t_ms = _num(get("t_ms"))
        raw = _num(get("phys_raw"))
        baseline = _num(get("phys_baseline"))
        if t_ms is None or raw is None or baseline is None:
            return
        pwm_a = _num(get("ctl_pwm_actual")) or 0
        pwm_c = _num(get("ctl_pwm_commanded")) or 0
        dir_str = get("ctl_dir")
        d = raw - baseline

        self._maybe_declare_start(t_ms)
        self._update_direction(dir_str)

        suspended_now = pwm_a <= SUSPEND_PWM_FLOOR
        if self.suspend_prev_state is True and suspended_now is False:
            # Sec 5.7: a completed enter+exit stall/dwell cycle invalidates
            # the interval reference for the NEXT candidate only.
            self.interval_ref_invalidated_pending = True
        self.suspend_prev_state = suspended_now

        if self.prev_marker is not None:
            self.since_prev_max_pwm = max(self.since_prev_max_pwm or 0, pwm_a)
            # Sec 7b: the liveness budget is TRAVEL time, not wall-clock time
            # since the last push -- a dwell (pwm<=SUSPEND_PWM_FLOOR) must not
            # silently consume it, including the single sample that ends a
            # long dwell (which would otherwise see the WHOLE dwell duration
            # as "elapsed since push" on its very first moving sample).
            if self._last_sample_t_ms is not None and not suspended_now:
                dt = t_ms - self._last_sample_t_ms
                if dt > 0:
                    self.moving_ms_since_prev_push += dt
        self._last_sample_t_ms = t_ms

        self._check_liveness(t_ms, pwm_a)

        if self.open_candidate is not None:
            self._update_open_candidate(t_ms, d, pwm_a, pwm_c)
            return

        if suspended_now:
            # Sec 2.4: not even recorded as a candidate, not even ARTIFACT.
            return

        if abs(d) >= ENTRY_COUNTS:
            self._open_candidate(t_ms, d, pwm_a, pwm_c, dir_str)

    def _maybe_declare_start(self, t_ms):
        if self._declared_once:
            return
        if t_ms < self.manifest.start_t_ms:
            return
        self.primary_position = self.qmap.route_mod(self.manifest.start_mm)
        if self.manifest.start_direction is not None:
            self.primary_direction = self.manifest.start_direction
        self._declared_once = True
        # A freshly-declared position has no interval history (Sec 5.5): the
        # first candidate evaluated against it is NO_PREV by construction,
        # which falls out naturally here since self.prev_marker is None.

    def _update_direction(self, dir_str):
        newdir = _DIR_MAP.get(dir_str)
        if newdir is None:
            return
        if self.primary_direction is not None and newdir != self.primary_direction:
            # True reversal -- mirrors QUORUM's applyDirection() gating
            # (prevDir!=UNSET && derived!=UNSET). Sec 5.6 + Sec 6.4(a).
            self.reversal_pending_singleshot = True
            self.contradiction_ring = []
            if self.is_stopped:
                # A direction reversal is the one explicit resume condition
                # this design grants (Sec 6.4(a) clears ContradictionRing on
                # reversal); mirrors QUORUM's own applyDirection() resetting
                # diagnostics on reversal, generalised here to also lift a
                # replay-local safe-stop so the tool can keep tracking
                # through a real, anchored mid-run reversal (see
                # otto_change1_20260825_run1.json). Documented deviation --
                # see task return summary.
                self.is_stopped = False
                self.stop_reason = None
        self.primary_direction = newdir

    # -- acquisition (Sec 2) ----------------------------------------------
    def _open_candidate(self, t_ms, d, pwm_a, pwm_c, dir_str):
        self.open_candidate = {
            "session": self.session_label, "open_t_ms": t_ms,
            "polarity_sign": 1 if d > 0 else -1,
            "peak_abs": abs(d), "peak_t_ms": t_ms,
            "pwm_actual_at_open": pwm_a, "pwm_commanded_at_open": pwm_c,
            "direction_at_open": dir_str,
            "pwm_actual_at_peak": pwm_a, "pwm_commanded_at_peak": pwm_c,
            "abs_flux": 0.0, "signed_flux": 0.0,
            "prev_t_ms": t_ms, "prev_d": d,
            "exit_hold_start_ms": None,
            "gap_overlap": False, "force_closed_excessive": False,
        }

    def _update_open_candidate(self, t_ms, d, pwm_a, pwm_c):
        c = self.open_candidate
        dt = t_ms - c["prev_t_ms"]
        if dt > 0:
            c["abs_flux"] += 0.5 * (abs(c["prev_d"]) + abs(d)) * dt
            c["signed_flux"] += 0.5 * (c["prev_d"] + d) * dt
        c["prev_t_ms"], c["prev_d"] = t_ms, d
        if abs(d) > c["peak_abs"]:
            c["peak_abs"] = abs(d); c["peak_t_ms"] = t_ms
            c["pwm_actual_at_peak"] = pwm_a; c["pwm_commanded_at_peak"] = pwm_c

        if t_ms - c["open_t_ms"] >= MAX_OPEN_MS:
            c["close_t_ms"] = t_ms
            c["pwm_actual_at_close"] = pwm_a; c["pwm_commanded_at_close"] = pwm_c
            c["force_closed_excessive"] = True
            self.open_candidate = None
            self._finalize_candidate(c)
            return

        if abs(d) <= EXIT_COUNTS:
            if c["exit_hold_start_ms"] is None:
                c["exit_hold_start_ms"] = t_ms
            elif t_ms - c["exit_hold_start_ms"] >= EXIT_HOLD_MS:
                c["close_t_ms"] = t_ms
                c["pwm_actual_at_close"] = pwm_a; c["pwm_commanded_at_close"] = pwm_c
                self.open_candidate = None
                self._finalize_candidate(c)
                return
        else:
            c["exit_hold_start_ms"] = None

    # -- Sec 4 merge contract + Sec 3 artifact rejection -------------------
    def _finalize_candidate(self, cand):
        if self.pending_anchor is not None and self._merge_eligible(cand, self.pending_anchor):
            self._merge_into_anchor(cand, self.pending_anchor)
            self._emit_merged_companion_row(cand, self.pending_anchor)
            return

        self._flush_pending_anchor()

        if cand["force_closed_excessive"]:
            self._emit_terminal_artifact(cand, "ARTIFACT_EXCESSIVE_OPEN",
                "candidate reached MAX_OPEN_MS=%d while still open (Sec 2.5/3.3); "
                "duration=%dms >= ceiling -- force-closed and trashed regardless of "
                "peak/flux" % (MAX_OPEN_MS, cand["close_t_ms"] - cand["open_t_ms"]))
            return
        if cand["gap_overlap"]:
            self._emit_terminal_artifact(cand, "ARTIFACT_INCOMPLETE",
                "candidate window [%d,%d] overlaps a SAMPLE-stream GAP row (Sec 3.4) "
                "-- not authoritative" % (cand["open_t_ms"], cand["close_t_ms"]))
            return
        if ADC_MIN_RAIL is not None or ADC_MAX_RAIL is not None:
            # Sec 3.5 -- structural no-op by default; see module constants.
            pass

        duration_ms = cand["close_t_ms"] - cand["open_t_ms"]
        boundary_ambiguous = BOUNDARY_AMBIGUOUS_LO_MS <= duration_ms <= BOUNDARY_AMBIGUOUS_HI_MS

        if duration_ms < DURATION_FLOOR_MS:
            self._emit_terminal_artifact(cand, "ARTIFACT_DURATION_FLOOR",
                "duration=%dms < %dms floor (Sec 3.1)" % (duration_ms, DURATION_FLOOR_MS),
                boundary_ambiguous=boundary_ambiguous)
            return
        if cand["abs_flux"] < FLUX_FLOOR:
            self._emit_terminal_artifact(cand, "ARTIFACT_FLUX_FLOOR",
                "abs_flux=%.0f < %d count*ms floor (Sec 3.2); duration=%dms cleared the "
                "floor on its own" % (cand["abs_flux"], FLUX_FLOOR, duration_ms),
                boundary_ambiguous=boundary_ambiguous)
            return

        # Cleared Sec 3 in full -- becomes the new pending merge anchor
        # (Sec 4.1: "a primary candidate P that has already independently
        # cleared Sec 3's duration and flux floors"). Sec 5/6/7 disposition
        # is decided now (state mutation is immediate); only the OUTPUT
        # ROW's lobe/companion metadata is deferred to flush time, since
        # Sec 4.2's invariant is explicit that companions never affect map
        # validation, the arrival-gate reference, or any hypothesis score.
        anchor = self._new_anchor_from(cand, boundary_ambiguous)
        self.pending_anchor = anchor
        self._evaluate_credible_passage(anchor)

    def _merge_eligible(self, cand, anchor):
        if len(anchor["companions"]) + 1 >= MAX_LOBES:
            return False
        dt = cand["open_t_ms"] - anchor["most_recent_lobe_close_t_ms"]
        if dt > MERGE_WINDOW_MS:
            return False
        ratio = (cand["peak_abs"] / anchor["merged_peak"]) if anchor["merged_peak"] else float("inf")
        if ratio > AMPLITUDE_RATIO_MAX:
            return False
        return True

    def _merge_into_anchor(self, cand, anchor):
        ratio = (cand["peak_abs"] / anchor["merged_peak"]) if anchor["merged_peak"] else None
        companion = {
            "offset_ms_from_previous_lobe_close":
                cand["open_t_ms"] - anchor["most_recent_lobe_close_t_ms"],
            "polarity": pol01_char(cand["polarity_sign"]),
            "peak_abs": cand["peak_abs"], "abs_flux": round(cand["abs_flux"], 1),
            "duration_ms": cand["close_t_ms"] - cand["open_t_ms"],
            "amplitude_ratio": round(ratio, 4) if ratio is not None else None,
        }
        anchor["companions"].append(companion)
        anchor["most_recent_lobe_close_t_ms"] = cand["close_t_ms"]
        anchor["merged_peak"] = max(anchor["merged_peak"], cand["peak_abs"])
        anchor["merged_total_abs_flux"] += cand["abs_flux"]
        lobe_abs_signed = abs(cand["signed_flux"])
        if lobe_abs_signed > anchor["_best_lobe_abs_signed_flux"]:
            anchor["_best_lobe_abs_signed_flux"] = lobe_abs_signed
            anchor["primary_polarity"] = pol01_char(cand["polarity_sign"])
        anchor["lobe_count"] = 1 + len(anchor["companions"])

    def _new_anchor_from(self, cand, boundary_ambiguous):
        duration_ms = cand["close_t_ms"] - cand["open_t_ms"]
        spans = abs(cand["pwm_actual_at_close"] - cand["pwm_actual_at_open"]) > RAMP_DELTA
        return {
            "event_id": self.new_event_id(), "session": cand["session"],
            "open_t_ms": cand["open_t_ms"], "close_t_ms": cand["close_t_ms"],
            "most_recent_lobe_close_t_ms": cand["close_t_ms"],
            "duration_ms": duration_ms,
            "polarity_sign": cand["polarity_sign"],
            "primary_polarity": pol01_char(cand["polarity_sign"]),
            "_best_lobe_abs_signed_flux": abs(cand["signed_flux"]),
            "peak_abs": cand["peak_abs"], "merged_peak": cand["peak_abs"],
            "primary_abs_flux": cand["abs_flux"],          # fixed, never laundered by a merge
            "merged_total_abs_flux": cand["abs_flux"],
            "signed_flux": cand["signed_flux"],
            "pwm_actual_at_open": cand["pwm_actual_at_open"],
            "pwm_commanded_at_open": cand["pwm_commanded_at_open"],
            "pwm_actual_at_peak": cand["pwm_actual_at_peak"],
            "pwm_commanded_at_peak": cand["pwm_commanded_at_peak"],
            "pwm_actual_at_close": cand["pwm_actual_at_close"],
            "pwm_commanded_at_close": cand["pwm_commanded_at_close"],
            "direction_at_open": cand["direction_at_open"],
            "lobe_count": 1, "companions": [],
            "boundary_ambiguous": boundary_ambiguous,
            "spans_pwm_transition": spans,
            "pwm_regime_unvalidated": False,
            "disposition": None, "rule_id": None, "reasoning": "",
            "map_position_before": "", "map_direction_before": "",
            "map_expected_polarity": "", "map_position_after": "",
            "contradiction_streak_index": "", "resync_offset": "",
            "resync_score_vector": "",
            "position_ring_inserted": False, "contradiction_ring_inserted": False,
            "_pwm_for_gate": None,
        }

    def _flush_pending_anchor(self):
        if self.pending_anchor is None:
            return
        a = self.pending_anchor
        self._count_disposition(a["disposition"], a["rule_id"])
        self._emit_row(a)
        self.pending_anchor = None

    def _emit_terminal_artifact(self, cand, rule_id, reasoning, boundary_ambiguous=False):
        duration_ms = cand["close_t_ms"] - cand["open_t_ms"]
        spans = abs(cand["pwm_actual_at_close"] - cand["pwm_actual_at_open"]) > RAMP_DELTA
        row = {
            "event_id": self.new_event_id(), "capture": self.capture_label,
            "session": cand["session"],
            "candidate_open_t_ms": cand["open_t_ms"], "candidate_close_t_ms": cand["close_t_ms"],
            "evidence_duration_ms": duration_ms, "evidence_peak": cand["peak_abs"],
            "evidence_polarity": pol01_char(cand["polarity_sign"]),
            "evidence_abs_flux": round(cand["abs_flux"], 1),
            "evidence_signed_flux": round(cand["signed_flux"], 1),
            "evidence_pwm_actual_at_open": cand["pwm_actual_at_open"],
            "evidence_pwm_commanded_at_open": cand["pwm_commanded_at_open"],
            "evidence_pwm_actual_at_peak": cand["pwm_actual_at_peak"],
            "evidence_pwm_commanded_at_peak": cand["pwm_commanded_at_peak"],
            "evidence_pwm_actual_at_close": cand["pwm_actual_at_close"],
            "evidence_pwm_commanded_at_close": cand["pwm_commanded_at_close"],
            "evidence_pwm_for_gate": "", "evidence_direction": cand["direction_at_open"],
            "evidence_lobe_count": 1, "evidence_companions_json": "",
            "evidence_spans_pwm_transition": spans,
            "evidence_boundary_ambiguous": boundary_ambiguous,
            "evidence_pwm_regime_unvalidated": False,
            "map_position_before": "", "map_direction_before": "",
            "map_expected_polarity": "", "map_position_after": "",
            "disposition": "ARTIFACT", "disposition_rule_id": rule_id,
            "disposition_reasoning": reasoning,
            "contradiction_streak_index": "", "resync_offset": "", "resync_score_vector": "",
            "position_ring_inserted": False, "contradiction_ring_inserted": False,
        }
        self._count_disposition("ARTIFACT", rule_id)
        self.rows_out.append(row)

    def _emit_row(self, a):
        row = {
            "event_id": a["event_id"], "capture": self.capture_label, "session": a["session"],
            "candidate_open_t_ms": a["open_t_ms"], "candidate_close_t_ms": a["close_t_ms"],
            "evidence_duration_ms": a["duration_ms"], "evidence_peak": a["peak_abs"],
            "evidence_polarity": a["primary_polarity"] if a["lobe_count"] > 1 else pol01_char(a["polarity_sign"]),
            "evidence_abs_flux": round(a["primary_abs_flux"], 1),
            "evidence_signed_flux": round(a["signed_flux"], 1),
            "evidence_pwm_actual_at_open": a["pwm_actual_at_open"],
            "evidence_pwm_commanded_at_open": a["pwm_commanded_at_open"],
            "evidence_pwm_actual_at_peak": a["pwm_actual_at_peak"],
            "evidence_pwm_commanded_at_peak": a["pwm_commanded_at_peak"],
            "evidence_pwm_actual_at_close": a["pwm_actual_at_close"],
            "evidence_pwm_commanded_at_close": a["pwm_commanded_at_close"],
            "evidence_pwm_for_gate": a["_pwm_for_gate"] if a["_pwm_for_gate"] is not None else "",
            "evidence_direction": a["direction_at_open"],
            "evidence_lobe_count": a["lobe_count"],
            "evidence_companions_json": json.dumps(a["companions"]) if a["companions"] else "",
            "evidence_spans_pwm_transition": a["spans_pwm_transition"],
            "evidence_boundary_ambiguous": a["boundary_ambiguous"],
            "evidence_pwm_regime_unvalidated": a["pwm_regime_unvalidated"],
            "map_position_before": a["map_position_before"],
            "map_direction_before": a["map_direction_before"],
            "map_expected_polarity": a["map_expected_polarity"],
            "map_position_after": a["map_position_after"],
            "disposition": a["disposition"], "disposition_rule_id": a["rule_id"],
            "disposition_reasoning": a["reasoning"],
            "contradiction_streak_index": a["contradiction_streak_index"],
            "resync_offset": a["resync_offset"], "resync_score_vector": a["resync_score_vector"],
            "position_ring_inserted": a["position_ring_inserted"],
            "contradiction_ring_inserted": a["contradiction_ring_inserted"],
        }
        self.rows_out.append(row)

    def _emit_merged_companion_row(self, cand, anchor):
        """Sec 9: 'one row per RawCandidate (including merged companions...)'.
        The companion's own row carries disposition=MERGED_COMPANION and
        never touches either ring (Sec 4.2 invariant); the primary anchor's
        own row (emitted later, at flush) carries the updated lobe_count/
        companions_json -- see _merge_into_anchor / _flush_pending_anchor."""
        duration_ms = cand["close_t_ms"] - cand["open_t_ms"]
        rule_id = "MERGED_INTO=%s" % anchor["event_id"]
        row = {
            "event_id": self.new_event_id(), "capture": self.capture_label,
            "session": cand["session"],
            "candidate_open_t_ms": cand["open_t_ms"], "candidate_close_t_ms": cand["close_t_ms"],
            "evidence_duration_ms": duration_ms, "evidence_peak": cand["peak_abs"],
            "evidence_polarity": pol01_char(cand["polarity_sign"]),
            "evidence_abs_flux": round(cand["abs_flux"], 1),
            "evidence_signed_flux": round(cand["signed_flux"], 1),
            "evidence_pwm_actual_at_open": cand["pwm_actual_at_open"],
            "evidence_pwm_commanded_at_open": cand["pwm_commanded_at_open"],
            "evidence_pwm_actual_at_peak": cand["pwm_actual_at_peak"],
            "evidence_pwm_commanded_at_peak": cand["pwm_commanded_at_peak"],
            "evidence_pwm_actual_at_close": cand["pwm_actual_at_close"],
            "evidence_pwm_commanded_at_close": cand["pwm_commanded_at_close"],
            "evidence_pwm_for_gate": "", "evidence_direction": cand["direction_at_open"],
            "evidence_lobe_count": 1, "evidence_companions_json": "",
            "evidence_spans_pwm_transition":
                abs(cand["pwm_actual_at_close"] - cand["pwm_actual_at_open"]) > RAMP_DELTA,
            "evidence_boundary_ambiguous": False,
            "evidence_pwm_regime_unvalidated": False,
            "map_position_before": "", "map_direction_before": "",
            "map_expected_polarity": "", "map_position_after": "",
            "disposition": "MERGED_COMPANION", "disposition_rule_id": rule_id,
            "disposition_reasoning": (
                "opened %dms after anchor %s's most recent lobe closed (<= "
                "MERGE_WINDOW_MS=%d), peak=%s (ratio %.3f <= AMPLITUDE_RATIO_MAX=%.2f) "
                "-- Sec 4: absorbed as a companion lobe, never independently evaluated "
                "against Sec 3/5/6/7" %
                (anchor["companions"][-1]["offset_ms_from_previous_lobe_close"],
                 anchor["event_id"], MERGE_WINDOW_MS, cand["peak_abs"],
                 anchor["companions"][-1]["amplitude_ratio"] or 0.0, AMPLITUDE_RATIO_MAX)),
            "contradiction_streak_index": "", "resync_offset": "", "resync_score_vector": "",
            "position_ring_inserted": False, "contradiction_ring_inserted": False,
        }
        self._count_disposition("MERGED_COMPANION", rule_id)
        self.rows_out.append(row)

    def _count_disposition(self, disposition, rule_id):
        self.report["disposition_counts"][disposition] = \
            self.report["disposition_counts"].get(disposition, 0) + 1
        if rule_id:
            self.report["rule_id_counts"][rule_id] = self.report["rule_id_counts"].get(rule_id, 0) + 1

    def _maybe_flag_pwm_regime(self, a):
        pf = a.get("_pwm_for_gate")
        if pf is not None and pf > PWM_VALIDATED_CEILING:
            a["pwm_regime_unvalidated"] = True

    # -- Sec 5 arrival gate + Sec 6 map validation + Sec 7a recovery -------
    def _evaluate_credible_passage(self, a):
        # Checked against primary_position directly (not the historical
        # _declared_once flag): a SESSION reset clears primary_position back
        # to None without clearing _declared_once (Sec 2.6 -- a manifest
        # declares exactly once; _declared_once must not spuriously suggest
        # a live declaration still holds after a reset wiped it).
        if self.primary_position is None or a["open_t_ms"] < self.manifest.start_t_ms:
            a["disposition"] = "PRE_DECLARATION"; a["rule_id"] = "PRE_DECLARATION"
            a["reasoning"] = (
                "candidate open_t_ms=%d precedes the manifest-declared start "
                "(t_ms=%s) or no start has been declared yet in this session; no "
                "map-validated disposition is possible -- diagnostic only, "
                "mirrors the doctrine-review's Finding 17 (do not silently "
                "mislabel pre-declaration data under a timing/map verdict)"
                % (a["open_t_ms"], self.manifest.start_t_ms))
            return

        if self.is_stopped:
            a["disposition"] = "POSITION_UNRESOLVED"; a["rule_id"] = self.stop_reason
            a["reasoning"] = (
                "primary position safe-stopped at t_ms=%s (%s); no further "
                "position-affecting evaluation until a resuming event (direction "
                "reversal) occurs" % (self.stop_t_ms, self.stop_reason))
            return

        direction = self.primary_direction
        if direction is None:
            a["disposition"] = "PRE_DECLARATION"; a["rule_id"] = "DIRECTION_UNKNOWN"
            a["reasoning"] = ("primary_direction not yet established from ctl_dir "
                              "telemetry (still UNSET/NEUTRAL) -- cannot compute "
                              "next_mm(); mirrors QUORUM's own NO_DIR gate branch")
            return

        a["map_position_before"] = self.primary_position
        a["map_direction_before"] = _dir_label(direction)

        gate_result, gate_ms, elapsed = self._arrival_gate(a, self.prev_marker, direction,
                                                            self.reversal_pending_singleshot)
        if self.reversal_pending_singleshot:
            self.reversal_pending_singleshot = False
        if gate_result == "NO_PREV_INVALIDATED":
            self.interval_ref_invalidated_pending = False
        self._maybe_flag_pwm_regime(a)

        if gate_result in ("ARTIFACT_HARD_IMPOSSIBLE", "ARTIFACT_CONTEXTUAL_TOO_SOON"):
            a["disposition"] = "ARTIFACT"; a["rule_id"] = gate_result
            a["reasoning"] = (
                "elapsed=%sms < gate=%.1fms since previous accepted marker mm=%s "
                "(close_t=%s); pwm_for_gate=%s -- %s"
                % (elapsed, gate_ms, self.prev_marker["mm"], self.prev_marker["close_t_ms"],
                   a["_pwm_for_gate"],
                   "unconditional physical-impossibility bound" if gate_result ==
                   "ARTIFACT_HARD_IMPOSSIBLE" else "context-aware bound"))
            return

        expected_mm = self.qmap.next_mm(self.primary_position, direction)
        expected_pol01 = self.qmap.dna_at(expected_mm)
        a["map_expected_polarity"] = polchar(expected_pol01)
        observed_pol01 = sgn_to_pol01(a["polarity_sign"])

        if observed_pol01 == expected_pol01:
            self.primary_position = expected_mm
            a["map_position_after"] = self.primary_position
            a["disposition"] = "EXPECTED_ADVANCE"; a["rule_id"] = "MAP_MATCH"
            a["reasoning"] = (
                "credible, physically-timely (%s), observed=%s matched expected=%s "
                "at marker %d -- advanced once" %
                (gate_result, polchar(observed_pol01), polchar(expected_pol01), expected_mm))
            a["position_ring_inserted"] = True
            self._push_position_ring(expected_mm, a["open_t_ms"], a["close_t_ms"], a["event_id"])
            self.contradiction_streak_next = 1
            return

        # Mismatch -> CREDIBLE_CONTRADICTION ("No Way"), Sec 6.2
        a["map_position_after"] = self.primary_position   # unchanged
        a["disposition"] = "CREDIBLE_CONTRADICTION"; a["rule_id"] = "MAP_MISMATCH"
        a["contradiction_streak_index"] = self.contradiction_streak_next
        a["reasoning"] = (
            "credible, physically-timely (%s), observed=%s != expected=%s at marker "
            "%d -- \"No Way\": not advancing, contradiction_streak=%d" %
            (gate_result, polchar(observed_pol01), polchar(expected_pol01), expected_mm,
             a["contradiction_streak_index"]))
        self.contradiction_streak_next += 1
        obs = self._build_contradiction_observation(a, direction)
        self.contradiction_ring.append(obs)
        self._try_recovery(a, direction)

    def _arrival_gate(self, a, prev, direction, reversal):
        if prev is None:
            a["_pwm_for_gate"] = None
            return "NO_PREV", None, None
        if reversal:
            a["_pwm_for_gate"] = None
            return "REVERSAL_EXEMPT", None, None
        if self.interval_ref_invalidated_pending:
            a["_pwm_for_gate"] = None
            return "NO_PREV_INVALIDATED", None, None

        spacing = self.qmap.spacing_between(prev["mm"], direction)
        hard_ms = 1000.0 * spacing / self.qmap.velocity_mm_s(255.0)
        elapsed = a["open_t_ms"] - prev["close_t_ms"]

        pwm_for_gate = self.since_prev_max_pwm
        a["_pwm_for_gate"] = pwm_for_gate

        if elapsed < hard_ms:
            return "ARTIFACT_HARD_IMPOSSIBLE", hard_ms, elapsed

        if pwm_for_gate is not None and pwm_for_gate >= PWM_MODEL_VALID_FLOOR:
            contextual_ms = 1000.0 * spacing / self.qmap.velocity_mm_s(pwm_for_gate)
        else:
            contextual_ms = hard_ms   # Sec 5.4 -- the gate is never skipped

        if elapsed < contextual_ms:
            return "ARTIFACT_CONTEXTUAL_TOO_SOON", contextual_ms, elapsed
        return "PASS", contextual_ms, elapsed

    def _push_position_ring(self, mm, open_t_ms, close_t_ms, event_id):
        entry = {"mm": mm, "t_ms": open_t_ms, "close_t_ms": close_t_ms, "event_id": event_id}
        self.position_ring.append(entry)
        if len(self.position_ring) > POSITION_RING_SIZE:
            self.position_ring.pop(0)
        self.prev_marker = entry
        self.since_prev_max_pwm = None
        self.since_prev_push_close_t_ms = close_t_ms
        self.moving_ms_since_prev_push = 0.0   # Sec 7b: TRAVEL time only, dwells excluded
        self.liveness_stage1_fired = False
        self.liveness_stage2_fired = False

    def _build_contradiction_observation(self, a, direction):
        obs = {
            "observation_id": self.new_observation_id(),
            "t_ms": a["open_t_ms"],
            "observed_polarity": pol01_char(a["polarity_sign"]),
            "peak_abs": a["peak_abs"], "duration_ms": a["duration_ms"],
            "abs_flux": a["primary_abs_flux"],
            "pwm_actual_at_open": a["pwm_actual_at_open"],
            "pwm_at_peak": a["pwm_actual_at_peak"], "pwm_at_close": a["pwm_actual_at_close"],
            "pwm_for_gate": a.get("_pwm_for_gate"),
            "primary_position_at_contradiction": self.primary_position,
            "expected_polarity": a["map_expected_polarity"],
            "direction_at_contradiction": _dir_label(direction),
            "contradiction_streak_index": a["contradiction_streak_index"],
            "rule_id": a["rule_id"], "event_id": a["event_id"],
        }
        if len(self.contradiction_ring) >= CONTRADICTION_RING_SIZE:
            # Defensive: should be unreachable, since is_stopped short-circuits
            # evaluation before another push can happen once the ring is full.
            self.contradiction_ring.pop(0)
        return obs

    def _try_recovery(self, a, direction):
        """Sec 7a -- event-driven, runs on every single CREDIBLE_CONTRADICTION.
        Mutates `a` in place: leaves it as CREDIBLE_CONTRADICTION (no-op) unless
        WIN (-> RESYNC_ADOPTED) or the ring is full with no winner (->
        POSITION_UNRESOLVED, terminal)."""
        ring = self.contradiction_ring
        scores = {}
        for o in range(1, MAX_OMITTED + 1):
            s = 0
            for r in ring:
                hyp_pos = self.qmap.route_mod(r["primary_position_at_contradiction"] + direction * o)
                hyp_next = self.qmap.next_mm(hyp_pos, direction)
                hyp_pol01 = self.qmap.dna_at(hyp_next)
                if (1 if r["observed_polarity"] == "N" else 0) == hyp_pol01:
                    s += 1
            scores[o] = s
        ranked = sorted(scores.items(), key=lambda kv: -kv[1])
        leader_o, leader_s = ranked[0]
        runner_o, runner_s = ranked[1]
        margin = leader_s - runner_s
        a["resync_score_vector"] = json.dumps({str(o): s for o, s in scores.items()})

        if margin >= MARGIN and leader_s >= MIN_SUPPORT:
            new_pos = self.qmap.next_mm(
                self.qmap.route_mod(self.primary_position + direction * leader_o), direction)
            contributing_ids = [r["observation_id"] for r in ring]
            self.primary_position = new_pos
            a["map_position_after"] = new_pos
            a["disposition"] = "RESYNC_ADOPTED"; a["rule_id"] = "RESYNC_OFFSET=%d" % leader_o
            a["resync_offset"] = leader_o
            a["reasoning"] = (
                "hypothesis offset +%d won: score=%d vs runner_up(+%d)=%d "
                "(margin=%d>=%d, support=%d>=%d) over observation_ids=%s" %
                (leader_o, leader_s, runner_o, runner_s, margin, MARGIN, leader_s,
                 MIN_SUPPORT, contributing_ids))
            # Final ring membership, not transient: C's brief ContradictionRing
            # entry is superseded by this same-step promotion+clear, so the
            # Sec 9 "at most one true" invariant is reported against the
            # settled state (position), not the instant before the clear.
            a["position_ring_inserted"] = True
            a["contradiction_ring_inserted"] = False
            recovery_latency = a["open_t_ms"] - ring[0]["t_ms"]
            self.report["recovery_events"].append({
                "event_id": a["event_id"], "adopted_offset": leader_o,
                "latency_ms": recovery_latency, "observations": len(ring),
                "contributing_observation_ids": contributing_ids,
                "score_vector": scores,
            })
            self._push_position_ring(new_pos, a["open_t_ms"], a["close_t_ms"], a["event_id"])
            self.contradiction_ring = []
            self.contradiction_streak_next = 1
            return

        a["contradiction_ring_inserted"] = True
        if len(ring) >= CONTRADICTION_RING_SIZE:
            a["disposition"] = "POSITION_UNRESOLVED"; a["rule_id"] = "CONTRADICTION_RING_FULL_NO_WIN"
            a["reasoning"] = (
                "ContradictionRing reached its %d-entry cap with no offset in "
                "{+1..+%d} reaching MARGIN>=%d and MIN_SUPPORT>=%d (best: offset+%d "
                "score=%d, runner_up offset+%d score=%d) -- terminal, requesting "
                "safe stop" % (CONTRADICTION_RING_SIZE, MAX_OMITTED, MARGIN, MIN_SUPPORT,
                               leader_o, leader_s, runner_o, runner_s))
            self.is_stopped = True
            self.stop_reason = "POSITION_UNRESOLVED"
            self.stop_t_ms = a["close_t_ms"]
            self.report["terminal_stops"].append({
                "event_id": a["event_id"], "t_ms": a["close_t_ms"],
                "reason": "POSITION_UNRESOLVED",
                "contradiction_ring_snapshot": ring, "position_ring_snapshot": list(self.position_ring),
                "final_scores": scores,
            })

    # -- Sec 7b liveness (independent of 7a; silence-driven) ---------------
    def _check_liveness(self, t_ms, pwm_actual):
        if self.primary_position is None or self.primary_direction is None:
            return
        if self.is_stopped:
            return
        if pwm_actual <= SUSPEND_PWM_FLOOR:
            return   # gated off while confirmed-stationary (Sec 7b)
        if self.prev_marker is None:
            return   # no anchor to measure elapsed against yet (mirrors NO_PREV)

        # Sec 7b: TRAVEL time since the last push -- see _on_sample's own
        # comment. NOT t_ms - since_prev_push_close_t_ms, which would count
        # dwell time and false-trigger on the very first sample after any
        # real station stop.
        elapsed = self.moving_ms_since_prev_push
        spacing = self.qmap.spacing_between(self.prev_marker["mm"], self.primary_direction)
        pwm_ref = self.since_prev_max_pwm or pwm_actual
        if pwm_ref >= PWM_MODEL_VALID_FLOOR:
            one_interval_ms = 1000.0 * spacing / self.qmap.velocity_mm_s(pwm_ref)
        else:
            # DEVIATION FROM A LITERAL READING, documented per the task's
            # "fix a genuine inconsistency minimally" instruction: Sec 5.3's
            # own low-PWM fallback (velocity_mm_s(255), i.e. HARD_IMPOSSIBLE_MS)
            # is deliberately the FASTEST/most permissive assumption there,
            # because a SMALL minimum-arrival-time bound is conservative
            # against the arrival gate's failure mode (false rejection).
            # Sec 7b's failure mode is the OPPOSITE (false/spurious timeout),
            # so reusing the same fastest-speed fallback SHRINKS the
            # per-interval yardstick and makes the alarm fire too early --
            # confirmed against Toby's clean-control capture, where a literal
            # reuse produced a LIVENESS_TIMEOUT within seconds of real travel
            # and froze ~94% of an otherwise-clean run. Using the model's
            # SLOWEST still-valid speed here instead keeps the yardstick
            # conservative in the direction Sec 7b actually cares about.
            one_interval_ms = 1000.0 * spacing / self.qmap.velocity_mm_s(PWM_MODEL_VALID_FLOOR)

        if elapsed >= N_LIVENESS * one_interval_ms and not self.liveness_stage1_fired:
            self.liveness_stage1_fired = True
            self.report["liveness_warnings"].append({
                "t_ms": t_ms, "elapsed_ms": elapsed, "one_interval_ms": one_interval_ms,
                "since_mm": self.prev_marker["mm"],
            })

        if elapsed >= 2 * N_LIVENESS * one_interval_ms and not self.liveness_stage2_fired:
            self.liveness_stage2_fired = True
            self.is_stopped = True
            self.stop_reason = "LIVENESS_TIMEOUT"
            self.stop_t_ms = t_ms
            self.report["terminal_stops"].append({
                "event_id": None, "t_ms": t_ms, "reason": "LIVENESS_TIMEOUT",
                "elapsed_ms": elapsed, "since_mm": self.prev_marker["mm"],
            })
            self._emit_liveness_timeout_row(t_ms, elapsed)

    def _emit_liveness_timeout_row(self, t_ms, elapsed):
        row = {c: "" for c in EVENT_COLUMNS}
        row.update({
            "event_id": self.new_event_id(), "capture": self.capture_label,
            "session": self.session_label,
            "candidate_open_t_ms": t_ms, "candidate_close_t_ms": t_ms,
            "evidence_lobe_count": 0,
            "map_position_before": self.primary_position,
            "map_direction_before": _dir_label(self.primary_direction),
            "map_position_after": self.primary_position,
            "disposition": "POSITION_UNRESOLVED", "disposition_rule_id": "LIVENESS_TIMEOUT",
            "disposition_reasoning": (
                "no credible arrival for %.0fms of continued travel since the last "
                "confirmed marker mm=%s -- Sec 7b silence-driven liveness timeout "
                "(2xN_LIVENESS intervals elapsed); NOT a position estimate, requests "
                "a safe stop only" % (elapsed, self.prev_marker["mm"])),
            "position_ring_inserted": False, "contradiction_ring_inserted": False,
        })
        self._count_disposition("POSITION_UNRESOLVED", "LIVENESS_TIMEOUT")
        self.rows_out.append(row)


_DIR_MAP = {"CW": 1, "CCW": -1}


def _dir_label(sign):
    if sign is None:
        return ""
    return "CW" if sign > 0 else "CCW"


def _num(v):
    if v is None or v == "":
        return None
    try:
        return int(v)
    except ValueError:
        return float(v)


# ===========================================================================
# Report assembly + post-hoc audits (Sec 8 acceptance criteria)
# ===========================================================================
def audit_false_inclusion(rows):
    """Sec 8.1: no candidate independently identifiable as belonging to a
    known-artifact class may reach EXPECTED_ADVANCE or RESYNC_ADOPTED. This
    is an independent, redundant check against the ALREADY-EMITTED rows --
    it does not trust the state machine's own bookkeeping, so it also
    catches a regression in this file, not just a hypothetical bad
    candidate."""
    violations = []
    for r in rows:
        if r["disposition"] not in ("EXPECTED_ADVANCE", "RESYNC_ADOPTED"):
            continue
        reasons = []
        dur = r["evidence_duration_ms"]
        flux = r["evidence_abs_flux"]
        if dur != "" and dur is not None and float(dur) < DURATION_FLOOR_MS:
            reasons.append("duration=%s < %d" % (dur, DURATION_FLOOR_MS))
        if flux != "" and flux is not None and float(flux) < FLUX_FLOOR:
            reasons.append("abs_flux=%s < %d" % (flux, FLUX_FLOOR))
        pwm_open = r["evidence_pwm_actual_at_open"]
        if pwm_open != "" and pwm_open is not None and float(pwm_open) <= SUSPEND_PWM_FLOOR:
            reasons.append("pwm_actual_at_open=%s <= suspend floor %d" % (pwm_open, SUSPEND_PWM_FLOOR))
        if reasons:
            violations.append({"event_id": r["event_id"], "disposition": r["disposition"],
                               "reasons": reasons})
    return {
        "method": "post-hoc scan of every EXPECTED_ADVANCE/RESYNC_ADOPTED row for "
                  "duration<floor, abs_flux<floor, or pwm_actual_at_open<=suspend "
                  "floor -- independent of the state machine that produced the rows",
        "count": len(violations), "violations": violations,
    }


def audit_ring_contamination(rows):
    """Sec 9's own programmatic invariant: position_ring_inserted and
    contradiction_ring_inserted must match disposition, and at most one may
    be true on any row."""
    violations = []
    expect_position = {"EXPECTED_ADVANCE", "RESYNC_ADOPTED"}
    expect_contradiction = {"CREDIBLE_CONTRADICTION"}
    for r in rows:
        pri = bool(r["position_ring_inserted"])
        cri = bool(r["contradiction_ring_inserted"])
        if pri and cri:
            violations.append({"event_id": r["event_id"], "problem": "both rings true"})
            continue
        disp = r["disposition"]
        # POSITION_UNRESOLVED via CONTRADICTION_RING_FULL_NO_WIN is the one
        # exception: the triggering candidate is genuinely the ring's final,
        # still-present 12th entry (the ring is preserved full, for its own
        # forensic snapshot, not cleared the way a RESYNC_ADOPTED clears it)
        # -- contradiction_ring_inserted=True is correct there, not a leak.
        ring_full_terminal = (disp == "POSITION_UNRESOLVED" and
                              r["disposition_rule_id"] == "CONTRADICTION_RING_FULL_NO_WIN")
        if disp in expect_position and not pri:
            violations.append({"event_id": r["event_id"],
                               "problem": "%s but position_ring_inserted=False" % disp})
        if disp in expect_contradiction and not cri:
            violations.append({"event_id": r["event_id"],
                               "problem": "%s but contradiction_ring_inserted=False" % disp})
        if disp not in expect_position and pri:
            violations.append({"event_id": r["event_id"],
                               "problem": "position_ring_inserted=True but disposition=%s" % disp})
        if disp not in expect_contradiction and cri and not ring_full_terminal:
            violations.append({"event_id": r["event_id"],
                               "problem": "contradiction_ring_inserted=True but disposition=%s" % disp})
    return {
        "method": "per-row check that position_ring_inserted/contradiction_ring_inserted "
                  "are each true iff disposition is EXPECTED_ADVANCE|RESYNC_ADOPTED / "
                  "CREDIBLE_CONTRADICTION respectively, and never both -- this is Sec 9's "
                  "own stated programmatic invariant",
        "count": len(violations), "violations": violations,
    }


def anchor_drift_report(session_obj, qmap):
    out = []
    for anch in session_obj.anchor_events:
        entry = {
            "t_ms": anch["t_ms"], "anchor_id": anch["anchor_id"], "text": anch["text"],
            "replay_primary_position_at_anchor": anch["primary_position_at_anchor"],
            "replay_primary_direction_at_anchor": anch["primary_direction_at_anchor"],
        }
        out.append(entry)
    return out


def build_report(args, manifest, qmap, session_obj, rows_processed):
    rows = session_obj.rows_out
    report = dict(session_obj.report)
    report["tool"] = "templates_replay_spec.py (implements docs/TEMPLATES_REPLAY_DESIGN_SPEC.md)"
    report["capture"] = args.capture
    report["manifest"] = manifest.path
    report["session"] = session_obj.session_label
    report["rows_processed"] = rows_processed
    report["candidate_rows_emitted"] = len(rows)
    report["false_inclusion_audit"] = audit_false_inclusion(rows)
    report["ring_contamination_audit"] = audit_ring_contamination(rows)

    n_recoveries = len(report["recovery_events"])
    if n_recoveries:
        lat = [e["latency_ms"] for e in report["recovery_events"]]
        report["recovery_latency_ms_summary"] = {
            "count": n_recoveries, "min": min(lat), "max": max(lat),
            "mean": sum(lat) / n_recoveries,
        }
    else:
        report["recovery_latency_ms_summary"] = {"count": 0}

    terminal_counts = {}
    for t in report["terminal_stops"]:
        terminal_counts[t["reason"]] = terminal_counts.get(t["reason"], 0) + 1
    report["terminal_stop_counts"] = terminal_counts

    report["anchor_drift"] = anchor_drift_report(session_obj, qmap)

    # Incorrect-adoption flags: a RESYNC_ADOPTED event whose next anchor
    # (before any later resync/session end) shows a nonzero, unexplained
    # drift is circumstantial evidence the adoption was wrong. Anchors with
    # no asserted mm in this manifest cannot support this check.
    flags = []
    manifest_anchor_mm = {a.get("t_ms"): a.get("asserted_mm") for a in manifest.anchors
                          if a.get("asserted_mm") is not None}
    for rec in report["recovery_events"]:
        eid = rec["event_id"]
        row = next((r for r in rows if r["event_id"] == eid), None)
        if row is None:
            continue
        adopted_mm = row["map_position_after"]
        later_anchors = [a for a in session_obj.anchor_events
                         if a["t_ms"] is not None and row["candidate_open_t_ms"] is not None
                         and a["t_ms"] > row["candidate_open_t_ms"]]
        for anch in sorted(later_anchors, key=lambda x: x["t_ms"])[:1]:
            asserted = manifest_anchor_mm.get(anch["t_ms"])
            if asserted is not None:
                flags.append({
                    "resync_event_id": eid, "adopted_mm": adopted_mm,
                    "next_anchor_t_ms": anch["t_ms"], "next_anchor_asserted_mm": asserted,
                    "replay_mm_at_anchor": anch["primary_position_at_anchor"],
                    "note": "circumstantial only -- other events between the resync and "
                            "this anchor could also explain any drift",
                })
    report["incorrect_adoption_flags"] = flags
    if not flags and n_recoveries:
        report["incorrect_adoption_flags_note"] = (
            "no manifest anchor with an asserted position follows any RESYNC_ADOPTED "
            "event in this capture -- not measurable from available ground truth")
    elif not n_recoveries:
        report["incorrect_adoption_flags_note"] = "no RESYNC_ADOPTED events occurred"

    report["final_primary_position"] = session_obj.primary_position
    report["final_primary_direction"] = _dir_label(session_obj.primary_direction)
    report["is_stopped_at_end"] = session_obj.is_stopped
    report["stop_reason_at_end"] = session_obj.stop_reason

    report["assumptions_and_limitations"] = [
        "PRE_DECLARATION is a disposition value added beyond the spec's Sec 9 six-value "
        "enum, used only for candidates before the manifest-declared start (or before "
        "direction is known) -- see the module docstring and the task return summary.",
        "LIVENESS_WARNING is reported only in this JSON report (liveness_warnings), not "
        "as an events-CSV row, since it has no candidate to attach to; LIVENESS_TIMEOUT "
        "IS emitted as a synthetic events-CSV row (disposition=POSITION_UNRESOLVED, "
        "rule_id=LIVENESS_TIMEOUT) since it has real navigational consequence.",
        "ADC saturation (Sec 3.5) is implemented as a structural no-op: ADC_MIN_RAIL/"
        "ADC_MAX_RAIL are not independently confirmed, exactly as the spec itself states.",
        "Anchors are read-only ground-truth checkpoints for drift reporting; they never "
        "mutate primary_position mid-replay (that would defeat the point of measuring "
        "drift). A direction reversal observed live in ctl_dir is the only mid-session "
        "event that can resume a safe-stopped replay.",
    ]
    return report


# ===========================================================================
# CLI
# ===========================================================================
def _row_get(header_idx, cols):
    def get(name):
        i = header_idx.get(name)
        if i is None or i >= len(cols):
            return None
        v = cols[i]
        return v if v != "" else None
    return get


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("capture", help="decoded QUORUM TRACE CSV (tools/qt_decode.py output)")
    ap.add_argument("--manifest", required=True, help="ground-truth manifest JSON")
    ap.add_argument("--out-events", required=True, help="per-candidate event stream CSV")
    ap.add_argument("--out-report", required=True, help="summary report JSON")
    ap.add_argument("--session", help="filter to this session id (overrides manifest.session)")
    ap.add_argument("--quorum-ino", default="firmware/QUORUM/QUORUM.ino",
                    help="firmware source to extract the map/velocity model from (read-only)")
    args = ap.parse_args()

    manifest = Manifest(args.manifest)
    target_session = args.session or manifest.session
    qmap = QuorumMap(args.quorum_ino)

    session_obj = None
    rows_processed = 0
    with open(args.capture, newline="") as fh:
        reader = csv.reader(fh)
        header = next(reader)
        header_idx = {name: i for i, name in enumerate(header)}
        row_type_i = header_idx["row_type"]
        session_i = header_idx["session"]

        for cols in reader:
            rows_processed += 1
            row_type = cols[row_type_i]
            if row_type in ("DECISION", "STATUS", "BAD"):
                continue
            row_session = cols[session_i]
            if target_session and row_session and row_session != target_session:
                continue
            if session_obj is None:
                label = target_session or row_session or "UNKNOWN"
                session_obj = ReplaySession(qmap, manifest, args.capture, label)
            get = _row_get(header_idx, cols)
            session_obj.process_row(row_type, get)

    if session_obj is None:
        session_obj = ReplaySession(qmap, manifest, args.capture, target_session or "NONE")
    session_obj.finalize()

    os.makedirs(os.path.dirname(os.path.abspath(args.out_events)) or ".", exist_ok=True)
    with open(args.out_events, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=EVENT_COLUMNS)
        w.writeheader()
        w.writerows(session_obj.rows_out)

    report = build_report(args, manifest, qmap, session_obj, rows_processed)
    os.makedirs(os.path.dirname(os.path.abspath(args.out_report)) or ".", exist_ok=True)
    with open(args.out_report, "w") as fh:
        json.dump(report, fh, indent=2, sort_keys=True, default=str)

    print("templates_replay_spec.py: %d rows processed, %d candidate rows emitted"
         % (rows_processed, len(session_obj.rows_out)))
    print("  disposition counts: %s" % json.dumps(report["disposition_counts"], sort_keys=True))
    print("  false_inclusion_audit.count = %d" % report["false_inclusion_audit"]["count"])
    print("  ring_contamination_audit.count = %d" % report["ring_contamination_audit"]["count"])
    print("  recoveries=%d terminal_stops=%s final_position=%s final_direction=%s" %
         (len(report["recovery_events"]), report["terminal_stop_counts"],
          report["final_primary_position"], report["final_primary_direction"]))
    return 0


if __name__ == "__main__":
    sys.exit(main())
