"""Figure: trailing-level variant misses and the two level references."""
import numpy as np, pandas as pd, matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
from common import *
t = pd.read_csv(RES/'ablation_causal_level/truth_A_all.csv'); m = t[(t.n_det == 0) & ~t.censored]
z = np.load(D/'tracing_boot1.npz'); tt = z['t_us']/1e6; h = z['hall']
a = np.load(D/'context_boot1.npz')['lvl']; b = np.load(D/'context_boot1_causal_level.npz')['lvl']
T0s = sorted(set(np.round(m.t_s.values/40)*40))
fig, axs = plt.subplots(len(T0s), 1, figsize=(13, 2.6*len(T0s)), squeeze=False)
for ax, T in zip(axs[:, 0], T0s):
    k = (tt > T-40) & (tt < T+40); ax.plot(tt[k], h[k], 'k', lw=.6)
    ax.plot(tt[k], a[k], 'g--', lw=1, label='centred +-10 s median'); ax.plot(tt[k], b[k], 'r', lw=1, label='trailing 20 s median')
    for x in m.t_s:
        if T-40 < x < T+40: ax.axvline(x, color='m', alpha=.5)
    ax.legend(fontsize=7); ax.set_xlabel('boot-1 time (s)'); ax.set_ylabel('ADC')
plt.suptitle('Trailing-level variant: Model A misses (magenta) and the two level references', fontsize=9)
plt.tight_layout(); plt.savefig(FIG/'causal_level_misses.png', dpi=80)
