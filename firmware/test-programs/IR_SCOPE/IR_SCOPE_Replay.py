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
    machine run sample-by-sample.

    The recorded runMin/runMax envelope is replayed as logged — the envelope
    is threshold-independent (percentiles of raw), so recorded bounds remain
    exact for every candidate.

CANDIDATES
    1/3 (current, exact integer span/3 arithmetic), 0.40, 0.50, 0.60,
    plus any extras via --frac. The 1/3 replay is also validated against the
    recorded rise/fall flags: it should reproduce the firmware's own edges,
    which proves the replay semantics before the other candidates are read.

REPORT (per candidate, per contiguous segment and pooled)
    completed pulses; width median/p10/p90/min/max; rise-to-rise interval
    median/min/max; apparent merged/doubled pulses (interval ratio >= ~1.7x
    the base interval); implausibly short pulses; latch/missing-fall events;
    pulses per apparent revolution where a base interval can be established
    (7 spokes x pulses / spoke-passages, spoke-passages = sum of rounded
    interval ratios).

USAGE
    python3 IR_SCOPE_Replay.py capture.csv
    python3 IR_SCOPE_Replay.py capture.csv --frac 0.45
    python3 IR_SCOPE_Replay.py capture.csv --plot 0.50 --start 12.0 --duration 4.0
    python3 IR_SCOPE_Replay.py capture.csv --plot 0.50 --save overlay.png

    --plot overlays one candidate on the recorded waveform: recorded
    thresholds and recorded inPulse band (grey) against the candidate's
    replayed band, fall edges, and thrLow line — so you can see exactly
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

SHORT_WIDTH_FRACTION = 0.4    # width < 0.4 x median width => implausibly short
MERGED_RATIO         = 1.7    # interval >= 1.7 x base => >=1 swallowed spoke


# ============================================================================
# CSV LOADING — split into contiguous segments at GAP/SESSION rows and at
# any sample discontinuity. Segments are never rejoined.
# ============================================================================
def load_segments(path):
    segments = []       # list of dicts of parallel lists
    cur = None

    def new_seg():
        return {"t": [], "raw": [], "mn": [], "mx": [], "cv": [],
                "rec_ip": [], "rec_rise": [], "rec_fall": [],
                "rec_hi": [], "rec_lo": []}

    last_sample = None
    with open(path, newline="") as f:
        rd = csv.DictReader(f)
        for row in rd:
            rt = row.get("row_type", "")
            if rt in ("GAP", "SESSION"):
                if cur and cur["t"]:
                    segments.append(cur)
                cur = None
                last_sample = None
                continue
            if rt != "SAMPLE":
                continue
            try:
                sample = int(row["sample"])
                raw    = int(row["raw"])
                mn     = int(row["run_min"])
                mx     = int(row["run_max"])
                cv     = int(row["contrast_valid"])
                ip     = int(row["in_pulse"])
                rise   = int(row["rise"])
                fall   = int(row["fall"])
                hi     = int(row["thr_high"])
                lo     = int(row["thr_low"])
            except (KeyError, ValueError):
                continue
            if cur is None or (last_sample is not None and sample != last_sample + 1):
                if cur and cur["t"]:
                    segments.append(cur)
                cur = new_seg()
            cur["t"].append(sample)          # ms; sample number IS ms
            cur["raw"].append(raw)
            cur["mn"].append(mn)
            cur["mx"].append(mx)
            cur["cv"].append(cv)
            cur["rec_ip"].append(ip)
            cur["rec_rise"].append(rise)
            cur["rec_fall"].append(fall)
            cur["rec_hi"].append(hi)
            cur["rec_lo"].append(lo)
            last_sample = sample
    if cur and cur["t"]:
        segments.append(cur)
    return segments


# ============================================================================
# DETECTOR REPLAY — IR_DIAG semantics on recorded raw + envelope.
# ============================================================================
def thr_low_of(mn, span, frac):
    if frac == "1/3":
        return mn + span // 3            # firmware's exact integer arithmetic
    return mn + int(span * frac)         # floor, matching integer truncation


def replay_segment(seg, frac):
    """Returns dict with pulses [(t_rise, t_fall, width, interval)],
    latches, closs_discards, rises, falls, open_at_end."""
    in_pulse = False
    last_edge = -1e12          # debounce anchor (rise-to-rise)
    pulse_start = 0.0
    prev_rise = None           # interval anchor
    pulses = []
    latches = 0
    closs = 0
    rise_t = []
    fall_t = []

    for i in range(len(seg["t"])):
        t   = float(seg["t"][i])
        raw = seg["raw"][i]
        mn  = seg["mn"][i]
        mx  = seg["mx"][i]
        cv  = seg["cv"][i]
        span = mx - mn

        # contrast gate — discard branch, as IR_DIAG
        if not cv:
            if in_pulse or prev_rise is not None:
                closs += 1
                in_pulse = False
                prev_rise = None
            continue

        hi = mn + (span * 2) // 3
        lo = thr_low_of(mn, span, frac)

        # latch timeout — pulse discarded, no fall edge, anchor cleared
        if in_pulse and (t - pulse_start) > LATCH_MS:
            in_pulse = False
            prev_rise = None
            latches += 1

        if not in_pulse:
            if raw > hi and (t - last_edge) > DEBOUNCE_MS:
                in_pulse = True
                last_edge = t
                pulse_start = t
                rise_t.append(t)
        else:
            if raw < lo:
                in_pulse = False
                width = t - pulse_start
                interval = (pulse_start - prev_rise) if prev_rise is not None else 0.0
                pulses.append((pulse_start, t, width, interval))
                fall_t.append(t)
                prev_rise = pulse_start

    return {"pulses": pulses, "latches": latches, "closs": closs,
            "rise_t": rise_t, "fall_t": fall_t, "open_at_end": in_pulse}


# ============================================================================
# METRICS
# ============================================================================
def pct(sorted_vals, p):
    if not sorted_vals:
        return float("nan")
    k = min(len(sorted_vals) - 1, int(len(sorted_vals) * p / 100))
    return sorted_vals[k]


def analyse(results):
    pulses = [p for r in results for p in r["pulses"]]
    widths = sorted(p[2] for p in pulses)
    intervals = sorted(p[3] for p in pulses if p[3] > 0)
    latches = sum(r["latches"] for r in results)
    closs = sum(r["closs"] for r in results)
    open_end = sum(1 for r in results if r["open_at_end"])

    out = {
        "n_pulses": len(pulses),
        "latches": latches,
        "closs": closs,
        "open_at_end": open_end,
        "w_med": pct(widths, 50), "w_p10": pct(widths, 10),
        "w_p90": pct(widths, 90),
        "w_min": widths[0] if widths else float("nan"),
        "w_max": widths[-1] if widths else float("nan"),
        "i_med": pct(intervals, 50),
        "i_min": intervals[0] if intervals else float("nan"),
        "i_max": intervals[-1] if intervals else float("nan"),
        "merged": 0, "short_pulses": 0,
        "spoke_passages": 0, "n_ratio_intervals": 0,
        "pulses_per_rev": float("nan"),
        "base_interval": float("nan"),
    }

    if widths:
        w_med = out["w_med"]
        out["short_pulses"] = sum(1 for w in widths
                                  if w < max(3.0, SHORT_WIDTH_FRACTION * w_med))

    if len(intervals) >= 5:
        # Base interval from a low percentile: a swallowed spoke can only
        # INFLATE an interval, never shrink it, so p25 approximates the true
        # single-spoke time even when much of the record is merged.
        base = pct(intervals, 25)
        out["base_interval"] = base
        if base > 0:
            ratios = [i / base for i in intervals]
            out["merged"] = sum(1 for r in ratios if r >= MERGED_RATIO)
            rounded = [max(1, int(round(r))) for r in ratios]
            out["spoke_passages"] = sum(rounded)
            out["n_ratio_intervals"] = len(rounded)
            if out["spoke_passages"] > 0:
                out["pulses_per_rev"] = (SPOKES * len(rounded)
                                         / out["spoke_passages"])
    return out


def validate_against_recording(segments, results):
    """1/3 replay vs the firmware's own recorded edges (tolerance +/-2 ms)."""
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

    rec_rises = []
    rec_falls = []
    for seg in segments:
        for i in range(len(seg["t"])):
            if seg["rec_rise"][i]:
                rec_rises.append(float(seg["t"][i]))
            if seg["rec_fall"][i]:
                rec_falls.append(float(seg["t"][i]))
    rep_rises = [t for r in results for t in r["rise_t"]]
    rep_falls = [t for r in results for t in r["fall_t"]]
    return {
        "rec_rises": len(rec_rises), "rep_rises": len(rep_rises),
        "rise_hits": match(rep_rises, rec_rises),
        "rec_falls": len(rec_falls), "rep_falls": len(rep_falls),
        "fall_hits": match(rep_falls, rec_falls),
    }


# ============================================================================
# OVERLAY PLOT
# ============================================================================
def overlay(segments, frac, results, t_start, duration, save):
    import matplotlib
    if save:
        matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    fig, ax = plt.subplots(figsize=(14, 6))
    label = frac if isinstance(frac, str) else ("%.2f" % frac)
    ax.set_title("IR_SCOPE replay overlay — falling threshold %s of span" % label)
    ax.set_xlabel("Firmware time (s)")
    ax.set_ylabel("ADC counts")

    lo_t, lo_v = [], []
    first = True
    for seg, res in zip(segments, results):
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
        for i in range(len(seg["t"])):
            mn = seg["mn"][i]
            span = seg["mx"][i] - mn
            lo_t.append(seg["t"][i] / 1000.0)
            lo_v.append(thr_low_of(mn, span, frac))
        # recorded inPulse: grey band at the bottom
        ax.fill_between(ts, 0, 200,
                        where=[v > 0 for v in seg["rec_ip"]],
                        color="#888888", alpha=0.5,
                        label="inPulse recorded" if first else None)
        # replayed pulses for this candidate: colored band above it
        rep_ip = [False] * len(ts)
        for (tr, tf, _, _) in res["pulses"]:
            a = int(tr - seg["t"][0])
            b = int(tf - seg["t"][0])
            for k in range(max(a, 0), min(b + 1, len(ts))):
                rep_ip[k] = True
        ax.fill_between(ts, 220, 420, where=rep_ip, color="#d62728", alpha=0.5,
                        label=("inPulse replay %s" % label) if first else None)
        if res["fall_t"]:
            ft = [t / 1000.0 for t in res["fall_t"]]
            fv = []
            for t in res["fall_t"]:
                idx = int(t - seg["t"][0])
                fv.append(seg["raw"][idx] if 0 <= idx < len(ts) else 0)
            ax.scatter(ft, fv, marker="v", s=50, color="#d62728", zorder=5,
                       label=("fall replay %s" % label) if first else None)
        first = False

    ax.plot(lo_t, lo_v, color="#d62728", lw=1.0,
            label="thrLow candidate %s" % label)
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

    segments = load_segments(a.csv_path)
    n_samples = sum(len(s["t"]) for s in segments)
    if not n_samples:
        print("No SAMPLE rows found in %s" % a.csv_path)
        sys.exit(1)
    print("[Replay] %s: %d samples in %d contiguous segment(s)"
          % (a.csv_path, n_samples, len(segments)))
    rec_pulses = sum(sum(s["rec_fall"]) for s in segments)
    print("[Replay] recorded detector: %d completed pulses in this capture"
          % rec_pulses)

    candidates = ["1/3", 0.40, 0.50, 0.60] + a.frac

    all_results = {}
    for frac in candidates:
        all_results[frac] = [replay_segment(seg, frac) for seg in segments]

    # semantics validation: 1/3 must reproduce the firmware's own edges
    v = validate_against_recording(segments, all_results["1/3"])
    print("\n[Replay] semantics check (1/3 vs recorded flags, +/-2 ms):")
    print("         rises  replay %d / recorded %d, matched %d"
          % (v["rep_rises"], v["rec_rises"], v["rise_hits"]))
    print("         falls  replay %d / recorded %d, matched %d"
          % (v["rep_falls"], v["rec_falls"], v["fall_hits"]))
    if v["rec_falls"] and v["fall_hits"] < v["rec_falls"] * 0.98:
        print("         WARNING: replay does not reproduce the recorded detector;"
              " treat candidate numbers as suspect")

    hdr = ("frac    pulses  latch  short  merged  w med/p10/p90      "
           "int med/min/max        base   pulses/rev")
    print("\n" + hdr)
    print("-" * len(hdr))
    for frac in candidates:
        m = analyse(all_results[frac])
        label = frac if isinstance(frac, str) else ("%.2f" % frac)
        print("%-6s  %6d  %5d  %5d  %6d  %5s/%5s/%5s ms  %6s/%6s/%6s ms  %5s  %s"
              % (label, m["n_pulses"], m["latches"], m["short_pulses"],
                 m["merged"],
                 fmt(m["w_med"], "%.0f"), fmt(m["w_p10"], "%.0f"),
                 fmt(m["w_p90"], "%.0f"),
                 fmt(m["i_med"], "%.0f"), fmt(m["i_min"], "%.0f"),
                 fmt(m["i_max"], "%.0f"),
                 fmt(m["base_interval"], "%.0f"),
                 fmt(m["pulses_per_rev"], "%.2f")))
    print("\npulses/rev: 7.00 = every spoke seen; ~3.5 = half merged."
          "\nmerged: intervals >= %.1fx the base (p25) interval."
          "\nEstablish phase with a hand-turn marker before trusting"
          " revolutions absolutely." % MERGED_RATIO)

    if a.plot is not None:
        frac = "1/3" if a.plot.strip() == "1/3" else float(a.plot)
        results = all_results.get(frac)
        if results is None:
            results = [replay_segment(seg, frac) for seg in segments]
        overlay(segments, frac, results, a.start, a.duration, a.save)


if __name__ == "__main__":
    main()
