"""Summary tables from the experiment outputs (no model fitting except the family analysis)."""
import numpy as np, pandas as pd, json
from common import *
out = {}
def counts(n):
    t = pd.read_csv(RES/f'truth_{n}.csv'); u = pd.read_csv(RES/f'unmatched_{n}.csv')
    ext = t[~t.censored]; cen = t[t.censored]
    m = ext[ext.n_det >= 1]; allm = t[t.n_det >= 1]
    r = dict(truth_total=len(t), truth_with_extent=len(ext), truth_gap_or_no_extent=len(cen),
             recognized_once=int((ext.n_det == 1).sum()), missed=int((ext.n_det == 0).sum()), split_lobes_counted_separately=int((ext.n_det > 1).sum()),
             no_extent_recognized=int((cen.n_det >= 1).sum()), no_extent_missed=int((cen.n_det == 0).sum()),
             unmatched_detections=len(u), unmatched_on_frozen_ground=int(u.on_ground.sum()) if len(u) else 0,
             unmatched_on_unlabeled_or_uncertain_activity=int(u.near_other.sum()) if len(u) else 0,
             unmatched_elsewhere=int(((~u.on_ground.astype(bool)) & (~u.near_other.astype(bool))).sum()) if len(u) else 0,
             polarity_matches_map=int((allm.pol == allm.pole).sum()), polarity_checked=len(allm),
             d_mid_ms=dict(median_abs=float(m.d_mid_ms.abs().median()), p95_abs=float(m.d_mid_ms.abs().quantile(.95)), max_abs=float(m.d_mid_ms.abs().max())),
             d_start_ms=dict(median=float(m.d_start_ms.median()), p05=float(m.d_start_ms.quantile(.05)), p95=float(m.d_start_ms.quantile(.95))),
             d_end_ms=dict(median=float(m.d_end_ms.median()), p05=float(m.d_end_ms.quantile(.05)), p95=float(m.d_end_ms.quantile(.95))),
             llr_of_recognized=dict(min=float(m.llr.min()), p05=float(m.llr.quantile(.05)), median=float(m.llr.median())))
    # amplitude / breadth strata (annotated amplitude from level; annotated duration)
    for col, lab in [('amp', 'amplitude_adc'), ('dur_ms', 'duration_ms')]:
        q = pd.qcut(ext[col], 4, duplicates='drop')
        r[f'by_{lab}'] = {str(k): dict(n=len(g), recognized_once=int((g.n_det == 1).sum()), missed=int((g.n_det == 0).sum()), split=int((g.n_det > 1).sum()))
                          for k, g in ext.groupby(q, observed=True)}
    r[f'range'] = dict(amp_adc=[float(ext.amp.min()), float(ext.amp.max())], dur_ms=[float(ext.dur_ms.min()), float(ext.dur_ms.max())])
    return r
for n in ['A_all', 'A_region', 'B_region']: out[n] = counts(n)
# by direction / type for A_all
t = pd.read_csv(RES/'truth_A_all.csv'); ext = t[~t.censored]
out['A_all_by_dir'] = {int(k): dict(n=len(g), recognized_once=int((g.n_det == 1).sum())) for k, g in ext.groupby('dir')}
out['A_all_by_type'] = {k: dict(n=len(g), recognized_once=int((g.n_det == 1).sum())) for k, g in ext.groupby('type')}
# separation
s = pd.read_csv(RES/'separation.csv')
sep = {}
for k, g in s.groupby('kind'):
    sep[k] = dict(n=len(g), llr_A=dict(min=float(g.llr_A.min()), p01=float(g.llr_A.quantile(.01)), median=float(g.llr_A.median()), p99=float(g.llr_A.quantile(.99)), max=float(g.llr_A.max()), frac_pos=float((g.llr_A > 0).mean())))
    gr = g[g.in_region == True] if 'in_region' in g else g.iloc[:0]
    if len(gr):
        sep[k]['region_n'] = len(gr)
        for col in ['llr_A', 'llr_B']:
            sep[k][f'region_{col}'] = dict(min=float(gr[col].min()), p01=float(gr[col].quantile(.01)), median=float(gr[col].median()), p99=float(gr[col].quantile(.99)), max=float(gr[col].max()), frac_pos=float((gr[col] > 0).mean()))
e = s[s.kind == 'ensemble']
for fac in [f for f in (0.5, 0.25, 0.1) if f'llr_A_amp{f}' in e]:
    sep['ensemble'][f'amp_scaled_{fac}_frac_pos'] = float((e[f'llr_A_amp{fac}'] > 0).mean())
out['separation_heldout'] = sep
json.dump(out, open(RES/'summary.json', 'w'), indent=1)
print(json.dumps(out, indent=1)[:6000])
