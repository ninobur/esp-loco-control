#!/usr/bin/env python3
"""Cut a transport GAP and a sampler MISSED hole out of a synthetic IR_SCOPE
CSV. The GAP is placed so the cut starts mid-pulse AND resumes mid-pulse
(recorded in_pulse=1 on both sides) — the worst case for replay state.
"""
import csv, sys

src, dst = sys.argv[1], sys.argv[2]
rows = list(csv.reader(open(src)))
hdr, data = rows[0], rows[1:]
S = {name: i for i, name in enumerate(hdr)}

def ip(r):
    return r[S["in_pulse"]] == "1"

def sample(r):
    return int(r[S["sample"]])

# GAP: start at the first in_pulse row after sample 2000, end at the first
# in_pulse row after sample 2600 (both mid-pulse).
g0 = next(i for i, r in enumerate(data) if sample(r) >= 2000 and ip(r))
g1 = next(i for i, r in enumerate(data) if sample(r) >= 2600 and ip(r))
# MISSED: 3 slots at sample >= 5000
m0 = next(i for i, r in enumerate(data) if sample(r) >= 5000)
m1 = m0 + 3

out = []
for i, r in enumerate(data):
    if g0 <= i < g1:
        if i == g0:
            out.append(["", "GAP", "aaaaaaaa", "", "", "", "", "", "", "", "",
                        "", "", "", "", "synthetic transport gap"])
        continue
    if m0 <= i < m1:
        if i == m0:
            out.append(["", "MISSED", "aaaaaaaa", "", "", "", "", "", "", "",
                        "", "", "", "", "", "3 slot(s) skipped by sampler stall"])
        continue
    out.append(r)

with open(dst, "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(hdr)
    w.writerows(out)

print("gap cut: samples %d..%d (start ip=%s, resume ip=%s); missed: %d..%d"
      % (sample(data[g0]), sample(data[g1]) - 1, ip(data[g0]), ip(data[g1]),
         sample(data[m0]), sample(data[m1]) - 1))
