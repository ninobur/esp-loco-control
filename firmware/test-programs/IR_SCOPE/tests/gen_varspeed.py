#!/usr/bin/env python3
"""Variable-speed synthetic: spoke period ramps 130 -> 300 ms (hand-push
deceleration), ALL troughs deep (1200 — every spoke resolvable at 1/3).
Correct classification: ~0 merged, ~7.00 pulses/rev at every candidate.
A pooled p25 base would misread the slow half as merged.
"""
import csv, math, sys

OUT = sys.argv[1]
RUNMIN, RUNMAX = 1000, 3000
SPAN = RUNMAX - RUNMIN
THR_HIGH = RUNMIN + (SPAN * 2) // 3
THR_LOW  = RUNMIN + SPAN // 3
N_SPOKES = 7 * 10

ext = [(-30.0, 1200.0)]
t = 0.0
for k in range(N_SPOKES + 1):
    period = 130.0 + (300.0 - 130.0) * k / N_SPOKES
    ext.append((t + 0.35 * period, 3000.0))
    ext.append((t + 0.85 * period, 1200.0))
    t += period
TOTAL = int(t)

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
for n in range(TOTAL):
    raw = waveform(float(n))
    rise = fall = 0
    if not in_pulse:
        if raw > THR_HIGH and (n - last_edge) > 15.0:
            in_pulse, last_edge, rise = True, n, 1
    else:
        if raw < THR_LOW:
            in_pulse, fall = False, 1
    rows.append(["", "SAMPLE", "bbbbbbbb", n // 200, n, "%.3f" % (n / 1000),
                 raw, RUNMIN, RUNMAX, THR_HIGH, THR_LOW, 1,
                 1 if in_pulse else 0, rise, fall, ""])

with open(OUT, "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["wall_time","row_type","session","batch_seq","sample","t_s",
                "raw","run_min","run_max","thr_high","thr_low",
                "contrast_valid","in_pulse","rise","fall","info"])
    w.writerows(rows)
print("wrote %d samples, %d spokes, period 130->300 ms; correct answer: 0 merged"
      % (len(rows), N_SPOKES))
