"""Shape families (descriptive, all boot-1 ensembles with extent) and report figures.
Families: Gaussian mixtures with K=1..10 on the ensemble feature vectors, BIC; the
chosen components are cross-tabulated against magnet type (bar/disk, 2026-08-28 type
map), map polarity and direction. Type and polarity are NOT recognizer inputs."""
import numpy as np, pandas as pd, json, matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
from common import *
import rvm_shape as R
from s04_casefig import casefig
z = np.load(D/'tracing_boot1.npz'); h = z['hall']; t_us = z['t_us']; ccw = z['dir'] == 2
cx = np.load(D/'context_boot1.npz'); lvl, noise = cx['lvl'], cx['noise']
idx = lambda us: (np.asarray(us) - t_us[0]) / (DT_MS*1000)
ann = pd.read_csv(D/'annotation_boot1.csv'); ann['s_i'] = idx(ann.start_us); ann['e_i'] = idx(ann.end_us)
ens = ann[ann.source.isin(['OPENING', 'SEQUENCE'])].reset_index(drop=True)
cw = np.array([R.true_window(r.s_i, r.e_i) for r in ens.itertuples()])
F, p, A, v = R.features(h, lvl, noise, ccw, cw[:, 0], cw[:, 1]); ens = ens[v].reset_index(drop=True); F = F[v]; p = p[v]
U = F[:, :R.NDCT] @ R.DCT          # shape reconstructed from the 12 DCT coefficients
bics = {}; models = {}
for K in range(1, 11):
    g = R.GMM(K, seed=1).fit(F); bics[K] = float(g.bic()); models[K] = g
Kb = min(bics, key=bics.get); g = models[Kb]; ens['family'] = g.predict(F)
g2 = models[2]; ens['family2'] = g2.predict(F)
fam = dict(bic=bics, K_best=Kb,
           crosstab_type=pd.crosstab(ens.family, ens.type).to_dict(), crosstab_pole=pd.crosstab(ens.family, ens.pole).to_dict(),
           crosstab_dir=pd.crosstab(ens.family, ens.dir).to_dict(),
           K2_crosstab_type=pd.crosstab(ens.family2, ens.type).to_dict(), K2_crosstab_pole=pd.crosstab(ens.family2, ens.pole).to_dict(),
           K2_crosstab_dir=pd.crosstab(ens.family2, ens.dir).to_dict(),
           family_duration_ms={int(k): dict(median=float(d.dur_ms.median()), p10=float(d.dur_ms.quantile(.1)), p90=float(d.dur_ms.quantile(.9))) for k, d in ens.groupby('family')},
           magnet_consistency=float(ens.groupby('mm').family.agg(lambda s: s.value_counts(normalize=True).iloc[0]).mean()))
json.dump(fam, open(RES/'families.json', 'w'), indent=1, default=str)
ens[['mm', 'dir', 'type', 'pole', 'dur_ms', 'family', 'family2']].to_csv(RES/'families_assignment.csv', index=False)
x = np.linspace(-1, 1, R.NPT)   # window coordinate: ensemble occupies -0.5..0.5
# Fig: families
fig, axs = plt.subplots(1, Kb, figsize=(3*Kb, 3), sharey=True)
for k, ax in zip(range(Kb), np.atleast_1d(axs)):
    Uk = U[ens.family == k]; d = ens[ens.family == k]
    ax.plot(x, Uk[:200].T, color='grey', lw=.3, alpha=.4); ax.plot(x, Uk.mean(0), color='k', lw=2)
    ax.axvspan(-.5, .5, color='orange', alpha=.1)
    ax.set_title(f'family {k}: n={len(d)}\nbar {int((d.type=="bar").sum())} / disk {int((d.type=="disk").sum())}', fontsize=8)
    ax.set_xlabel('window (annotated extent shaded)')
np.atleast_1d(axs)[0].set_ylabel('normalised shape (principal lobe = +1)')
plt.suptitle(f'Model A shape families (BIC best K={Kb}); map orientation, polarity-normalised', fontsize=9)
plt.tight_layout(); plt.savefig(FIG/'families.png', dpi=90); plt.close()
# Fig: Model B per-magnet portraits MM050-MM070 vs the general mean
fig, axs = plt.subplots(3, 7, figsize=(16, 6.5), sharex=True, sharey=True)
for m, ax in zip(range(50, 71), axs.ravel()):
    Um = U[ens.mm == m]; d = ens[ens.mm == m]
    ax.plot(x, U.mean(0), color='tab:blue', lw=1, ls='--')
    ax.plot(x, Um.T, color='k', lw=.6, alpha=.6)
    ax.set_title(f'MM{m:03d} {d.type.iloc[0] if len(d) else ""} n={len(d)}', fontsize=8)
plt.suptitle('MM050-MM070: every boot-1 observation (black) vs general mean (blue dashed); normalised shape', fontsize=9)
plt.tight_layout(); plt.savefig(FIG/'region_portraits.png', dpi=90); plt.close()
# Fig: separation histograms (held-out), primary and shape-only
fig, axs = plt.subplots(1, 2, figsize=(13, 3.6))
for ax, (lab, path) in zip(axs, [('primary (shape + log A/noise)', RES/'separation.csv'), ('ablation: shape only', RES/'ablation_shape_only/separation.csv')]):
    if not path.exists(): continue
    s = pd.read_csv(path)
    bins = np.concatenate([-np.logspace(4, 0, 40), [0], np.logspace(0, 2.2, 15)])
    for k, c in [('baseline', 'tab:green'), ('partial_or_straddling', 'tab:purple'), ('ensemble', 'tab:orange')]:
        ax.hist(s[s.kind == k].llr_A, bins=bins, color=c, alpha=.6, label=f'{k} (n={int((s.kind==k).sum())})')
    ax.set_xscale('symlog', linthresh=1); ax.axvline(0, color='k', lw=1); ax.set_yscale('log')
    ax.set_title(f'Model A, held-out passes: {lab}', fontsize=9); ax.set_xlabel('LLR = log p(E) - log p(N)'); ax.legend(fontsize=7)
plt.tight_layout(); plt.savefig(FIG/'separation.png', dpi=90); plt.close()
# Fig: overview stretch and cases
casefig([1653.0, 3327.0], FIG/'overview_cw_ccw.png', half=7.0, title='Held-out passes: tracing (black), context level (green), annotation (orange), Model A recognitions (blue bars)')
um = pd.read_csv(RES/'unmatched_A_all.csv')
casefig(list(um.t_s.values), FIG/'unmatched_detections_A.png', half=3.0, title='Every Model A recognition with no annotated identity (all on UNLABELED/UNCERTAIN activity)')
tr = pd.read_csv(RES/'truth_A_all.csv'); ne = tr[(tr.source == 'NO_EXTENT')]
casefig(list(ne.t_s.values), FIG/'no_extent_openings.png', half=2.5, title='NAVI openings whose Hall-ground encounter was gap-censored (magenta) - recognized ones carry blue bars')
lg = ens.sort_values('dur_ms').iloc[-4:]
casefig(list(lg.start_us.values/1e6 + lg.dur_ms.values/2000), FIG/'longest_ensembles.png', half=4.0, title='Longest annotated ensembles (slow passages / decelerations)')
print(json.dumps({k: fam[k] for k in ['bic', 'K_best', 'crosstab_type', 'K2_crosstab_type', 'K2_crosstab_pole', 'K2_crosstab_dir', 'family_duration_ms', 'magnet_consistency']}, indent=0, default=str))
