#!/usr/bin/env python3
"""Latch/open synthetic: 2 s of normal spokes, then a 3.5 s plateau at 2100
counts (entered via a real peak) — above thrLow at 1/3 (1666), 0.40 (1800)
and 0.50 (2000) so those candidates LATCH at 2500 ms, but below 0.60's bar
(2200) so 0.60 falls normally. Then normal spokes again, and the record
ends mid-pulse (open_end). Recorded flags replicate firmware semantics
INCLUDING the 2500 ms latch discard (inPulse clears with no fall flag).
"""
import csv, math, sys

OUT = sys.argv[1]
RUNMIN, RUNMAX = 1000, 3000
SPAN = RUNMAX - RUNMIN
THR_HIGH = RUNMIN + (SPAN * 2) // 3      # 2333
THR_LOW  = RUNMIN + SPAN // 3            # 1666
PERIOD = 150.0

ext = [(-22.5, 1200.0)]
t = 0.0
for k in range(13):                      # ~2 s of normal spokes
    ext.append((t + 0.35 * PERIOD, 3000.0))
    ext.append((t + 0.85 * PERIOD, 1200.0))
    t += PERIOD
ext.append((t + 50.0, 3000.0))           # rise into the plateau
ext.append((t + 150.0, 2100.0))          # settle at 2100
ext.append((t + 3500.0, 2100.0))         # hold 3.35 s (latch fires at 2.5 s)
ext.append((t + 3650.0, 1200.0))         # finally a real dark state
t += 3800.0
for k in range(6):                       # normal spokes again
    ext.append((t + 0.35 * PERIOD, 3000.0))
    ext.append((t + 0.85 * PERIOD, 1200.0))
    t += PERIOD
ext.append((t + 60.0, 3000.0))           # final rise...
TOTAL = int(t + 60.0)                    # ...record ends ON the peak: open

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
    if in_pulse and (n - pulse_start) > 2500.0:      # firmware latch discard
        in_pulse = False                             # no fall flag — honest
    if not in_pulse:
        if raw > THR_HIGH and (n - last_edge) > 15.0:
            in_pulse, last_edge, pulse_start, rise = True, n, n, 1
    else:
        if raw < THR_LOW:
            in_pulse, fall = False, 1
    rows.append(["", "SAMPLE", "cccccccc", n // 200, n, "%.3f" % (n / 1000),
                 raw, RUNMIN, RUNMAX, THR_HIGH, THR_LOW, 1,
                 1 if in_pulse else 0, rise, fall, ""])

with open(OUT, "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["wall_time","row_type","session","batch_seq","sample","t_s",
                "raw","run_min","run_max","thr_high","thr_low",
                "contrast_valid","in_pulse","rise","fall","info"])
    w.writerows(rows)
print("wrote %d samples; expect latch=1 at 1/3,0.40,0.50; latch=0 at 0.60; open=1 all"
      % len(rows))
