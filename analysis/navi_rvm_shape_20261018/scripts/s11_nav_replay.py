"""Deployable model (trained on ALL of boot 1) + continuous navigation replay.

1. Fits Model A with the local-baseline features on every boot-1 ensemble with an
   extent and writes the model for the firmware (results/local/model_A.json).
   Recognition accuracy must be read from s10 (leave-one-pass-out); this replay is
   in-sample for recognition and exists to exercise the NAVIGATION logic.
2. Runs the sequential recognizer over the whole of boot 1, started at each
   recorded operator declaration (start_interval), and the navigator:
     declare "a-b" + direction -> CW: last=a, next=b; CCW: last=b, next=a
     recognized ensemble, polarity == ROUTE polarity of next -> MM = next, advance
     recognized ensemble, polarity != ROUTE polarity of next -> CONTRADICTION;
         no further geographic claims until the next declaration
3. Scores every navigator claim against the annotation identity."""
import numpy as np, pandas as pd, json
from common import *
import rvm_shape as R
import rvm_local as L
import s10_local_experiment as X        # data, annotation, build() (runs nothing heavy at import? see guard)

OUT = ROOT / 'results' / 'local'
ROUTE_POL = np.loadtxt(ROOT / 'scripts' / 'route_polarity.txt', dtype=int)    # 1 = North = Hall above baseline (Otto)


class Navigator:
    def __init__(self): self.state = 'UNDECLARED'; self.next = None; self.dir = 0; self.last = None

    def declare(self, a, b, d):
        self.dir = d; self.state = 'TRACKING'
        self.last, self.next = (a, b) if d > 0 else (b, a)

    def on_ensemble(self, pol):
        if self.state != 'TRACKING': return 'NO_GEO_' + self.state, None, None
        exp = self.next; want = 1 if ROUTE_POL[exp] == 1 else -1
        if pol == want:
            self.last = exp; self.next = (exp + self.dir) % 171
            return 'MM', exp, exp
        self.state = 'CONTRADICTION'
        return 'CONTRADICTION', None, exp


if __name__ == '__main__':
    train = X.ens[~X.ens.censored & X.ens.source.isin(['OPENING', 'SEQUENCE'])]
    Em, Nm, info = X.build(train, [(0, len(X.h))], 40000)
    model = dict(note='Model A, local-baseline features, trained on all boot-1 ensembles (2026-10-15, Otto)',
                 NPT=R.NPT, NDCT=R.NDCT, SCALES=[int(s) for s in R.SCALES], MIN_BL=L.MIN_BL, REG=R.REG, train=info,
                 E=dict(w=Em.w.tolist(), mu=Em.mu.tolist(), S=Em.S.tolist()), N=dict(w=Nm.w.tolist(), mu=Nm.mu.tolist(), S=Nm.S.tolist()))
    json.dump(model, open(OUT / 'model_A.json', 'w'))
    dec = X.decl.copy(); dec['i'] = X.decl_i
    starts = [(int(r.i), f'decl {r.payload}') for r in dec.itertuples()]
    det = L.run(X.T, Em, Nm, starts)
    # declaration direction: from session_direction commands
    sd = pd.read_csv(RP / 'declarations.csv'); sd = sd[(sd.boot == BOOT1) & sd.topic.str.contains('session_direction')]
    dir_at = lambda us: (1 if sd[sd.t_us <= us].payload.iloc[-1] == 'CW' else -1)
    events = [(int(r.i), 'decl', r) for r in dec.itertuples()] + [(float(r.decided), 'ens', r) for r in det.itertuples()]
    events.sort(key=lambda e: (e[0], 0 if e[1] == 'decl' else 1))
    nav = Navigator(); rows = []
    truth = X.ens[X.ens.source != 'NO_EXTENT']
    for t, kind, r in events:
        if kind == 'decl':
            a, b = [int(x) for x in r.payload.split('-')]; d = dir_at(r.t_us)
            nav.declare(a, b, d); rows.append(dict(t_s=X.t_us[int(t)] / 1e6, event='DECLARE', payload=r.payload, dir=d)); continue
        status, mm, expected = nav.on_ensemble(int(r.pol))
        m = truth[(truth.s_i - 5 <= r.c) & (truth.e_i + 5 >= r.c)]
        tmm = int(m.mm.iloc[0]) if len(m) else -1
        rows.append(dict(t_s=X.t_us[int(r.c)] / 1e6, decided_s=X.t_us[min(int(r.decided), len(X.h) - 1)] / 1e6, event=status, mm=mm, expected=expected,
                         pol=int(r.pol), truth_mm=tmm, llr=float(r.llr), correct=(mm == tmm) if mm is not None else None))
    nr = pd.DataFrame(rows); nr.to_csv(OUT / 'nav_replay.csv', index=False)
    claims = nr[nr.event == 'MM']
    summ = dict(recognitions=int((nr.event != 'DECLARE').sum()), mm_claims=len(claims),
                claims_correct=int((claims.correct == True).sum()), claims_wrong=int((claims.correct == False).sum()),
                claims_without_truth=int((claims.truth_mm < 0).sum()),
                contradictions=nr[nr.event == 'CONTRADICTION'][['t_s', 'expected', 'pol', 'truth_mm']].to_dict('records'),
                silent_after_contradiction=int((nr.event == 'CONTRADICTION').sum()))
    json.dump(summ, open(OUT / 'nav_summary.json', 'w'), indent=1, default=float)
    print(json.dumps(summ, indent=1, default=float))
