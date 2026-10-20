"""Plot frozen outputs only; no inference or parameter adjustment here."""
import json
import os
os.environ.setdefault('MPLCONFIGDIR', '/tmp/navi-cpr-mpl')
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from experiment import ROOT

plt.rcParams.update({'font.size': 10, 'axes.spines.top': False, 'axes.spines.right': False})


def overlay(ax, name, start, end, title):
    z = np.load(ROOT/f'results/{name}_trace.npz')
    mask = (z['t']>=start)&(z['t']<=end)
    t = z['t'][mask]
    ax.plot(t, z['hall'][mask], color='#252a33', lw=.85, label='Hall: original 20 ms means')
    ax.plot(t, z['baseline'][mask], color='#1867b5', lw=1.3, label='causal baseline')
    ax.fill_between(t, z['baseline'][mask]-3*z['noise'][mask], z['baseline'][mask]+3*z['noise'][mask],
                    color='#1867b5', alpha=.13, label='3 x estimated noise')
    events = json.loads((ROOT/f'results/{name}_events.json').read_text())
    for e in events:
        if e['end']<start or e['start']>end:
            continue
        yes = e['scores']['pca']['recognized']
        color = '#16835b' if yes else '#e68e25'
        ax.axvspan(e['start'], e['end'], alpha=.14, color=color)
        ax.axvline(e['completed'], color=color, ls=':', alpha=.75)
        if end-start < 40 and sum(
            r['end'] >= start and r['start'] <= end for r in events
        ) <= 6:
            ax.text((e['start']+e['end'])/2, .94,
                    f"{'N' if e['polarity']>0 else 'S'} {e['amplitude']:.0f} ADC\n{'recognized' if yes else 'unrecognized'}",
                    ha='center', va='top', transform=ax.get_xaxis_transform(), color=color, fontsize=8)
    journal = [json.loads(line) for line in (ROOT/f'results/{name}_journal.jsonl').read_text().splitlines()]
    for r in journal:
        if r['kind']=='incomplete' and r['t']>=start and r.get('start', end)<=end:
            ax.axvspan(max(start,r['start']), min(end,r['t']), color='#b73b4a', alpha=.12, hatch='//')
        if r['kind']=='provisional_open' and start<=r['t']<=end:
            ax.axvline(r['t'], color='#8b67ac', alpha=.6, lw=.7)
    ax.set(xlim=(start,end), title=title, ylabel='Hall ADC')
    ax.grid(alpha=.16)


def main():
    fig, axes = plt.subplots(3,1,figsize=(13,10), constrained_layout=True)
    overlay(axes[0], 'otto_cw', 1290, 1340, 'Otto CW: completion and recognition, frozen evaluation')
    overlay(axes[1], 'otto_ccw', 70, 95, 'Otto CCW: magnetic encounters and baseline return')
    overlay(axes[2], 'toby_cw', 0, 100, 'Toby: baseline displacement is not resolved; red = incomplete')
    axes[0].legend(loc='lower left', fontsize=8)
    axes[-1].set_xlabel('Boot clock (seconds); purple = opening, dotted = completion')
    fig.savefig(ROOT/'figures/field_examples.png', dpi=160)
    plt.close(fig)

    fig, axes = plt.subplots(2,1,figsize=(13,8), constrained_layout=True)
    overlay(axes[0], 'otto_ccw', 955, 981, 'Prespecified MM101 case: weak North observation, then a strong South event')
    overlay(axes[1], 'otto_ccw', 964, 968.5, 'Weak-observation detail: CPR opens no candidate here')
    for ax in axes:
        ax.axvspan(965.43,967.05,color='#c55d6d',alpha=.16,label='historical 12.4-ADC North window')
        ax.axvspan(965.8,966.66,color='#777777',alpha=.14,hatch='//',label='missing native samples')
    axes[1].legend(loc='lower left', fontsize=8)
    axes[1].set_xlabel('Boot clock (seconds); geographic labels are evaluation-only context')
    fig.savefig(ROOT/'figures/mm101.png', dpi=180)
    plt.close(fig)

    cfg = json.loads((ROOT/'models.json').read_text())['config']
    comparisons = json.loads((ROOT/'results/component_comparison.json').read_text())
    labels = [f"{r['config']['baseline']} / {r['config']['return_s']:.2f}s" for r in comparisons]
    fig, axes = plt.subplots(1,2,figsize=(13,4.5),constrained_layout=True)
    axes[0].barh(labels,[r['calibration']['f1_proxy'] for r in comparisons],color='#4e7da7')
    axes[0].set(xlim=(0,1),xlabel='Segmentation F1 against historical intervals',title='Calibration comparison (not physical truth)')
    model = json.loads((ROOT/'models.json').read_text())['population']
    for s in model['templates']:
        axes[1].plot(np.linspace(0,1,64),s,alpha=.75)
    axes[1].set(xlabel='Completed-event phase',ylabel='Canonical amplitude / actual peak',title='Empirical shape families; physical amplitude retained separately')
    fig.savefig(ROOT/'figures/component_comparison.png',dpi=160)
    plt.close(fig)

    z = np.load(ROOT/'results/synthetic_traces.npz')
    chosen = ['secondary_lobe','close_without_sustained_return','abrupt_baseline_step','incomplete']
    results = json.loads((ROOT/'results/synthetic.json').read_text())
    fig, axes = plt.subplots(4,1,figsize=(12,9),constrained_layout=True)
    for ax,name in zip(axes,chosen):
        ax.plot(z['t'],z[name],color='#252a33')
        for e in results[name]['events']:
            ax.axvspan(e['start'],e['end'],alpha=.17,color='#16835b')
        for e in results[name]['incomplete']:
            ax.axvspan(e['start'],e['t'],alpha=.13,color='#b73b4a')
        ax.set(title=name.replace('_',' '),ylabel='ADC',xlim=(2,12))
    axes[-1].set_xlabel('Synthetic time (seconds); arithmetic/limitation examples only')
    fig.savefig(ROOT/'figures/synthetic_limits.png',dpi=150)
    plt.close(fig)


if __name__ == '__main__':
    main()
