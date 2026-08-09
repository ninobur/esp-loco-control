#!/usr/bin/env python3
"""Uniform-merging synthetic: troughs alternate deep (1200) / shallow
(1900), so at 1/3 EVERY pulse merges exactly two spokes and every interval
is exactly two spoke periods — uniform, so interval ratios all read 1.0 and
the interval method reports a healthy 7.00 pulses/rev. Only the waveform
structure (two peaks per pulse) exposes it. 0.50 recovers all spokes.
"""
import csv, math, sys

OUT = sys.argv[1]
RUNMIN, RUNMAX = 1000, 3000
SPAN = RUNMAX - RUNMIN
THR_HIGH = RUNMIN + (SPAN * 2) // 3      # 2333
THR_LOW  = RUNMIN + SPAN // 3            # 1666
PERIOD = 150.0
N_SPOKES = 7 * 8

ext = [(-22.5, 1200.0)]
t = 0.0
for k in range(N_SPOKES + 1):
    ext.append((t + 0.35 * PERIOD, 3000.0))
    trough = 1900.0 if (k % 2 == 0) else 1200.0   # after even spokes: shallow
    ext.append((t + 0.85 * PERIOD, trough))
    t += PERIOD
TOTAL = int(N_SPOKES * PERIOD)

def waveform(tq):
    for j in range(len(ext) - 1):
        t0, v0 = ext[j]
        t1, v1 = ext[j + 1]
        if t0 <= tq <= t1:
            x = (tq - t0) / (t1 - t0)
            return int(v0 + (v1 - v0) * (1 - math.cos(math.pi * x)) / 2)
    return int(ext[-1][1])

rows = []
in_pulse = False
last_edge = -1e9
pulse_start = 0.0
for n in range(TOTAL):
    raw = waveform(float(n))
    rise = fall = 0
    if in_pulse and (n - pulse_start) > 2500.0:
        in_pulse = False
    if not in_pulse:
        if raw > THR_HIGH and (n - last_edge) > 15.0:
            in_pulse, last_edge, pulse_start, rise = True, n, n, 1
    else:
        if raw < THR_LOW:
            in_pulse, fall = False, 1
    rows.append(["", "SAMPLE", "dddddddd", n // 200, n, "%.3f" % (n / 1000),
                 raw, RUNMIN, RUNMAX, THR_HIGH, THR_LOW, 1,
                 1 if in_pulse else 0, rise, fall, ""])

# Operator revolution markers: one per 7 spokes (hand-turn protocol).
markers = []
for r in range(N_SPOKES // 7 + 1):
    tm = r * 7 * PERIOD
    if tm <= TOTAL:
        markers.append(["", "MARKER", "dddddddd", "", int(tm),
                        "%.3f" % (tm / 1000), "", "", "", "", "", "", "",
                        "", "", "rev %d" % r])

with open(OUT, "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["wall_time","row_type","session","batch_seq","sample","t_s",
                "raw","run_min","run_max","thr_high","thr_low",
                "contrast_valid","in_pulse","rise","fall","info"])
    w.writerows(rows)
    w.writerows(markers)
print("wrote %d samples + %d rev markers, %d spokes; at 1/3 every pulse "
      "merges 2 spokes UNIFORMLY" % (len(rows), len(markers), N_SPOKES))
