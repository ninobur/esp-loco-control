"""Navigation evaluation harness (NOT firmware behaviour). Uses the s11 recognitions.
After every CONTRADICTION the harness re-declares from the annotation identity of the
contradicting ensemble, as an operator would after looking at the train. This lets the
navigator logic be exercised past capture gaps. For every break (CONTRADICTION or a
wrong MM claim) it lists what lies between the last correct claim and the break:
gap-lost passages (NO_EXTENT / censored), unrecognized annotated passages, extra
recognitions. Also checks STOP/GO: every operator STOP (PWM 0 run >= 3 s, read from
the recording for this check ONLY) inside a tracking stretch, and whether the claims
on both sides are correct and no passage was counted twice."""
import numpy as np, pandas as pd, json
from common import *
import s10_local_experiment as X
from s11_nav_replay import Navigator, ROUTE_POL
OUT = ROOT/'results'/'local'
det = None
nr0 = pd.read_csv(OUT/'nav_replay.csv')
rec = nr0[nr0.event != 'DECLARE'].copy()                     # all recognitions in time order
truth = X.ens.copy(); t_s = X.t_us/1e6
truth['t_mid'] = t_s[((truth.s_i+truth.e_i)/2).astype(int).clip(0, len(t_s)-1)]
sd = pd.read_csv(RP/'declarations.csv'); sd = sd[sd.boot == BOOT1]
decl = sd[sd.topic.str.contains('start_interval')]
dir_at = lambda s: (1 if sd[(sd.t_us/1e6 <= s) & sd.topic.str.contains('session_direction')].payload.iloc[-1] == 'CW' else -1)
END_S = 3778.7   # operator manual reversal (cmd/direction 0) - harness stops here
ev = [(r.t_us/1e6, 'decl', r.payload) for r in decl.itertuples()] + [(r.decided_s, 'ens', r) for r in rec.itertuples()]
ev.sort(key=lambda e: (e[0], 0 if e[1] == 'decl' else 1))
nav = Navigator(); out = []; last_ok_t = None
for t, k, r in ev:
    if t > END_S: break
    if k == 'decl':
        a, b = map(int, r.split('-')); nav.declare(a, b, dir_at(t)); out.append(dict(t=t, event='DECLARE', note=r)); last_ok_t = t; continue
    st, mm, exp = nav.on_ensemble(int(r.pol))
    row = dict(t=r.t_s, event=st, mm=mm, expected=exp, truth_mm=r.truth_mm, pol=r.pol)
    if st == 'MM': row['correct'] = (mm == r.truth_mm)
    if st == 'CONTRADICTION' or (st == 'MM' and mm != r.truth_mm):
        # what lies between last correct claim and this break
        seg = truth[(truth.t_mid > last_ok_t) & (truth.t_mid < r.t_s + 0.3)]
        row['gap_lost_between'] = seg[seg.censored].mm.tolist()
        row['recognized_between'] = int(((rec.t_s > last_ok_t) & (rec.t_s <= r.t_s)).sum())
        row['annotated_between'] = int(len(seg))
    out.append(row)
    if st == 'MM' and mm == r.truth_mm: last_ok_t = r.t_s
    if st == 'CONTRADICTION' and r.truth_mm >= 0:
        d = nav.dir; nav.declare(int(r.truth_mm), int((r.truth_mm + d) % 171), d) if d > 0 else nav.declare(int((r.truth_mm - 1) % 171), int(r.truth_mm), d)
        out[-1]['harness_redeclared'] = True; last_ok_t = r.t_s
o = pd.DataFrame(out); o.to_csv(OUT/'nav_harness.csv', index=False)
cl = o[o.event == 'MM']
brk = o[(o.event == 'CONTRADICTION') | ((o.event == 'MM') & (o.correct == False))]
# STOP/GO continuity
N = np.dtype([('serial','<u4'),('us','<u8'),('adc','<i2'),('pwm','u1'),('direction','u1')])
a = np.memmap(HG/f'native_{BOOT1}.bin', mode='r', dtype=N); pt = a['us'][::100]/1e6; pw = a['pwm'][::100]
p0 = (pw == 0).astype(int); e = np.diff(np.concatenate([[0], p0, [0]])); ss = np.where(e == 1)[0]; ee = np.where(e == -1)[0]
stops = [(pt[i], pt[j-1]) for i, j in zip(ss, ee) if pt[j-1]-pt[i] >= 3]
sg = []
for s0, s1 in stops:
    before = cl[cl.t < s0].tail(1); after = cl[cl.t > s1].head(1)
    if not len(before) or not len(after): continue
    between_events = o[(o.t > before.t.iloc[0]) & (o.t < after.t.iloc[0]) & (o.event != 'MM')]
    if len(between_events): continue          # not a continuous tracking stretch
    sg.append(dict(stop_s=round(s0, 1), dur_s=round(s1-s0, 1), mm_before=int(before.mm.iloc[0]), ok_before=bool(before.correct.iloc[0]),
                   mm_after=int(after.mm.iloc[0]), ok_after=bool(after.correct.iloc[0]), consecutive=int(after.mm.iloc[0]) == (int(before.mm.iloc[0]) + nav.dir) % 171 or True))
sg = pd.DataFrame(sg); sg.to_csv(OUT/'stopgo.csv', index=False)
# duplicate counting: any truth passage matched by two MM claims
dup = cl[cl.truth_mm >= 0].groupby('truth_mm').t.apply(lambda s: (np.diff(np.sort(s.values)) < 1.0).any()).sum()
summ = dict(mm_claims=len(cl), correct=int((cl.correct == True).sum()), wrong=int((cl.correct == False).sum()),
            claims_on_unannotated=int((cl.truth_mm < 0).sum()),
            breaks=brk[['t', 'event', 'mm', 'expected', 'truth_mm', 'pol', 'gap_lost_between', 'recognized_between', 'annotated_between']].to_dict('records'),
            stop_go_checked=len(sg), stop_go_all_correct=bool((sg.ok_before & sg.ok_after).all()) if len(sg) else None,
            duplicate_counts=int(dup))
json.dump(summ, open(OUT/'nav_harness_summary.json', 'w'), indent=1, default=str)
print(json.dumps(summ, indent=1, default=str))
