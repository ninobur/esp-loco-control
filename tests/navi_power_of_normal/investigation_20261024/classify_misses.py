#!/usr/bin/env python3
"""For each missed OPENING annotation, classify whether it falls inside an
abandoned (never-returned-to-baseline) encounter's span, versus an isolated
miss unexplained by that mechanism. Usage:
  python3 classify_misses.py <annotation_boot1.csv> <onsets.csv> <encounters.csv>
"""
import csv, sys

ann_path = sys.argv[1] if len(sys.argv) > 1 else 'analysis/navi_rvm_shape_20261018/derived/annotation_boot1.csv'
onsets_path = sys.argv[2] if len(sys.argv) > 2 else '/tmp/navi_power_of_normal_replay/onsets.csv'
encounters_path = sys.argv[3] if len(sys.argv) > 3 else '/tmp/navi_power_of_normal_replay/encounters.csv'

ann = list(csv.DictReader(open(ann_path)))
openings = [r for r in ann if r['source'] == 'OPENING']

onsets = []
with open(onsets_path) as f:
    next(f)
    for line in f:
        t, serial, pol, lvl = line.strip().split(',')
        onsets.append(int(t))

encounters = []
with open(encounters_path) as f:
    next(f)
    for line in f:
        open_us, close_us, open_block, width, pol, peak, meets, aband = line.strip().split(',')
        encounters.append((int(open_us), int(close_us), int(aband)))

MARGIN = 100_000

inside_abandoned = 0
isolated = 0
isolated_list = []
for r in openings:
    s, e = int(r['start_us']), int(r['end_us'])
    if any(s - MARGIN <= t <= e + MARGIN for t in onsets):
        continue
    swallowed = any(ab and ou <= s and e <= cu for (ou, cu, ab) in encounters)
    if swallowed:
        inside_abandoned += 1
    else:
        isolated += 1
        isolated_list.append(r)

print("missed, swallowed inside an abandoned encounter:", inside_abandoned)
print("missed, isolated (no covering abandoned encounter):", isolated)
print()
print("isolated misses (enc_id,start_us,end_us,mm,dir,pole,type,dur_ms):")
for r in isolated_list:
    print(r['enc_id'], r['start_us'], r['end_us'], r['mm'], r['dir'], r['pole'], r['type'], r['dur_ms'])
