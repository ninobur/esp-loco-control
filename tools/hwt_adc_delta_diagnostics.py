#!/usr/bin/env python3
"""hwt_adc_delta_diagnostics.py — independent characterization of raw Hall
ADC sample-to-sample behavior, in three region types identified WITHOUT any
reference to continuity_ratio (tools/hwt_gate_replay.py) at all.

INVESTIGATORY. This exists because tools/hwt_gate_replay.py's continuity
measure was found to have been calibrated against only two hand-picked
excursions, with no independent characterization of what raw sample-to-
sample deltas actually look like anywhere in these captures. This tool is a
first step toward evidence that could someday support or rule out a
smoothness-based gate -- it implements no gate itself, and it must never be
used to derive a value called an "ADC noise floor": none of the three
regions below is a verified noise-only reference (a disconnected sensor, a
locomotive known to be truly motionless with no physical field nearby).
They are simply three regions identified by criteria independent of
continuity, so their delta distributions can be compared to each other and
to continuity_ratio's own behavior without circularity.

Regions (identified independently of continuity_ratio):
  stationary    samples inside a low-PWM dwell window (PWM telemetry only
               -- tools/hwt_excursions.detect_low_pwm_dwells; a proxy for
               "commanded to a stop", NOT independently verified as
               motionless -- see that function's own module-docstring
               caveats, which apply here unchanged)
  moving_quiet  samples where PWM is above the dwell threshold (moving) AND
               the sample is NOT part of any frozen-baseline excursion --
               i.e. the frozen detector's own "quiet" classification, which
               is an amplitude (entry/exit threshold) criterion, not a
               continuity one
  broad_response  samples inside an excursion whose duration and integrated
               |flux| clear hwt_gate_replay.passes_morphology()'s bars --
               duration/flux only, continuity plays no part in selecting
               these regions

For each region, raw[i+lag]-raw[i] is computed only within contiguous
same-region, gap-free stretches (a sequence-number jump ends a stretch) at
several lags, and reported as a distribution (mean, stdev, percentiles).

    python3 tools/hwt_adc_delta_diagnostics.py run.csv --out diagnostics.csv
"""
import argparse
import csv
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import hwt_excursions as E   # noqa: E402

LAGS = (1, 5, 10, 20)


def region_labels(samples, dwell_windows, excursions_idx_ranges, broad_idx_set):
    """One label per sample index: 'stationary', 'moving_quiet', or
    'broad_response' (or None if in a narrow/incomplete excursion, which is
    neither quiet nor a qualifying broad response, so excluded from all
    three regions rather than mislabeled)."""
    labels = [None] * len(samples)
    dwell_i = 0
    for i, s in enumerate(samples):
        t = s["t_s"]
        while dwell_i < len(dwell_windows) and t > dwell_windows[dwell_i][1]:
            dwell_i += 1
        in_dwell = dwell_i < len(dwell_windows) and dwell_windows[dwell_i][0] <= t <= dwell_windows[dwell_i][1]
        if in_dwell:
            labels[i] = "stationary"
    for i in broad_idx_set:
        labels[i] = "broad_response"
    excursion_idx = set()
    for rng in excursions_idx_ranges:
        excursion_idx.update(rng)
    for i, s in enumerate(samples):
        if labels[i] is not None:
            continue
        if i in excursion_idx:
            continue   # narrow spike or non-qualifying excursion: excluded, not "quiet"
        labels[i] = "moving_quiet"
    return labels


def collect_deltas(samples, labels, region, lags):
    """raw[i+lag]-raw[i] for consecutive same-region, seq-contiguous i,i+lag
    pairs. A gap in phys_sample_seq (transport loss, sampler stall) or a
    region-label change anywhere in [i, i+lag] breaks the pair."""
    out = {lag: [] for lag in lags}
    n = len(samples)
    for lag in lags:
        for i in range(n - lag):
            if labels[i] != region:
                continue
            ok = True
            for k in range(1, lag + 1):
                if labels[i + k] != region:
                    ok = False
                    break
                if samples[i + k]["seq"] != samples[i + k - 1]["seq"] + 1:
                    ok = False
                    break
            if ok:
                out[lag].append(samples[i + lag]["raw"] - samples[i]["raw"])
    return out


def stats(vals):
    if not vals:
        return {"n": 0}
    vals = sorted(vals)
    n = len(vals)
    mean = sum(vals) / n
    var = sum((v - mean) ** 2 for v in vals) / n
    def pct(p):
        return vals[min(n - 1, int(p * n))]
    return {"n": n, "mean": mean, "stdev": var ** 0.5, "min": vals[0], "max": vals[-1],
           "p5": pct(0.05), "p50": pct(0.50), "p95": pct(0.95)}


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("capture", help=".hwt capture or decoded CSV")
    ap.add_argument("--out", help="CSV of per-region, per-lag statistics")
    ap.add_argument("--min-duration-ms", type=float, default=40.0)
    ap.add_argument("--min-abs-flux", type=float, default=300.0)
    ap.add_argument("--dwell-pwm-max", type=float, default=5.0)
    ap.add_argument("--dwell-min-ms", type=float, default=1000.0)
    args = ap.parse_args()

    result = E.analyze_captures(
        args.capture, entry_threshold=30.0, exit_threshold=15.0, baseline_mode="frozen",
        pre_window=200, baseline_method="mean", dwell_pwm_max=args.dwell_pwm_max,
        dwell_min_ms=args.dwell_min_ms)

    rows_out = []
    for sid, ctx in result["sessions"].items():
        samples = ctx["samples"]
        dwell_windows = ctx["dwell_windows"]
        session_excursions = [e for e in result["excursions"] if e["session"] == sid]

        seq_index = {s["seq"]: i for i, s in enumerate(samples)}
        excursion_ranges = []
        broad_idx = set()
        for e in session_excursions:
            i0 = seq_index.get(int(e["start_sample"]))
            i1 = seq_index.get(int(e["end_sample"]))
            if i0 is None or i1 is None:
                continue
            rng = range(i0, i1 + 1)
            excursion_ranges.append(rng)
            qualifies = (not int(e["incomplete"])
                        and float(e["duration_ms"]) >= args.min_duration_ms
                        and float(e["integrated_abs_flux_count_ms"]) >= args.min_abs_flux)
            if qualifies:
                broad_idx.update(rng)

        labels = region_labels(samples, dwell_windows, excursion_ranges, broad_idx)
        counts = {}
        for l in labels:
            if l:
                counts[l] = counts.get(l, 0) + 1
        print("session %s: %d samples (%s)" % (
            sid, len(samples),
            ", ".join("%s=%d" % (k, v) for k, v in sorted(counts.items()))))

        for region in ("stationary", "moving_quiet", "broad_response"):
            deltas = collect_deltas(samples, labels, region, LAGS)
            for lag in LAGS:
                st = stats(deltas[lag])
                if st["n"] == 0:
                    print("  %-15s lag=%2d  n=0 (no qualifying pairs)" % (region, lag))
                    continue
                print("  %-15s lag=%2d  n=%7d  mean=%7.2f  stdev=%7.2f  "
                     "p5=%7.1f  p50=%7.1f  p95=%7.1f  min=%7d  max=%7d"
                     % (region, lag, st["n"], st["mean"], st["stdev"],
                        st["p5"], st["p50"], st["p95"], st["min"], st["max"]))
                r = {"session": sid, "region": region, "lag_samples": lag}
                r.update(st)
                rows_out.append(r)

    if args.out:
        cols = ["session", "region", "lag_samples", "n", "mean", "stdev", "min", "max", "p5", "p50", "p95"]
        with open(args.out, "w", newline="") as fh:
            w = csv.DictWriter(fh, fieldnames=cols)
            w.writeheader()
            w.writerows(rows_out)
        print("\nwrote %s (%d rows)" % (args.out, len(rows_out)))

    return 0


if __name__ == "__main__":
    sys.exit(main())
