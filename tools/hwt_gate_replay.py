#!/usr/bin/env python3
"""hwt_gate_replay.py — offline, host-side prototype of a target-acquisition
gate for HALL_WAVEFORM_TEST captures, evaluated against QUORUM's own track
map and polarity sequence.

INVESTIGATORY / UNAPPROVED. Nothing here is implemented in QUORUM. This is
an evaluation instrument: it measures what a candidate set of gates WOULD
have decided, on real captures, against QUORUM's real map — it does not
select production thresholds, and it never touches firmware.

    python3 tools/hwt_gate_replay.py \\
        tools/manifests/grillers.json \\
        -o run_decisions.csv --summary run_summary.txt \\
        --plot-dir run_plots

================================================================================
WHY THIS EXISTS
================================================================================
QUORUM's current pipeline (see the audit in the accompanying report) advances
navMm and inserts an event into the evidence ring for almost every detector
event that clears the §3 timing gate — BEFORE polarity is ever compared
against the map. Polarity disagreement is reported (DISAGREE) but does not
prevent the advance that already happened. A narrow noise spike with the
wrong polarity therefore has exactly the same navigation standing as a
genuine, correctly-polarised magnet passage, until three disagreements in a
row eventually trigger a QUORUM re-evaluation.

The operator's intended model (see the task this tool was built for) inverts
that order: decide whether a response is a PLAUSIBLE PHYSICAL MAGNET CURVE
before it is compared to anything, decide whether its timing is PHYSICALLY
POSSIBLE before it is compared to anything, and only if both hold does its
polarity get compared to the one expected next mapped marker. A response
that fails any of those checks is trashed for navigation immediately: no
ring entry, no scoring against alternate hypotheses, no later resurrection.

================================================================================
ARCHITECTURE
================================================================================
1. Candidate events come from tools/hwt_excursions.py's frozen-baseline
   detector (analyze_captures(), baseline_mode="frozen") — the corrected
   measurement layer from the prior investigation. This tool does not
   re-implement excursion detection.
2. The track map (polarity sequence + inter-marker spacing) and QUORUM's own
   navigation constants are read directly from firmware/QUORUM/QUORUM.ino via
   tools/quorum_map.py — never hand-copied.
3. A RunManifest (JSON) supplies exactly the facts a person actually knows
   about one capture — operator-confirmed starting position, direction,
   polarity-orientation convention, anchors, known stops/stalls/assistance,
   and explicit uncertainty notes — kept syntactically separate from
   anything this tool infers.
4. Every candidate event is built into an AcquisitionEvent (see
   build_acquisition_event()) carrying physical measurement, detector
   interpretation and map expectation in separate fields, then run through
   an explicit, ordered gate pipeline (evaluate_event()) that assigns
   exactly one Disposition and, for ACCEPT_EXPECTED_MARKER only, advances a
   PREDICTED position that is kept entirely separate from the operator's own
   confirmed position (anchors are compared against the prediction; they
   never feed it).
5. Every decision — accepted or rejected — is logged to a chronological CSV,
   in order, with the raw measurement, every gate's verdict, and the
   predicted position before and after. Nothing rejected is dropped from
   this record; only ACCEPT_EXPECTED_MARKER events can move the predicted
   position, and each does so exactly once.

================================================================================
GATE ORDER (evaluate_event(), fixed and logged every time)
================================================================================
  1. COMPLETENESS   incomplete/gapped/forced-end            -> REJECT_INCOMPLETE
  2. MORPHOLOGY     duration and absolute integrated flux    -> REJECT_SPIKE
                    must both clear their (evaluation-only,
                    CLI-configurable) bars. A third measure,
                    "continuity" (see CONTINUITY IS
                    DIAGNOSTIC-ONLY below), is computed and
                    carried on every event but does NOT
                    participate in this or any other decision.
  3. PHYSICAL TIMING  elapsed time since the last ACCEPTED    -> REJECT_PHYSICALLY_TOO_SOON
                    marker must be at least the minimum       or REJECT_PROBABLE_RETURN
                    time QUORUM's own (explicitly provisional (if ALSO opposite polarity
                    — see quorum_map.py) velocity model,      to the previous accepted
                    evaluated at full throttle, allows for    marker: a same-magnet
                    the map spacing to the expected next      return/secondary response
                    marker. No dead-time is guessed; the      is a strictly MORE
                    bound comes from the map (extracted, not  specific and better-
                    hand-typed) and QUORUM's own speed model. supported diagnosis than
                                                               a bare timing failure)
  4. EXPECTED POLARITY  a plausible, timely curve whose        -> REJECT_WRONG_EXPECTED_POLARITY
                    opening polarity does not match the        ("No Way" — do not advance,
                    expected next mapped marker's polarity     keep waiting)
  5. (report-only)  a streak of REJECT_WRONG_EXPECTED_POLARITY is counted and
                    reported (what WOULD happen after N in a row) without any
                    new recovery policy being implemented.

  Anything surviving all four gates is ACCEPT_EXPECTED_MARKER: the predicted
  position advances by exactly one marker, in the manifest's declared
  direction, and this event's polarity/time become the new "previous
  accepted" reference for the next candidate.

REVIEW_AMBIGUOUS and REJECT_UNSTABLE_BASELINE are part of the Disposition
enum but are NOT wired into evaluate_event() as active rules in this
version: the prior investigation (see the independent-comparison report)
found that this tool's own baseline-quality diagnostics (pre_range_counts,
pre_stdev_counts) do NOT cleanly separate trustworthy from untrustworthy
excursions in real data, so inventing a cutoff for REJECT_UNSTABLE_BASELINE
here would be exactly the unjustified rule the task warns against. The
diagnostics are still computed and carried on every event for a person to
judge; REVIEW_AMBIGUOUS remains available for a future gate with better
evidence behind it.

================================================================================
CONTINUITY IS DIAGNOSTIC-ONLY (corrected; it was briefly an active gate)
================================================================================
An earlier version of this tool gated REJECT_SPIKE partly on
continuity_ratio() — the fraction of sample-to-sample sign changes in the
baseline-relative deviation trace, computed with a "derivative dead zone"
(a minimum delta before a step counts as a direction at all) — and shipped
a default dead zone of 20 counts, described as filtering "ordinary ADC
jitter". That description overstated the evidence. What was actually shown
was: (a) with a zero dead zone, ONE selected clean broad response and ONE
selected merged excursion scored nearly identically (~0.68), so the
zero-dead-zone metric could not separate that specific pair; (b) a dead
zone of 20 counts separated that SAME pair (0.000 vs 0.681). Neither
observation demonstrates the metric is measuring ADC noise, that 20 counts
is a meaningful physical threshold, or that the metric generalizes beyond
those two hand-picked examples — no independently-collected noise-only
data (e.g. a known-stationary, powered, no-magnet-nearby period) was ever
compared against it. Using this metric to reject or accept events was
exactly the kind of unjustified rule the rest of this tool's design
explicitly tries to avoid (see the REJECT_UNSTABLE_BASELINE note above).

continuity_ratio() and its dead-zone parameter are RETAINED as a
diagnostic-only field (det_continuity_ratio, det_continuity_dead_zone) on
every event, computed and reported but never read by evaluate_event() or
any Disposition. The metric itself may turn out to be unsuitable for this
purpose regardless of parameter choice — that has not been established
either way. tools/hwt_adc_delta_diagnostics.py independently characterizes
raw sample-to-sample deltas (at several lags) in regions identified WITHOUT
reference to continuity_ratio at all — known-stationary (PWM-dwell),
moving-quiet (between excursions, PWM elevated), and broad-response
regions (duration+flux only) — as a first step toward evidence that could
someday support (or rule out) a smoothness-based gate. No such gate is
implemented here.
================================================================================
"""

import argparse
import csv
import json
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hwt_excursions as E   # noqa: E402
from quorum_map import QuorumMap   # noqa: E402

DIRECTION = {"CW": 1, "CCW": -1}

DISPOSITIONS = (
    "ACCEPT_EXPECTED_MARKER",
    "REJECT_SPIKE",
    "REJECT_INCOMPLETE",
    "REJECT_PHYSICALLY_TOO_SOON",
    "REJECT_PROBABLE_RETURN",
    "REJECT_WRONG_EXPECTED_POLARITY",
    "REJECT_UNSTABLE_BASELINE",
    "REVIEW_AMBIGUOUS",
)

# ---------------------------------------------------------------------------
# Acquisition-event contract (task part B). Physical measurement, detector
# interpretation, map expectation and final disposition are kept in
# separate, clearly-prefixed field groups on purpose — see the docstring.
# ---------------------------------------------------------------------------
EVENT_COLUMNS = [
    # identity / provenance
    "event_id", "capture", "session", "excursion_id",
    # phys_*: physical measurement (from hwt_excursions, unaltered)
    "phys_open_sample", "phys_close_sample", "phys_open_time_s", "phys_close_time_s",
    "phys_duration_ms", "phys_max_pos_flux", "phys_max_neg_flux",
    "phys_integrated_signed_flux", "phys_integrated_abs_flux",
    "phys_baseline_value", "phys_baseline_mode", "phys_baseline_n_quiet",
    "phys_pre_range_counts", "phys_pre_stdev_counts",
    "phys_incomplete", "phys_gaps_within_count", "phys_gaps_near_count",
    "phys_in_low_pwm_dwell",
    "phys_pwm_actual_at_open", "phys_pwm_commanded_at_open", "phys_direction",
    # det_*: detector interpretation (derived from phys_*, still not map-aware)
    "det_open_polarity",
    "det_continuity_ratio", "det_continuity_dead_zone",  # DIAGNOSTIC ONLY -- see module docstring
    # ctx_*: context since the previous ACCEPTED marker (replay state, not
    # a property of this event alone)
    "ctx_time_since_prev_accepted_s", "ctx_min_time_to_next_marker_s",
    "ctx_prev_accepted_mm", "ctx_prev_accepted_polarity",
    # map_*: map expectation (requires a declared position; blank until one exists)
    "map_expected_next_mm", "map_expected_next_polarity",
    # disposition
    "disp_final", "disp_reason",
    "disp_predicted_mm_before", "disp_predicted_mm_after",
]


class Disposition:
    def __init__(self, name, reason):
        assert name in DISPOSITIONS, "unknown disposition %r" % name
        self.name = name
        self.reason = reason


# ---------------------------------------------------------------------------
# Run manifest (task part C.3)
# ---------------------------------------------------------------------------
class RunManifest:
    def __init__(self, path):
        with open(path) as fh:
            data = json.load(fh)
        self._load(data, path)

    @classmethod
    def from_data(cls, **kwargs):
        """Build a manifest from keyword arguments instead of a JSON file --
        for host tests. Required: capture, direction, start_mm, start_time_s.
        Optional: start_confirmed, positive_deviation_is, anchors,
        uncertainty_notes (all default to the same values an unfilled JSON
        manifest would produce)."""
        data = {
            "capture": kwargs["capture"],
            "direction": kwargs["direction"],
            "operator_confirmed_start": {
                "mm": kwargs["start_mm"], "time_s": kwargs["start_time_s"],
                "confirmed": kwargs.get("start_confirmed", True),
                "note": kwargs.get("start_note", ""),
            },
            "polarity_convention": {
                "positive_deviation_is": kwargs.get("positive_deviation_is", "N"),
                "confirmed": kwargs.get("polarity_convention_confirmed", True),
                "note": kwargs.get("polarity_convention_note", ""),
            },
            "known_stalls_stops_assistance": kwargs.get("known_stalls_stops_assistance", []),
            "anchors": kwargs.get("anchors", []),
            "uncertainty_notes": kwargs.get("uncertainty_notes", []),
        }
        self = cls.__new__(cls)
        self._load(data, "<in-memory>")
        return self

    def _load(self, data, path):
        self.path = path
        self.capture = data["capture"]
        self.direction_label = data["direction"]
        if self.direction_label not in DIRECTION:
            raise ValueError("manifest direction must be CW or CCW, got %r" % self.direction_label)
        self.direction = DIRECTION[self.direction_label]
        start = data["operator_confirmed_start"]
        self.start_mm = int(start["mm"])
        self.start_time_s = float(start["time_s"])
        self.start_confirmed = bool(start.get("confirmed", False))
        self.start_note = start.get("note", "")
        conv = data.get("polarity_convention", {})
        self.positive_deviation_is = conv.get("positive_deviation_is", "N")
        self.polarity_convention_confirmed = bool(conv.get("confirmed", False))
        self.polarity_convention_note = conv.get("note", "")
        self.known_events = data.get("known_stalls_stops_assistance", [])
        self.anchors = data.get("anchors", [])
        self.uncertainty_notes = data.get("uncertainty_notes", [])
        self.raw = data

    def open_polarity_of(self, dev_sign):
        """dev_sign: +1 or -1. Returns 1 (N) or 0 (S) using this manifest's
        stated convention, so a manifest with the OPPOSITE convention
        produces the opposite polarity from the SAME raw measurement --
        the "fails visibly" behaviour the synthetic tests require."""
        is_n = (dev_sign > 0) if self.positive_deviation_is == "N" else (dev_sign < 0)
        return 1 if is_n else 0


# ---------------------------------------------------------------------------
# Candidate events: hwt_excursions' frozen detector, converted to the
# acquisition-event contract.
# ---------------------------------------------------------------------------
def continuity_ratio(samples, baseline, idxs, dead_zone=20.0):
    """DIAGNOSTIC ONLY -- see the module docstring's "CONTINUITY IS
    DIAGNOSTIC-ONLY" section. Not read by evaluate_event() or any
    Disposition.

    A candidate smoothness measure: the fraction of sample-to-sample sign
    changes in the baseline-relative deviation trace, after ignoring any
    step smaller than `dead_zone` counts (the "experimental derivative dead
    zone" -- a parameter of this measurement, not a validated ADC noise
    figure; see tools/hwt_adc_delta_diagnostics.py for an attempt to
    characterize actual sample-to-sample behavior independently).

    What is actually known about this measure: on exactly two hand-picked
    excursions (one visually clean 333 ms broad response, one known 5.5 s
    merged excursion), a dead_zone of 0 scored them nearly identically
    (~0.68 each) and a dead_zone of 20 separated them (0.000 vs 0.681).
    That is the entire evidentiary basis for dead_zone=20 -- it was NOT
    derived from an independently measured noise or delta distribution,
    was NOT validated across the full event population, and may not
    generalize. The measure itself may be unsuitable for distinguishing
    genuine curves from spikes or merged excursions regardless of
    dead_zone; that question is open, not resolved, and this function must
    not be used to gate or grade events until it is."""
    devs = [samples[i]["raw"] - baseline[i] for i in idxs]
    if len(devs) < 3:
        return 0.0
    signs = []
    for a, b in zip(devs, devs[1:]):
        d = b - a
        if d > dead_zone:
            signs.append(1)
        elif d < -dead_zone:
            signs.append(-1)
    if len(signs) < 2:
        return 0.0
    changes = sum(1 for a, b in zip(signs, signs[1:]) if a != b)
    return changes / (len(signs) - 1)


def build_seq_index(samples):
    return {s["seq"]: i for i, s in enumerate(samples)}


def excursion_sample_range(e, seq_index):
    i0 = seq_index.get(int(e["start_sample"]))
    i1 = seq_index.get(int(e["end_sample"]))
    if i0 is None or i1 is None:
        return None
    return range(i0, i1 + 1)


def build_acquisition_events(capture_path, manifest, *, entry_threshold=30.0,
                             exit_threshold=15.0, pre_window=200,
                             baseline_method="mean", gap_margin_s=0.05,
                             dwell_pwm_max=5.0, dwell_min_ms=1000.0,
                             continuity_dead_zone=20.0):
    """Runs hwt_excursions' frozen detector and converts every resulting
    excursion, in chronological order, into an AcquisitionEvent dict
    (EVENT_COLUMNS' phys_*/det_* fields only — ctx_*/map_*/disp_* are filled
    in by the replay loop, which needs sequential state). Returns
    (events, sessions_ctx) — sessions_ctx is exposed for plotting.

    continuity_dead_zone only affects the DIAGNOSTIC det_continuity_ratio
    field (see continuity_ratio()'s docstring) -- it cannot change any
    event's disposition; see test_continuity_settings_cannot_change_disposition."""
    result = E.analyze_captures(
        capture_path, entry_threshold=entry_threshold, exit_threshold=exit_threshold,
        baseline_mode="frozen", pre_window=pre_window, baseline_method=baseline_method,
        gap_margin_s=gap_margin_s, dwell_pwm_max=dwell_pwm_max, dwell_min_ms=dwell_min_ms)
    excursions = result["excursions"]
    sessions_ctx = result["sessions"]

    events = []
    for eid, e in enumerate(excursions, start=1):
        ctx = sessions_ctx.get(e["session"])
        samples, baseline = (ctx["samples"], ctx["baseline"]) if ctx else ([], [])
        seq_index = build_seq_index(samples) if samples else {}
        rng = excursion_sample_range(e, seq_index) if samples else None
        idxs = list(rng) if rng is not None else []

        if idxs:
            open_i = idxs[0]
            pwm_open = samples[open_i]["pwm_actual"]
            pwm_cmd_open = samples[open_i]["pwm_commanded"]
            dir_open = samples[open_i]["dir"]
            cont = continuity_ratio(samples, baseline, idxs, continuity_dead_zone)
        else:
            pwm_open = e["pwm_actual_at_peak"]
            pwm_cmd_open = e["pwm_commanded_at_peak"]
            dir_open = e["dir_at_peak"]
            cont = 0.0

        dev_sign = 1 if float(e["max_pos_flux"]) >= abs(float(e["max_neg_flux"])) else -1
        open_polarity = manifest.open_polarity_of(dev_sign)

        ev = {c: "" for c in EVENT_COLUMNS}
        ev.update({
            "event_id": eid, "capture": os.path.basename(capture_path), "session": e["session"],
            "excursion_id": e["excursion_id"],
            "phys_open_sample": e["start_sample"], "phys_close_sample": e["end_sample"],
            "phys_open_time_s": float(e["start_time_s"]), "phys_close_time_s": float(e["end_time_s"]),
            "phys_duration_ms": float(e["duration_ms"]),
            "phys_max_pos_flux": float(e["max_pos_flux"]), "phys_max_neg_flux": float(e["max_neg_flux"]),
            "phys_integrated_signed_flux": float(e["integrated_signed_flux_count_ms"]),
            "phys_integrated_abs_flux": float(e["integrated_abs_flux_count_ms"]),
            "phys_baseline_value": float(e["baseline_at_excursion"]),
            "phys_baseline_mode": e["baseline_mode"], "phys_baseline_n_quiet": e["baseline_n_quiet"],
            "phys_pre_range_counts": e["pre_range_counts"], "phys_pre_stdev_counts": e["pre_stdev_counts"],
            "phys_incomplete": int(e["incomplete"]), "phys_gaps_within_count": e["gaps_within_count"],
            "phys_gaps_near_count": e["gaps_near_count"], "phys_in_low_pwm_dwell": int(e["in_low_pwm_dwell"]),
            "phys_pwm_actual_at_open": pwm_open, "phys_pwm_commanded_at_open": pwm_cmd_open,
            "phys_direction": dir_open,
            "det_open_polarity": open_polarity, "det_continuity_ratio": "%.4f" % cont,
            "det_continuity_dead_zone": continuity_dead_zone,
        })
        events.append(ev)
    return events, sessions_ctx


# ---------------------------------------------------------------------------
# Gate pipeline (task part D)
# ---------------------------------------------------------------------------
def passes_morphology(ev, *, min_duration_ms, min_abs_flux):
    """Duration and absolute integrated flux only. continuity_ratio is
    diagnostic-only (see module docstring) and deliberately does NOT
    appear here."""
    if float(ev["phys_duration_ms"]) < min_duration_ms:
        return False
    if float(ev["phys_integrated_abs_flux"]) < min_abs_flux:
        return False
    return True


def evaluate_event(ev, prev, qmap, manifest, *, min_duration_ms, min_abs_flux,
                   legacy_continuity_max_ratio=None):
    """prev: dict with keys mm, polarity, accepted_time_s, or None if no
    marker has been accepted yet (seeded from the manifest's declared start
    before the first candidate is evaluated — see replay()). Returns a
    Disposition and, for ACCEPT_EXPECTED_MARKER, the new predicted mm.

    legacy_continuity_max_ratio: None (default) means continuity plays no
    part in this decision at all -- the corrected, current behaviour. A
    numeric value reproduces the EARLIER, WITHDRAWN gate exactly, for the
    sole purpose of the continuity-removal comparison report
    (tools/hwt_gate_replay_continuity_comparison.py) -- it must never be
    set by default production code, only by that explicit comparison."""
    if int(ev["phys_incomplete"]):
        return Disposition("REJECT_INCOMPLETE",
                           "excursion overlaps a transport gap, sampler stall, or session "
                           "boundary (gaps_within=%s) -- not authoritative" % ev["phys_gaps_within_count"]), None

    if not passes_morphology(ev, min_duration_ms=min_duration_ms, min_abs_flux=min_abs_flux):
        return Disposition("REJECT_SPIKE",
                           "duration=%.1fms abs_flux=%.1f did not clear the evaluation "
                           "morphology bar (>=%.0fms, >=%.0f)"
                           % (float(ev["phys_duration_ms"]), float(ev["phys_integrated_abs_flux"]),
                              min_duration_ms, min_abs_flux)), None

    if legacy_continuity_max_ratio is not None and \
            float(ev["det_continuity_ratio"]) > legacy_continuity_max_ratio:
        return Disposition("REJECT_SPIKE",
                           "[LEGACY COMPARISON PATH -- not used by the default pipeline; see "
                           "module docstring's CONTINUITY IS DIAGNOSTIC-ONLY section] "
                           "continuity=%.3f exceeded the since-withdrawn continuity gate's "
                           "threshold %.2f" % (float(ev["det_continuity_ratio"]),
                                                legacy_continuity_max_ratio)), None

    if prev is None:
        return Disposition("REVIEW_AMBIGUOUS",
                           "no accepted predecessor and no manifest-declared start -- "
                           "cannot be adjudicated without an operator declaration"), None

    elapsed_s = float(ev["phys_open_time_s"]) - prev["accepted_time_s"]
    min_time_s = qmap.spacing_between(prev["mm"], manifest.direction) / qmap.max_credible_speed_mm_s()
    too_soon = elapsed_s < min_time_s
    opposite = ev["det_open_polarity"] != prev["polarity"]

    ev["ctx_time_since_prev_accepted_s"] = "%.6f" % elapsed_s
    ev["ctx_min_time_to_next_marker_s"] = "%.6f" % min_time_s
    ev["ctx_prev_accepted_mm"] = prev["mm"]
    ev["ctx_prev_accepted_polarity"] = prev["polarity"]

    if too_soon and opposite:
        return Disposition("REJECT_PROBABLE_RETURN",
                           "elapsed=%.3fs < min-physical-time=%.3fs AND opposite polarity to "
                           "the previous accepted marker -- consistent with a same-magnet "
                           "return/secondary response" % (elapsed_s, min_time_s)), None
    if too_soon:
        return Disposition("REJECT_PHYSICALLY_TOO_SOON",
                           "elapsed=%.3fs < min-physical-time=%.3fs (spacing %d mm at "
                           "QUORUM's own max-credible-speed %.1f mm/s) -- the locomotive "
                           "could not physically have reached the next marker yet"
                           % (elapsed_s, min_time_s,
                              qmap.spacing_between(prev["mm"], manifest.direction),
                              qmap.max_credible_speed_mm_s())), None

    expected_mm = qmap.next_mm(prev["mm"], manifest.direction)
    expected_pol = qmap.dna_at(expected_mm)
    ev["map_expected_next_mm"] = expected_mm
    ev["map_expected_next_polarity"] = expected_pol

    if ev["det_open_polarity"] != expected_pol:
        return Disposition("REJECT_WRONG_EXPECTED_POLARITY",
                           "plausible curve, physically-timely, but polarity %s != expected "
                           "%s for marker %d -- \"No Way\": not advancing, still waiting for "
                           "marker %d" % (polchar(ev["det_open_polarity"]), polchar(expected_pol),
                                          expected_mm, expected_mm)), None

    return Disposition("ACCEPT_EXPECTED_MARKER",
                       "plausible curve, physically-timely, expected polarity %s matched "
                       "at marker %d" % (polchar(expected_pol), expected_mm)), expected_mm


def polchar(p):
    return "N" if p else "S"


# ---------------------------------------------------------------------------
# Replay loop (task part C.5-C.7)
# ---------------------------------------------------------------------------
def replay(events, qmap, manifest, *, min_duration_ms, min_abs_flux,
          legacy_continuity_max_ratio=None):
    """legacy_continuity_max_ratio: see evaluate_event()'s docstring --
    leave None for the corrected, current pipeline."""
    prev = {"mm": manifest.start_mm, "polarity": qmap.dna_at(manifest.start_mm),
           "accepted_time_s": manifest.start_time_s}
    predicted_mm = manifest.start_mm
    wrong_polarity_streak = 0
    streak_reports = []

    for ev in events:
        ev["disp_predicted_mm_before"] = predicted_mm
        disp, new_mm = evaluate_event(
            ev, prev, qmap, manifest, min_duration_ms=min_duration_ms,
            min_abs_flux=min_abs_flux, legacy_continuity_max_ratio=legacy_continuity_max_ratio)
        ev["disp_final"] = disp.name
        ev["disp_reason"] = disp.reason

        if disp.name == "ACCEPT_EXPECTED_MARKER":
            predicted_mm = new_mm
            prev = {"mm": new_mm, "polarity": ev["det_open_polarity"],
                   "accepted_time_s": float(ev["phys_open_time_s"])}
            wrong_polarity_streak = 0
        elif disp.name == "REJECT_WRONG_EXPECTED_POLARITY":
            wrong_polarity_streak += 1
            if wrong_polarity_streak == qmap.quorum_trigger:
                streak_reports.append(
                    "event_id=%s: %d REJECT_WRONG_EXPECTED_POLARITY in a row -- under the "
                    "operator's model this is where repeated credible contradiction would "
                    "reduce confidence / stop navigation / request an operator declaration "
                    "(report-only; no new recovery policy implemented here)"
                    % (ev["event_id"], wrong_polarity_streak))
        # every other disposition (REJECT_SPIKE, REJECT_INCOMPLETE,
        # REJECT_PHYSICALLY_TOO_SOON, REJECT_PROBABLE_RETURN, REVIEW_AMBIGUOUS)
        # leaves prev/predicted_mm/wrong_polarity_streak untouched: rejected
        # observations are never retained as candidate positions.

        ev["disp_predicted_mm_after"] = predicted_mm

    return events, streak_reports


# ---------------------------------------------------------------------------
def write_summary(events, manifest, streak_reports, fh):
    p = lambda s: print(s, file=fh)
    p("HALL_WAVEFORM_TEST gate-replay summary (investigatory)")
    p("  manifest                 %s" % manifest.path)
    p("  capture                  %s" % manifest.capture)
    p("  direction                %s (%+d)" % (manifest.direction_label, manifest.direction))
    p("  declared start            mm=%d t=%.3fs  confirmed=%s"
      % (manifest.start_mm, manifest.start_time_s, manifest.start_confirmed))
    if manifest.start_note:
        p("    note: %s" % manifest.start_note)
    p("  polarity convention      positive deviation = %s  confirmed=%s"
      % (manifest.positive_deviation_is, manifest.polarity_convention_confirmed))
    if manifest.polarity_convention_note:
        p("    note: %s" % manifest.polarity_convention_note)
    p("  total candidate events    %d" % len(events))

    by_disp = {}
    for ev in events:
        by_disp.setdefault(ev["disp_final"], []).append(ev)
    p("")
    p("  --- disposition counts (this is NOT a claim of accurate detection --")
    p("      see part E of the report for what would validate these) ---")
    for name in DISPOSITIONS:
        p("    %-32s %d" % (name, len(by_disp.get(name, []))))

    accepted = by_disp.get("ACCEPT_EXPECTED_MARKER", [])
    p("")
    p("  predicted position: start=%d -> end=%d (%d markers advanced)"
      % (manifest.start_mm, events[-1]["disp_predicted_mm_after"] if events else manifest.start_mm,
         len(accepted)))

    if streak_reports:
        p("")
        p("  --- repeated-disagreement streaks (report-only) ---")
        for s in streak_reports:
            p("  " + s)

    if manifest.anchors:
        p("")
        p("  --- operator anchors in this manifest (for cross-checking, not fed to the gate) ---")
        for a in manifest.anchors:
            p("    t=%s  %s" % (a.get("time_s"), a.get("text", "")))

    if manifest.uncertainty_notes:
        p("")
        p("  --- uncertainty notes (from the manifest; not resolved by this tool) ---")
        for n in manifest.uncertainty_notes:
            p("    - %s" % n)


# ---------------------------------------------------------------------------
def plot_event_detail(samples, baseline, ev, out_path, margin_s, capture_label):
    try:
        import matplotlib
        matplotlib.use("Agg")
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib not available -- skipping plot", file=sys.stderr)
        return False

    t_lo = float(ev["phys_open_time_s"]) - margin_s
    t_hi = float(ev["phys_close_time_s"]) + margin_s
    idx_win = [i for i, s in enumerate(samples) if t_lo <= s["t_s"] <= t_hi]
    if not idx_win:
        return False
    t0 = samples[idx_win[0]]["t_s"]
    ts = [samples[i]["t_s"] - t0 for i in idx_win]
    raw = [samples[i]["raw"] for i in idx_win]
    base = [baseline[i] for i in idx_win]
    x_lo = float(ev["phys_open_time_s"]) - t0
    x_hi = float(ev["phys_close_time_s"]) - t0

    fig, ax = plt.subplots(figsize=(9, 4.5))
    ax.plot(ts, raw, lw=0.9, color="#1f77b4", label="raw ADC")
    ax.plot(ts, base, lw=1.1, color="#2ca02c", ls="--", label="frozen baseline")
    ax.axvspan(x_lo, x_hi, color="#ff7f0e", alpha=0.15)
    ax.axvline(x_lo, color="#ff7f0e", lw=1.2)
    ax.axvline(x_hi, color="#8c564b", lw=1.2)
    ax.set_ylabel("Hall A raw ADC counts")
    ax.set_xlabel("time (s), window start = %.6f s firmware time" % t0)
    ax.legend(fontsize=7, loc="best")
    ax.grid(alpha=0.25)

    info = (
        "%s   event #%s   excursion #%s   [%s]\n"
        "sample %s..%s   dur=%.1fms   pol=%s   continuity=%s\n"
        "abs_flux=%.1f   pwm_open=%s   prev_mm=%s -> expected_mm=%s\n"
        "%s"
        % (capture_label, ev["event_id"], ev["excursion_id"], ev["disp_final"],
           ev["phys_open_sample"], ev["phys_close_sample"], float(ev["phys_duration_ms"]),
           polchar(ev["det_open_polarity"]), ev["det_continuity_ratio"],
           float(ev["phys_integrated_abs_flux"]), ev["phys_pwm_actual_at_open"],
           ev["ctx_prev_accepted_mm"], ev["map_expected_next_mm"], ev["disp_reason"]))
    ax.set_title(info, fontsize=7, family="monospace", loc="left")
    fig.tight_layout()
    fig.savefig(out_path, dpi=130)
    plt.close(fig)
    return True


# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("manifest", help="run manifest JSON (see tools/manifests/*.json)")
    ap.add_argument("-o", "--out", help="decision CSV output")
    ap.add_argument("--summary", help="summary report output")
    ap.add_argument("--quorum-ino", default="firmware/QUORUM/QUORUM.ino",
                    help="path to QUORUM.ino to extract the map/constants from")
    ap.add_argument("--min-duration-ms", type=float, default=40.0,
                    help="[evaluation only] morphology bar: minimum duration (default 40, "
                         "matching QUORUM's own EVENT_FLOOR_MS)")
    ap.add_argument("--min-abs-flux", type=float, default=300.0,
                    help="[evaluation only] morphology bar: minimum integrated |flux| (default 300)")
    ap.add_argument("--continuity-dead-zone", type=float, default=20.0,
                    help="DIAGNOSTIC ONLY, does not affect any disposition (see module "
                         "docstring's CONTINUITY IS DIAGNOSTIC-ONLY section): counts below "
                         "this sample-to-sample delta are not counted as a direction change "
                         "at all, when computing the reported det_continuity_ratio field "
                         "(default 20; not derived from an independently measured delta "
                         "distribution -- see tools/hwt_adc_delta_diagnostics.py)")
    ap.add_argument("--legacy-continuity-max-ratio", type=float, default=None,
                    help="FOR COMPARISON ONLY: reproduces the earlier, withdrawn continuity "
                         "gate exactly, rejecting as REJECT_SPIKE any event whose "
                         "det_continuity_ratio exceeds this. Omit (default) for the "
                         "corrected pipeline, where continuity plays no part in any "
                         "decision. See tools/hwt_gate_replay_continuity_comparison.py")
    ap.add_argument("--plot-dir", help="write representative detail PNGs into subdirectories by disposition")
    ap.add_argument("--plot-margin-s", type=float, default=0.3)
    ap.add_argument("--plot-max-per-disposition", type=int, default=6,
                    help="representative sample size per disposition (default 6; "
                         "this tool asks for REPRESENTATIVE plots per disposition, "
                         "not exhaustive coverage -- found/plotted counts are always "
                         "printed so truncation is visible, not silent)")
    args = ap.parse_args()

    manifest = RunManifest(args.manifest)
    qmap = QuorumMap(args.quorum_ino)

    capture_dir = os.path.dirname(os.path.abspath(args.manifest))
    capture_path = manifest.capture
    if not os.path.isabs(capture_path) and not os.path.exists(capture_path):
        # resolve relative to the manifest's own directory as a convenience
        alt = os.path.join(capture_dir, capture_path)
        if os.path.exists(alt):
            capture_path = alt

    events, sessions_ctx = build_acquisition_events(
        capture_path, manifest, continuity_dead_zone=args.continuity_dead_zone)
    events, streak_reports = replay(
        events, qmap, manifest, min_duration_ms=args.min_duration_ms,
        min_abs_flux=args.min_abs_flux,
        legacy_continuity_max_ratio=args.legacy_continuity_max_ratio)

    base = os.path.splitext(os.path.basename(capture_path))[0]
    out = args.out or base + "_decisions.csv"
    with open(out, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=EVENT_COLUMNS)
        w.writeheader()
        w.writerows(events)
    print("wrote %s (%d events)" % (out, len(events)))

    summary_path = args.summary or base + "_gate_summary.txt"
    with open(summary_path, "w") as fh:
        write_summary(events, manifest, streak_reports, fh)
    write_summary(events, manifest, streak_reports, sys.stdout)
    print("wrote %s" % summary_path)

    if args.plot_dir:
        label = os.path.basename(capture_path)
        by_disp = {}
        for ev in events:
            by_disp.setdefault(ev["disp_final"], []).append(ev)
        cap = args.plot_max_per_disposition
        for disp_name in DISPOSITIONS:
            evs = by_disp.get(disp_name, [])
            if not evs:
                print("disposition %-32s : none found" % disp_name)
                continue
            # Evenly spaced through the chronological list, not just the
            # first N, so the representative sample spans the whole run
            # rather than clustering wherever the disposition first appears.
            if cap is None or len(evs) <= cap:
                sample = evs
            else:
                step = len(evs) / cap
                sample = [evs[int(i * step)] for i in range(cap)]
            if len(sample) < len(evs):
                print("disposition %-32s : %d found, plotting %d representative (capped by "
                     "--plot-max-per-disposition)" % (disp_name, len(evs), len(sample)))
            else:
                print("disposition %-32s : %d found, plotting %d" % (disp_name, len(evs), len(sample)))
            out_dir = os.path.join(args.plot_dir, disp_name)
            os.makedirs(out_dir, exist_ok=True)
            for ev in sample:
                ctx = sessions_ctx.get(ev["session"])
                if ctx is None:
                    continue
                out_path = os.path.join(out_dir, "event_%04d.png" % int(ev["event_id"]))
                if plot_event_detail(ctx["samples"], ctx["baseline"], ev, out_path,
                                     args.plot_margin_s, label):
                    print("  wrote %s" % out_path)

    return 0


if __name__ == "__main__":
    sys.exit(main())
