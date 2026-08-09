#!/usr/bin/env python3
"""CODEX fix-7 reproduction: GAP, resync-low at t=100, high at 105, low at
110. A rise the real detector accepted may sit inside the gap < 15 ms before
t=105, so the 105-110 pulse must NOT be attributed — it belongs inside the
resync-unknown span. Also a second variant (gen_repro9): a QUIET post-gap
stretch (mid-band, never above thrHigh) that must still be counted/rendered
as an unknown span.
"""
import csv, sys

OUT = sys.argv[1]
MODE = sys.argv[2] if len(sys.argv) > 2 else "repro7"
MN, MX = 1000, 3000
HI = MN + ((MX - MN) * 2) // 3   # 2333
LO = MN + (MX - MN) // 3         # 1666

rows = []

def sample(n, raw, ip=0, rise=0, fall=0):
    rows.append(["", "SAMPLE", "ffffffff", n // 200, n, "%.3f" % (n / 1000),
                 raw, MN, MX, HI, LO, 1, ip, rise, fall, ""])

# pre-gap: one clean pulse so the record has context (samples 0..49)
for n in range(0, 20):
    sample(n, 1200)
sample(20, 2500, ip=1, rise=1)
for n in range(21, 30):
    sample(n, 2500, ip=1)
sample(30, 1200, fall=1)
for n in range(31, 50):
    sample(n, 1200)

rows.append(["", "GAP", "ffffffff", "", "", "", "", "", "", "", "", "", "",
             "", "", "synthetic transport gap"])

if MODE == "repro7":
    # resync-low at 100, high 105-109, low from 110
    for n in range(100, 105):
        sample(n, 1200)
    for n in range(105, 110):
        sample(n, 2500)          # recorded flags unknowable here; leave idle
    for n in range(110, 160):
        sample(n, 1200)
else:  # repro9: quiet resync — mid-band, never above thrHigh, then low
    for n in range(100, 130):
        sample(n, 2000)          # between LO (1666) and HI (2333)
    for n in range(130, 170):
        sample(n, 1200)

with open(OUT, "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["wall_time","row_type","session","batch_seq","sample","t_s",
                "raw","run_min","run_max","thr_high","thr_low",
                "contrast_valid","in_pulse","rise","fall","info"])
    w.writerows(rows)
print("wrote %s (%s): expected 1 completed pre-gap pulse and NO attributed "
      "post-gap pulse" % (OUT, MODE))
