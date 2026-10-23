#!/usr/bin/env python3
"""Build confirmed magnet-to-magnet segment dataset: mapped distance,
elapsed time, average speed, PWM, and independently-measured physical
waveform duration -- from real recorded data, not from the evaluator."""
import sys, json, csv, bisect
import numpy as np
from exp3_parse_closes import parse

# RouteMap.h ROUTE_SPACING_MM, verbatim
ROUTE_SPACING_MM = [
  330,340,330,315,325,330,315,300,300,295,
  300,335,324,360,315,325,310,300,300,320,
  315,315,305,300,295,300,300,300,300,315,
  330,320,315,310,300,300,300,300,300,300,
  300,300,300,300,300,300,300,300,300,300,
  300,300,300,300,300,300,300,295,320,300,
  315,320,315,325,315,305,300,305,300,300,
  295,295,300,300,300,300,300,300,300,305,
  300,300,305,300,300,330,300,300,305,300,
  300,300,300,300,300,300,300,300,300,300,
  300,300,295,300,300,300,300,300,305,300,
  300,320,320,300,300,300,300,300,300,300,
  300,300,300,300,300,300,280,300,300,290,
  300,300,300,300,300,300,300,300,300,300,
  300,300,300,300,305,300,305,300,295,300,
  300,300,305,300,300,305,320,290,320,300,
  300,305,330,330,320,325,315,355,330,330,
  330,
]
ROUTE_N = 171

def span_mm(mm, direction):
    if direction > 0:
        return ROUTE_SPACING_MM[mm % ROUTE_N]
    return ROUTE_SPACING_MM[(mm - 1) % ROUTE_N]


def load_native_csv(path):
    """Returns two views of the same data: recv-sorted (for recv_us-based
    lookups: nearest_device_t, pwm_near) and t_us-sorted (for anything
    that bins/walks in device-clock time order, e.g. waveform measurement)."""
    recv, t_us, adc, pwm = [], [], [], []
    with open(path) as f:
        r = csv.reader(f)
        next(r)
        for row in r:
            recv.append(int(row[0])); t_us.append(int(row[2]))
            adc.append(int(row[3])); pwm.append(int(row[4]))
    recv = np.array(recv); t_us = np.array(t_us); adc = np.array(adc); pwm = np.array(pwm)
    rsort = np.argsort(recv, kind="stable")
    tsort = np.argsort(t_us, kind="stable")
    by_recv = (recv[rsort], t_us[rsort], adc[rsort], pwm[rsort])
    by_t = (t_us[tsort], adc[tsort])
    return by_recv, by_t


def nearest_device_t(recv_sorted, t_us_sorted, target_recv_us):
    i = bisect.bisect_left(recv_sorted, target_recv_us)
    i = min(max(i, 0), len(recv_sorted) - 1)
    return t_us_sorted[i]


def pwm_near(recv_sorted, pwm_sorted, target_recv_us, span=200000):
    lo = bisect.bisect_left(recv_sorted, target_recv_us - span)
    hi = bisect.bisect_left(recv_sorted, target_recv_us + span)
    if hi <= lo:
        return None
    return float(np.median(pwm_sorted[lo:hi]))


def measure_waveform_duration(adc_by_t_us, t_us_sorted, adc_sorted, center_device_t_us,
                               search_half_window_us=1_500_000, bg_half_window_us=1_000_000,
                               sigma_mult=3.0):
    """Independent physical waveform width: bin to 20ms around the known
    approximate encounter time, establish background from the flanks, and
    measure start/end via the first/last 2-consecutive-block departure
    from that background -- same convention used elsewhere."""
    BLOCK_US = 20000
    lo_t = center_device_t_us - search_half_window_us
    hi_t = center_device_t_us + search_half_window_us
    m = (t_us_sorted >= lo_t) & (t_us_sorted <= hi_t)
    if m.sum() < 50:
        return None
    tt = t_us_sorted[m]; aa = adc_sorted[m]
    t0 = tt[0]
    blk = (tt - t0) // BLOCK_US
    nb = int(blk[-1]) + 1
    cnt = np.bincount(blk, minlength=nb)
    sums = np.bincount(blk, weights=aa.astype(float), minlength=nb)
    hall = np.full(nb, np.nan)
    ok = cnt >= 50
    hall[ok] = sums[ok] / cnt[ok]
    bt0 = t0 + np.arange(nb) * BLOCK_US
    # background from first and last ~bg_half_window_us/BLOCK_US blocks
    nbg = max(5, bg_half_window_us // BLOCK_US)
    left_bg = hall[:nbg][np.isfinite(hall[:nbg])]
    right_bg = hall[-nbg:][np.isfinite(hall[-nbg:])]
    bg_all = np.concatenate([left_bg, right_bg])
    if len(bg_all) < 10:
        return None
    med, std = np.median(bg_all), np.std(bg_all)
    band = max(3.0, sigma_mult * std)
    dep = np.abs(hall - med)
    exceed = dep > band
    # first 2-consecutive exceed
    start_i = None
    for i in range(len(exceed) - 1):
        if np.isfinite(hall[i]) and np.isfinite(hall[i+1]) and exceed[i] and exceed[i+1]:
            start_i = i
            break
    if start_i is None:
        return None
    # last 2-consecutive exceed (scanning backward)
    end_i = None
    for i in range(len(exceed) - 2, -1, -1):
        if np.isfinite(hall[i]) and np.isfinite(hall[i+1]) and exceed[i] and exceed[i+1]:
            end_i = i + 1
            break
    if end_i is None or end_i <= start_i:
        return None
    peak_idx = int(np.argmax(np.abs(hall[start_i:end_i+1] - med))) + start_i
    return {
        "bg_median": float(med), "bg_std": float(std),
        "onset_t_us": float(bt0[start_i]), "end_t_us": float(bt0[end_i]),
        "duration_s": float((bt0[end_i] - bt0[start_i]) / 1e6),
        "peak_t_us": float(bt0[peak_idx]), "peak_adc": float(hall[peak_idx]),
        "peak_departure": float(hall[peak_idx] - med),
    }


def main():
    log_path = sys.argv[1]
    csv_path = sys.argv[2]
    label = sys.argv[3]
    exclude_mm = set(int(x) for x in sys.argv[4].split(",")) if len(sys.argv) > 4 and sys.argv[4] else set()

    closes = parse(log_path)
    (recv, t_us_by_recv, adc_by_recv, pwm), (t_us, adc) = load_native_csv(csv_path)

    segments = []
    for i in range(len(closes) - 1):
        a, b = closes[i], closes[i + 1]
        if a["mm"] in exclude_mm or b["mm"] in exclude_mm:
            continue
        d = span_mm(a["mm"], a["dir"])
        dt_s = (b["wall_us"] - a["wall_us"]) / 1e6
        if dt_s <= 0:
            continue
        v_mm_s = d / dt_s
        dev_t_at_b = nearest_device_t(recv, t_us_by_recv, b["wall_us"])
        pwm_at_b = pwm_near(recv, pwm, b["wall_us"])
        # Bound the search window by neighboring confirmed closes, so it
        # never reaches into the adjacent magnet's own excursion -- half
        # the gap to the previous close, half the gap to the next one
        # (falling back to the b<->next-a gap itself if there's no
        # further neighbor), capped at 1.5s either way.
        prev_gap_s = (b["wall_us"] - closes[i]["wall_us"]) / 1e6
        next_gap_s = ((closes[i + 2]["wall_us"] - b["wall_us"]) / 1e6) if i + 2 < len(closes) else prev_gap_s
        half_back = min(1.5, max(0.15, prev_gap_s * 0.5))
        half_fwd = min(1.5, max(0.15, next_gap_s * 0.5))
        safe_half = min(half_back, half_fwd)
        wf = measure_waveform_duration(None, t_us, adc, dev_t_at_b,
                                        search_half_window_us=int(safe_half * 1e6),
                                        bg_half_window_us=int(safe_half * 0.6 * 1e6))
        segments.append({
            "label": label, "mm_from": a["mm"], "mm_to": b["mm"], "dir": a["dir"],
            "mapped_distance_mm": d, "elapsed_s": dt_s, "v_mm_s": v_mm_s,
            "pwm_at_arrival": pwm_at_b, "device_t_us_at_arrival": float(dev_t_at_b),
            "waveform": wf,
        })
    json.dump(segments, open(sys.argv[5], "w"), indent=1)
    print(f"[{label}] {len(segments)} segments written to {sys.argv[5]}")
    for s in segments:
        wf = s["waveform"]
        dur = wf["duration_s"] if wf else None
        pk = wf["peak_departure"] if wf else None
        print(f"  mm {s['mm_from']:>3}->{s['mm_to']:>3}  v={s['v_mm_s']:.1f}mm/s  "
              f"pwm={s['pwm_at_arrival']}  elapsed={s['elapsed_s']:.2f}s  "
              f"wf_dur={dur if dur is None else round(dur,3)}  wf_peak_dep={pk if pk is None else round(pk,1)}")


if __name__ == "__main__":
    main()
