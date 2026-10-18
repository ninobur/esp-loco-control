"""Leave-one-pass-out comparison of Model A (general, all 171 magnets) and
Model B (MM050-MM070 specific). Same features, same class-density method, same
decision and grouping; only the training windows differ.
Folds: boot 1 cut at NAVI's MM145 openings and at the CCW declaration (8 passes).
Every held-out fold is scored by models built without it. Boot 2 is not used."""
import numpy as np, pandas as pd, json, sys, time
from common import *
import rvm_shape as R

rng = np.random.default_rng(20261018)
z = np.load(D / 'tracing_boot1.npz'); h = z['hall']; t_us = z['t_us']; ccw = z['dir'] == 2
T0 = t_us[0]
idx = lambda us: (np.asarray(us) - T0) / (DT_MS * 1000)
lvl, noise = R.context_fast(h)
np.savez_compressed(D / f'context_boot1{"_" + VARIANT if VARIANT else ""}.npz', lvl=lvl, noise=noise)

ann = pd.read_csv(D / 'annotation_boot1.csv')
ann['s_i'] = idx(ann.start_us); ann['e_i'] = idx(ann.end_us)
ens = ann[ann.source.isin(['OPENING', 'SEQUENCE', 'NO_EXTENT'])].copy()
# a truth ensemble is gap-censored if its true window touches a gap
nan_cum = np.concatenate([[0], np.cumsum(~np.isfinite(h))])
cw = [R.true_window(r.s_i, r.e_i) for r in ens.itertuples()]
ens['censored'] = ens.source.eq('NO_EXTENT').to_numpy() | np.array([(nan_cum[min(len(h), int(c + w / 2) + 2)] - nan_cum[max(0, int(c - w / 2))]) > 0 for c, w in cw])
all_int = ann[['s_i', 'e_i']].to_numpy()             # every annotated activity (for 'other' sampling)
all_int = all_int[np.argsort(all_int[:, 0])]

# folds
sel = pd.read_csv(RP / 'selections_scoring_only.csv'); sel = sel[sel.boot == BOOT1]
# cut in the middle of the baseline that follows the MM145 ensemble (so no ensemble straddles a cut)
es = ens.sort_values('s_i').reset_index(drop=True)
def cut_after(us):
    k = es[(es.s_i <= idx(us) + 15)].index.max()
    return int((es.e_i[k] + es.s_i[k + 1]) / 2)
cuts = sorted([cut_after(u) for u in sel[sel.target == 145].open_us.values] + [int(idx(2726833596))])
edges = [0] + cuts + [len(h)]
fold_of = lambda i: np.searchsorted(edges, i, side='right') - 1
ens['fold'] = fold_of(((ens.s_i + ens.e_i) / 2).to_numpy())
ann['fold'] = fold_of(((ann.s_i + ann.e_i) / 2).to_numpy())
NF = len(edges) - 1
REGION = list(range(50, 71))

# region passes: contiguous runs of region ensembles
reg = ens[ens.mm.isin(REGION)].sort_values('s_i')
grp = (reg.s_i.diff() > 60 * 50).cumsum()
passes = []
for g, d in reg.groupby(grp):
    a, b = int(d.s_i.min() - 75), int(d.e_i.max() + 75)
    passes.append(dict(pass_id=int(g), a=a, b=b, fold=int(fold_of(np.array([(a + b) / 2]))[0]), n=len(d),
                       mms=f'{d.mm.iloc[0]}->{d.mm.iloc[-1]}', t0_s=float(t_us[a] / 1e6)))
passes = pd.DataFrame(passes)


def fit_bic(X, kmax=6, seed=0):
    best = None
    for K in range(1, kmax + 1):
        g = R.GMM(K, seed=seed).fit(X)
        if best is None or g.bic() < best.bic(): best = g
    return best


def build(train_ens, other_spans, n_other, specific=False):
    C, W, ii = R.ensemble_windows(train_ens, rng)
    FE, pE, _, vE = R.features(h, lvl, noise, ccw, C, W)
    Co, Wo = R.other_windows(len(h), all_int, rng, n_other, restrict=other_spans)
    FN, _, _, vN = R.features(h, lvl, noise, ccw, Co, Wo)
    Nm = fit_bic(FN[vN], kmax=8)
    if specific:
        parts, wts = [], []
        mmv = train_ens.loc[ii].mm.to_numpy()
        for m in sorted(set(mmv)):
            sel_ = (mmv == m) & vE
            parts.append(R.GMM(1).fit(FE[sel_])); wts.append(sel_.sum())
        Em = R.Mixture(parts, wts); Em.K = len(parts)
    else:
        Em = fit_bic(FE[vE]); Em.K = len(Em.w)
    return Em, Nm, dict(nE=int(vE.sum()), nN=int(vN.sum()), KE=int(Em.K), KN=int(len(Nm.w)))


def score(det, truth, other_act, a, b):
    """truth: ensembles inside [a,b). other_act: UNLABELED/UNCERTAIN activity."""
    det = det[(det.c >= a) & (det.c < b)].copy()
    tr = truth[(truth.s_i >= a) & (truth.e_i < b)].copy()
    tol = 5  # 100 ms
    det['match'] = -1; det['nmatch'] = 0
    rows = []
    for r in tr.itertuples():
        tl = 15 if r.source == 'NO_EXTENT' else tol
        m = det[(det.c >= r.s_i - tl) & (det.c <= r.e_i + tl)]
        det.loc[m.index, 'nmatch'] += 1; det.loc[m.index, 'match'] = r.Index
        best = m.iloc[(m.c - (r.s_i + r.e_i) / 2).abs().argmin()] if len(m) else None
        rows.append(dict(ann_index=r.Index, source=r.source, mm=r.mm, dir=r.dir, pole=r.pole, type=r.type, censored=r.censored,
                         dur_ms=r.dur_ms, amp=float(np.nanmax(np.abs(h[int(r.s_i):int(r.e_i) + 1] - lvl[int((r.s_i + r.e_i) / 2)]))),
                         n_det=len(m), pol=(int(best.pol) if best is not None else 0), llr=(float(best.llr) if best is not None else np.nan),
                         d_start_ms=(float(best.s - r.s_i) * DT_MS if best is not None else np.nan),
                         d_end_ms=(float(best.e - r.e_i) * DT_MS if best is not None else np.nan),
                         d_mid_ms=(float(best.c - (r.s_i + r.e_i) / 2) * DT_MS if best is not None else np.nan),
                         t_s=float(t_us[int(r.s_i)] / 1e6)))
    tr_res = pd.DataFrame(rows)
    um = det[det.nmatch == 0].copy()
    oa = other_act[['s_i', 'e_i', 'source']].to_numpy()
    um['near_other'] = [any((o[0] - tol <= c <= o[1] + tol) for o in oa) for c in um.c]
    gs = ground[:, 0]; ge = ground[:, 1]
    def in_ground(s, e):
        k = np.searchsorted(gs, e); cov = 0.0
        for j in range(max(0, k - 3), k):
            cov += max(0, min(e, ge[j]) - max(s, gs[j]))
        return cov >= 0.5 * (e - s)
    um['on_ground'] = [in_ground(r.s, r.e) for r in um.itertuples()]
    um['t_s'] = t_us[um.c.astype(int).clip(0, len(h) - 1)] / 1e6
    return tr_res, um, det


results = dict(folds=[], passes=passes.to_dict('records'))
TR_A, TR_Ar, TR_B, FP_A, FP_Ar, FP_B, DET, SEP = [], [], [], [], [], [], [], []
other_act = ann[ann.source.isin(['UNLABELED', 'UNCERTAIN'])]
gr = pd.read_csv(D / 'ground_boot1.csv'); ground = np.column_stack([idx(gr.start_us), idx(gr.end_us)]); ground = ground[np.argsort(ground[:, 0])]
for f in range(NF):
    t1 = time.time()
    a, b = edges[f], edges[f + 1]
    train = ens[(ens.fold != f) & ~ens.censored & ens.source.isin(['OPENING', 'SEQUENCE'])]
    test = ens[ens.fold == f]
    oth_spans = [(edges[k], edges[k + 1]) for k in range(NF) if k != f and edges[k + 1] > edges[k]]
    EA, NA, infoA = build(train, oth_spans, 40000)
    det = R.best_explanation(R.scan(h, lvl, noise, ccw, EA, NA, span=(a, b)))
    det['fold'] = f; det['model'] = 'A'; DET.append(det)
    tr, fp, _ = score(det, test, other_act, a, b); tr['fold'] = f; fp['fold'] = f
    TR_A.append(tr); FP_A.append(fp)
    info = dict(fold=f, t0_s=float(t_us[a] / 1e6), t1_s=float(t_us[b - 1] / 1e6), A=infoA)
    # region passes in this fold: A and B on the same spans
    fp_ = passes[passes.fold == f]
    if len(fp_):
        trainB = train[train.mm.isin(REGION)]
        spansB = [(p.a, p.b) for p in passes.itertuples() if p.fold != f]
        EB, NB, infoB = build(trainB, spansB, 8000, specific=True)
        info['B'] = infoB
        for p in fp_.itertuples():
            trA, fpA, _ = score(det, test, other_act, p.a, p.b); trA['pass_id'] = p.pass_id; fpA['pass_id'] = p.pass_id
            TR_Ar.append(trA); FP_Ar.append(fpA)
            dB = R.best_explanation(R.scan(h, lvl, noise, ccw, EB, NB, span=(p.a, p.b)))
            dB['fold'] = f; dB['model'] = 'B'; DET.append(dB)
            trB, fpB, _ = score(dB, test, other_act, p.a, p.b); trB['pass_id'] = p.pass_id; fpB['pass_id'] = p.pass_id
            TR_B.append(trB); FP_B.append(fpB)
    # ---- separation on held-out fold: whole-ensemble windows vs everything else ----
    te = test[~test.censored]
    cwin = np.array([R.true_window(r.s_i, r.e_i) for r in te.itertuples()]).reshape(-1, 2)
    Co, Wo = R.other_windows(len(h), all_int, rng, 6000, restrict=[(a, b)])
    cs, ce = Co - Wo / 4, Co + Wo / 4
    kind = np.where([any(ce_ > s_ and cs_ < e_ for s_, e_ in all_int[max(0, np.searchsorted(all_int[:, 0], ce_) - 4):np.searchsorted(all_int[:, 0], ce_)])
                     for cs_, ce_ in zip(Co - Wo / 2, Co + Wo / 2)], 'partial_or_straddling', 'baseline')
    for nm, C_, W_, kd, mm_ in [('ensemble', cwin[:, 0], cwin[:, 1], np.array(['ensemble'] * len(te)), te.mm.to_numpy()),
                               ('other', Co, Wo, kind, np.full(len(Co), -1))]:
        F, p_, A_, v = R.features(h, lvl, noise, ccw, C_, W_)
        d = pd.DataFrame(dict(fold=f, kind=kd[v], mm=mm_[v], c=C_[v], w=W_[v], snr=F[v, -1], llr_A=EA.score(F[v]) - NA.score(F[v])))
        if 'B' in info:
            inreg = np.zeros(len(d), bool)
            for p in fp_.itertuples(): inreg |= (d.c.to_numpy() >= p.a) & (d.c.to_numpy() < p.b)
            d['in_region'] = inreg
            d['llr_B'] = np.where(inreg, EB.score(F[v]) - NB.score(F[v]), np.nan)
        # amplitude tolerance: same windows with the excursion from level scaled down
        if nm == 'ensemble':
            for fac in ((0.5, 0.25, 0.1) if F.shape[1] == R.NDCT + 1 else ()):
                Fs = F[v].copy(); Fs[:, -1] += np.log(fac)
                d[f'llr_A_amp{fac}'] = EA.score(Fs) - NA.score(Fs)
        SEP.append(d)
    info['sec'] = round(time.time() - t1, 1); results['folds'].append(info); print(info, flush=True)

cat = lambda L: pd.concat(L, ignore_index=True) if L else pd.DataFrame()
for name, L in [('truth_A_all', TR_A), ('truth_A_region', TR_Ar), ('truth_B_region', TR_B),
                ('unmatched_A_all', FP_A), ('unmatched_A_region', FP_Ar), ('unmatched_B_region', FP_B), ('detections', DET), ('separation', SEP)]:
    cat(L).to_csv(RES / f'{name}.csv', index=False)
json.dump(results, open(RES / 'experiment_folds.json', 'w'), indent=1, default=float)
