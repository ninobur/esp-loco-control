"""SYNTHETIC CHARACTERIZATION (software behaviour, not railway evidence).
What does the local-baseline recognizer do when the Hall level is held still in
the middle of a magnetic ensemble (locomotive stopped with the sensor over a
magnet)? Real recorded stretches from boot 1 (MM050-MM070 passes) are copied; in
one ensemble per stretch the tracing is frozen at a chosen point of the
principal excursion for D seconds (the held value plus the stretch's own
baseline noise), then resumes unchanged. The deployable model (model_A.json) and
the firmware-equivalent sequential recognizer run from a declaration 3 s before.
Reported per case: is the held ensemble counted once, and are the following five
ensembles recognized (does the reference recover)."""
import numpy as np, pandas as pd, json
from common import *
import rvm_shape as R
import rvm_local as L
import s10_local_experiment as X
OUT = ROOT/'results'/'local'
m = json.load(open(OUT/'model_A.json'))
def gmm(c):
    g = R.GMM(len(c['w'])); g.w = np.array(c['w']); g.mu = np.array(c['mu']); g.S = np.array(c['S']); return g
Em, Nm = gmm(m['E']), gmm(m['N'])
rng = np.random.default_rng(5)
truth = X.ens[(X.ens.source == 'OPENING') & ~X.ens.censored].sort_values('s_i').reset_index(drop=True)
cases = []
targets = truth[truth.mm.isin(range(52, 66))].groupby(truth.fold).head(1)          # one ensemble per pass
for r in targets.itertuples():
    k = truth.index[truth.ann_index == r.ann_index][0] if 'ann_index' in truth else truth.index[(truth.s_i == r.s_i)][0]
    nxt = truth.iloc[k + 1:k + 6]
    a = int(r.s_i) - 150; b = int(nxt.e_i.max()) + 100
    base = X.h[a:b].astype(float).copy(); ccw = X.ccw[a:b].copy()
    if not np.isfinite(base).all(): continue
    s_rel, e_rel = int(r.s_i) - a, int(r.e_i) - a
    seg = base[s_rel:e_rel + 1]; lvl = np.median(base[:100]); pk = s_rel + int(np.argmax(np.abs(seg - lvl)))
    noise = np.std(np.diff(base[:100])) / np.sqrt(2)
    for where, at in (('rising_half', s_rel + (pk - s_rel) // 2), ('peak', pk)):
        for D in (0, 1, 3, 5, 8, 15, 30):
            n = int(D * 50)
            held = base[at] + rng.normal(0, noise, n)
            h2 = np.concatenate([base[:at], held, base[at:]]); c2 = np.concatenate([ccw[:at], np.repeat(ccw[at], n), ccw[at:]])
            T = L.Tracing(h2, c2)
            det = L.run(T, Em, Nm, [(0, 'decl')])
            shift = lambda i: i + (n if i >= at else 0)
            held_s, held_e = s_rel, e_rel + n
            on_held = det[(det.c >= held_s - 5) & (det.c <= held_e + 5)]
            later = [((det.c >= shift(int(q.s_i) - a) - 5) & (det.c <= shift(int(q.e_i) - a) + 5)).sum() for q in nxt.itertuples()]
            cases.append(dict(mm=int(r.mm), dir=int(r.dir), where=where, hold_s=D, held_counted=int(len(on_held)),
                              next5_recognized=int(sum(1 for x in later if x == 1)), next5_split=int(sum(1 for x in later if x > 1)),
                              extra=int(len(det) - len(on_held) - sum(later))))
c = pd.DataFrame(cases); c.to_csv(OUT/'hold_characterization.csv', index=False)
summ = c.groupby(['where', 'hold_s']).agg(cases=('mm', 'size'), held_counted_once=('held_counted', lambda s: int((s == 1).sum())),
        held_missed=('held_counted', lambda s: int((s == 0).sum())), held_split=('held_counted', lambda s: int((s > 1).sum())),
        next5_recognized=('next5_recognized', 'sum'), extra=('extra', 'sum')).reset_index()
summ['next5_of'] = summ.cases * 5
print(summ.to_string()); json.dump(summ.to_dict('records'), open(OUT/'hold_characterization_summary.json', 'w'), indent=1)
