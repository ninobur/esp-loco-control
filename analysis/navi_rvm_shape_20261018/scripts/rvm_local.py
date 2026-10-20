"""Local-baseline statistical shape recognizer (reference implementation for the
firmware). Same windows, shape representation, DCT basis, Gaussian-mixture class
densities, LLR > 0 decision and whole-ensemble preference as rvm_shape.py.
What changes is the REFERENCE:

  The reference for a candidate window is the recognized baseline that precedes it:
  BL = every valid sample from the end of the previously recognized ensemble's
  window (or from the operator's declaration at startup) up to the start of the
  candidate window. A least-squares straight line through ALL of BL, evaluated at
  the window centre, is the level; the noise is the mean absolute 20 ms
  difference over all of BL (converted to a Gaussian sigma). No fixed-duration
  window. A gently curving baseline is followed by the line's slope.

No circularity: a candidate for MM2 is only ever evaluated against BL1, and BL1's
start is fixed by MM1, which was itself recognized against BL0, back to the
declared startup baseline. A candidate window must start inside BL1 (its leading
context is baseline by the definition of the E class), so it never uses itself
as its own reference.

Sequential (retrospective) decision, the same rule offline and on the ESP32:
  - every 20 ms, every grid width W, the window ending now is scored (if it starts
    at least 2 BL samples after the BL start);
  - the best LLR > 0 window is held; a later window whose core overlaps the held
    core and scores higher replaces it (lobes, half-magnets and straddles lose to
    the whole ensemble);
  - the held window is accepted when the tracing has run on for half its own
    width beyond its end (relational, not a fixed time);
  - on acceptance: report midpoint/polarity, BL1 starts at the accepted window's
    end, all pending candidates are discarded and windows from BL1 are rescored.
"""
import numpy as np
import pandas as pd
import rvm_shape as R

MIN_BL = 2


class Tracing:
    def __init__(self, h, ccw):
        self.h = h; self.ccw = ccw; n = len(h)
        ok = np.isfinite(h); y = np.where(ok, h, 0.0); x = np.arange(n, dtype=float)
        c = lambda v: np.concatenate([[0.0], np.cumsum(v)])
        self.S0, self.Sx, self.Sy, self.Sxx, self.Sxy = c(ok * 1.0), c(ok * x), c(y), c(ok * x * x), c(ok * x * y)
        d = np.abs(np.diff(h, prepend=np.nan)); okd = np.isfinite(d)
        self.D0, self.D1 = c(okd * 1.0), c(np.where(okd, d, 0.0))
        self.nan_cum = c(~ok * 1.0)

    def ref(self, b, ws, c):
        """Level at centre c and noise from BL = [b, ws). Arrays allowed."""
        b = np.asarray(b, int); ws = np.asarray(ws, int)
        g = lambda S: S[ws] - S[b]
        n, sx, sy, sxx, sxy = g(self.S0), g(self.Sx), g(self.Sy), g(self.Sxx), g(self.Sxy)
        den = n * sxx - sx * sx
        slope = np.where(den > 1e-9, (n * sxy - sx * sy) / np.where(den > 1e-9, den, 1), 0.0)
        icpt = (sy - slope * sx) / np.maximum(n, 1)
        lvl = icpt + slope * np.asarray(c, float)
        nd = self.D1[ws] - self.D1[b]; cnt = self.D0[ws] - self.D0[b]
        noise = np.maximum(np.sqrt(np.pi) / 2 * nd / np.maximum(cnt, 1), 0.5)
        return lvl, noise, n


def features_ref(T, c, W, lvl, noise):
    """rvm_shape.features with a per-window level and noise (no context arrays)."""
    h = T.h; n = len(h)
    c = np.asarray(c, float); W = np.asarray(W, float)
    pos = c[:, None] + (np.linspace(-0.5, 0.5, R.NPT)[None, :] * W[:, None])
    lo = np.floor(pos).astype(int); fr = pos - lo
    valid = (lo.min(1) >= 0) & (lo.max(1) + 1 < n)
    lo = np.clip(lo, 0, n - 2)
    x = h[lo] * (1 - fr) + h[lo + 1] * fr
    a = np.clip((c - W / 2).astype(int), 0, n); b = np.clip((c + W / 2).astype(int) + 2, 0, n)
    valid &= (T.nan_cum[b] - T.nan_cum[a]) == 0
    x = np.nan_to_num(x - lvl[:, None])
    j = np.abs(x).argmax(1); pk = x[np.arange(len(x)), j]
    p = np.where(pk >= 0, 1, -1); A = np.maximum(np.abs(pk), 1e-6)
    u = p[:, None] * x / A[:, None]
    ci = np.clip(c.astype(int), 0, n - 1); rev = T.ccw[ci]; u[rev] = u[rev, ::-1]
    F = np.column_stack([u @ R.DCT.T, np.log(A / noise)])
    valid &= np.isfinite(F).all(1)
    return F, p, A, valid


def win_bounds(c, W):
    return np.floor(c - W / 2).astype(int), np.ceil(c + W / 2).astype(int)   # [start, end)


MIN_EXCURSION_ADC = 70.0


def score_windows(T, b, end_lo, end_hi, Em, Nm):
    """All grid windows with end in [end_lo, end_hi) and start >= b + MIN_BL."""
    out = []
    for W in R.SCALES:
        ends = np.arange(max(end_lo, b + MIN_BL + W), end_hi)
        if not len(ends): continue
        c = ends - W / 2.0; ws = ends - W
        ws_i, we_i = win_bounds(c, np.full(len(c), float(W)))
        ws_i = np.maximum(ws_i, b + MIN_BL)
        lvl, noise, nbl = T.ref(np.full(len(c), b), ws_i, c)
        F, p, A, v = features_ref(T, c, np.full(len(c), float(W)), lvl, noise)
        v &= nbl >= MIN_BL
        if not v.any(): continue
        llr = np.full(len(c), -np.inf); llr[v] = Em.score(F[v]) - Nm.score(F[v])
        k = (llr > 0) & (A >= MIN_EXCURSION_ADC)   # operator 70-count excursion gate (2026-10-20)
        if k.any():
            out.append(pd.DataFrame(dict(end=ends[k], c=c[k], w=W, llr=llr[k], pol=p[k], amp=A[k], lvl=lvl[k])))
    if not out: return pd.DataFrame(columns=['end', 'c', 'w', 'llr', 'pol', 'amp', 'lvl'])
    return pd.concat(out).sort_values(['end', 'llr'], ascending=[True, False]).reset_index(drop=True)


def run(T, Em, Nm, starts, stop=None, chunk=500):
    """starts: sorted list of (index, tag) baseline-establishing events (declarations).
    Recognition runs from the first start. Returns accepted ensembles."""
    n = len(T.h) if stop is None else stop
    acc = []
    starts = sorted(starts)
    for si, (s0, tag) in enumerate(starts):
        seg_end = starts[si + 1][0] if si + 1 < len(starts) else n
        # As on the locomotive: with seg_end blocks available, windows ending at
        # e <= seg_end - 2 have been scored (a window needs the block after its end).
        seg_lim = seg_end - 1
        b = s0; cursor = s0; B = None
        while cursor < seg_lim:
            hi = min(cursor + chunk, seg_lim)
            cand = score_windows(T, b, cursor, hi, Em, Nm)
            accepted_at = None
            for r in cand.itertuples():
                t = r.end
                if B is not None and t >= B['end'] + B['w'] / 2:
                    accepted_at = B['end'] + B['w'] / 2; break
                cs, ce = r.c - r.w / 4, r.c + r.w / 4
                if B is None:
                    B = r._asdict()
                elif cs < B['c'] + B['w'] / 4 and ce > B['c'] - B['w'] / 4:
                    if r.llr > B['llr']: B = r._asdict()
            if accepted_at is None and B is not None and hi - 1 >= B['end'] + B['w'] / 2:   # last scored end
                accepted_at = B['end'] + B['w'] / 2
            if accepted_at is not None:
                acc.append(dict(c=B['c'], w=B['w'], s=B['c'] - B['w'] / 4, e=B['c'] + B['w'] / 4, llr=B['llr'], pol=B['pol'],
                                amp=B['amp'], lvl=B['lvl'], bl_start=b, decided=accepted_at, seg=tag))
                b = int(np.ceil(B['end'])); cursor = b + MIN_BL + int(R.SCALES[0]); B = None
                cursor = max(cursor, b)
            else:
                cursor = hi
    return pd.DataFrame(acc)


# ---------------- training windows with the local reference ----------------
def bl_start_for(win_start, act_win_end_sorted):
    """BL start for a window starting at win_start = latest activity-window end <= win_start."""
    k = np.searchsorted(act_win_end_sorted, win_start, side='right') - 1
    return np.where(k >= 0, act_win_end_sorted[np.maximum(k, 0)], 0)


def train_features(T, C, W, act_win_end_sorted, floors=None):
    ws, _ = win_bounds(np.asarray(C, float), np.asarray(W, float))
    b = bl_start_for(ws, act_win_end_sorted).astype(int)
    if floors is not None: b = np.maximum(b, floors)
    lvl, noise, nbl = T.ref(b, np.maximum(ws, b), C)
    F, p, A, v = features_ref(T, C, W, lvl, noise)
    return F, p, A, v & (nbl >= MIN_BL) & (ws >= b + MIN_BL)
