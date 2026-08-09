#!/usr/bin/env python3
"""Synthetic IR_SCOPE CSV: 7-spoke wheel, every 3rd inter-spoke trough is
shallow (1900 counts) so it stays above thrLow at 1/3 (1666) and 0.40 (1800)
but below 0.50 (2000). Continuous waveform built by half-cosine interpolation
between explicit peak/trough extremes. Recorded flags use the firmware's 1/3
semantics so the replay's self-validation should pass.
"""
import csv, math, sys

OUT = sys.argv[1]
RUNMIN, RUNMAX = 1000, 3000
SPAN = RUNMAX - RUNMIN
THR_HIGH = RUNMIN + (SPAN * 2) // 3      # 2333
THR_LOW  = RUNMIN + SPAN // 3            # 1666
PERIOD = 150.0                           # spoke-to-spoke ms
N_SPOKES = 7 * 8                         # 8 revolutions

# Extremes: trough(k) at k*150+127.5, peak(k) at k*150+52.5, alternating.
ext = [(-22.5, 1200.0)]                  # trough before spoke 0
for k in range(N_SPOKES + 1):
    ext.append((k * PERIOD + 52.5, 3000.0))                      # peak k
    trough = 1900.0 if (k % 3 == 2) else 1200.0                  # trough after k
    ext.append((k * PERIOD + 127.5, trough))

def waveform(t):
    for j in range(len(ext) - 1):
        t0, v0 = ext[j]
        t1, v1 = ext[j + 1]
        if t0 <= t <= t1:
            x = (t - t0) / (t1 - t0)
            return int(v0 + (v1 - v0) * (1 - math.cos(math.pi * x)) / 2)
    return int(ext[-1][1])

rows = []
in_pulse = False
last_edge = -1e9
DEB = 15.0
total = int(N_SPOKES * PERIOD)
for n in range(total):
    raw = waveform(float(n))
    rise = fall = 0
    if not in_pulse:
        if raw > THR_HIGH and (n - last_edge) > DEB:
            in_pulse, last_edge, rise = True, n, 1
    else:
        if raw < THR_LOW:
            in_pulse, fall = False, 1
    rows.append(["", "SAMPLE", "aaaaaaaa", n // 200, n, "%.3f" % (n / 1000),
                 raw, RUNMIN, RUNMAX, THR_HIGH, THR_LOW, 1,
                 1 if in_pulse else 0, rise, fall, ""])

with open(OUT, "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["wall_time","row_type","session","batch_seq","sample","t_s",
                "raw","run_min","run_max","thr_high","thr_low",
                "contrast_valid","in_pulse","rise","fall","info"])
    w.writerows(rows)

exp_merged = sum(1 for k in range(N_SPOKES) if k % 3 == 2)
print("wrote %d samples; %d shallow troughs -> expect ~%d merged at 1/3, ~%d pulses"
      % (len(rows), exp_merged, exp_merged, N_SPOKES - exp_merged))
