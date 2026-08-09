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
                  as `gap_interrupted` (never silently dropped), the
                  interval anchor is cleared, and at resume the replay is
                  SEEDED from the recorded inPulse flag — a seeded pulse's
                  rise time is unknown, so its width and the interval it
                  would anchor are excluded from statistics (counted as
                  `seed`), while its fall remains a real fall. Resetting to
                  idle instead would fire a spurious rise on the first
                  above-threshold sample after every gap.
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
                state["session"] = {"segments": [], "boundaries": []}
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
# `seeded` episodes began at a gap resume with the rise time unknown; their
# widths/intervals are excluded from statistics, their falls are real.
# ============================================================================
def thr_low_of(mn, span, frac):
    if frac == "1/3":
        return mn + span // 3            # firmware's exact integer arithmetic
    return mn + int(span * frac)         # floor, matching integer truncation


def replay_session(session, frac):
    episodes = []
    closs_count = [0]
    st = {"in_pulse": False, "seeded": False, "last_edge": -1e12,
          "pulse_start": 0.0, "prev_rise": None}

    def close(t_end, outcome, width=None, interval=None):
        episodes.append({"t_rise": st["pulse_start"], "t_end": t_end,
                         "outcome": outcome, "seeded": st["seeded"],
                         "width": width, "interval": interval})

    for si, seg in enumerate(session["segments"]):
        if si > 0:
            kind = session["boundaries"][si - 1]
            if kind == "GAP":
                if st["in_pulse"]:
                    # end unknown — close explicitly, never silently
                    close(float(session["segments"][si - 1]["t"][-1]),
                          "gap_interrupted")
                    st["in_pulse"] = False
                st["prev_rise"] = None    # nothing anchors across a gap
                st["seeded"] = False
                if seg["rec_ip"][0]:
                    # the firmware was (recorded) mid-pulse at resume: seed,
                    # rather than firing a spurious replay rise on the first
                    # above-threshold sample after every gap
                    st["in_pulse"] = True
                    st["seeded"] = True
                    st["pulse_start"] = float(seg["t"][0])
            # kind == "MISSED": the detector ran continuously — carry all
            # state; absolute times keep debounce and latch arithmetic honest

        for i in range(len(seg["t"])):
            t = float(seg["t"][i])
            raw = seg["raw"][i]
            mn = seg["mn"][i]
            mx = seg["mx"][i]
            cv = seg["cv"][i]
            span = mx - mn

            # contrast gate — discard branch, as IR_DIAG
            if not cv:
                if st["in_pulse"] or st["prev_rise"] is not None:
                    closs_count[0] += 1
                    if st["in_pulse"]:
                        close(t, "closs")
                    st["in_pulse"] = False
                    st["seeded"] = False
                    st["prev_rise"] = None
                continue

            hi = mn + (span * 2) // 3
            lo = thr_low_of(mn, span, frac)

            # latch timeout — pulse discarded, no fall edge, anchor cleared
            if st["in_pulse"] and (t - st["pulse_start"]) > LATCH_MS:
                close(t, "latch")
                st["in_pulse"] = False
                st["seeded"] = False
                st["prev_rise"] = None

            if not st["in_pulse"]:
                if raw > hi and (t - st["last_edge"]) > DEBOUNCE_MS:
                    st["in_pulse"] = True
                    st["seeded"] = False
                    st["last_edge"] = t
                    st["pulse_start"] = t
            else:
                if raw < lo:
                    if st["seeded"]:
                        # real fall, unknown rise: no width, and the next
                        # interval must not anchor on a synthetic rise time
                        close(t, "completed")
                        st["prev_rise"] = None
                    else:
                        width = t - st["pulse_start"]
                        interval = ((st["pulse_start"] - st["prev_rise"])
                                    if st["prev_rise"] is not None else None)
                        close(t, "completed", width, interval)
                        st["prev_rise"] = st["pulse_start"]
                    st["in_pulse"] = False
                    st["seeded"] = False

    if st["in_pulse"]:
        close(float(session["segments"][-1]["t"][-1]), "open_end")

    return {"episodes": episodes, "closs": closs_count[0]}


def flat_episodes(results):
    return [ep for r in results for ep in r["episodes"]]


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


def analyse(results):
    eps = flat_episodes(results)
    done = [ep for ep in eps if ep["outcome"] == "completed"
            and not ep["seeded"]]
    widths = sorted(ep["width"] for ep in done)
    intervals_sorted = sorted(ep["interval"] for ep in done
                              if ep["interval"] is not None
                              and ep["interval"] > 0)

    out = {
        "n_pulses": len([ep for ep in eps if ep["outcome"] == "completed"]),
        "latches": len([ep for ep in eps if ep["outcome"] == "latch"]),
        "open": len([ep for ep in eps
                     if ep["outcome"] in ("open_end", "gap_interrupted")]),
        "seeded": len([ep for ep in eps if ep["seeded"]]),
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
    }

    # Locality is judged per session and in chronological order — speed at
    # a hand-pushed wheel is only comparable to its neighbours in time.
    local_bases = []
    for r in results:
        ses_done = [ep for ep in r["episodes"]
                    if ep["outcome"] == "completed" and not ep["seeded"]]
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
    Seeded rises are synthetic (gap-resume time) and excluded; falls of
    seeded episodes are real falls and included."""
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

    rec_rises, rec_falls = [], []
    for ses in sessions:
        for seg in ses["segments"]:
            for i in range(len(seg["t"])):
                if seg["rec_rise"][i]:
                    rec_rises.append(float(seg["t"][i]))
                if seg["rec_fall"][i]:
                    rec_falls.append(float(seg["t"][i]))
    eps = flat_episodes(results)
    rep_rises = [ep["t_rise"] for ep in eps if not ep["seeded"]]
    rep_falls = [ep["t_end"] for ep in eps if ep["outcome"] == "completed"]
    return {
        "rec_rises": len(rec_rises), "rep_rises": len(rep_rises),
        "rise_hits": match(rep_rises, rec_rises),
        "rec_falls": len(rec_falls), "rep_falls": len(rep_falls),
        "fall_hits": match(rep_falls, rec_falls),
    }


# ============================================================================
# OVERLAY PLOT
# ============================================================================
def overlay(sessions, frac, results, t_start, duration, save):
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
            if ep["seeded"]:
                # rise time unknown (seeded at a gap resume)
                ax.text(tr, 430, "seeded", color=color, fontsize=6,
                        ha="left", va="bottom")

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

    hdr = ("frac    pulses  latch  open  seed  short  merged  uncls  "
           "w med/p10/p90      int med/min/max        base   pulses/rev")
    print("\n" + hdr)
    print("-" * len(hdr))
    for frac in candidates:
        m = analyse(all_results[frac])
        label = frac if isinstance(frac, str) else ("%.2f" % frac)
        print("%-6s  %6d  %5d  %4d  %4d  %5d  %6d  %5d  %5s/%5s/%5s ms  "
              "%6s/%6s/%6s ms  %5s  %s"
              % (label, m["n_pulses"], m["latches"], m["open"], m["seeded"],
                 m["short_pulses"], m["merged"], m["unclassified"],
                 fmt(m["w_med"], "%.0f"), fmt(m["w_p10"], "%.0f"),
                 fmt(m["w_p90"], "%.0f"),
                 fmt(m["i_med"], "%.0f"), fmt(m["i_min"], "%.0f"),
                 fmt(m["i_max"], "%.0f"),
                 fmt(m["base_interval"], "%.0f"),
                 fmt(m["pulses_per_rev"], "%.2f")))
    print("\npulses/rev: 7.00 = every spoke seen; ~3.5 = half merged."
          "\nmerged: interval >= %.1fx its LOCAL base (p25 of the +/-%d"
          " neighbouring intervals, per session) — a pooled base would"
          " misread hand-push speed changes as merges. uncls = intervals"
          " with too few neighbours to classify."
          "\nlatch/open episodes are pulses that never produced a fall edge."
          "\nEstablish phase with a hand-turn marker before trusting"
          " revolutions absolutely." % (MERGED_RATIO, LOCAL_HALF))

    if a.plot is not None:
        frac = "1/3" if a.plot.strip() == "1/3" else float(a.plot)
        results = all_results.get(frac)
        if results is None:
            results = [replay_session(s, frac) for s in sessions]
        overlay(sessions, frac, results, a.start, a.duration, a.save)


if __name__ == "__main__":
    main()
