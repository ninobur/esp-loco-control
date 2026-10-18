"""Annotation (evaluation truth and training labels). Frozen before any model is built.

Extents: the frozen ADC-only Hall-ground encounters (hall-ground-20261015, native
stream, 'supported_activity'), which were produced without openings, map or PWM.
Identity: NAVI's recorded openings (scoring/annotation only, never recognizer input).
  - an opening is attached to the supported encounter that contains it, else the
    nearest one whose edge is within 300 ms;
  - an encounter carrying openings of two different magnets is kept and flagged MERGED;
  - an unlabeled supported encounter lying between consecutive-in-sequence openings
    k-1 and k+1 (same direction) is labeled k, source SEQUENCE;
  - any other supported encounter without identity is UNLABELED activity;
  - 'uncertain_instability' encounters are kept as UNCERTAIN.
Baseline: the frozen ground intervals. Nothing here is physical ground truth."""
import numpy as np, pandas as pd, json
from common import *
e = pd.read_csv(HG/'encounters_adc_only.csv'); e = e[(e.boot == BOOT1) & (e.stream == 'native')].sort_values('start_us').reset_index(drop=True)
g = pd.read_csv(HG/'ground_intervals.csv'); g = g[(g.boot == BOOT1) & (g.stream == 'native')]
s = pd.read_csv(RP/'selections_scoring_only.csv'); s = s[s.boot == BOOT1].sort_values('open_us').reset_index(drop=True)
mp = pd.read_csv(RP/'map_positions.csv').set_index('mm')
types = json.load(open(NG/'navi-ewo-0-3-ir-validity/field-records/20260828_magnet_types.json'))
sup = e[e.classification == 'supported_activity'].reset_index(drop=True)
st, en = sup.start_us.values, sup.end_us.values
att = {}
r_idx = set()
for r in s.itertuples():
    inside = np.where((st <= r.open_us) & (en >= r.open_us))[0]
    if len(inside): k = inside[0]
    else:
        d = np.minimum(np.abs(st - r.open_us), np.abs(en - r.open_us)); k = int(d.argmin())
        if d[k] > 300_000: continue
    att.setdefault(k, []).append(r)
    r_idx.add(r.Index)
rows = []
for k, enc in sup.iterrows():
    ops = att.get(k, [])
    tg = sorted({o.target for o in ops})
    rows.append(dict(enc_id=int(enc.encounter_id), start_us=int(enc.start_us), end_us=int(enc.end_us),
                     mm=tg[0] if len(tg) == 1 else (-1 if not tg else tg[0]), dir=(ops[0].dir if ops else 0),
                     source='OPENING' if len(tg) == 1 else ('MERGED' if tg else 'UNLABELED'),
                     n_openings=len(ops), targets='|'.join(map(str, tg))))
a = pd.DataFrame(rows)
# sequence fill: unlabeled between k-1 and k+1 in same direction
for i in range(1, len(a)-1):
    if a.source[i] != 'UNLABELED': continue
    p, q = a.iloc[i-1], a.iloc[i+1]
    if p.source == 'OPENING' and q.source == 'OPENING' and p.dir == q.dir:
        step = 1 if p.dir == 1 else -1
        if (p.mm + 2*step) % 171 == q.mm:
            a.loc[i, ['mm', 'dir', 'source']] = [(p.mm+step) % 171, p.dir, 'SEQUENCE']
# openings attached to no encounter (encounter censored at a serial gap by the frozen
# ADC-only rule): identity known, extent unknown -> NO_EXTENT (scored +-300 ms, no boundary stats)
ne = [dict(enc_id=-1, start_us=int(r.open_us), end_us=int(r.open_us), mm=int(r.target), dir=int(r.dir), source='NO_EXTENT',
           n_openings=1, targets=str(r.target)) for r in s.itertuples() if r.Index not in r_idx]
a = pd.concat([a, pd.DataFrame(ne)], ignore_index=True)
a['pole'] = [int(mp.pole.get(m, 0)) if m >= 0 else 0 for m in a.mm]
a['type'] = [types.get(str(m), '') if m >= 0 else '' for m in a.mm]
unc = e[e.classification == 'uncertain_instability'][['encounter_id', 'start_us', 'end_us']].rename(columns={'encounter_id': 'enc_id'})
unc['source'] = 'UNCERTAIN'; unc['mm'] = -1; unc['dir'] = 0; unc['pole'] = 0; unc['type'] = ''
ann = pd.concat([a, unc]).sort_values('start_us').reset_index(drop=True)
ann['dur_ms'] = (ann.end_us - ann.start_us)/1000
ann.to_csv(D/'annotation_boot1.csv', index=False)
g[['ground_id', 'start_us', 'end_us', 'duration_s']].to_csv(D/'ground_boot1.csv', index=False)
summ = ann.source.value_counts().to_dict()
summ['openings'] = len(s); summ['openings_attached'] = int(sum(len(v) for v in att.values()))
json.dump(summ, open(RES/'annotation_summary.json', 'w'), indent=1); print(summ)
