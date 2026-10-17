"""Scoring only: compares aligner positions with NAVI-selected openings (which the aligner never reads)."""
import numpy as np, pandas as pd, json, pathlib, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import aligner
ROOT = aligner.ROOT; D = aligner.D
meta = json.load(open(D / 'template_meta.json')); LAP = json.load(open(D / 'map_meta.json'))['lap_mm']
def wrap(e): return (e + LAP / 2) % LAP - LAP / 2
def score(out, boot, cw, ccw, lag_s=4.0, tag=''):
    sel = pd.read_csv(D / 'selections_scoring_only.csv'); sel = sel[sel.boot == boot]
    t = out['t']; rows = []
    for r in sel.itertuples():
        i = int(np.searchsorted(t, r.open_us / 1e6))
        if i >= len(t) or out['seg'][i] < 0 or not np.isfinite(out['x'][i]): continue
        name = cw if r.dir == 1 else ccw
        ref = meta[name]['opening_coord_mm'].get(str(r.target))
        if ref is None: continue
        xo = out['x'][i]; xr = out['retro'](i, int(lag_s / aligner.DT))
        rows.append(dict(t=r.open_us / 1e6, mm=r.target, dir='CW' if r.dir == 1 else 'CCW', pwm=r.pwm, seg=int(out['seg'][i]),
                         err_online=wrap(xo - ref), err_retro=wrap(xr - ref)))
    df = pd.DataFrame(rows)
    return df
def summary(df, excl=None):
    d = df if excl is None else df[~((df.t >= excl[0]) & (df.t < excl[1]))]
    res = {}
    for k in ['err_online', 'err_retro']:
        a = d[k].abs(); res[k] = dict(n=int(len(d)), median_abs=round(float(a.median()), 1), p95_abs=round(float(a.quantile(.95)), 1),
                                      max_abs=round(float(a.max()), 1), wrong_identity=int((a > 150).sum()))
    return res
