"""Boot-1 replay WITH the STOP/GO WaveformPause.
1. Leave-one-pass-out (same folds, same per-fold models as s10: the model is
   unchanged, only the waveform the recognizer sees changes). Each fold start is
   treated as a declaration, as in s10.
2. Continuous replay from the recorded declarations with the deployable model
   (model_A.json): nav rows for s12's harness (nav_replay_pause.csv).
3. Firmware parity: tests/navi_rvm_shape/replay.cpp (with RvmPause.h) vs this.
Writes results/local_pause/."""
import numpy as np, pandas as pd, json, subprocess, tempfile, pathlib
from common import *
import rvm_shape as R
import rvm_local as L
import stopgo as G
import s10_local_experiment as X
from s11_nav_replay import Navigator

OUT = ROOT/'results'/'local_pause'; OUT.mkdir(exist_ok=True)
REPO = ROOT.parents[1]
pwm = G.block_pwm(len(X.h))
np.save(D/'block_pwm_boot1.npy', pwm)

# ---- 1. LOPO ----
TR, UM, DET = [], [], []
for f in range(X.NF):
    a, b = X.edges[f], X.edges[f + 1]
    train = X.ens[(X.ens.fold != f) & ~X.ens.censored & X.ens.source.isin(['OPENING', 'SEQUENCE'])]
    spans = [(X.edges[k], X.edges[k + 1]) for k in range(X.NF) if k != f]
    Em, Nm, info = X.build(train, spans, 40000)            # identical to s10 (same rng sequence)
    starts = [a] + [d for d in X.decl_i if a < d < b]
    keep = G.admit_mask(pwm[a:b], [s - a for s in starts])
    C = G.Compressed(X.h[a:b], X.ccw[a:b], keep)
    det = L.run(L.Tracing(C.h, C.ccw), Em, Nm, [(C.start_of(s - a), 'start') for s in starts])
    if len(det):
        for col in ('c', 's', 'e', 'decided'): det[col] = C.to_ids(det[col]) + a
        det['bl_start'] = C.to_ids(det['bl_start']) + a
    det['fold'] = f; DET.append(det)
    tr, um = X.score(det, X.ens[X.ens.fold == f], a, b); tr['fold'] = f; um['fold'] = f
    TR.append(tr); UM.append(um)
    print(dict(fold=f, admitted=int(keep.sum()), blocks=int(b - a), recognized=len(det)), flush=True)
tr = pd.concat(TR, ignore_index=True); um = pd.concat(UM, ignore_index=True); det = pd.concat(DET, ignore_index=True)
tr.to_csv(OUT/'truth_A_all.csv', index=False); um.to_csv(OUT/'unmatched_A_all.csv', index=False); det.to_csv(OUT/'detections.csv', index=False)
ext = tr[~tr.censored]; m = ext[ext.n_det == 1]
lopo = dict(with_extent=len(ext), recognized_once=int((ext.n_det == 1).sum()), missed=int((ext.n_det == 0).sum()),
            split=int((ext.n_det > 1).sum()), unmatched=len(um), unmatched_on_ground=int(um.on_ground.sum()),
            unmatched_on_other_activity=int(um.near_other.sum()),
            polarity_ok=int((tr[tr.n_det >= 1].pol == tr[tr.n_det >= 1].pole).sum()), polarity_checked=int((tr.n_det >= 1).sum()),
            d_mid_ms=dict(median=float(m.d_mid_ms.abs().median()), p95=float(m.d_mid_ms.abs().quantile(.95)), max=float(m.d_mid_ms.abs().max())),
            missed_list=ext[ext.n_det == 0][['mm', 't_s']].to_dict('records'))

# ---- 2. continuous replay with the deployable model ----
mj = json.load(open(ROOT/'results'/'local'/'model_A.json'))
def gmm(c):
    g = R.GMM(len(c['w'])); g.w = np.array(c['w']); g.mu = np.array(c['mu']); g.S = np.array(c['S']); return g
Em, Nm = gmm(mj['E']), gmm(mj['N'])
keep = G.admit_mask(pwm, X.decl_i)
C = G.Compressed(X.h, X.ccw, keep)
py = L.run(L.Tracing(C.h, C.ccw), Em, Nm, [(C.start_of(d), 'decl') for d in X.decl_i])
py['c_id'] = np.floor(py.c).astype(int).map(lambda i: int(C.ids[min(i, len(C.ids) - 1)]))
py['c_orig'] = C.to_ids(py.c); py['decided_orig'] = C.to_ids(py.decided)
py.to_csv(OUT/'python_reference_detections.csv', index=False)
sd = pd.read_csv(RP/'declarations.csv'); sd = sd[sd.boot == BOOT1]
dir_at = lambda us: (1 if sd[(sd.t_us <= us) & sd.topic.str.contains('session_direction')].payload.iloc[-1] == 'CW' else -1)
events = [(int(i), 'decl', r) for r, i in zip(X.decl.itertuples(), X.decl_i)] + [(float(r.decided_orig), 'ens', r) for r in py.itertuples()]
events.sort(key=lambda e: (e[0], 0 if e[1] == 'decl' else 1))
nav = Navigator(); rows = []; truth = X.ens[X.ens.source != 'NO_EXTENT']
for t, kind, r in events:
    if kind == 'decl':
        a_, b_ = map(int, r.payload.split('-')); d = dir_at(r.t_us); nav.declare(a_, b_, d)
        rows.append(dict(t_s=X.t_us[int(t)] / 1e6, event='DECLARE', payload=r.payload, dir=d)); continue
    st, mm, expected = nav.on_ensemble(int(r.pol))
    mt = truth[(truth.s_i - 5 <= r.c_orig) & (truth.e_i + 5 >= r.c_orig)]
    tmm = int(mt.mm.iloc[0]) if len(mt) else -1
    rows.append(dict(t_s=X.t_us[int(r.c_orig)] / 1e6, decided_s=X.t_us[min(int(r.decided_orig), len(X.h) - 1)] / 1e6, event=st, mm=mm,
                     expected=expected, pol=int(r.pol), truth_mm=tmm, llr=float(r.llr), correct=(mm == tmm) if mm is not None else None))
pd.DataFrame(rows).to_csv(ROOT/'results'/'local'/'nav_replay_pause.csv', index=False)

# ---- 3. firmware parity ----
tmp = pathlib.Path(tempfile.mkdtemp(prefix='rvm_pause_parity_')); (tmp/'out').mkdir()
SK = REPO/'firmware'/'test-programs'/'NAVI_RVM_SHAPE_MANUAL'
subprocess.run(['c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror', f'-I{SK}', str(REPO/'tests'/'navi_rvm_shape'/'replay.cpp'), '-o', str(tmp/'replay')], check=True)
with open(tmp/'decl.csv', 'w') as fh:
    for r, i in zip(X.decl.itertuples(), X.decl_i):
        a_, b_ = map(int, r.payload.split('-')); d = dir_at(r.t_us)
        fh.write('%d,%d,%d,%d\n' % ((i,) + ((a_, b_) if d > 0 else (b_, a_)) + (d,)))
res = subprocess.run([str(tmp/'replay'), str(HG/f'native_{BOOT1}.bin'), str(tmp/'decl.csv'), str(tmp/'out')], capture_output=True, text=True, check=True)
stats = json.loads(res.stdout.strip().splitlines()[-1])
ce = pd.read_csv(tmp/'out'/'ensembles.csv'); ce.to_csv(OUT/'cpp_detections.csv', index=False)
cn = pd.read_csv(tmp/'out'/'nav.csv'); cn.to_csv(OUT/'cpp_nav.csv', index=False)
same_n = len(ce) == len(py)
parity = dict(python=len(py), cpp=len(ce), same_count=same_n,
              identical_waveform_centre=int((np.abs(ce.c.values - py.c.values) < 1e-9).sum()) if same_n else None,
              identical_block_id=int((ce.c_id.values == py.c_id.values).sum()) if same_n else None,
              polarity_same=int((ce.pol.values == py.pol.values).sum()) if same_n else None,
              max_llr_diff=float(np.abs(ce.llr.values - py.llr.values).max()) if same_n else None,
              cpp_runtime=stats, cpp_nav_events=cn.event.value_counts().to_dict())
summary = dict(lopo_with_pause=lopo, parity=parity)
json.dump(summary, open(OUT/'summary.json', 'w'), indent=1, default=float)
print(json.dumps(summary, indent=1, default=float))
