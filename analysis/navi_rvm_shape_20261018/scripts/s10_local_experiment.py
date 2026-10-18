"""Leave-one-pass-out evaluation of the LOCAL-BASELINE sequential recognizer (Model A,
general composite). Same folds, annotation and scoring as s03 (research), so the
results compare directly with results/summary.json.
Each held-out pass is run continuously from its start (the fold cut lies mid-baseline,
treated like a declaration) and from every operator declaration inside it.
Writes results/local/."""
import numpy as np, pandas as pd, json, time
from common import *
import rvm_shape as R
import rvm_local as L

OUT = ROOT / 'results' / 'local'; OUT.mkdir(exist_ok=True)
rng = np.random.default_rng(20261018)
z = np.load(D / 'tracing_boot1.npz'); h = z['hall']; t_us = z['t_us']; ccw = z['dir'] == 2
idx = lambda us: (np.asarray(us) - t_us[0]) / (DT_MS * 1000)
T = L.Tracing(h, ccw)
lvl_ctx = np.load(D / 'context_boot1.npz')['lvl']          # ONLY for reporting annotated amplitude, as in s03

ann = pd.read_csv(D / 'annotation_boot1.csv'); ann['s_i'] = idx(ann.start_us); ann['e_i'] = idx(ann.end_us)
ens = ann[ann.source.isin(['OPENING', 'SEQUENCE', 'NO_EXTENT'])].copy()
nan_cum = T.nan_cum
cw = [R.true_window(r.s_i, r.e_i) for r in ens.itertuples()]
ens['censored'] = ens.source.eq('NO_EXTENT').to_numpy() | np.array([(nan_cum[min(len(h), int(c + w / 2) + 2)] - nan_cum[max(0, int(c - w / 2))]) > 0 for c, w in cw])
sup = ann[ann.source.isin(['OPENING', 'SEQUENCE', 'UNLABELED'])]
act_win_end = np.sort((sup.e_i + 0.5 * (sup.e_i - sup.s_i)).to_numpy())     # where BL starts after each activity
all_int = ann[['s_i', 'e_i']].to_numpy(); all_int = all_int[np.argsort(all_int[:, 0])]
gr = pd.read_csv(D / 'ground_boot1.csv'); ground = np.column_stack([idx(gr.start_us), idx(gr.end_us)]); ground = ground[np.argsort(ground[:, 0])]
other_act = ann[ann.source.isin(['UNLABELED', 'UNCERTAIN'])]

# folds exactly as s03
sel = pd.read_csv(RP / 'selections_scoring_only.csv'); sel = sel[sel.boot == BOOT1]
es = ens[ens.source != 'NO_EXTENT'].sort_values('s_i').reset_index(drop=True)
def cut_after(us):
    k = es[(es.s_i <= idx(us) + 15)].index.max()
    return int((es.e_i[k] + es.s_i[k + 1]) / 2)
cuts = sorted([cut_after(u) for u in sel[sel.target == 145].open_us.values] + [int(idx(2726833596))])
edges = [0] + cuts + [len(h)]
fold_of = lambda i: np.searchsorted(edges, i, side='right') - 1
ens['fold'] = fold_of(((ens.s_i + ens.e_i) / 2).to_numpy())
NF = len(edges) - 1
decl = pd.read_csv(RP / 'declarations.csv'); decl = decl[(decl.boot == BOOT1) & decl.topic.str.contains('start_interval')]
decl_i = [int(np.ceil(idx(u))) for u in decl.t_us]


def fit_bic(X, kmax=6, seed=0):
    best = None
    for K in range(1, kmax + 1):
        g = R.GMM(K, seed=seed).fit(X)
        if best is None or g.bic() < best.bic(): best = g
    return best


def build(train_ens, spans, n_other):
    C, W, _ = R.ensemble_windows(train_ens, rng)
    FE, _, _, vE = L.train_features(T, C, W, act_win_end)
    Co, Wo = R.other_windows(len(h), all_int, rng, n_other, restrict=spans)
    FN, _, _, vN = L.train_features(T, Co.astype(float), Wo.astype(float), act_win_end)
    return fit_bic(FE[vE]), fit_bic(FN[vN], kmax=8), dict(nE=int(vE.sum()), nN=int(vN.sum()))


def score(det, truth, a, b):
    det = det[(det.c >= a) & (det.c < b)].copy()
    tr = truth[(truth.s_i >= a) & (truth.e_i < b)].copy()
    tol = 5
    det['nmatch'] = 0; det['match'] = -1
    rows = []
    for r in tr.itertuples():
        tl = 15 if r.source == 'NO_EXTENT' else tol
        m = det[(det.c >= r.s_i - tl) & (det.c <= r.e_i + tl)]
        det.loc[m.index, 'nmatch'] += 1; det.loc[m.index, 'match'] = r.Index
        best = m.iloc[(m.c - (r.s_i + r.e_i) / 2).abs().argmin()] if len(m) else None
        seg = h[int(r.s_i):int(r.e_i) + 1]
        rows.append(dict(ann_index=r.Index, source=r.source, mm=r.mm, dir=r.dir, pole=r.pole, type=r.type, censored=r.censored,
                         dur_ms=r.dur_ms, amp=float(np.nanmax(np.abs(seg - lvl_ctx[int((r.s_i + r.e_i) / 2)]))) if np.isfinite(seg).any() else np.nan,
                         n_det=len(m), pol=(int(best.pol) if best is not None else 0), llr=(float(best.llr) if best is not None else np.nan),
                         d_start_ms=(float(best.s - r.s_i) * DT_MS if best is not None else np.nan),
                         d_end_ms=(float(best.e - r.e_i) * DT_MS if best is not None else np.nan),
                         d_mid_ms=(float(best.c - (r.s_i + r.e_i) / 2) * DT_MS if best is not None else np.nan),
                         latency_ms=(float(best.decided - r.e_i) * DT_MS if best is not None else np.nan),
                         t_s=float(t_us[int(r.s_i)] / 1e6)))
    tr_res = pd.DataFrame(rows)
    um = det[det.nmatch == 0].copy()
    oa = other_act[['s_i', 'e_i']].to_numpy()
    um['near_other'] = [any((o[0] - tol <= c <= o[1] + tol) for o in oa) for c in um.c]
    gs, ge = ground[:, 0], ground[:, 1]
    def in_ground(s, e):
        k = np.searchsorted(gs, e); cov = 0.0
        for j in range(max(0, k - 3), k): cov += max(0, min(e, ge[j]) - max(s, gs[j]))
        return cov >= 0.5 * (e - s)
    um['on_ground'] = [in_ground(r.s, r.e) for r in um.itertuples()]
    um['t_s'] = t_us[um.c.astype(int).clip(0, len(h) - 1)] / 1e6
    return tr_res, um


if __name__ == '__main__':
    TR, UM, DET, INFO = [], [], [], []
    for f in range(NF):
        t1 = time.time(); a, b = edges[f], edges[f + 1]
        train = ens[(ens.fold != f) & ~ens.censored & ens.source.isin(['OPENING', 'SEQUENCE'])]
        spans = [(edges[k], edges[k + 1]) for k in range(NF) if k != f]
        Em, Nm, info = build(train, spans, 40000)
        starts = [(a, f'fold{f}_start')] + [(d, f'decl@{t_us[d]/1e6:.0f}s') for d in decl_i if a < d < b]
        det = L.run(T, Em, Nm, starts, stop=b)
        det['fold'] = f; DET.append(det)
        tr, um = score(det, ens[ens.fold == f], a, b); tr['fold'] = f; um['fold'] = f
        TR.append(tr); UM.append(um)
        info.update(fold=f, KE=len(Em.w), KN=len(Nm.w), n_acc=len(det), sec=round(time.time() - t1, 1)); INFO.append(info); print(info, flush=True)

    pd.concat(TR, ignore_index=True).to_csv(OUT / 'truth_A_all.csv', index=False)
    pd.concat(UM, ignore_index=True).to_csv(OUT / 'unmatched_A_all.csv', index=False)
    pd.concat(DET, ignore_index=True).to_csv(OUT / 'detections.csv', index=False)
    json.dump(dict(folds=INFO, edges=edges, declarations=decl_i), open(OUT / 'folds.json', 'w'), indent=1, default=int)
