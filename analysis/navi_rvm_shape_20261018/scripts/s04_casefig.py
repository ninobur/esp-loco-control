"""Plot listed times: tracing, context level, annotation extents (orange = magnet with identity; magenta = NO_EXTENT opening;
identity, red = UNLABELED, grey = UNCERTAIN), detections (blue core spans, A)."""
import sys, numpy as np, pandas as pd, matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
from common import *
def casefig(times, out, half=4.0, model='A', title=''):
    z = np.load(D/'tracing_boot1.npz'); t = z['t_us']/1e6; h = z['hall']; lvl = np.load(D/'context_boot1.npz')['lvl']
    ann = pd.read_csv(D/'annotation_boot1.csv'); det = pd.read_csv(RES/'detections.csv'); det = det[det.model == model]
    fig, axs = plt.subplots(len(times), 1, figsize=(12, 2.3*len(times)), squeeze=False)
    for ax, T in zip(axs[:, 0], times):
        m = (t > T-half) & (t < T+half); ax.plot(t[m], h[m], lw=.9, color='k'); ax.plot(t[m], lvl[m], lw=.6, color='g', ls='--')
        for r in ann[(ann.end_us/1e6 > T-half) & (ann.start_us/1e6 < T+half)].itertuples():
            col = {'OPENING': 'orange', 'SEQUENCE': 'orange', 'UNLABELED': 'red', 'UNCERTAIN': 'grey', 'NO_EXTENT': 'magenta'}[r.source]
            ax.axvspan(r.start_us/1e6, max(r.end_us, r.start_us+30000)/1e6, color=col, alpha=.25 if r.source != 'NO_EXTENT' else .8)
            if r.mm >= 0: ax.text(r.start_us/1e6, np.nanmax(h[m]), f'MM{r.mm:03d}', fontsize=7, va='top')
        for r in det[(det.c > (T-half-t[0])/0.02) & (det.c < (T+half-t[0])/0.02)].itertuples():
            ax.axvspan(t[0]+r.s*0.02, t[0]+r.e*0.02, ymin=0, ymax=.08, color='b', alpha=.8)
        ax.set_xlim(T-half, T+half); ax.set_ylabel('ADC'); ax.set_title(f't = {T:.2f} s', fontsize=8, loc='left')
    axs[0, 0].set_title(title, fontsize=10)
    plt.tight_layout(); plt.savefig(out, dpi=80); plt.close()
if __name__ == '__main__':
    casefig([float(x) for x in sys.argv[2:]], sys.argv[1])
