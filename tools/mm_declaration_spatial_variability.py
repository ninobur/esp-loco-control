#!/usr/bin/env python3
"""Spatial variability of Hall magnet declaration points vs IR-measured travel.

Extends the September 20 noon run alignment (ir_noon_analysis.py) with the
per-event Hall waveform characteristics (peak, ratio, shape residual,
polarity, gap, open/close timing) that the noon audit computes internally
but does not publish, and with repeated-pass grouping across the run's three
circuits. Produces a per-interval CSV and a JSON summary answering the
NAVI_COHERENCE eligibility-window question. No pulse repair, no recalibration
of IR against the map, no assumption that the mapped magnet center and the
Hall declaration point are the same physical location.

Reuses parse_ir(), stamp(), quant(), stats(), TZ and NOTES from
ir_noon_analysis.py verbatim; ir_noon_analysis.py itself is not modified and
remains independently reproducible.
"""
import argparse
import collections
import csv
import datetime as dt
import json
import math
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from ir_noon_analysis import parse_ir, stamp, quant, stats, TZ, NOTES

MM_PER_PULSE = 9.652
ROUTE_N = 171


def parse_hall_rich(path):
    """Like ir_noon_analysis's inline hall parse, but keeps every marker and
    wave_meta field instead of narrowing to (time, mm, trust, count, offset_ms)."""
    alerts, markers, events, metadata = [], [], [], None
    with open(path, errors='replace') as source:
        for line in source:
            s = line.rstrip().split('\t', 2)
            if len(s) != 3 or '/9950012/' not in s[1]:
                continue
            try:
                t = dt.datetime.fromisoformat(s[0]).replace(tzinfo=TZ).timestamp()
                d = json.loads(s[2])
            except ValueError:
                continue
            if not NOTES[0][1] - 180 <= t <= NOTES[-1][1] + 60:
                continue
            if s[1].endswith('/alert'):
                alerts.append(dict(t=t, **d))
            elif s[1].endswith('/diag/wave_meta'):
                metadata = (t, d)
            elif s[1].endswith('/mm/marker'):
                markers.append(dict(t=t, **d))
                if d.get('ruling') == 'ADVANCED' and d.get('why') == 'MAGNET' and metadata and 0 <= t - metadata[0] < 1:
                    events.append(dict(receipt=t, wave=metadata[1], **d))
                metadata = None
    return alerts, markers, events


def align_events(ir_path, hall_path, spacing):
    import bisect
    raw, motion, bad = parse_ir(ir_path)
    raw = [x for x in raw if x['boot'] == 0xe4410597]
    motion = [x for x in motion if x['boot'] & 0xffffffff == 0xe4410597]
    assert len({x['boot'] for x in motion}) == 1
    offsets = [x['t'] - x['captured_us'] / 1e6 for x in motion]
    offset = quant(offsets, .05)
    for x in motion:
        x['aligned_t'] = offset + x['captured_us'] / 1e6
    motion.sort(key=lambda x: x['aligned_t'])
    times = [x['aligned_t'] for x in motion]

    def near(t):
        i = bisect.bisect_left(times, t)
        return min(motion[max(0, i - 1):min(len(motion), i + 1)], key=lambda x: abs(x['aligned_t'] - t))

    alerts, markers, events = parse_hall_rich(hall_path)
    for e in events:
        local = [x['t'] - x['uptime_ms'] / 1000 for x in alerts if abs(x['t'] - e['receipt']) < 30]
        e['t'] = e['wave']['close_ms'] / 1000 + quant(local, .05)
        x = near(e['t'])
        e.update(ir_count=x['completed_pulses'], ir_offset_ms=(x['aligned_t'] - e['t']) * 1000,
                  unreliable=x['unreliable_samples'])
    events.sort(key=lambda x: x['t'])
    assert len(events) == 513, 'expected the known 513-event noon run; got %d' % len(events)
    assert all((y['mm'] - x['mm']) % ROUTE_N == 1 and x['dir'] == y['dir'] == 'CW'
               for x, y in zip(events, events[1:]))
    return events


def build_intervals(events, spacing, offset_gate_ms=150.0):
    intervals = []
    pass_seen = collections.Counter()
    for a, b in zip(events, events[1:]):
        if a['dir'] != 'CW' or b['dir'] != 'CW' or (b['mm'] - a['mm']) % ROUTE_N != 1:
            continue
        offset_ms = max(abs(a['ir_offset_ms']), abs(b['ir_offset_ms']))
        if offset_ms > offset_gate_ms:
            continue
        count = b['ir_count'] - a['ir_count']
        map_mm = spacing[a['mm']]
        ir_mm = count * MM_PER_PULSE
        signed_residual_mm = ir_mm - map_mm
        pass_seen[a['mm']] += 1
        aw, bw = a['wave'], b['wave']
        intervals.append(dict(
            prev_mm=a['mm'], next_mm=b['mm'],
            prev_time=stamp(a['t']), next_time=stamp(b['t']),
            direction='CW',
            map_mm=map_mm, ir_mm=round(ir_mm, 3), pulses=count,
            signed_residual_mm=round(signed_residual_mm, 3),
            abs_residual_mm=round(abs(signed_residual_mm), 3),
            pct_residual=round(100.0 * signed_residual_mm / map_mm, 4),
            ratio=round(ir_mm / map_mm, 6),
            seconds=round(b['t'] - a['t'], 3),
            offset_ms=round(offset_ms, 1),
            unreliable_delta=b['unreliable'] - a['unreliable'],
            pass_index=pass_seen[a['mm']],
            prev_peak=a.get('peak'), prev_ratio=a.get('ratio'), prev_resid=a.get('resid'),
            prev_obs=a.get('obs'), prev_gap_ms=a.get('gap_ms'),
            prev_open_ms=aw.get('open_ms'), prev_close_ms=aw.get('close_ms'),
            prev_duration_ms=(aw.get('close_ms', 0) - aw.get('open_ms', 0)) if aw else None,
            prev_pwm_actual_close=aw.get('pwm_actual_close'),
            next_peak=b.get('peak'), next_ratio=b.get('ratio'), next_resid=b.get('resid'),
            next_obs=b.get('obs'), next_gap_ms=b.get('gap_ms'),
            next_open_ms=bw.get('open_ms'), next_close_ms=bw.get('close_ms'),
            next_duration_ms=(bw.get('close_ms', 0) - bw.get('open_ms', 0)) if bw else None,
            next_pwm_actual_close=bw.get('pwm_actual_close'),
        ))
    return intervals


def mean(v):
    return sum(v) / len(v) if v else None


def sd(v, ddof=0):
    if len(v) <= ddof:
        return None
    m = mean(v)
    return math.sqrt(sum((x - m) ** 2 for x in v) / (len(v) - ddof))


def pearson(xs, ys):
    pts = [(x, y) for x, y in zip(xs, ys) if x is not None and y is not None]
    if len(pts) < 3:
        return None, len(pts)
    xs, ys = zip(*pts)
    mx, my = mean(xs), mean(ys)
    sxy = sum((x - mx) * (y - my) for x, y in zip(xs, ys))
    sxx = sum((x - mx) ** 2 for x in xs)
    syy = sum((y - my) ** 2 for y in ys)
    if sxx == 0 or syy == 0:
        return None, len(pts)
    return sxy / math.sqrt(sxx * syy), len(pts)


def percentile(values, p):
    if not values:
        return None
    v = sorted(values)
    k = (len(v) - 1) * p
    f, c = math.floor(k), math.ceil(k)
    if f == c:
        return v[int(k)]
    return v[f] + (v[c] - v[f]) * (k - f)


def distribution(values):
    return dict(
        n=len(values), mean=mean(values), sd=sd(values, ddof=1),
        minimum=min(values) if values else None,
        p05=percentile(values, .05), p10=percentile(values, .10),
        p25=percentile(values, .25), median=percentile(values, .50),
        p75=percentile(values, .75), p90=percentile(values, .90),
        p95=percentile(values, .95),
        maximum=max(values) if values else None,
    )


def repeatability(intervals):
    groups = collections.defaultdict(list)
    for iv in intervals:
        groups[iv['prev_mm']].append(iv)
    per_interval = []
    grand_vals = []
    for mm, rows in sorted(groups.items()):
        rows = sorted(rows, key=lambda r: r['prev_time'])
        residuals = [r['signed_residual_mm'] for r in rows]
        pct = [r['pct_residual'] for r in rows]
        grand_vals.extend(residuals)
        per_interval.append(dict(
            prev_mm=mm, next_mm=rows[0]['next_mm'], map_mm=rows[0]['map_mm'],
            repeats=len(rows),
            residuals_mm=residuals, pct_residuals=pct,
            mean_residual_mm=mean(residuals), sd_residual_mm=sd(residuals, ddof=1) if len(residuals) > 1 else None,
            range_residual_mm=(max(residuals) - min(residuals)) if len(residuals) > 1 else None,
            mean_pct=mean(pct), sd_pct=sd(pct, ddof=1) if len(pct) > 1 else None,
        ))
    # One-way ANOVA / variance-components decomposition, restricted to
    # intervals with >=2 surviving repeats (repeatability requires repeats).
    multi = [g for g in per_interval if g['repeats'] >= 2]
    all_multi_vals = [v for g in multi for v in g['residuals_mm']]
    grand_mean = mean(all_multi_vals)
    ss_between = sum(g['repeats'] * (g['mean_residual_mm'] - grand_mean) ** 2 for g in multi)
    ss_within = sum((v - g['mean_residual_mm']) ** 2 for g in multi for v in g['residuals_mm'])
    ss_total = ss_between + ss_within
    df_within = sum(g['repeats'] - 1 for g in multi)
    df_between = len(multi) - 1
    anova = dict(
        n_intervals_with_repeats=len(multi),
        n_observations=len(all_multi_vals),
        grand_mean_residual_mm=grand_mean,
        ss_between=ss_between, ss_within=ss_within, ss_total=ss_total,
        frac_variance_between=(ss_between / ss_total) if ss_total else None,
        frac_variance_within=(ss_within / ss_total) if ss_total else None,
        between_group_sd_mm=math.sqrt(ss_between / df_between) if df_between > 0 else None,
        pooled_within_group_sd_mm=math.sqrt(ss_within / df_within) if df_within > 0 else None,
        repeat_count_histogram=dict(collections.Counter(g['repeats'] for g in per_interval)),
    )
    # Same decomposition in percent-of-map terms, to remove interval-length effects.
    all_multi_pct = [v for g in multi for v in g['pct_residuals']]
    grand_mean_pct = mean(all_multi_pct)
    ss_between_pct = sum(g['repeats'] * (g['mean_pct'] - grand_mean_pct) ** 2 for g in multi)
    ss_within_pct = sum((v - g['mean_pct']) ** 2 for g in multi for v in g['pct_residuals'])
    ss_total_pct = ss_between_pct + ss_within_pct
    anova_pct = dict(
        grand_mean_pct=grand_mean_pct,
        frac_variance_between=(ss_between_pct / ss_total_pct) if ss_total_pct else None,
        frac_variance_within=(ss_within_pct / ss_total_pct) if ss_total_pct else None,
        between_group_sd_pct=math.sqrt(ss_between_pct / df_between) if df_between > 0 else None,
        pooled_within_group_sd_pct=math.sqrt(ss_within_pct / df_within) if df_within > 0 else None,
    )
    return per_interval, anova, anova_pct


def per_magnet_offset_signature(per_interval):
    """Test for a genuine per-magnet declaration-offset effect: correlate,
    across magnets, the mean residual of the interval ENDING at mm against
    the mean residual of the interval STARTING at mm. A negative correlation
    is the expected signature of a real per-magnet offset (see report for
    derivation); map-spacing error alone predicts ~zero correlation."""
    by_start = {g['prev_mm']: g for g in per_interval}
    by_end = {g['next_mm']: g for g in per_interval}
    pairs = []
    for mm in range(ROUTE_N):
        a = by_end.get(mm)
        b = by_start.get(mm)
        if a and b and a['repeats'] >= 2 and b['repeats'] >= 2:
            pairs.append((mm, a['mean_residual_mm'], b['mean_residual_mm']))
    xs = [p[1] for p in pairs]
    ys = [p[2] for p in pairs]
    r, n = pearson(xs, ys)
    implied_offset_sd_mm = math.sqrt(-sd(xs, ddof=1) * sd(ys, ddof=1) * r) if (r is not None and r < 0) else None
    return dict(n_magnets=n, correlation=r, r_squared=(r * r if r is not None else None),
                implied_per_magnet_offset_sd_mm=implied_offset_sd_mm,
                mean_in_sd_mm=sd(xs, ddof=1), mean_out_sd_mm=sd(ys, ddof=1))


def correlations(intervals):
    abs_res = [iv['abs_residual_mm'] for iv in intervals]
    signed_res = [iv['signed_residual_mm'] for iv in intervals]
    pct_res = [iv['pct_residual'] for iv in intervals]
    out = {}
    for label in ('prev_peak', 'prev_ratio', 'prev_resid', 'prev_gap_ms', 'prev_duration_ms',
                  'next_peak', 'next_ratio', 'next_resid', 'next_gap_ms', 'next_duration_ms',
                  'map_mm'):
        xs = [iv[label] for iv in intervals]
        r_abs, n_abs = pearson(xs, abs_res)
        r_signed, _ = pearson(xs, signed_res)
        r_pct, _ = pearson(xs, pct_res)
        out[label] = dict(n=n_abs, r_vs_abs_residual_mm=r_abs, r_vs_signed_residual_mm=r_signed,
                           r_vs_pct_residual=r_pct)
    # Polarity: split by the declaring (next) event's observed pole.
    by_pole = collections.defaultdict(list)
    for iv in intervals:
        by_pole[iv['next_obs']].append(iv['pct_residual'])
    polarity = {pole: dict(n=len(v), mean_pct=mean(v), sd_pct=sd(v, ddof=1)) for pole, v in by_pole.items()}
    return out, polarity


def eligibility_window(intervals, per_interval, windows=(0.02, 0.05, 0.08, 0.10, 0.12, 0.15, 0.20)):
    n = len(intervals)
    raw_ratio = [iv['ratio'] for iv in intervals]
    out_raw = {}
    for w in windows:
        inside = sum(1 for r in raw_ratio if (1 - w) <= r <= (1 + w))
        out_raw[w] = dict(inside=inside, outside=n - inside, pct_inside=100.0 * inside / n)
    # Same test applied to each interval's OWN mean (systematic component only).
    means = [g['mean_pct'] / 100.0 + 1 for g in per_interval if g['repeats'] >= 1]
    out_systematic = {}
    for w in windows:
        inside = sum(1 for r in means if (1 - w) <= r <= (1 + w))
        out_systematic[w] = dict(inside=inside, outside=len(means) - inside, pct_inside=100.0 * inside / len(means))
    return dict(n_single_pass=n, by_window_single_pass=out_raw,
                n_interval_means=len(means), by_window_interval_mean=out_systematic)


def ir_noise_floor(intervals):
    """Quantify the two known, non-Hall sources of scatter so the leftover
    repeat-to-repeat variance can be judged against a floor: IR count
    quantization (+/-1 pulse) and endpoint clock-alignment jitter converted
    to distance via each interval's own average speed."""
    quant_pct = [100.0 * MM_PER_PULSE / iv['map_mm'] for iv in intervals]
    speed_mm_s = [iv['ir_mm'] / iv['seconds'] if iv['seconds'] else None for iv in intervals]
    jitter_mm = [(iv['offset_ms'] / 1000.0) * s for iv, s in zip(intervals, speed_mm_s) if s is not None]
    return dict(
        quantization_pct_of_map=distribution(quant_pct),
        endpoint_offset_ms=distribution([iv['offset_ms'] for iv in intervals]),
        speed_mm_s=distribution([s for s in speed_mm_s if s is not None]),
        implied_alignment_jitter_mm=distribution(jitter_mm),
    )


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('ir')
    p.add_argument('hall')
    p.add_argument('route_header')
    p.add_argument('--csv-out', default=None)
    p.add_argument('--json-out', default=None)
    args = p.parse_args()

    with open(args.route_header) as f:
        initializer = re.search(r'ROUTE_SPACING_MM\[ROUTE_N\]\s*=\s*\{([^}]+)\}', f.read()).group(1)
    spacing = [int(v.strip()) for v in initializer.split(',') if v.strip()]
    assert len(spacing) == ROUTE_N and sum(spacing) == 52150

    events = align_events(args.ir, args.hall, spacing)
    intervals = build_intervals(events, spacing)
    per_interval, anova_mm, anova_pct = repeatability(intervals)
    offset_signature = per_magnet_offset_signature(per_interval)
    corr, polarity = correlations(intervals)
    window = eligibility_window(intervals, per_interval)
    noise_floor = ir_noise_floor(intervals)

    summary = dict(
        sources=vars(args),
        n_intervals=len(intervals),
        ratio_distribution=distribution([iv['ratio'] for iv in intervals]),
        abs_residual_mm_distribution=distribution([iv['abs_residual_mm'] for iv in intervals]),
        pct_residual_distribution=distribution([iv['pct_residual'] for iv in intervals]),
        map_mm_distribution=distribution([iv['map_mm'] for iv in intervals]),
        repeatability_anova_mm=anova_mm,
        repeatability_anova_pct=anova_pct,
        per_magnet_offset_signature=offset_signature,
        correlations_with_hall_characteristics=corr,
        residual_by_declared_polarity=polarity,
        eligibility_window_check=window,
        ir_noise_floor=noise_floor,
        per_interval_repeatability=per_interval,
    )

    if args.csv_out:
        fields = ['prev_mm', 'next_mm', 'direction', 'pass_index', 'map_mm', 'ir_mm', 'pulses',
                  'signed_residual_mm', 'abs_residual_mm', 'pct_residual', 'ratio',
                  'seconds', 'offset_ms', 'unreliable_delta',
                  'prev_time', 'next_time',
                  'prev_peak', 'prev_ratio', 'prev_resid', 'prev_obs', 'prev_gap_ms', 'prev_duration_ms',
                  'next_peak', 'next_ratio', 'next_resid', 'next_obs', 'next_gap_ms', 'next_duration_ms']
        with open(args.csv_out, 'w', newline='') as f:
            w = csv.DictWriter(f, fieldnames=fields, extrasaction='ignore')
            w.writeheader()
            for iv in intervals:
                w.writerow(iv)

    if args.json_out:
        with open(args.json_out, 'w') as f:
            json.dump(summary, f, indent=2, default=str)
    else:
        print(json.dumps(summary, indent=2, default=str))


if __name__ == '__main__':
    main()
