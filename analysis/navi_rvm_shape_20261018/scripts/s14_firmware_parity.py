"""Parity: firmware C++ path (tests/navi_rvm_shape/replay.cpp with the sketch's
RvmTracing.h / RvmShape.h / RvmNavigator.h / RvmShapeModel.h) versus the Python
reference (rvm_local.py) on the whole of boot 1, same model (model_A.json), same
declarations. Writes results/local/parity.json."""
import numpy as np, pandas as pd, json, subprocess, pathlib
from common import *
import rvm_shape as R
import rvm_local as L
import s10_local_experiment as X
OUT = ROOT/'results'/'local'; REPO = ROOT.parents[1]
import tempfile
TMP = pathlib.Path(tempfile.mkdtemp(prefix='rvm_parity_'))
BIN = TMP/'replay'; CPP_OUT = TMP/'out'; CPP_OUT.mkdir()
SK = REPO/'firmware'/'test-programs'/'NAVI_RVM_SHAPE_MANUAL'
subprocess.run(['c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror', f'-I{SK}', str(REPO/'tests'/'navi_rvm_shape'/'replay.cpp'), '-o', str(BIN)], check=True)
m = json.load(open(OUT/'model_A.json'))
def gmm(c):
    g = R.GMM(len(c['w'])); g.w = np.array(c['w']); g.mu = np.array(c['mu']); g.S = np.array(c['S']); return g
Em, Nm = gmm(m['E']), gmm(m['N'])
sd = pd.read_csv(RP/'declarations.csv'); sd = sd[sd.boot == BOOT1]
dir_at = lambda us: (1 if sd[(sd.t_us <= us) & sd.topic.str.contains('session_direction')].payload.iloc[-1] == 'CW' else -1)
rows = []
for r, i in zip(X.decl.itertuples(), X.decl_i):
    a, b = map(int, r.payload.split('-')); d = dir_at(r.t_us)
    last, nxt = (a, b) if d > 0 else (b, a)
    rows.append((i, last, nxt, d))
with open(CPP_OUT/'decl.csv', 'w') as f:
    for row in rows: f.write('%d,%d,%d,%d\n' % row)
# Python reference
py = L.run(X.T, Em, Nm, [(i, 'decl') for i, *_ in rows])
py.to_csv(OUT/'python_reference_detections.csv', index=False)
# C++
res = subprocess.run([str(BIN), str(HG/f'native_{BOOT1}.bin'), str(CPP_OUT/'decl.csv'), str(CPP_OUT), '--no-pause'], capture_output=True, text=True, check=True)
stats = json.loads(res.stdout.strip().splitlines()[-1])
cb = np.fromfile(CPP_OUT/'blocks.bin', dtype=np.float32)
ph = X.h
n = min(len(cb), len(ph)); bothnan = np.isnan(cb[:n]) == np.isnan(ph[:n])
diff = np.nanmax(np.abs(cb[:n].astype(float) - ph[:n]))
ce = pd.read_csv(CPP_OUT/'ensembles.csv'); ce.to_csv(OUT/'cpp_detections.csv', index=False)
# match by centre within 2 blocks
pm = py.sort_values('c').reset_index(drop=True); cm = ce.sort_values('c').reset_index(drop=True)
j = np.searchsorted(cm.c.values, pm.c.values)
match = []
for k, c in enumerate(pm.c.values):
    best = None
    for jj in (j[k]-1, j[k]):
        if 0 <= jj < len(cm) and abs(cm.c[jj] - c) <= 2 and (best is None or abs(cm.c[jj]-c) < abs(cm.c[best]-c)): best = jj
    match.append(best)
matched = [(k, b) for k, b in enumerate(match) if b is not None]
pol_same = sum(pm.pol[k] == cm.pol[b] for k, b in matched)
dllr = [abs(pm.llr[k] - cm.llr[b]) for k, b in matched]
dc = [abs(pm.c[k] - cm.c[b]) for k, b in matched]
nav = pd.read_csv(CPP_OUT/'nav.csv'); nav.to_csv(OUT/'cpp_nav.csv', index=False)
out = dict(blocks_cpp=int(len(cb)), blocks_python=int(len(ph)), nan_pattern_identical=bool(bothnan.all()), max_abs_block_diff=float(diff),
           ensembles_python=len(pm), ensembles_cpp=len(cm), matched_within_2_blocks=len(matched),
           identical_centre=int(sum(d == 0 for d in dc)), polarity_same=int(pol_same),
           max_llr_diff=float(max(dllr)) if dllr else None, python_only=[float(pm.c[k]) for k, b in enumerate(match) if b is None],
           cpp_only=[float(c) for c in cm.c if not any(abs(c - x) <= 2 for x in pm.c)],
           cpp_runtime=stats, cpp_nav_events=nav.event.value_counts().to_dict())
json.dump(out, open(OUT/'parity.json', 'w'), indent=1); print(json.dumps(out, indent=1))
