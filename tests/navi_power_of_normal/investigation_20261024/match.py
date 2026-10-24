#!/usr/bin/env python3
"""Match replay.cpp's onsets.csv against annotation_boot1.csv's OPENING rows.
Part of the 2026-10-24 investigation into the 924-vs-1,087 onset shortfall
(see docs/navi/... investigation report). Usage:
  python3 match.py <annotation_boot1.csv> <onsets.csv>
"""
import csv, sys

ann_path = sys.argv[1] if len(sys.argv) > 1 else 'analysis/navi_rvm_shape_20261018/derived/annotation_boot1.csv'
onsets_path = sys.argv[2] if len(sys.argv) > 2 else '/tmp/navi_power_of_normal_replay/onsets.csv'

ann = list(csv.DictReader(open(ann_path)))
openings = [r for r in ann if r['source'] == 'OPENING']
print("total OPENING annotations:", len(openings))

onsets = []
with open(onsets_path) as f:
    next(f)
    for line in f:
        t, serial, pol, lvl = line.strip().split(',')
        onsets.append((int(t), int(serial), int(pol), float(lvl)))

MARGIN_US = 100_000  # 100 ms either side of the annotated extent

def find_onset(start_us, end_us):
    lo, hi = start_us - MARGIN_US, end_us + MARGIN_US
    return [o for o in onsets if lo <= o[0] <= hi]

missed = []
matched = 0
for r in openings:
    s, e = int(r['start_us']), int(r['end_us'])
    if find_onset(s, e):
        matched += 1
    else:
        missed.append(r)

print("matched:", matched, "missed:", len(missed))
print()
print("missed (enc_id,start_us,end_us,mm,dir,pole,type,dur_ms):")
for r in missed:
    print(r['enc_id'], r['start_us'], r['end_us'], r['mm'], r['dir'], r['pole'], r['type'], r['dur_ms'])

extra = []
for t, serial, pol, lvl in onsets:
    hit = any(int(r['start_us']) - MARGIN_US <= t <= int(r['end_us']) + MARGIN_US for r in openings)
    if not hit:
        extra.append((t, serial, pol, lvl))
print()
print("onsets with no matching OPENING annotation within +-100ms:", len(extra))
for x in extra[:20]:
    print(x)
