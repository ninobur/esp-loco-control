"""Statistical magnetic shape recognizer (research). One method; Model A and Model B
differ ONLY in the training windows given to fit_classes().

Representation of a window of the 20 ms tracing
  - window = [c - W/2, c + W/2] on a fixed multi-scale grid (centre every 20 ms,
    widths geometric, factor 1.25). The grid range is a search range, not a gate.
  - level  = running median of the tracing over +-10 s (context level; no threshold)
  - noise  = running robust s.d. of 20 ms first differences over +-10 s
  - x      = window resampled LINEARLY to NPT points minus level. Linear time
             scaling only: no warping, so feature order and proportions are kept
             (no rubber map: a shoulder cannot be stretched into a principal lobe).
  - polarity p = sign of x at its largest |x|; amplitude A = max|x|
  - shape u = p * x / A  (principal excursion -> +1), reversed in time when the
             declared session direction is CCW (map orientation)
  - features = first NDCT DCT-II coefficients of u (fixed basis, not learned)
               + log(A / noise)
Classes: E = an ensemble seen whole and centred in the window (ensemble with its
lobes, shoulders and transitions, i.e. window = ensemble + half its duration of
context on each side); N = everything else (baseline windows, and windows that
cut through or straddle ensembles). Each class is a Gaussian mixture.
Decision: log-likelihood ratio LLR = log p(f|E) - log p(f|N), equal priors, LLR > 0.
Grouping: greedy best-explanation - repeatedly accept the highest-LLR window whose
core (middle half = the ensemble estimate) does not overlap an accepted core.
No PWM, IR, speed, distance, arrival time or duration enters anywhere."""
import os
import numpy as np
import pandas as pd

NPT = 32
NDCT = 12
SCALES = np.unique(np.round(8 * 1.25 ** np.arange(0, 19)).astype(int))  # widths in samples: 8..~444 (0.16-8.9 s)
CTX = 500        # +-10 s context for level / noise
REG = 1e-3       # covariance regularisation (same for A and B)

_k = np.arange(NPT)
DCT = np.cos(np.pi / NPT * (_k[None, :] + 0.5) * np.arange(NDCT)[:, None])   # (NDCT, NPT)
DCT[0] *= 1 / np.sqrt(2)
DCT *= np.sqrt(2 / NPT)


def context(h):
    s = pd.Series(h)
    lvl = s.rolling(2 * CTX + 1, center=True, min_periods=50).median().to_numpy()
    d = s.diff()
    mad = d.rolling(2 * CTX + 1, center=True, min_periods=50).apply(
        lambda v: np.nanmedian(np.abs(v - np.nanmedian(v))), raw=True).to_numpy()
    noise = np.maximum(1.4826 * mad / np.sqrt(2), 0.5)
    return lvl, noise


def context_fast(h):
    """Same as context() but subsampled every 25 samples then interpolated (speed)."""
    n = len(h); idx = np.arange(0, n, 25)
    lv = np.empty(len(idx)); nz = np.empty(len(idx))
    d = np.diff(h, prepend=np.nan)
    for j, i in enumerate(idx):
        if os.environ.get('RVM_VARIANT') == 'causal_level':
            a, b = max(0, i - 2 * CTX), i + 1          # trailing 20 s only
        else:
            a, b = max(0, i - CTX), min(n, i + CTX + 1)
        w = h[a:b]; w = w[np.isfinite(w)]
        lv[j] = np.median(w) if len(w) > 50 else np.nan
        dd = d[a:b]; dd = dd[np.isfinite(dd)]
        nz[j] = 1.4826 * np.median(np.abs(dd - np.median(dd))) / np.sqrt(2) if len(dd) > 50 else np.nan
    ok = np.isfinite(lv)
    lvl = np.interp(np.arange(n), idx[ok], lv[ok]); noise = np.maximum(np.interp(np.arange(n), idx[ok], nz[ok]), 0.5)
    return lvl, noise


def features(h, lvl, noise, ccw, centres, widths):
    """centres, widths: integer arrays (sample index, samples). Returns F (n, NDCT+1),
    polarity, amplitude, valid mask. Windows touching a gap (NaN) or the edge are invalid."""
    n = len(h)
    c = np.asarray(centres, float); W = np.asarray(widths, float)
    pos = c[:, None] + (np.linspace(-0.5, 0.5, NPT)[None, :] * W[:, None])
    lo = np.floor(pos).astype(int); fr = pos - lo
    valid = (lo.min(1) >= 0) & (lo.max(1) + 1 < n)
    lo = np.clip(lo, 0, n - 2)
    x = h[lo] * (1 - fr) + h[lo + 1] * fr
    # a gap anywhere inside the window (not just at sample points) invalidates it
    nan_cum = np.concatenate([[0], np.cumsum(~np.isfinite(h))])
    a = np.clip((c - W / 2).astype(int), 0, n); b = np.clip((c + W / 2).astype(int) + 2, 0, n)
    valid &= (nan_cum[b] - nan_cum[a]) == 0
    ci = np.clip(c.astype(int), 0, n - 1)
    x = x - lvl[ci][:, None]
    x = np.nan_to_num(x)
    j = np.abs(x).argmax(1)
    pk = x[np.arange(len(x)), j]
    p = np.where(pk >= 0, 1, -1)
    A = np.maximum(np.abs(pk), 1e-6)
    u = p[:, None] * x / A[:, None]
    rev = ccw[ci]
    u[rev] = u[rev, ::-1]
    F = np.column_stack([u @ DCT.T, np.log(A / noise[ci])])
    if os.environ.get('RVM_VARIANT') == 'shape_only':
        F = F[:, :-1]
    return F, p, A, valid


# ---------------- Gaussian mixture (EM), numpy only ----------------
class GMM:
    def __init__(self, K, seed=0, iters=200):
        self.K, self.seed, self.iters = K, seed, iters

    def fit(self, X):
        X = X[np.isfinite(X).all(1)]
        rng = np.random.default_rng(self.seed); n, d = X.shape; K = min(self.K, n)
        mu = X[rng.choice(n, K, replace=False)]
        # k-means++-lite init then EM
        for _ in range(10):
            lab = ((X[:, None, :] - mu[None]) ** 2).sum(-1).argmin(1)
            mu = np.array([X[lab == k].mean(0) if (lab == k).any() else mu[k] for k in range(K)])
        S = np.array([np.cov(X[lab == k].T) + REG * np.eye(d) if (lab == k).sum() > d else np.cov(X.T) + REG * np.eye(d) for k in range(K)])
        w = np.array([(lab == k).mean() for k in range(K)]) + 1e-6; w /= w.sum()
        prev = -np.inf
        for _ in range(self.iters):
            L = self._comp_ll(X, w, mu, S)
            m = L.max(1, keepdims=True); ll = m[:, 0] + np.log(np.exp(L - m).sum(1))
            R = np.exp(L - ll[:, None]); Nk = np.maximum(R.sum(0), 1e-6)
            w = np.maximum(Nk / n, 1e-12); w /= w.sum(); mu = (R.T @ X) / Nk[:, None]
            S = np.array([((R[:, k, None] * (X - mu[k])).T @ (X - mu[k])) / Nk[k] + REG * np.eye(d) for k in range(K)])
            if ll.mean() - prev < 1e-6: break
            prev = ll.mean()
        self.w, self.mu, self.S = w, mu, S; self.n, self.d = n, d
        self.loglik = float(self.score(X).sum())
        return self

    @staticmethod
    def _comp_ll(X, w, mu, S):
        out = np.empty((len(X), len(w)))
        for k in range(len(w)):
            Lc = np.linalg.cholesky(S[k]); z = np.linalg.solve(Lc, (X - mu[k]).T)
            out[:, k] = np.log(w[k]) - 0.5 * (z ** 2).sum(0) - np.log(np.diag(Lc)).sum() - 0.5 * X.shape[1] * np.log(2 * np.pi)
        return out

    def score(self, X):
        L = self._comp_ll(X, self.w, self.mu, self.S); m = L.max(1, keepdims=True)
        return m[:, 0] + np.log(np.exp(L - m).sum(1))

    def predict(self, X):
        return self._comp_ll(X, self.w, self.mu, self.S).argmax(1)

    def bic(self):
        K, d = len(self.w), self.d
        return -2 * self.loglik + (K - 1 + K * d + K * d * (d + 1) / 2) * np.log(self.n)


class Mixture:
    """Union of fitted GMMs as one class density (used for Model B's 21 magnets)."""
    def __init__(self, parts, weights):
        self.parts = parts; self.lw = np.log(np.asarray(weights) / np.sum(weights))

    def score(self, X):
        L = np.column_stack([p.score(X) + lw for p, lw in zip(self.parts, self.lw)])
        m = L.max(1, keepdims=True); return m[:, 0] + np.log(np.exp(L - m).sum(1))


# ---------------- training windows ----------------
def true_window(start_i, end_i):
    """Ensemble extent in samples -> centred window holding it plus half its duration each side."""
    dur = max(end_i - start_i, 1)
    return (start_i + end_i) / 2, 2.0 * dur


def ensemble_windows(ens, rng, jitter=8):
    """ens: DataFrame with s_i, e_i (sample indices). Jittered copies represent the
    annotation's own boundary uncertainty (centre +-5 % W, width x0.9-1.1)."""
    C, W, idx = [], [], []
    for r in ens.itertuples():
        c, w = true_window(r.s_i, r.e_i)
        for _ in range(jitter):
            C.append(c + rng.uniform(-0.05, 0.05) * w); W.append(w * rng.uniform(0.9, 1.1)); idx.append(r.Index)
    return np.array(C), np.array(W), np.array(idx)


def other_windows(n_total, intervals_ens, rng, n_draw, restrict=None):
    """Random grid windows whose core is not a good match to any ensemble:
    core IoU with every annotated extent < 0.3. Includes pure baseline, partial
    and straddling windows. restrict: optional list of (a,b) sample spans."""
    st = intervals_ens[:, 0]; en = intervals_ens[:, 1]
    C, W = [], []
    tries = 0
    while len(C) < n_draw and tries < n_draw * 20:
        tries += 1
        if restrict is not None:
            a, b = restrict[rng.integers(len(restrict))]; c = rng.integers(a, b)
        else:
            c = rng.integers(0, n_total)
        w = SCALES[rng.integers(len(SCALES))]
        cs, ce = c - w / 4, c + w / 4
        k = np.searchsorted(st, ce)
        ok = True
        for j in range(max(0, k - 3), min(len(st), k + 3)):
            inter = max(0, min(ce, en[j]) - max(cs, st[j])); uni = max(ce, en[j]) - min(cs, st[j])
            if inter / uni >= 0.3: ok = False; break
        if ok: C.append(c); W.append(w)
    return np.array(C), np.array(W)


# ---------------- detection ----------------
def scan(h, lvl, noise, ccw, Emodel, Nmodel, span=None, chunk=200000):
    """LLR for every grid window. span=(a,b) restricts window centres."""
    a, b = span if span is not None else (0, len(h))
    out = []
    for w in SCALES:
        cs = np.arange(a, b)
        for k in range(0, len(cs), chunk):
            c = cs[k:k + chunk]
            F, p, A, v = features(h, lvl, noise, ccw, c, np.full(len(c), w))
            c, F, p, A = c[v], F[v], p[v], A[v]
            if not len(c): continue
            llr = Emodel.score(F) - Nmodel.score(F)
            keep = llr > 0
            out.append(pd.DataFrame(dict(c=c[keep], w=w, llr=llr[keep], pol=p[keep], amp=A[keep])))
    return pd.concat(out) if out else pd.DataFrame(columns=['c', 'w', 'llr', 'pol', 'amp'])


def best_explanation(cand):
    """Greedy: highest LLR first; accept if its core does not overlap an accepted core."""
    cand = cand.sort_values('llr', ascending=False)
    cs = (cand.c - cand.w / 4).to_numpy(); ce = (cand.c + cand.w / 4).to_numpy()
    acc_s, acc_e, keep = [], [], []
    import bisect
    for i in range(len(cand)):
        s, e = cs[i], ce[i]
        k = bisect.bisect_left(acc_s, s)
        bad = (k > 0 and acc_e[k - 1] > s) or (k < len(acc_s) and acc_s[k] < e)
        if not bad:
            acc_s.insert(k, s); acc_e.insert(k, e); keep.append(i)
    d = cand.iloc[keep].copy()
    d['s'] = d.c - d.w / 4; d['e'] = d.c + d.w / 4
    return d.sort_values('c').reset_index(drop=True)
