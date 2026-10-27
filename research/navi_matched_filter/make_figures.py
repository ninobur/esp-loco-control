#!/usr/bin/env python3
"""Diagnostic figures for the report (all saved under figures/)."""
import json
from pathlib import Path

import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

import corpus
import exp_overlap as ov
import mf
import plots
import resolve
from config import FROZEN

HERE = Path(__file__).resolve().parent
FIG = HERE / 'figures'


def fig_template():
    rows, x = mf.load_curves()
    T = mf.template_mean()
    fig, ax = plt.subplots(1, 2, figsize=(11, 3.2))
    ax[0].plot(np.linspace(0, 1, 128), x[::10].T, color='0.8', lw=.4)
    ax[0].plot(np.linspace(0, 1, 128), T, 'k', lw=2)
    ax[0].set(title='Template: mean of 945 Oct-15 curves (every 10th shown)',
              xlabel='departure → return', ylabel='normalised')
    for D in FROZEN.scales:
        t, ipk = mf.sampled_template(T, D, FROZEN.pad)
        ax[1].plot((np.arange(len(t)) - len(t) // 2) * mf.DT_S, t, lw=1, label='%.2f s' % D)
    ax[1].legend(fontsize=7)
    ax[1].set(title='Scale bank, padded windows (pad = 1.0·D each side)', xlabel='s from window centre')
    fig.tight_layout()
    fig.savefig(FIG / 'template_and_bank.png', dpi=110)


def fig_level1():
    s = corpus.load(corpus.LEVEL1)
    ev, _ = mf.recognize(s['trace'], FROZEN)
    corpus.score(s, ev)
    fig, ax = plt.subplots(2, 1, figsize=(15, 5.5))
    plots.panel(ax[0], s, ev, 0, 97, 'Level 1: 9950011_20261025_133357 (whole file)', raw=False)
    plots.panel(ax[1], s, ev, 44, 60, 'Level 1, 44–60 s')
    fig.tight_layout()
    fig.savefig(FIG / 'level1.png', dpi=110)


def fig_overlap():
    lib, bgs = ov.library()
    rng = np.random.default_rng(7)
    fast = [p for p in lib if p['pwm'] >= 85]
    N = [p for p in fast if p['pol'] > 0]
    S = [p for p in fast if p['pol'] < 0]
    cases = [('NN', 150), ('NN', 300), ('NS', 150), ('SN', 200), ('SS', 250), ('SS', 1000)]
    methods = [('baseline', resolve.method_baseline), ('B_pursuit', resolve.method_B)]
    fig, axes = plt.subplots(len(cases), 2, figsize=(14, 2.3 * len(cases)), squeeze=False)
    for row, (pols, sep) in enumerate(cases):
        p1 = (N if pols[0] == 'N' else S)[rng.integers(100)]
        p2 = (N if pols[1] == 'N' else S)[rng.integers(100)]
        tr, truth = ov.synth(bgs[rng.integers(len(bgs))], p1, p2, sep / 1e3,
                             amp2=p1['amp'] / p2['amp'])
        for col, (mname, fn) in enumerate(methods):
            ax = axes[row, col]
            ev = fn(tr, FROZEN)
            out, _ = ov.classify(ev, truth, sep / 1e3)
            w = (tr.t > 1.2) & (tr.t < 2.8 + sep / 1e3)
            ax.plot(tr.t[w], tr.x[w], 'k', lw=.8)
            for tt in truth:
                ax.axvline(tt['t'], color='g', ls=':', lw=1)
            for e in ev:
                if 1.2 < e['t_peak'] < 2.8 + sep / 1e3:
                    ax.axvline(e['t_peak'], color='tab:blue', lw=1)
                    ax.text(e['t_peak'], ax.get_ylim()[1], 'r%.2f A%d D%.2f' % (e['r'], e['A'], e['scale']),
                            fontsize=6, rotation=90, va='top')
            ax.set_title('SYNTHETIC %s sep %d ms — %s → %s' % (pols, sep, mname, out), fontsize=8)
            ax.tick_params(labelsize=6)
    fig.tight_layout()
    fig.savefig(FIG / 'overlap_examples.png', dpi=100)


def fig_overlap_curves():
    rows = json.loads((HERE / 'results' / 'overlap_rows.json').read_text())
    fig, ax = plt.subplots(1, 2, figsize=(12, 3.6))
    for k, pol_group in enumerate((('NN', 'SS'), ('NS', 'SN'))):
        for m, c in (('baseline', 'k'), ('A_local_max', 'tab:gray'), ('B_pursuit', 'tab:blue'), ('C_joint', 'tab:orange')):
            ys = []
            for sep in ov.SEPS_MS:
                sel = [r for r in rows if r['method'] == m and r['sep_ms'] == sep and r['pols'] in pol_group]
                ys.append(100 * np.mean([r['outcome'] == 'two_correct' for r in sel]))
            ax[k].plot(ov.SEPS_MS, ys, 'o-', color=c, label=m, ms=3)
        ax[k].set(title='SYNTHETIC pairs %s: both encounters correct' % '/'.join(pol_group),
                  xlabel='centre-to-centre separation (ms)', ylabel='% of pairs', xscale='log', ylim=(0, 102))
        ax[k].axvspan(700, 1100, color='g', alpha=.08)
        ax[k].legend(fontsize=7)
    fig.tight_layout()
    fig.savefig(FIG / 'overlap_by_separation.png', dpi=110)


def fig_field_cases():
    """Field disturbances: background steps, bursts, map-polarity disagreement."""
    picks = [('7463C9467F2273E6', 212, 217), ('8139C11783F1F6A0', 138.5, 144),
             ('F420A05DCE376BCB', 92, 98), ('F420A05DCE376BCB', 646, 661),
             ('278F53E8D6A6A152', 100, 107), ('D1EC9A0B0A105CD2', 184, 191)]
    cases = []
    for key, t0, t1 in picks:
        n = [x for x in corpus.sessions() if key in x][0]
        s = corpus.load(n)
        ev, _ = mf.recognize(s['trace'], FROZEN)
        corpus.score(s, ev)
        cases.append((s, ev, t0, t1, None))
    plots.grid(cases, FIG / 'field_cases.png', cols=1, h=2.6)


if __name__ == '__main__':
    FIG.mkdir(exist_ok=True)
    fig_template()
    fig_level1()
    fig_overlap_curves()
    fig_overlap()
    fig_field_cases()
