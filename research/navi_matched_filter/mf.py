"""Normalized matched-filter Hall recognizer — research core (no firmware).

Signal path, all of it local; nothing is carried from one magnet to the next:

1. Native samples (serial order, ~5 kHz in bursts of 5) pass a running
   median of 5 consecutive samples. A single-sample spike cannot survive it.
2. The median stream is averaged into uniform DT_S bins. Empty bins are gaps:
   gaps up to GAP_FILL_S are linearly bridged; longer gaps split the trace and
   no window may straddle them.
3. For each template time scale D the window is the template (duration D)
   padded by PAD*D of flat ground on each side. Within every window the
   observed samples x are regressed on [1, t, T]: a local level, a local slope
   and the template. Two numbers come out:
     score  r = <x, T'> / ||x'||   (T' = T with level+slope removed, unit norm;
                                     x' = x with level+slope removed)
     amplitude A = least-squares coefficient of T, in ADC counts.
   A constant offset or a linear drift inside the window is absorbed by the
   level and slope terms, so neither the score nor A needs a baseline that was
   frozen earlier. sign(A) is polarity (+ = ADC rises = North on Otto/Toby).
4. Events are local maxima of |r| (over time and scale) that pass a score
   threshold and |A| >= an amplitude floor, with non-maximum suppression.

Everything here is offline, vectorised numpy. Causal (real-time) behaviour is
emulated separately in `causal_events`, which only uses samples up to the
decision time.
"""
from __future__ import annotations

import csv
from dataclasses import dataclass, field
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parent
CURVES = ROOT.parent / 'navi_wolf_pack' / 'independent_width_study' / 'curve_shapes' / 'aligned_curves.csv'

DT_S = 0.002          # 500 Hz analysis grid
GAP_FILL_S = 0.020    # bridge gaps up to 20 ms
MEDIAN_N = 5


# --------------------------------------------------------------------------
# Template
# --------------------------------------------------------------------------
def load_curves(path=CURVES):
    rows = list(csv.DictReader(open(path)))
    x = np.array([[float(r['x_%03d' % i]) for i in range(128)] for r in rows])
    return rows, x


def template_from_curves(x):
    """Mean oriented, amplitude-normalised curve on [0,1] (departure→return)."""
    m = x.mean(axis=0)
    return m / np.quantile(m, 0.98)


_T_CACHE = {}


def template_mean():
    if 'mean' not in _T_CACHE:
        _T_CACHE['mean'] = template_from_curves(load_curves()[1])
    return _T_CACHE['mean']


def sampled_template(shape, duration_s, pad=0.5, dt=DT_S):
    """Template of `duration_s` with pad*duration of zeros each side, on dt."""
    n_core = max(int(round(duration_s / dt)), 8)
    n_pad = int(round(pad * duration_s / dt))
    grid = np.linspace(0, 1, len(shape))
    core = np.interp(np.linspace(0, 1, n_core), grid, shape)
    t = np.concatenate([np.zeros(n_pad), core, np.zeros(n_pad)])
    if len(t) % 2 == 0:
        t = np.append(t, 0.0)
    return t, int(np.argmax(core)) + n_pad   # template, index of template peak


# --------------------------------------------------------------------------
# Conditioning
# --------------------------------------------------------------------------
def running_median(raw, n=MEDIAN_N):
    raw = np.asarray(raw, float)
    if len(raw) < n:
        return raw.copy()
    from numpy.lib.stride_tricks import sliding_window_view
    med = np.median(sliding_window_view(raw, n), axis=1)
    h = n // 2
    return np.concatenate([raw[:h], med, raw[-h:]])


@dataclass
class Trace:
    t: np.ndarray          # seconds from trace start (bin centres)
    x: np.ndarray          # ADC, NaN in unfilled gaps
    pwm: np.ndarray        # applied PWM per bin (last sample), 0 in gaps
    direction: np.ndarray  # recorded direction per bin
    t0_us: int = 0
    name: str = ''
    meta: dict = field(default_factory=dict)


def condition(native, dt=DT_S, gap_fill=GAP_FILL_S, median_n=MEDIAN_N, name=''):
    t_us = native['t_us'].astype(np.int64)
    t0 = int(t_us[0])
    ts = (t_us - t0) / 1e6
    y = running_median(native['raw'], median_n) if median_n > 1 else native['raw'].astype(float)
    nb = int(ts[-1] / dt) + 1
    idx = np.minimum((ts / dt).astype(np.int64), nb - 1)
    cnt = np.bincount(idx, minlength=nb)
    s = np.bincount(idx, weights=y, minlength=nb)
    x = np.full(nb, np.nan)
    ok = cnt > 0
    x[ok] = s[ok] / cnt[ok]
    pwm = np.zeros(nb)
    pwm[idx] = native['pwm']            # last sample in bin wins
    d = np.zeros(nb)
    d[idx] = native['direction']
    # forward-fill pwm/direction across empty bins
    last = np.maximum.accumulate(np.where(ok, np.arange(nb), 0))
    pwm, d = pwm[last], d[last]
    # bridge short gaps
    good = np.flatnonzero(ok)
    if len(good) > 1:
        gaps = np.diff(good) - 1
        for g0, g in zip(good[:-1][gaps > 0], gaps[gaps > 0]):
            if g * dt <= gap_fill:
                a, b = x[g0], x[g0 + g + 1]
                x[g0 + 1:g0 + g + 1] = a + (b - a) * np.arange(1, g + 1) / (g + 1)
    return Trace(t=(np.arange(nb) + 0.5) * dt, x=x, pwm=pwm, direction=d, t0_us=t0, name=name)


# --------------------------------------------------------------------------
# Sliding regression on [1, t, T]
# --------------------------------------------------------------------------
def _sliding_dot(x, k):
    """y[i] = sum_j x[i - h + j] * k[j], centred, 'same' length (NaN-free x)."""
    n = len(x) + len(k) - 1
    nf = 1 << (n - 1).bit_length()
    full = np.fft.irfft(np.fft.rfft(x, nf) * np.fft.rfft(k[::-1], nf), nf)[:n]
    h = len(k) // 2
    return full[h:h + len(x)]


def matched_filter(x, template, detrend='line'):
    """Return (score r, amplitude A) at every centre index; NaN near gaps/edges.

    detrend='line' removes level and slope inside each window; 'mean' removes
    the level only (ordinary Pearson correlation)."""
    L = len(template)
    h = L // 2
    u = np.linspace(-1, 1, L)
    basis = [np.ones(L)] + ([u] if detrend == 'line' else [])
    B = np.array(basis).T
    Q, _ = np.linalg.qr(B)                        # orthonormal basis columns
    Tr = template - Q @ (Q.T @ template)          # template residual
    nTr = np.linalg.norm(Tr)
    Tn = Tr / nTr
    bad = ~np.isfinite(x)
    xf = np.where(bad, 0.0, x)
    # mean-centre globally for numerical stability (offset invariant anyway)
    xf = xf - (np.mean(xf[~bad]) if (~bad).any() else 0.0)
    xf = np.where(bad, 0.0, xf)
    dot_T = _sliding_dot(xf, Tn)
    ss = _sliding_dot(xf * xf, np.ones(L))
    proj2 = np.zeros_like(xf)
    for j in range(Q.shape[1]):
        proj2 += _sliding_dot(xf, Q[:, j]) ** 2
    res2 = np.maximum(ss - proj2, 1e-9)
    r = dot_T / np.sqrt(res2)
    A = dot_T / nTr
    # invalidate windows touching gaps or edges
    badw = _sliding_dot(bad.astype(float), np.ones(L)) > 0.5
    badw[:h] = True
    badw[-h:] = True
    r[badw] = np.nan
    A[badw] = np.nan
    return r, A


# --------------------------------------------------------------------------
# Event extraction (offline, uses future samples)
# --------------------------------------------------------------------------
DEFAULT_SCALES = (0.15, 0.24, 0.38, 0.60, 0.95, 1.50)


@dataclass
class Config:
    scales: tuple = DEFAULT_SCALES
    pad: float = 0.5
    detrend: str = 'line'
    r_min: float = 0.6
    a_min: float = 70.0
    nms: float = 0.5          # suppression radius, fraction of event scale
    shape: np.ndarray | None = None


def scale_stack(trace, cfg):
    shape = cfg.shape if cfg.shape is not None else template_mean()
    R, A, peak_off = [], [], []
    for D in cfg.scales:
        T, ipk = sampled_template(shape, D, cfg.pad)
        r, a = matched_filter(trace.x, T, cfg.detrend)
        R.append(r)
        A.append(a)
        peak_off.append(ipk - len(T) // 2)     # template peak relative to centre
    return np.array(R), np.array(A), np.array(peak_off)


def events_from_stack(trace, R, A, peak_off, cfg, qualify=True):
    """Local maxima of |r| across time and scale, then NMS by radius nms*D."""
    absr = np.where(np.isfinite(R), np.abs(R), -1.0)
    ok = (absr >= cfg.r_min)
    if qualify:
        ok &= np.isfinite(A) & (np.abs(A) >= cfg.a_min)
    sc = np.where(ok, absr, -1.0)
    best_s = np.argmax(sc, axis=0)
    best = sc[best_s, np.arange(sc.shape[1])]
    cand = np.flatnonzero((best > 0)
                          & (best >= np.roll(best, 1)) & (best > np.roll(best, -1)))
    order = cand[np.argsort(-best[cand])]
    taken = []
    blocked = np.zeros(len(best), bool)
    for i in order:
        if blocked[i]:
            continue
        s = best_s[i]
        D = cfg.scales[s]
        rad = int(cfg.nms * D / DT_S)
        blocked[max(0, i - rad):i + rad + 1] = True
        taken.append(i)
    ev = []
    for i in sorted(taken):
        s = best_s[i]
        ipk = min(max(i + peak_off[s], 0), len(trace.t) - 1)
        ev.append(dict(i=int(i), t=float(trace.t[i]), t_peak=float(trace.t[ipk]),
                       scale=float(cfg.scales[s]), r=float(R[s, i]), A=float(A[s, i]),
                       pol=int(np.sign(A[s, i])), pwm=float(trace.pwm[i]),
                       direction=int(trace.direction[i])))
    return ev


def recognize(trace, cfg):
    R, A, po = scale_stack(trace, cfg)
    return events_from_stack(trace, R, A, po, cfg), (R, A, po)
