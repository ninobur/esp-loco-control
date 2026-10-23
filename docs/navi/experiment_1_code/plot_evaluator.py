#!/usr/bin/env python3
"""Plot Hall trace with the rolling evaluator's LINE/CURVE classification
overlaid, for independent review. Reconstructs the state timeline from the
saved excursion list (CURVE regions) against the full binned trace."""
import sys, json
import numpy as np
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

result_json = sys.argv[1]
trace_npz = sys.argv[2]
t_lo = float(sys.argv[3])
t_hi = float(sys.argv[4])
out_png = sys.argv[5]
title = sys.argv[6] if len(sys.argv) > 6 else ""

d = json.load(open(result_json))
tr = np.load(trace_npz)
block_t0, hall, pwm_blk = tr["block_t0"], tr["hall"], tr["pwm_blk"]

m = (block_t0 >= t_lo) & (block_t0 <= t_hi)
t = (block_t0[m] - t_lo) / 1e6
h = hall[m]

fig, ax = plt.subplots(figsize=(14, 5))
ax.plot(t, h, '.', ms=2, color='tab:blue', label='20ms Hall mean')

for e in d["excursions"]:
    s, en = e["curve_start_t_us_est"], e["curve_end_t_us"]
    if en < t_lo or s > t_hi:
        continue
    ax.axvspan((max(s, t_lo) - t_lo) / 1e6, (min(en, t_hi) - t_lo) / 1e6,
               color='tab:red', alpha=0.2)
    if e.get("line_segment_median") == e.get("line_segment_median"):  # not nan
        ax.annotate(f"med={e['line_segment_median']:.0f}", (( max(s,t_lo)-t_lo)/1e6, h.max()),
                    fontsize=7, rotation=90, va='top', color='gray')

ax.set_xlabel(f"seconds since device t_us={t_lo:.0f}")
ax.set_ylabel("Hall ADC (20ms mean)")
ax.set_title(title + "  (red shading = evaluator CURVE state)")
ax.legend(loc='upper right')
fig.tight_layout()
fig.savefig(out_png, dpi=130)
print("wrote", out_png)
