#!/usr/bin/env python3
"""Render the four summary charts for mm_declaration_spatial_variability.py's
output: single-pass residual histogram, residual-by-loop-position across the
three circuits, systematic/random variance share, and eligibility-window
coverage. Colors follow the repo's dataviz reference palette."""
import argparse
import csv
import json

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt

BLUE = '#2a78d6'
ORANGE = '#eb6834'
AQUA = '#1baf7a'
CRITICAL = '#d03b3b'
GRID = '#e1e0d9'
MUTED = '#898781'
PRIMARY = '#0b0b0b'
SECONDARY = '#52514e'

plt.rcParams.update({
    'font.family': 'sans-serif',
    'text.color': PRIMARY,
    'axes.edgecolor': GRID,
    'axes.labelcolor': SECONDARY,
    'xtick.color': MUTED,
    'ytick.color': MUTED,
    'axes.grid': True,
    'grid.color': GRID,
    'grid.linewidth': 0.8,
    'axes.axisbelow': True,
    'figure.facecolor': '#fcfcfb',
    'axes.facecolor': '#fcfcfb',
})


def getk(d, w):
    for k in d:
        if abs(float(k) - w) < 1e-9:
            return d[k]
    raise KeyError(w)


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('csv_in')
    p.add_argument('json_in')
    p.add_argument('png_out')
    args = p.parse_args()

    rows = list(csv.DictReader(open(args.csv_in)))
    for r in rows:
        for k in ('pct_residual', 'signed_residual_mm', 'abs_residual_mm', 'map_mm', 'ratio'):
            r[k] = float(r[k])
        r['prev_mm'] = int(r['prev_mm'])
        r['pass_index'] = int(r['pass_index'])
    summary = json.load(open(args.json_in))

    fig, axes = plt.subplots(2, 2, figsize=(13, 9.5))
    fig.suptitle('September 20 noon run - Hall declaration point vs IR-measured travel '
                 '(n=%d adjacent-MM intervals, 3 CW circuits)' % len(rows),
                 fontsize=12, color=PRIMARY, y=0.985)

    # A: histogram of single-pass percentage residual
    ax = axes[0, 0]
    vals = [r['pct_residual'] for r in rows]
    bins = list(range(-30, 22, 2))
    inside = [v for v in vals if -10 <= v <= 10]
    outside = [v for v in vals if not (-10 <= v <= 10)]
    ax.hist([inside, outside], bins=bins, stacked=True, color=[BLUE, CRITICAL],
            label=[f'inside +/-10%  (n={len(inside)})', f'outside +/-10%  (n={len(outside)})'],
            edgecolor='#fcfcfb', linewidth=0.5)
    ax.axvline(0, color=MUTED, linewidth=1)
    ax.axvline(-10, color=CRITICAL, linewidth=1.2, linestyle='--')
    ax.axvline(10, color=CRITICAL, linewidth=1.2, linestyle='--')
    med = summary['pct_residual_distribution']['median']
    ax.axvline(med, color=PRIMARY, linewidth=1.2, linestyle=':')
    ax.text(med, ax.get_ylim()[1] * 0.95 if ax.get_ylim()[1] else 1, f' median {med:+.2f}%',
            fontsize=8, color=PRIMARY, va='top')
    ax.set_xlabel('percentage residual, (IR - map) / map  [%]')
    ax.set_ylabel('single-pass intervals')
    ax.set_title('A.  Single-pass IR/map residual, all %d intervals' % len(rows), fontsize=10, loc='left', color=PRIMARY)
    ax.legend(fontsize=8, frameon=False, loc='upper left')

    # B: residual by loop position, three circuits overlaid
    ax = axes[0, 1]
    colors = {1: BLUE, 2: ORANGE, 3: AQUA}
    ax.axhspan(-10, 10, color=BLUE, alpha=0.07, zorder=0)
    for pass_n in (1, 2, 3):
        xs = [r['prev_mm'] for r in rows if r['pass_index'] == pass_n]
        ys = [r['pct_residual'] for r in rows if r['pass_index'] == pass_n]
        ax.scatter(xs, ys, s=10, color=colors[pass_n], label=f'pass {pass_n}', alpha=0.75, linewidths=0)
    ax.axhline(0, color=MUTED, linewidth=1)
    ax.axhline(-10, color=CRITICAL, linewidth=1, linestyle='--', alpha=0.6)
    ax.axhline(10, color=CRITICAL, linewidth=1, linestyle='--', alpha=0.6)
    ax.set_xlabel('mapped magnet number (position around the 171-magnet loop)')
    ax.set_ylabel('percentage residual  [%]')
    ax.set_title('B.  Residual by loop position, 3 circuits overlaid', fontsize=10, loc='left', color=PRIMARY)
    ax.legend(fontsize=8, frameon=False, loc='upper right', ncol=3)

    # C: systematic vs random share of repeat variance
    ax = axes[1, 0]
    anova = summary['repeatability_anova_pct']
    between = anova['frac_variance_between'] * 100
    within = anova['frac_variance_within'] * 100
    ax.barh([0], [between], color=ORANGE)
    ax.barh([0], [within], left=[between], color=BLUE)
    ax.set_yticks([])
    ax.set_xlim(0, 100)
    ax.set_ylim(-1, 1)
    n_multi = summary['repeatability_anova_mm']['n_intervals_with_repeats']
    n_obs = summary['repeatability_anova_mm']['n_observations']
    ax.set_xlabel(f'share of residual variance, %-of-map terms  ({n_multi} intervals with >=2 repeats, {n_obs} obs)')
    ax.set_title('C.  Systematic vs random share of repeat variance', fontsize=10, loc='left', color=PRIMARY)
    bsd = anova['between_group_sd_pct']
    wsd = anova['pooled_within_group_sd_pct']
    ax.text(between / 2, 0.18, 'systematic\n(per-interval)', ha='center', va='center', fontsize=9, color='white', fontweight='bold')
    ax.text(between / 2, -0.32, f'{bsd:.1f}% SD, {between:.0f}% of variance', ha='center', va='center', fontsize=8, color='white')
    ax.text(between + within / 2, 0.18, 'random\n(lap-to-lap)', ha='center', va='center', fontsize=9, color='white', fontweight='bold')
    ax.text(between + within / 2, -0.32, f'{wsd:.1f}% SD, {within:.0f}% of variance', ha='center', va='center', fontsize=8, color='white')

    # D: eligibility-window coverage curve
    ax = axes[1, 1]
    win = summary['eligibility_window_check']
    ws = sorted(float(w) for w in win['by_window_single_pass'].keys())
    single = [getk(win['by_window_single_pass'], w)['pct_inside'] for w in ws]
    means = [getk(win['by_window_interval_mean'], w)['pct_inside'] for w in ws]
    xs = [w * 100 for w in ws]
    ax.plot(xs, single, marker='o', color=BLUE, label='single-pass intervals (n=%d)' % len(rows))
    ax.plot(xs, means, marker='s', color=ORANGE,
            label='per-interval means, systematic only (n=%d)' % win['n_interval_means'])
    ax.axvline(10, color=CRITICAL, linewidth=1.2, linestyle='--')
    ax.axhline(90, color=MUTED, linewidth=0.8, linestyle=':')
    ax.set_xlabel('eligibility window, +/- % of mapped MM-to-MM distance')
    ax.set_ylabel('% of intervals inside window')
    ax.set_title('D.  Coverage vs window width', fontsize=10, loc='left', color=PRIMARY)
    ax.legend(fontsize=8, frameon=False, loc='lower right')
    ax.set_ylim(0, 102)

    for ax in axes.flat:
        ax.spines['top'].set_visible(False)
        ax.spines['right'].set_visible(False)

    fig.tight_layout(rect=[0, 0, 1, 0.965])
    fig.savefig(args.png_out, dpi=160)
    print('wrote', args.png_out)


if __name__ == '__main__':
    main()
