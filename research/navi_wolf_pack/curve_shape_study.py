#!/usr/bin/env python3
"""Research-only morphology study of the corrected independent Hall intervals."""
from __future__ import annotations

import csv
import json
from pathlib import Path

import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
import numpy as np
from scipy.optimize import curve_fit

ROOT = Path(__file__).resolve().parent
STUDY = ROOT / 'independent_width_study'
SOURCE = Path('/Users/davidbrown/NGR/NGR-Files/hall-ground-20261015/derived')
DTYPE = np.dtype([
    ('serial', '<u4'), ('us', '<u8'), ('adc', '<i2'),
    ('pwm', 'u1'), ('direction', 'u1'),
])
GRID = np.linspace(0, 1, 128)


def quantiles(x):
    a = np.asarray(x, float)
    a = a[np.isfinite(a)]
    if not len(a):
        return {k: float('nan') for k in ('min', 'p05', 'median', 'p95', 'max')}
    return {k: float(v) for k, v in zip(
        ('min', 'p05', 'median', 'p95', 'max'),
        np.quantile(a, [0, .05, .5, .95, 1]))}


def kmeans(x, k, seed=20261021, iterations=100):
    """Small deterministic k-means implementation; no classifier is fitted."""
    rng = np.random.default_rng(seed + k)
    centers = x[rng.choice(len(x), size=k, replace=False)].copy()
    for _ in range(iterations):
        d = ((x[:, None] - centers[None, :]) ** 2).sum(axis=2)
        labels = d.argmin(axis=1)
        revised = np.vstack([x[labels == j].mean(axis=0) for j in range(k)])
        if np.allclose(revised, centers):
            break
        centers = revised
    d = ((x - centers[labels]) ** 2).sum(axis=1)
    return labels, centers, float(d.sum())


def silhouette_sample(x, labels, limit=300, seed=20261021):
    """Mean silhouette on a deterministic sample; skipped for k=1."""
    if len(set(labels)) < 2:
        return None
    rng = np.random.default_rng(seed)
    indices = np.arange(len(x))
    if len(indices) > limit:
        indices = rng.choice(indices, limit, replace=False)
    distances = np.sqrt(((x[indices, None] - x[None, :]) ** 2).sum(axis=2))
    scores = []
    for row, i in enumerate(indices):
        same = labels == labels[i]
        a = distances[row, same].sum() / max(same.sum() - 1, 1)
        b = min(distances[row, labels == group].mean()
                for group in set(labels) if group != labels[i])
        scores.append((b-a) / max(a, b))
    return float(np.mean(scores))


def gaussian_with_line(t, a, b, amplitude, mu, sigma):
    """The competing per-curve physical approximation: line + one lobe."""
    return a + b*t + amplitude*np.exp(-.5*((t-mu)/sigma)**2)


def gaussian_errors(x):
    """Fit the user's line-plus-Gaussian model after alignment/normalization."""
    errors = []
    for y in x:
        try:
            params, _ = curve_fit(
                gaussian_with_line, GRID, y,
                p0=(0, 0, 1, .5, .2),
                bounds=((-1, -3, 0, 0, .03), (1, 3, 3, 1, 1)),
                maxfev=3000)
            errors.append(float(np.mean((y-gaussian_with_line(GRID, *params))**2)))
        except (RuntimeError, ValueError):
            errors.append(float('nan'))
    return np.asarray(errors)


def main():
    rows = list(csv.DictReader((STUDY / 'measured_supported_matched.csv').open()))
    selections = {(r['boot'], r['start_us'], r['mm'], r['direction']): r
                  for r in csv.DictReader((SOURCE / 'selected_matches.csv').open())}
    encounters = {(r['boot'], r['stream'], str(int(float(r['encounter_id'])))): r
                  for r in csv.DictReader((SOURCE / 'encounters_adc_only.csv').open())}
    ground = {(r['boot'], r['stream'], r['ground_id']): r
              for r in csv.DictReader((SOURCE / 'ground_intervals.csv').open())}
    traces = {boot: np.memmap(SOURCE / f'native_{boot}.bin', dtype=DTYPE, mode='r')
              for boot in {r['boot'] for r in rows}}

    shapes, metadata = [], []
    for row in rows:
        selected = selections[(row['boot'], row['start_us'], row['mm'], row['direction'])]
        event = encounters[(row['boot'], 'native', str(int(float(selected['encounter_id']))))]
        prior = ground[(row['boot'], 'native', event['left_ground_id'])]
        trace = traces[row['boot']]
        start, end = int(event['start_us']), int(event['end_us'])
        i0, i1 = np.searchsorted(trace['us'], [start, end], side='left')
        if i1 - i0 < 8:
            continue
        y = trace['adc'][i0:i1].astype(float)
        t = trace['us'][i0:i1].astype(float)
        baseline = float(prior['adc_mean'])
        curve = np.interp(start + GRID * (end-start), t, y) - baseline
        # Orient toward the larger excursion, then normalize amplitude.  The
        # resulting shape test asks whether the *form*, not polarity or gain,
        # needs multiple templates.
        polarity = 1 if abs(curve.max()) >= abs(curve.min()) else -1
        oriented = polarity * curve
        amplitude = float(np.quantile(oriented, .98))
        if amplitude <= 0:
            continue
        shapes.append(oriented / amplitude)
        metadata.append(dict(boot=row['boot'], mm=row['mm'], direction=row['direction'],
                    duration_s=float(row['ensemble_s']),
                    ir_mm=float(row['ensemble_ir_mm']) if row['ensemble_ir_mm'] else float('nan'),
                             amplitude_adc=amplitude, polarity=polarity,
                             samples=int(row['ensemble_native_samples'])))
    x = np.asarray(shapes)
    mean = x.mean(axis=0)
    centered = x - mean
    _, singular, vh = np.linalg.svd(centered, full_matrices=False)
    explained = singular**2 / (singular**2).sum()
    one_error = ((x-mean)**2).mean(axis=1)
    # Leave-one-out avoids giving a curve credit for contributing to the
    # empirical template used to reconstruct it.
    loo_error = ((x - ((x.sum(axis=0)-x)/(len(x)-1)))**2).mean(axis=1)
    gaussian_error = gaussian_errors(x)
    boots = np.asarray([r['boot'] for r in metadata])
    cross_boot = []
    for boot in sorted(set(boots)):
        test = boots == boot
        train_template = x[~test].mean(axis=0)
        cross_boot.append(dict(held_out_boot=boot, n=int(test.sum()),
            mse=quantiles(((x[test]-train_template)**2).mean(axis=1))))

    results = []
    labels_by_k = {}
    for k in range(1, 6):
        labels, centers, inertia = kmeans(x, k)
        labels_by_k[k] = labels
        results.append(dict(k=k, inertia=inertia,
                            mean_squared_error=float(inertia / x.size),
                            silhouette=silhouette_sample(x, labels),
                            cluster_sizes=[int((labels == j).sum()) for j in range(k)]))
    labels = labels_by_k[2]
    cluster_rows = []
    for cluster in (0, 1):
        subset = [metadata[i] for i in np.flatnonzero(labels == cluster)]
        cluster_rows.append(dict(cluster=cluster, n=len(subset),
            duration_s=quantiles([r['duration_s'] for r in subset]),
            ir_mm=quantiles([r['ir_mm'] for r in subset]),
            amplitude_adc=quantiles([r['amplitude_adc'] for r in subset]),
            cw=sum(r['direction'] == 'CW' for r in subset),
            ccw=sum(r['direction'] == 'CCW' for r in subset),
            positive_polarity=sum(r['polarity'] > 0 for r in subset)))

    out = STUDY / 'curve_shapes'
    out.mkdir(exist_ok=True)
    with (out / 'aligned_curves.csv').open('w', newline='') as f:
        fields = list(metadata[0]) + ['cluster_2'] + [f'x_{i:03d}' for i in range(len(GRID))]
        writer = csv.DictWriter(f, fieldnames=fields)
        writer.writeheader()
        for r, values, cluster in zip(metadata, x, labels):
            writer.writerow({**r, 'cluster_2': int(cluster),
                             **{f'x_{i:03d}': float(v) for i, v in enumerate(values)}})
    summary = dict(n=len(x), grid_points=len(GRID), orientation='larger absolute excursion is positive',
                   amplitude='98th percentile of oriented ADC departure from preceding observed baseline',
                   one_template_mse=quantiles(one_error),
                   one_template_leave_one_out_mse=quantiles(loo_error),
                   gaussian_line_mse=quantiles(gaussian_error),
                   gaussian_line_fit_failures=int((~np.isfinite(gaussian_error)).sum()),
                   cross_boot_template=cross_boot,
                   pca_explained=[float(v) for v in explained[:5]],
                   cluster_tests=results, two_cluster_description=cluster_rows)
    (out / 'summary.json').write_text(json.dumps(summary, indent=2) + '\n')

    fig, ax = plt.subplots(figsize=(10, 5))
    for values in x[::max(1, len(x)//160)]:
        ax.plot(GRID, values, color='#8aa4b8', alpha=.13, lw=.7)
    ax.plot(GRID, mean, color='#101820', lw=2.6, label='one-template mean')
    for j, color in enumerate(('#d95f02', '#1b9e77')):
        ax.plot(GRID, x[labels == j].mean(axis=0), color=color, lw=2,
                label=f'k=2 cluster {j} (n={(labels==j).sum()})')
    ax.set(xlabel='Observed ensemble progress (baseline departure → baseline return)',
           ylabel='Oriented, amplitude-normalized ADC departure',
           title='Corrected independent Hall ensembles: aligned raw curves')
    ax.legend(); fig.tight_layout(); fig.savefig(out / 'aligned_templates.png', dpi=160); plt.close(fig)

    s = summary
    lines = ['# Aligned independent Hall-curve morphology', '',
        f"This study aligns **{s['n']}** raw Hall curves from the corrected October-15 independent set. Each curve runs from observed baseline departure to observed baseline return; its preceding observed ground mean is subtracted, the larger excursion is oriented positive, and its amplitude is normalized. No RVM window or RVM acceptance is used.", '',
        '## Template test', '',
        f"The first five principal components explain {', '.join(f'{v:.1%}' for v in s['pca_explained'])} of shape variance. The one-template per-point squared-error distribution is median {s['one_template_mse']['median']:.4f} (5th–95th {s['one_template_mse']['p05']:.4f}–{s['one_template_mse']['p95']:.4f}). When each curve is reconstructed from a leave-one-out version of that template, the median is {s['one_template_leave_one_out_mse']['median']:.4f}.", '',
        '## Gaussian comparison', '',
        f"For context, each normalized curve was also fit individually to the proposed **line + one Gaussian lobe** model. Its median per-point squared error is {s['gaussian_line_mse']['median']:.4f} (5th–95th {s['gaussian_line_mse']['p05']:.4f}–{s['gaussian_line_mse']['p95']:.4f}; failed fits {s['gaussian_line_fit_failures']}). This is not a BIC contest: the Gaussian is refit with five parameters for every curve, whereas the empirical template is learned from other curves and fixed at application time. It answers the narrower question of whether the measured forms are close enough to one reusable shape to justify testing it as an alternative recognizer model.", '',
        '## Independent-boot check', '']
    for r in s['cross_boot_template']:
        lines.append(f"- Holding out boot `{r['held_out_boot']}` (n={r['n']}), the template learned from the other boot has median error {r['mse']['median']:.4f} (5th–95th {r['mse']['p05']:.4f}–{r['mse']['p95']:.4f}).")
    lines += [
        '',
        '| k | mean squared error | silhouette | cluster sizes |', '|---:|---:|---:|---:|']
    for r in results:
        sil = '—' if r['silhouette'] is None else f"{r['silhouette']:.3f}"
        lines.append(f"| {r['k']} | {r['mean_squared_error']:.5f} | {sil} | {r['cluster_sizes']} |")
    lines += ['', '## Two-cluster descriptive split', '',
        'The k=2 split is descriptive only; a positive silhouette would indicate separation, not physical magnet families. Duration, distance, direction and polarity summaries are retained so any apparent split can be checked for context rather than mistaken for a universal template.']
    for r in cluster_rows:
        lines.append(f"- Cluster {r['cluster']} (n={r['n']}): duration median {r['duration_s']['median']:.3f} s; IR-width median {r['ir_mm']['median']:.2f} mm; CW/CCW {r['cw']}/{r['ccw']}; oriented-positive original polarity {r['positive_polarity']}/{r['n']}.")
    lines += ['', '## Limit', '',
        'The result tests whether normalized observed forms are compact or separable in this one Otto dataset. It cannot prove physical magnet identity, establish a universal shape family, or transfer automatically to Toby, another sensor height, or a different reference regime.']
    (out / 'REPORT.md').write_text('\n'.join(lines) + '\n')


if __name__ == '__main__':
    main()
