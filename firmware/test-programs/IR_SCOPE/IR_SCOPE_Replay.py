#!/usr/bin/env python3
"""
IR_SCOPE_Replay.py
Ninobur Garden Railway — offline falling-threshold replay over a recorded
IR_SCOPE CSV. Companion to IR_SCOPE_Plotter.py.

PURPOSE
    Answer: would a different falling threshold recover seven pulses per
    revolution without introducing false edges? The rising rule stays fixed
    at the production expression (thrHigh = runMin + 2*span/3); candidate
    falling thresholds thrLow = runMin + frac*span are replayed over the
    SAME recorded raw waveform with the REAL detector semantics:

      - contrast validity gate (recorded contrast_valid column; a loss
        discards the open pulse and the interval anchor, as IR_DIAG does)
      - 15 ms rise-to-rise debounce (DEBOUNCE_US)
      - 2500 ms latch timeout, discarding the open pulse (LATCH_TIMEOUT_MS)

    This is NOT a threshold-crossing counter. It is the IR_DIAG state
    machine run sample-by-sample, with every pulse episode tracked to an
    explicit OUTCOME (completed / latch-discarded / contrast-discarded /
    gap-interrupted / open at end) so nothing disappears silently.

    The recorded runMin/runMax envelope is replayed as logged — the envelope
    is threshold-independent (percentiles of raw), so recorded bounds remain
    exact for every candidate.

DISCONTINUITY SEMANTICS (matches what physically happened)
    SESSION row   firmware reboot: full detector reset.
    MISSED row    the firmware sampler skipped slots but the detector ran
                  continuously: ALL replay state is carried across (times
                  are absolute, so debounce and latch timing stay correct).
    GAP row       batches lost in transport: the firmware detector ran but
                  its samples are missing. The open replay pulse is closed
                  as `gap_interrupted` (never silently dropped) and the
                  interval anchor is cleared. After the gap the candidate's
                  state is UNKNOWN and is re-established from the waveform
                  for THIS candidate's own thresholds — never from the
                  recorded 1/3 detector, whose state is only exact for the
                  1/3 candidate (a higher thrLow may have fallen inside the
                  gap where 1/3 did not). The resync point is certain by
                  construction: at the first sample with raw < thrLow
                  (candidate), or at a contrast loss, EVERY variant of the
                  detector is idle regardless of its history. The unknown
                  stretch is reported as a `resync_unknown` episode (`unkn`
                  column), excluded from all statistics, and drawn grey in
                  the overlay — never guessed, never silently dropped.
    An unmarked sample-number jump is treated as a GAP (conservative).

CANDIDATES
    1/3 (current, exact integer span/3 arithmetic), 0.40, 0.50, 0.60,
    plus any extras via --frac. The 1/3 replay is also validated against the
    recorded rise/fall flags: it should reproduce the firmware's own edges,
    which proves the replay semantics before the other candidates are read.

USAGE
    python3 IR_SCOPE_Replay.py capture.csv
    python3 IR_SCOPE_Replay.py capture.csv --frac 0.45
    python3 IR_SCOPE_Replay.py capture.csv --plot 0.50 --start 12.0 --duration 4.0
    python3 IR_SCOPE_Replay.py capture.csv --plot 0.50 --save overlay.png

    --plot overlays one candidate on the recorded waveform: recorded
    thresholds and recorded inPulse band (grey) against the candidate's
    replayed episodes, fall edges, and thrLow line — so you can see exactly
    which troughs produce a fall edge at that candidate.

REQUIREMENTS
    pip install matplotlib     (only needed for --plot)
"""

import argparse
import bisect
import csv
import math
import sys

DEBOUNCE_MS = 15.0        # DEBOUNCE_US, IR_DIAG
LATCH_MS    = 2500.0      # LATCH_TIMEOUT_MS, IR_DIAG
SPOKES      = 7

SHORT_WIDTH_FRACTION = 0.4    # width < 0.4 x local median width => implausibly short
MERGED_RATIO         = 1.7    # interval >= 1.7 x LOCAL base => >=1 swallowed spoke

# Merged/short classification is judged against a LOCAL base, not a pooled
# one: a hand-pushed wheel changes speed continuously, and against a pooled
# p25 every slow-phase interval would read as "merged" while fast-phase
# merges could hide below it. The local base is the 25th percentile of the
# +/-LOCAL_HALF neighbouring intervals (low percentile because a swallowed
# spoke can only INFLATE an interval, never shrink it — so even in a run
# with many merges the local p25 sits on the single-spoke time). Intervals
# without enough neighbours are reported as unclassified, never guessed.
LOCAL_HALF  = 10              # neighbours each side (window of up to 21)
LOCAL_MIN_N = 5               # minimum window population to classify

# INTERVAL RATIOS ARE SCALE-FREE and therefore blind to UNIFORM distortion:
# if every pulse merges exactly two spokes (or every spoke doubles), all
# intervals shift together, every ratio reads 1.0, and the table above
# reports a healthy 7.00 pulses/rev. The waveform STRUCTURE is not blind
# to merging: a merged pulse physically contains two reflective peaks
# separated by the trough that failed to cross thrLow. So a
# threshold-independent peak counter runs over the raw trace — a peak is a
# local maximum above the band midpoint from which the signal falls by at
# least PEAK_PROM_FRAC x span before the next one (hysteresis/prominence
# tracking, --prom to adjust). A completed pulse containing >= 2 peaks is
# direct merged-spoke evidence (mpk column) independent of intervals.
#
# p/7pk = 7 x pulses / peaks is APPARENT PULSES PER SEVEN PEAKS — it is
# NOT a physical-revolution measurement, because its denominator comes
# from the same waveform it judges: if each spoke presents two optical
# features (the historic bare-spoke edge-doubling), peaks double along
# with pulses and p/7pk still reads a healthy 7.00. The ONLY absolute
# figure is pulses per OPERATOR-MARKED revolution: MARKER rows whose text
# contains --rev-marker (default "rev"), pressed once per hand-turned
# revolution per the README protocol, give p/rev-mk — and peaks per
# marked revolution directly tests the one-peak-per-spoke assumption.
# Without markers the report states that the absolute figure is
# unavailable; it never presents p/7pk as physical truth.
PEAK_PROM_FRAC = 0.25         # prominence, as a fraction of live span
REV_MARKER_DEFAULT = "rev"    # marker-text substring marking one revolution


# ============================================================================
# CSV LOADING — sessions of contiguous segments with TYPED boundaries.
# ============================================================================
def new_seg():
    return {"t": [], "raw": [], "mn": [], "mx": [], "cv": [],
            "rec_ip": [], "rec_rise": [], "rec_fall": [],
            "rec_hi": [], "rec_lo": []}


def load_sessions(path):
    """Returns a list of sessions; each is
    {"segments": [seg, ...], "boundaries": [kind, ...]} where boundaries[i]
    ("GAP" or "MISSED") sits between segments[i] and segments[i+1]."""
    sessions = []
    state = {"session": None, "seg": None, "pending": None, "last": None}

    def close_seg():
        if state["seg"] is not None and state["seg"]["t"]:
            state["session"]["segments"].append(state["seg"])
        state["seg"] = None

    def close_session():
        if state["session"] is not None:
            close_seg()
            if state["session"]["segments"]:
                sessions.append(state["session"])
        state["session"] = None
        state["pending"] = None
        state["last"] = None

    with open(path, newline="") as f:
        rd = csv.DictReader(f)
        for row in rd:
            rt = row.get("row_type", "")
            if rt == "SESSION":
                close_session()
                continue
            if rt == "GAP":
                if state["session"] is not None:
                    close_seg()
                state["pending"] = "GAP"          # GAP outranks MISSED
                state["last"] = None
                continue
            if rt == "MISSED":
                if state["session"] is not None:
                    close_seg()
                if state["pending"] != "GAP":
                    state["pending"] = "MISSED"
                state["last"] = None
                continue
            if rt == "MARKER":
                # operator markers, kept with their session — revolution
                # markers are the only source of absolute revolution truth
                try:
                    tm = float(row["t_s"]) * 1000.0
                except (KeyError, TypeError, ValueError):
                    continue
                if state["session"] is None:
                    state["session"] = {"segments": [], "boundaries": [],
                                        "markers": []}
                state["session"]["markers"].append((tm, row.get("info", "")))
                continue
            if rt != "SAMPLE":
                continue
            try:
                sample = int(row["sample"])
                vals = (int(row["raw"]), int(row["run_min"]),
                        int(row["run_max"]), int(row["contrast_valid"]),
                        int(row["in_pulse"]), int(row["rise"]),
                        int(row["fall"]), int(row["thr_high"]),
                        int(row["thr_low"]))
            except (KeyError, ValueError):
                continue
            if state["session"] is None:
                state["session"] = {"segments": [], "boundaries": [],
                                    "markers": []}
            if (state["seg"] is not None and state["last"] is not None
                    and sample != state["last"] + 1):
                close_seg()              # unmarked jump: conservative GAP
                if state["pending"] is None:
                    state["pending"] = "GAP"
            if state["seg"] is None:
                state["seg"] = new_seg()
                if state["session"]["segments"]:
                    state["session"]["boundaries"].append(
                        state["pending"] or "GAP")
                state["pending"] = None
            seg = state["seg"]
            raw, mn, mx, cv, ip, rise, fall, hi, lo = vals
            seg["t"].append(sample)      # ms; sample number IS ms
            seg["raw"].append(raw)
            seg["mn"].append(mn)
            seg["mx"].append(mx)
            seg["cv"].append(cv)
            seg["rec_ip"].append(ip)
            seg["rec_rise"].append(rise)
            seg["rec_fall"].append(fall)
            seg["rec_hi"].append(hi)
            seg["rec_lo"].append(lo)
            state["last"] = sample
    close_session()
    return sessions


# ============================================================================
# DETECTOR REPLAY — IR_DIAG semantics on recorded raw + envelope.
# Every pulse becomes an EPISODE with an explicit outcome:
#   completed        rise and fall both replayed
#   latch            discarded by the 2500 ms latch timeout (no fall edge)
#   closs            discarded by a contrast-validity loss (no fall edge)
#   gap_interrupted  open when a transport gap began; its end is unknown
#   open_end         still open when the record ends
#   resync_unknown   post-gap stretch where the candidate's state could not
#                    yet be established (see below); excluded from stats
# ============================================================================
def thr_low_of(mn, span, frac):
    if frac == "1/3":
        return mn + span // 3            # firmware's exact integer arithmetic
    return mn + int(span * frac)         # floor, matching integer truncation


def replay_session(session, frac):
    episodes = []
    closs_count = [0]
    resync_spans = []
    st = {"in_pulse": False, "last_edge": -1e12,
          "pulse_start": 0.0, "prev_rise": None,
          "unknown": False, "resync_start": None, "resync_activity": False,
          "resync_anchor": 0.0, "resync_below_lo": False,
          "resync_prev_above_hi": False}

    def close(t_end, outcome, width=None, interval=None):
        episodes.append({"t_rise": st["pulse_start"], "t_end": t_end,
                         "outcome": outcome,
                         "width": width, "interval": interval})

    def record_unknown_span(t_end):
        # EVERY unknown span is recorded and rendered — a quiet one (never
        # above thrHigh) is still a stretch where the candidate's state was
        # not established, and hiding it would contradict the documented
        # contract. Activity rides along as metadata. Also called when a
        # NEW gap arrives while still unknown, so no span is ever dropped.
        if st["resync_start"] is None:
            return
        episodes.append({"t_rise": st["resync_start"], "t_end": t_end,
                         "outcome": "resync_unknown",
                         "activity": st["resync_activity"],
                         "width": None, "interval": None})
        resync_spans.append((st["resync_start"], t_end))
        st["resync_start"] = None
        st["resync_activity"] = False
        st["resync_below_lo"] = False
        st["resync_prev_above_hi"] = False

    def end_unknown(t_end):
        # The candidate's state AND timing are pinned from here: raw is
        # below thrLow (or contrast collapsed), so every variant of the
        # detector has fallen or discarded — and the debounce horizon of
        # the latest POSSIBLE unobserved rise has expired, so no future
        # rise can be one the real detector would suppress.
        anchor = st["resync_anchor"]
        record_unknown_span(t_end)
        st["unknown"] = False
        st["in_pulse"] = False
        # The latest possible real-detector rise (resync_anchor) bounds
        # the debounce exactly-or-conservatively, never permissively.
        st["last_edge"] = anchor
        st["prev_rise"] = None           # nothing anchors across the gap

    for si, seg in enumerate(session["segments"]):
        if si > 0:
            kind = session["boundaries"][si - 1]
            if kind == "GAP":
                seg_end = float(session["segments"][si - 1]["t"][-1])
                if st["unknown"]:
                    # a new gap before the previous resync resolved: the
                    # pending span is still evidence — record it, never drop
                    record_unknown_span(seg_end)
                if st["in_pulse"]:
                    # end unknown — close explicitly, never silently
                    close(seg_end, "gap_interrupted")
                    st["in_pulse"] = False
                st["prev_rise"] = None    # nothing anchors across a gap
                # Candidate state after the gap is UNKNOWN and is
                # re-established from the waveform for THIS candidate's own
                # thresholds — never from the recorded 1/3 detector, whose
                # state is exact only for the 1/3 candidate.
                st["unknown"] = True
                st["resync_start"] = None
                st["resync_activity"] = False
            # kind == "MISSED": the detector ran continuously — carry all
            # state; absolute times keep debounce and latch arithmetic honest

        for i in range(len(seg["t"])):
            t = float(seg["t"][i])
            raw = seg["raw"][i]
            mn = seg["mn"][i]
            mx = seg["mx"][i]
            cv = seg["cv"][i]
            span = mx - mn

            # --- post-gap resynchronization -------------------------------
            # A below-thrLow sample pins in_pulse=false, but NOT timing: a
            # rise the real detector accepted may have happened inside the
            # gap (or during this unknown stretch) less than DEBOUNCE_MS
            # before a later crossing, which the real detector would then
            # debounce away while a naive replay would emit it. So the
            # unknown period ends only at a sample that is BOTH below
            # thrLow AND more than DEBOUNCE_MS after the latest possible
            # rise — tracked as resync_last_high: initialized to the first
            # post-gap sample time (an unobserved in-gap rise can be no
            # later than the gap's end) and advanced by every above-thrHigh
            # sample seen while unknown. Until both conditions hold, edges
            # stay unattributed inside the resync span.
            if st["unknown"]:
                if st["resync_start"] is None:
                    st["resync_start"] = t
                    st["resync_anchor"] = t        # in-gap rise <= gap end
                    st["resync_below_lo"] = False
                    st["resync_prev_above_hi"] = False
                if not cv:
                    # contrast loss discards any open pulse in the firmware:
                    # state is certainly idle
                    end_unknown(t)
                    continue
                hi = mn + (span * 2) // 3
                lo = thr_low_of(mn, span, frac)
                above_hi = raw > hi
                if above_hi:
                    st["resync_activity"] = True   # activity we cannot
                                                   # attribute — report it
                    # TWO-PHASE ANCHOR (the naive "last above-thrHigh
                    # sample" deadlocks on high-duty signals — an 84% duty
                    # trough never sits 15 ms clear of the plateau, so
                    # post-gap segments would never resolve). Until the
                    # first below-thrLow observation, ANY above-hi sample
                    # could be a real-detector rise (an in-gap pulse could
                    # even latch-abort and re-arm mid-plateau), so the
                    # anchor tracks every above-hi sample. The first
                    # below-thrLow sample provably idles every detector
                    # variant; from then on a rise can only fire at an
                    # upward thrHigh CROSSING, so only crossings advance
                    # the anchor — and the next trough resolves.
                    if (not st["resync_below_lo"]
                            or not st["resync_prev_above_hi"]):
                        st["resync_anchor"] = t
                if raw < lo:
                    st["resync_below_lo"] = True
                    if (t - st["resync_anchor"]) > DEBOUNCE_MS:
                        end_unknown(t)
                st["resync_prev_above_hi"] = above_hi
                continue

            # contrast gate — discard branch, as IR_DIAG
            if not cv:
                if st["in_pulse"] or st["prev_rise"] is not None:
                    closs_count[0] += 1
                    if st["in_pulse"]:
                        close(t, "closs")
                    st["in_pulse"] = False
                    st["prev_rise"] = None
                continue

            hi = mn + (span * 2) // 3
            lo = thr_low_of(mn, span, frac)

            # latch timeout — pulse discarded, no fall edge, anchor cleared
            if st["in_pulse"] and (t - st["pulse_start"]) > LATCH_MS:
                close(t, "latch")
                st["in_pulse"] = False
                st["prev_rise"] = None

            if not st["in_pulse"]:
                if raw > hi and (t - st["last_edge"]) > DEBOUNCE_MS:
                    st["in_pulse"] = True
                    st["last_edge"] = t
                    st["pulse_start"] = t
            else:
                if raw < lo:
                    width = t - st["pulse_start"]
                    interval = ((st["pulse_start"] - st["prev_rise"])
                                if st["prev_rise"] is not None else None)
                    close(t, "completed", width, interval)
                    st["prev_rise"] = st["pulse_start"]
                    st["in_pulse"] = False

    last_t = float(session["segments"][-1]["t"][-1])
    if st["unknown"]:
        end_unknown(last_t)
    elif st["in_pulse"]:
        close(last_t, "open_end")

    return {"episodes": episodes, "closs": closs_count[0],
            "resync_spans": resync_spans}


def flat_episodes(results):
    return [ep for r in results for ep in r["episodes"]]


def usable_rev_windows(session, substr):
    """Consecutive revolution-marker pairs [(t0, t1), ...] with markers
    whose text contains substr (case-insensitive). Windows that span a
    transport-gap boundary are excluded (their pulse/peak counts would be
    undercounts) and reported separately."""
    ts = sorted(t for (t, txt) in session["markers"]
                if substr in txt.lower())
    gap_starts = [session["segments"][i + 1]["t"][0]
                  for i, b in enumerate(session["boundaries"]) if b == "GAP"]
    wins, excluded = [], 0
    for i in range(len(ts) - 1):
        a, b = ts[i], ts[i + 1]
        if any(a <= g <= b for g in gap_starts):
            excluded += 1
        else:
            wins.append((a, b))
    return wins, excluded


# ============================================================================
# WAVEFORM STRUCTURE — candidate-independent physical peak count.
# ============================================================================
def physical_peaks(session, prom_frac):
    """Hysteresis/prominence peak tracker over the raw trace. A peak is a
    local maximum above the band midpoint from which the signal
    subsequently falls by at least prom_frac*span; the tracker re-arms
    when it rises by the same amount off the following minimum. Resets at
    GAP boundaries and contrast-invalid stretches (structure unjudgeable
    there); continues across MISSED (the wheel kept turning, a few-ms hole
    does not break a spoke summit). Returns [(t_ms, value), ...]."""
    peaks = []

    def fresh():
        return {"mode": "up", "cmax": -1, "cmax_t": 0.0, "cmin": 1 << 14}

    tr = fresh()
    for si, seg in enumerate(session["segments"]):
        if si > 0 and session["boundaries"][si - 1] == "GAP":
            tr = fresh()
        for i in range(len(seg["t"])):
            if not seg["cv"][i]:
                tr = fresh()
                continue
            t = float(seg["t"][i])
            raw = seg["raw"][i]
            mn = seg["mn"][i]
            span = seg["mx"][i] - mn
            prom = max(50, int(span * prom_frac))
            mid = mn + span // 2
            if tr["mode"] == "up":
                if raw > tr["cmax"]:
                    tr["cmax"] = raw
                    tr["cmax_t"] = t
                elif raw < tr["cmax"] - prom:
                    if tr["cmax"] > mid:      # bright-side peaks only: spokes
                        peaks.append((tr["cmax_t"], tr["cmax"]))
                    tr["mode"] = "down"
                    tr["cmin"] = raw
            else:
                if raw < tr["cmin"]:
                    tr["cmin"] = raw
                elif raw > tr["cmin"] + prom:
                    tr["mode"] = "up"
                    tr["cmax"] = raw
                    tr["cmax_t"] = t
    return peaks


# ============================================================================
# METRICS
# ============================================================================
def pct(sorted_vals, p):
    if not sorted_vals:
        return float("nan")
    k = min(len(sorted_vals) - 1, int(len(sorted_vals) * p / 100))
    return sorted_vals[k]


def local_pct(vals, i, p):
    """Percentile over the chronological window vals[i-LOCAL_HALF ..
    i+LOCAL_HALF]; None when the window is too thin to judge."""
    lo = max(0, i - LOCAL_HALF)
    hi = min(len(vals), i + LOCAL_HALF + 1)
    win = sorted(vals[lo:hi])
    if len(win) < LOCAL_MIN_N:
        return None
    return pct(win, p)


def analyse(results, peaks_by_session=None, rev_windows_by_session=None):
    eps = flat_episodes(results)
    done = [ep for ep in eps if ep["outcome"] == "completed"]
    widths = sorted(ep["width"] for ep in done)
    intervals_sorted = sorted(ep["interval"] for ep in done
                              if ep["interval"] is not None
                              and ep["interval"] > 0)

    out = {
        "n_pulses": len([ep for ep in eps if ep["outcome"] == "completed"]),
        "latches": len([ep for ep in eps if ep["outcome"] == "latch"]),
        "open": len([ep for ep in eps
                     if ep["outcome"] in ("open_end", "gap_interrupted")]),
        "resync": len([ep for ep in eps if ep["outcome"] == "resync_unknown"]),
        "closs": sum(r["closs"] for r in results),
        "w_med": pct(widths, 50), "w_p10": pct(widths, 10),
        "w_p90": pct(widths, 90),
        "w_min": widths[0] if widths else float("nan"),
        "w_max": widths[-1] if widths else float("nan"),
        "i_med": pct(intervals_sorted, 50),
        "i_min": intervals_sorted[0] if intervals_sorted else float("nan"),
        "i_max": intervals_sorted[-1] if intervals_sorted else float("nan"),
        "merged": 0, "short_pulses": 0, "unclassified": 0,
        "spoke_passages": 0, "n_ratio_intervals": 0,
        "pulses_per_rev": float("nan"),
        "base_interval": float("nan"),
        "multi_peak": 0,
        "p_per_7pk": float("nan"),
        "p_rev_marked": float("nan"),
        "duty_med": float("nan"),
    }

    # Per-pulse duty cycle: merged pulses carry the swallowed trough inside
    # the width, so duty rises well above the ~50% of a clean spoke — a
    # scale-free indicator that survives uniform distortion.
    duties = sorted(100.0 * ep["width"] / ep["interval"] for ep in done
                    if ep["interval"] is not None and ep["interval"] > 0)
    if duties:
        out["duty_med"] = pct(duties, 50)

    # Waveform-structure comparison (candidate-independent peaks). p_per_7pk
    # is apparent pulses per seven peaks — structural, NOT a revolution
    # measurement (see the header comment).
    if peaks_by_session is not None:
        n_peaks = sum(len(p) for p in peaks_by_session)
        if n_peaks > 0 and done:
            out["p_per_7pk"] = SPOKES * len(done) / n_peaks
        for r, peaks in zip(results, peaks_by_session):
            pt = [p[0] for p in peaks]        # sorted by construction
            for ep in r["episodes"]:
                if ep["outcome"] != "completed":
                    continue
                k = (bisect.bisect_right(pt, ep["t_end"])
                     - bisect.bisect_left(pt, ep["t_rise"]))
                if k >= 2:
                    out["multi_peak"] += 1

    # The only ABSOLUTE pulses/revolution figure: operator-marked windows.
    if rev_windows_by_session is not None:
        per_rev = []
        for r, wins in zip(results, rev_windows_by_session):
            comp = sorted(ep["t_rise"] for ep in r["episodes"]
                          if ep["outcome"] == "completed")
            for (a, b) in wins:
                per_rev.append(float(bisect.bisect_left(comp, b)
                                     - bisect.bisect_left(comp, a)))
        if per_rev:
            out["p_rev_marked"] = pct(sorted(per_rev), 50)

    # Locality is judged per session and in chronological order — speed at
    # a hand-pushed wheel is only comparable to its neighbours in time.
    local_bases = []
    for r in results:
        ses_done = [ep for ep in r["episodes"]
                    if ep["outcome"] == "completed"]
        # widths: short vs the local median width
        ws = [ep["width"] for ep in ses_done]
        for i, w in enumerate(ws):
            ref = local_pct(ws, i, 50)
            if ref is None:
                ref = out["w_med"]      # thin record: pooled median fallback
            if not (isinstance(ref, float) and math.isnan(ref)):
                if w < max(3.0, SHORT_WIDTH_FRACTION * ref):
                    out["short_pulses"] += 1
        # intervals: merged vs the LOCAL base
        ivs = [ep["interval"] for ep in ses_done
               if ep["interval"] is not None and ep["interval"] > 0]
        for i, iv in enumerate(ivs):
            base = local_pct(ivs, i, 25)
            if base is None or base <= 0:
                out["unclassified"] += 1
                continue
            local_bases.append(base)
            ratio = iv / base
            if ratio >= MERGED_RATIO:
                out["merged"] += 1
            out["spoke_passages"] += max(1, int(round(ratio)))
            out["n_ratio_intervals"] += 1

    if local_bases:
        out["base_interval"] = pct(sorted(local_bases), 50)
    if out["spoke_passages"] > 0:
        out["pulses_per_rev"] = (SPOKES * out["n_ratio_intervals"]
                                 / out["spoke_passages"])
    return out


def validate_against_recording(sessions, results):
    """1/3 replay vs the firmware's own recorded edges (tolerance +/-2 ms).
    Recorded edges inside the 1/3 candidate's resync spans are excluded:
    the replay declares state unknown there by design (the recorded fall
    that ENDS such a span is the resync trigger itself), so counting them
    as misses would penalize the honesty."""
    def match(a, b, tol=2.0):
        b = sorted(b)
        used = [False] * len(b)
        hits = 0
        for x in a:
            for j, y in enumerate(b):
                if not used[j] and abs(x - y) <= tol:
                    used[j] = True
                    hits += 1
                    break
        return hits

    spans = [sp for r in results for sp in r["resync_spans"]]

    def in_resync(x):
        return any(a <= x <= b for (a, b) in spans)

    rec_rises, rec_falls = [], []
    for ses in sessions:
        for seg in ses["segments"]:
            for i in range(len(seg["t"])):
                tt = float(seg["t"][i])
                if in_resync(tt):
                    continue
                if seg["rec_rise"][i]:
                    rec_rises.append(tt)
                if seg["rec_fall"][i]:
                    rec_falls.append(tt)
    eps = flat_episodes(results)
    rep_rises = [ep["t_rise"] for ep in eps
                 if ep["outcome"] != "resync_unknown"]
    rep_falls = [ep["t_end"] for ep in eps if ep["outcome"] == "completed"]
    return {
        "rec_rises": len(rec_rises), "rep_rises": len(rep_rises),
        "rise_hits": match(rep_rises, rec_rises),
        "rec_falls": len(rec_falls), "rep_falls": len(rep_falls),
        "fall_hits": match(rep_falls, rec_falls),
        "excluded_resync_spans": len(spans),
    }


# ============================================================================
# OVERLAY PLOT
# ============================================================================
def overlay(sessions, frac, results, t_start, duration, save,
            peaks_by_session=None):
    import matplotlib
    if save:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(14, 6))
    label = frac if isinstance(frac, str) else ("%.2f" % frac)
    ax.set_title("IR_SCOPE replay overlay — falling threshold %s of span" % label)
    ax.set_xlabel("Firmware time (s)")
    ax.set_ylabel("ADC counts")

    first = True
    for ses in sessions:
        for seg in ses["segments"]:
            ts = [t / 1000.0 for t in seg["t"]]
            if t_start is not None:
                if ts[-1] < t_start:
                    continue
                if duration is not None and ts[0] > t_start + duration:
                    continue
            ax.plot(ts, seg["raw"], color="#1f77b4", lw=0.8,
                    label="raw" if first else None)
            ax.plot(ts, seg["rec_hi"], color="#ff7f0e", lw=0.8,
                    label="thrHigh (recorded)" if first else None)
            ax.plot(ts, seg["rec_lo"], color="#2ca02c", lw=0.8, linestyle="--",
                    label="thrLow recorded (1/3)" if first else None)
            lo_v = [thr_low_of(seg["mn"][i], seg["mx"][i] - seg["mn"][i], frac)
                    for i in range(len(seg["t"]))]
            ax.plot(ts, lo_v, color="#d62728", lw=1.0,
                    label=("thrLow candidate %s" % label) if first else None)
            # recorded inPulse: grey band at the bottom
            ax.fill_between(ts, 0, 200,
                            where=[v > 0 for v in seg["rec_ip"]],
                            color="#888888", alpha=0.5,
                            label="inPulse recorded" if first else None)
            first = False

    # Replayed episodes: EVERY outcome is drawn, styled by what happened —
    # the pulses that never produced a fall edge (latch, closs, gap, open)
    # are precisely the failure modes under investigation and must not be
    # invisible in the overlay.
    STYLE = {
        # outcome: (color, alpha, hatch, legend)
        "completed":       ("#d62728", 0.50, None, "replay %s: completed"),
        "latch":           ("#ff9900", 0.55, "xx", "replay %s: LATCH discard (no fall)"),
        "closs":           ("#9467bd", 0.55, "xx", "replay %s: contrast discard (no fall)"),
        "gap_interrupted": ("#d62728", 0.25, "//", "replay %s: open at gap (end unknown)"),
        "open_end":        ("#d62728", 0.25, "..", "replay %s: still open at record end"),
        "resync_unknown":  ("#7f7f7f", 0.35, "\\\\", "replay %s: state unknown (post-gap resync)"),
    }
    seen = set()
    for ses_result in results:
        for ep in ses_result["episodes"]:
            color, alpha, hatch, leg = STYLE[ep["outcome"]]
            tr, tf = ep["t_rise"] / 1000.0, ep["t_end"] / 1000.0
            lab = None
            if ep["outcome"] not in seen:
                seen.add(ep["outcome"])
                lab = leg % label
            ax.fill_between([tr, tf], 220, 420, color=color, alpha=alpha,
                            hatch=hatch, edgecolor=color, linewidth=0.5,
                            label=lab)
            if ep["outcome"] == "completed":
                ax.scatter([tf], [300], marker="v", s=50, color="#d62728",
                           zorder=5)
            elif ep["outcome"] in ("latch", "closs"):
                # discard instant: inPulse cleared with NO fall edge emitted
                ax.scatter([tf], [320], marker="x", s=60, color=color,
                           zorder=5)

    # physical peaks: the structural ground truth the thresholds interpret
    if peaks_by_session:
        pk = [(t / 1000.0, v) for peaks in peaks_by_session
              for (t, v) in peaks]
        if pk:
            ax.scatter([p[0] for p in pk], [p[1] for p in pk], marker=".",
                       s=30, color="#111111", zorder=6,
                       label="physical peak (structure)")

    if t_start is not None:
        ax.set_xlim(t_start, t_start + (duration or 5.0))
    ax.legend(fontsize=8, loc="upper right")
    fig.tight_layout()
    if save:
        fig.savefig(save, dpi=140)
        print("[Replay] overlay saved -> %s" % save)
    else:
        plt.show()


# ============================================================================
def fmt(v, spec="%.1f"):
    if isinstance(v, float) and math.isnan(v):
        return "---"
    return spec % v


def main():
    ap = argparse.ArgumentParser(description="IR_SCOPE offline threshold replay")
    ap.add_argument("csv_path")
    ap.add_argument("--frac", type=float, action="append", default=[],
                    help="extra falling-threshold fraction(s) of span")
    ap.add_argument("--plot", default=None,
                    help="overlay one candidate ('1/3' or a fraction, e.g. 0.50)")
    ap.add_argument("--start", type=float, default=None,
                    help="overlay window start, firmware seconds")
    ap.add_argument("--duration", type=float, default=None,
                    help="overlay window length, seconds")
    ap.add_argument("--save", default=None, help="save overlay PNG instead of showing")
    ap.add_argument("--prom", type=float, default=PEAK_PROM_FRAC,
                    help="physical-peak prominence as a fraction of span "
                         "(default %.2f)" % PEAK_PROM_FRAC)
    ap.add_argument("--rev-marker", default=REV_MARKER_DEFAULT,
                    help="marker-text substring that marks one hand-turned "
                         "revolution (default '%s')" % REV_MARKER_DEFAULT)
    a = ap.parse_args()

    sessions = load_sessions(a.csv_path)
    n_samples = sum(len(seg["t"]) for s in sessions for seg in s["segments"])
    n_segs = sum(len(s["segments"]) for s in sessions)
    if not n_samples:
        print("No SAMPLE rows found in %s" % a.csv_path)
        sys.exit(1)
    n_gaps = sum(1 for s in sessions for b in s["boundaries"] if b == "GAP")
    n_miss = sum(1 for s in sessions for b in s["boundaries"] if b == "MISSED")
    print("[Replay] %s: %d samples, %d session(s), %d segment(s) "
          "(%d transport gap(s), %d sampler stall(s))"
          % (a.csv_path, n_samples, len(sessions), n_segs, n_gaps, n_miss))
    rec_pulses = sum(sum(seg["rec_fall"]) for s in sessions
                     for seg in s["segments"])
    print("[Replay] recorded detector: %d completed pulses in this capture"
          % rec_pulses)

    # Candidate-independent waveform structure: optical peaks counted from
    # the raw trace itself, immune to uniform interval shifts. Structural
    # evidence only — NOT a revolution count (see header comment).
    peaks_by_session = [physical_peaks(s, a.prom) for s in sessions]
    n_peaks = sum(len(p) for p in peaks_by_session)
    print("[Replay] waveform structure: %d physical peaks (prominence "
          "%.2f x span) — structural evidence, not a revolution count"
          % (n_peaks, a.prom))

    # Absolute revolutions come ONLY from operator markers.
    sub = a.rev_marker.lower()
    rev_pairs = [usable_rev_windows(s, sub) for s in sessions]
    rev_windows_by_session = [p[0] for p in rev_pairs]
    n_wins = sum(len(w) for w in rev_windows_by_session)
    n_excl = sum(p[1] for p in rev_pairs)
    if n_wins:
        peaks_per_rev = []
        for peaks, wins in zip(peaks_by_session, rev_windows_by_session):
            pt = sorted(p[0] for p in peaks)
            for (w0, w1) in wins:
                peaks_per_rev.append(float(bisect.bisect_left(pt, w1)
                                           - bisect.bisect_left(pt, w0)))
        ppr_med = pct(sorted(peaks_per_rev), 50)
        print("[Replay] revolution markers ('%s'): %d usable revolution(s)"
              "%s; peaks per marked revolution med=%.1f (7.0 = one optical "
              "peak per spoke; ~14 = duplicated optical features per spoke)"
              % (a.rev_marker, n_wins,
                 (", %d excluded across gaps" % n_excl) if n_excl else "",
                 ppr_med))
    else:
        print("[Replay] no revolution markers ('%s') found — absolute "
              "pulses/revolution UNAVAILABLE; p/7pk is structural evidence "
              "only" % a.rev_marker)

    candidates = ["1/3", 0.40, 0.50, 0.60] + a.frac

    all_results = {}
    for frac in candidates:
        all_results[frac] = [replay_session(s, frac) for s in sessions]

    # semantics validation: 1/3 must reproduce the firmware's own edges
    v = validate_against_recording(sessions, all_results["1/3"])
    print("\n[Replay] semantics check (1/3 vs recorded flags, +/-2 ms):")
    print("         rises  replay %d / recorded %d, matched %d"
          % (v["rep_rises"], v["rec_rises"], v["rise_hits"]))
    print("         falls  replay %d / recorded %d, matched %d"
          % (v["rep_falls"], v["rec_falls"], v["fall_hits"]))
    if v["rec_falls"] and v["fall_hits"] < v["rec_falls"] * 0.98:
        print("         WARNING: replay does not reproduce the recorded detector;"
              " treat candidate numbers as suspect")

    hdr = ("frac    pulses  latch  open  unkn  short  merged  uncls  mpk  "
           "duty%  w med/p10/p90      int med/min/max        base   "
           "p/rev-int  p/7pk  p/rev-mk")
    print("\n" + hdr)
    print("-" * len(hdr))
    notes = []
    for frac in candidates:
        m = analyse(all_results[frac], peaks_by_session,
                    rev_windows_by_session)
        label = frac if isinstance(frac, str) else ("%.2f" % frac)
        print("%-6s  %6d  %5d  %4d  %4d  %5d  %6d  %5d  %3d  %5s  "
              "%5s/%5s/%5s ms  %6s/%6s/%6s ms  %5s  %9s  %5s  %s"
              % (label, m["n_pulses"], m["latches"], m["open"], m["resync"],
                 m["short_pulses"], m["merged"], m["unclassified"],
                 m["multi_peak"], fmt(m["duty_med"], "%.0f"),
                 fmt(m["w_med"], "%.0f"), fmt(m["w_p10"], "%.0f"),
                 fmt(m["w_p90"], "%.0f"),
                 fmt(m["i_med"], "%.0f"), fmt(m["i_min"], "%.0f"),
                 fmt(m["i_max"], "%.0f"),
                 fmt(m["base_interval"], "%.0f"),
                 fmt(m["pulses_per_rev"], "%.2f"),
                 fmt(m["p_per_7pk"], "%.2f"),
                 fmt(m["p_rev_marked"], "%.1f")))
        if (not math.isnan(m["pulses_per_rev"])
                and not math.isnan(m["p_per_7pk"])
                and abs(m["pulses_per_rev"] - m["p_per_7pk"]) > 0.5):
            notes.append(
                "NOTE %s: interval-ratio p/rev-int %.2f disagrees with "
                "structural p/7pk %.2f — the interval method is blind to "
                "uniform distortion. Inspect mpk and the overlay; use "
                "revolution markers for any absolute revolution claim."
                % (label, m["pulses_per_rev"], m["p_per_7pk"]))
    for n in notes:
        print(n)
    print("\np/rev-int: interval-ratio estimate (7.00 = every spoke seen;"
          " ~3.5 = half merged). BLIND to uniform distortion."
          "\np/7pk: apparent pulses per seven physical peaks — structural"
          " evidence, NOT a revolution measurement (the denominator comes"
          " from the same waveform: per-spoke optical doubling still reads"
          " 7.00). mpk = completed pulses containing >= 2 physical peaks"
          " (direct merged-spoke evidence)."
          "\np/rev-mk: pulses per operator-marked revolution (markers"
          " containing '%s') — the ONLY absolute pulses/revolution figure;"
          " '---' means no usable marked revolutions in this capture."
          "\nmerged: interval >= %.1fx its LOCAL base (p25 of the +/-%d"
          " neighbouring intervals, per session) — a pooled base would"
          " misread hand-push speed changes as merges. uncls = intervals"
          " with too few neighbours to classify."
          "\nlatch/open episodes are pulses that never produced a fall edge;"
          " unkn = post-gap stretches where candidate state was unknowable."
          % (a.rev_marker, MERGED_RATIO, LOCAL_HALF))

    if a.plot is not None:
        frac = "1/3" if a.plot.strip() == "1/3" else float(a.plot)
        results = all_results.get(frac)
        if results is None:
            results = [replay_session(s, frac) for s in sessions]
        overlay(sessions, frac, results, a.start, a.duration, a.save,
                peaks_by_session)


if __name__ == "__main__":
    main()
