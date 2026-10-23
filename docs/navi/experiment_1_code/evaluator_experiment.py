#!/usr/bin/env python3
"""Offline research prototype: causal rolling line/curve evaluator.

Experiment 1 (per Sam's framing): can a continuously operating, causal
line/curve evaluator reliably distinguish magnetic excursions from
intervening background in actual Hall recordings?

Not firmware. No TARGET/NOT_THE_TARGET decision, no navigation, no map.
Pure observation: segment the trace into LINE (ordinary) and CURVE
(excursion) regions, using only a trailing window of already-seen samples
(causal), PWM-gated (PWM=0 suspends, no reset), ΔBIC as the line-vs-curve
test (the one tool already in this project's research history).

Outputs: per-recording excursion list (onset, peak, duration, polarity,
detection latency vs. an independent simple estimate), the finalized
intervening-segment medians, and a plot of the trace with classifications
overlaid, for independent review.
"""
import sys, csv, json
import numpy as np

BLOCK_US = 20000
MIN_READS = 50


def load_csv(path):
    t_us, adc, serial, pwm = [], [], [], []
    with open(path) as f:
        r = csv.reader(f)
        next(r)
        for row in r:
            serial.append(int(row[1]))
            t_us.append(int(row[2]))
            adc.append(int(row[3]))
            pwm.append(int(row[4]))
    t_us = np.array(t_us, dtype=np.int64)
    adc = np.array(adc, dtype=np.float64)
    serial = np.array(serial, dtype=np.int64)
    pwm = np.array(pwm, dtype=np.int64)
    order = np.argsort(t_us, kind="stable")
    return t_us[order], adc[order], serial[order], pwm[order]


def bin_20ms(t_us, adc, serial, pwm):
    t0 = t_us[0]
    blk = (t_us - t0) // BLOCK_US
    nb = int(blk[-1]) + 1
    cnt = np.bincount(blk, minlength=nb)
    sums = np.bincount(blk, weights=adc, minlength=nb)
    hall = np.full(nb, np.nan)
    ok = cnt >= MIN_READS
    hall[ok] = sums[ok] / cnt[ok]
    dser = np.diff(serial)
    jump_idx = np.where(dser != 1)[0]
    bad = np.zeros(nb, dtype=bool)
    bad[blk[jump_idx]] = True
    bad[blk[jump_idx + 1]] = True
    hall[bad] = np.nan
    pwm_blk = np.zeros(nb, dtype=np.int64)
    np.maximum.at(pwm_blk, blk, pwm)
    block_t0 = t0 + np.arange(nb) * BLOCK_US
    return block_t0, hall, pwm_blk, cnt


def bic(sse, n, p):
    return n * np.log(max(sse / n, 1e-9)) + p * np.log(n)


def delta_bic(t, y):
    if len(y) < 9:
        return float("-inf")
    x = (t - t[0]) / max(t[-1] - t[0], 1e-9)
    line_x = np.column_stack((np.ones(len(x)), x))
    line_sse = float(np.sum((y - line_x @ np.linalg.lstsq(line_x, y, rcond=None)[0]) ** 2))
    best = float("inf")
    for mu in np.linspace(.18, .82, 9):
        for sigma in (.08, .12, .18, .25, .35, .50):
            g = np.exp(-.5 * ((x - mu) / sigma) ** 2)
            design = np.column_stack((np.ones(len(x)), x, g))
            residual = y - design @ np.linalg.lstsq(design, y, rcond=None)[0]
            best = min(best, float(np.sum(residual ** 2)))
    return bic(line_sse, len(y), 2) - bic(best, len(y), 5)


def run_evaluator(block_t0, hall, pwm_blk, window_blocks=25, threshold=10.0, persistence=1):
    """Causal rolling evaluator. Only considers PWM>0, finite blocks, in
    their natural time order (PWM=0 / missing blocks are skipped --
    'suspended without reset', per the agreed design). Returns a list of
    events and the finalized LINE-segment medians."""
    valid = np.where((pwm_blk > 0) & np.isfinite(hall))[0]
    vt = block_t0[valid].astype(float)
    vh = hall[valid]

    state = "LINE"
    line_start_k = 0          # index into `valid` where current LINE segment began
    excursions = []           # list of dicts
    db_trace = np.full(len(valid), np.nan)
    run_above = 0
    run_below = 0

    for k in range(window_blocks, len(valid)):
        w = slice(k - window_blocks + 1, k + 1)
        db = delta_bic(vt[w], vh[w])
        db_trace[k] = db
        run_above = run_above + 1 if db > threshold else 0
        run_below = run_below + 1 if db < threshold else 0
        if state == "LINE" and run_above >= persistence:
            # transition to CURVE: finalize the just-ended LINE segment's median
            seg = vh[line_start_k:k - window_blocks + 1]
            seg_t = vt[line_start_k:k - window_blocks + 1]
            if len(seg) > 0:
                median_val = float(np.median(seg))
            else:
                median_val = float("nan")
            cur_start_k = k - window_blocks + 1
            state = "CURVE"
            cur_record = {
                "line_segment_n": int(len(seg)),
                "line_segment_span_s": float((seg_t[-1] - seg_t[0]) / 1e6) if len(seg) > 1 else 0.0,
                "line_segment_median": median_val,
                "curve_detect_k": k,
                "curve_detect_t_us": float(vt[k]),
                "curve_start_k_est": cur_start_k,
                "curve_start_t_us_est": float(vt[cur_start_k]),
            }
            run_above = run_below = 0
        elif state == "CURVE" and run_below >= persistence:
            # transition back to LINE: finalize the excursion
            cstart, cend = cur_record["curve_start_k_est"], k
            seg = vh[cstart:cend + 1]
            seg_t = vt[cstart:cend + 1]
            ref = cur_record["line_segment_median"]
            peak_idx = int(np.argmax(np.abs(seg - ref))) if len(seg) and np.isfinite(ref) else 0
            cur_record.update({
                "curve_end_k": cend,
                "curve_end_t_us": float(vt[cend]),
                "curve_confirm_t_us": float(vt[k]),
                "peak_adc": float(seg[peak_idx]) if len(seg) else float("nan"),
                "peak_t_us": float(seg_t[peak_idx]) if len(seg) else float("nan"),
                "peak_departure": float(seg[peak_idx] - ref) if len(seg) and np.isfinite(ref) else float("nan"),
                "duration_s": float((seg_t[-1] - seg_t[0]) / 1e6) if len(seg) > 1 else 0.0,
            })
            excursions.append(cur_record)
            state = "LINE"
            line_start_k = k
            run_above = run_below = 0

    return excursions, valid, vt, vh, db_trace


def independent_true_onset(vt, vh, around_idx, lookback=400, sigma_mult=3.0):
    """Simple, independent estimate of excursion onset: scan backward from
    a point already inside the excursion for the first run of >=6
    consecutive blocks within a tight band of the locally-established
    background (median/std of a quiet stretch just before). Used only to
    measure the evaluator's detection latency, not as part of the
    evaluator itself."""
    lo = max(0, around_idx - lookback)
    bg = vh[lo:around_idx]
    if len(bg) < 20:
        return None
    med, std = np.median(bg), np.std(bg)
    band = max(3.0, sigma_mult * std)
    dep = np.abs(vh[lo:around_idx + 50] - med)
    ok = dep <= band
    # find the LAST index (scanning forward) where 6 consecutive are quiet,
    # then the first index after that run where it's no longer quiet
    run = 0
    last_quiet_end = None
    for i, q in enumerate(ok):
        if q:
            run += 1
            if run >= 6:
                last_quiet_end = i
        else:
            run = 0
    if last_quiet_end is None:
        return None
    return lo + last_quiet_end


def main():
    label = sys.argv[1]
    path = sys.argv[2]
    out_json = sys.argv[3]
    window_blocks = int(sys.argv[4]) if len(sys.argv) > 4 else 25
    threshold = float(sys.argv[5]) if len(sys.argv) > 5 else 10.0
    persistence = int(sys.argv[6]) if len(sys.argv) > 6 else 1

    t_us, adc, serial, pwm = load_csv(path)
    block_t0, hall, pwm_blk, cnt = bin_20ms(t_us, adc, serial, pwm)
    print(f"[{label}] blocks={len(block_t0)} nan_blocks={np.isnan(hall).sum()} "
          f"pwm0_blocks={(pwm_blk == 0).sum()} span_s={(block_t0[-1]-block_t0[0])/1e6:.1f}")

    excursions, valid, vt, vh, db_trace = run_evaluator(block_t0, hall, pwm_blk, window_blocks, threshold, persistence)
    print(f"[{label}] excursions detected: {len(excursions)}")

    # latency measurement
    for e in excursions:
        k = e["curve_start_k_est"]
        onset_idx = independent_true_onset(vt, vh, k)
        if onset_idx is not None:
            e["independent_onset_t_us"] = float(vt[onset_idx])
            e["detection_latency_ms"] = (e["curve_start_t_us_est"] - vt[onset_idx]) / 1e3
        else:
            e["independent_onset_t_us"] = None
            e["detection_latency_ms"] = None

    lat = [e["detection_latency_ms"] for e in excursions if e["detection_latency_ms"] is not None]
    if lat:
        lat = np.array(lat)
        print(f"[{label}] detection latency ms: n={len(lat)} median={np.median(lat):.1f} "
              f"min={lat.min():.1f} max={lat.max():.1f} negative(early)={int((lat<0).sum())} "
              f"zero_or_more(late_or_exact)={int((lat>=0).sum())}")

    durations = np.array([e["duration_s"] for e in excursions])
    peaks = np.array([e["peak_departure"] for e in excursions if np.isfinite(e["peak_departure"])])
    medians = np.array([e["line_segment_median"] for e in excursions if np.isfinite(e["line_segment_median"])])
    print(f"[{label}] duration_s: median={np.median(durations):.3f} min={durations.min():.3f} max={durations.max():.3f}")
    if len(peaks):
        print(f"[{label}] peak_departure: median={np.median(np.abs(peaks)):.1f} min={np.abs(peaks).min():.1f} max={np.abs(peaks).max():.1f}")
    if len(medians):
        print(f"[{label}] line_segment_median: range {medians.min():.1f} .. {medians.max():.1f}")

    json.dump({
        "label": label, "window_blocks": window_blocks, "threshold": threshold,
        "n_blocks": int(len(block_t0)), "n_excursions": len(excursions),
        "excursions": excursions,
    }, open(out_json, "w"), indent=1)

    np.savez_compressed(out_json.replace(".json", "_trace.npz"),
                         block_t0=block_t0, hall=hall, pwm_blk=pwm_blk,
                         valid=valid, db_trace=db_trace)
    print(f"[{label}] wrote {out_json} and trace npz")


if __name__ == "__main__":
    main()
