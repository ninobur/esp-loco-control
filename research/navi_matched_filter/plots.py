"""Annotated diagnostic plots of recognizer events against the reference."""
from __future__ import annotations

import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

import corpus

COLORS = {'magnet': 'tab:blue', 'magnet_map_polarity_differs': 'tab:orange',
          'false': 'tab:red', 'duplicate': 'tab:purple', 'unscored': '0.6'}


def panel(ax, sess, ev, t0, t1, title=None, raw=True, R=None):
    tr = sess['trace']
    w = (tr.t > t0) & (tr.t < t1)
    if raw:
        nat = np.load(corpus.NSR_DIR / (sess['name'] + '.npz'))['native']
        ts = (nat['t_us'].astype(np.int64) - tr.t0_us) / 1e6
        m = (ts > t0) & (ts < t1)
        ax.plot(ts[m], nat['raw'][m], ',', color='0.75')
    ax.plot(tr.t[w], tr.x[w], 'k', lw=.7)
    lo, hi = np.nanmin(tr.x[w]), np.nanmax(tr.x[w])
    ax.set_ylim(lo - .15 * (hi - lo), hi + .25 * (hi - lo))
    for e in ev:
        if t0 < e['t_peak'] < t1:
            ax.axvline(e['t_peak'], color=COLORS.get(e['class'], 'y'), lw=1)
            ax.text(e['t_peak'], hi + .2 * (hi - lo), '%s r%.2f A%d D%.2f' % (
                e['class'][:6], e['r'], e['A'], e['scale']), fontsize=5.5, rotation=90, va='top')
    for r in sess['refs']:
        if t0 < r['t'] < t1:
            ax.text(r['t'], lo - .12 * (hi - lo), 'MM%03d%s%s' % (r['mm'], 'N' if r['pol'] > 0 else 'S',
                    '?' if r['kind'] == 'expected' else ''), fontsize=6, color='g', ha='center')
    ax2 = ax.twinx()
    ax2.plot(tr.t[w], tr.pwm[w], color='tab:green', lw=.5, alpha=.6)
    ax2.set_ylim(0, 260)
    ax2.tick_params(labelsize=6)
    ax.tick_params(labelsize=6)
    ax.set_title(title or '%s  %.1f–%.1f s' % (sess['name'], t0, t1), fontsize=7)


def grid(cases, path, cols=2, h=2.3):
    """cases: list of (sess, ev, t0, t1, title)."""
    rows = int(np.ceil(len(cases) / cols))
    fig, axes = plt.subplots(rows, cols, figsize=(8 * cols, h * rows), squeeze=False)
    for ax, c in zip(axes.flat, cases):
        panel(ax, *c)
    for ax in list(axes.flat)[len(cases):]:
        ax.axis('off')
    fig.tight_layout()
    fig.savefig(path, dpi=90)
    plt.close(fig)
