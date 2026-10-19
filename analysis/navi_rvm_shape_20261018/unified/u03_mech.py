"""Waveform-continuity mechanisms, offline models (Python). Nothing here runs on Otto.
Each returns (keep, feed): keep[j] = block j joins the recognition waveform; feed[j] = index of the
incoming block at which block j reaches the recognizer (NaN if never), so feed[j] - j is the
reporting delay added by the mechanism. Order of kept blocks is always chronological.

R0  uninterrupted: every block, at once.
F   current firmware (RvmPause.h 3a8953c): STOPPED = applied PWM < 15; hold-back from commanded
    PWM < 15 (HOLD_MAX 512, oldest released when full); arrival trim + pattern resumption at GO;
    bounded feeding (one waiting block per incoming block). keep is continuation.pause_sequence
    (parity with the C++ firmware path established in s17/s21); feed is modelled here.
U   unified Hall-only candidate (this evaluation): every block passes through a delay line of
    k-1 blocks. A block is a STATIONARY REPETITION, and is withheld, when it lies in some k-block
    span whose range (max - min) is <= band, and (variant U1) the span lies off the baseline,
    i.e. |hall - baseline level| > m * noise for the block. No PWM, no state machine: when the
    tracing develops again, its blocks are no longer in any flat span and pass.
    Baseline level and noise (U1): established at the declaration (first 50 blocks, the declared
    baseline) and followed by an exponential mean (time constant 5 s) of blocks within m * noise
    of it; noise = sqrt(pi)/2 * mean |delta| of the declared baseline, floor 0.5 (the recognizer's
    estimator). This stands in for the recognizer's own local baseline reference.
U0  as U without the off-baseline condition (withholds flat baseline too)."""
import sys, pathlib, numpy as np
HERE = pathlib.Path(__file__).resolve().parent; sys.path.insert(0, str(HERE.parent / 'scripts'))
import continuation as K

def r0(h):
    n = len(h); return np.ones(n, bool), np.arange(n, dtype=float)

def _first_decl(decl, n):
    return min(decl) if len(decl) else n

def f_current(h, pwm, cmd, decl, hold_max=512):
    """Firmware F. cmd < 0 (not recorded) -> the s17 commanded-stop proxy."""
    pwm = np.asarray(pwm); stopcmd = (np.asarray(cmd) < 15) if (np.asarray(cmd) >= 0).all() else K.commanded_stop_proxy(pwm)
    keep = K.pause_sequence(np.asarray(h, float), pwm, stopcmd, decl, hold_max=hold_max)
    # --- release instants (mirrors pause_sequence's control flow) ---
    n = len(h); rel = np.full(n, np.nan); ds = set(int(d) for d in decl)
    state = 'Running'; held = []; lo = hi = None; nobs = 0
    for i in range(n):
        if i in ds:
            for j in held: rel[j] = i
            held.clear(); state = 'Startup'
        stopped = pwm[i] < 15
        if state == 'Startup':
            rel[i] = i
            if not stopped: state = 'Running'
            continue
        if state == 'Running':
            if stopped: state = 'Stopped'; nobs = 0; continue
            if stopcmd[i]:
                held.append(i)
                if len(held) > hold_max: rel[held.pop(0)] = i
            else:
                for j in held: rel[j] = i
                held.clear(); rel[i] = i
            continue
        if state == 'Stopped':
            if stopped: nobs += 1; continue
            for j in held: rel[j] = i                 # trimmed ones are not kept; harmless
            held.clear()
            if nobs == 0: state = 'Running'; rel[i] = i; continue
            state = 'Awaiting'
        if state == 'Awaiting':
            if stopped: state = 'Stopped'; continue
            rel[i] = i                                # pend/i: kept ones are fed at resumption (<= 1 block later)
            if keep[i] and i > 0 and keep[i - 1] and np.isnan(rel[i - 1]): rel[i - 1] = i
            if keep[i]: state = 'Running'
    rel = np.where(keep, rel, np.nan)
    return keep, _drain(keep, rel), rel

def _drain(keep, rel, per_block=2):
    """Feeding: blocks become waiting when released; at each incoming block at most per_block waiting
    blocks reach the recognizer (the field ESP32 drained about 70 blocks/s above the incoming 50/s,
    i.e. about 2.4 per incoming block; per_block=1 is the bench --drain 1 bound)."""
    n = len(keep); feed = np.full(n, np.nan)
    idx = np.where(keep & np.isfinite(rel))[0]; r = rel[idx].astype(np.int64)
    order = np.lexsort((idx, r)); idx, r = idx[order], r[order]
    q = 0; head = 0; t = 0
    from collections import deque
    Q = deque(); p = 0
    for i in range(n):
        while p < len(idx) and r[p] <= i: Q.append(idx[p]); p += 1
        for _ in range(per_block):
            if not Q: break
            feed[Q.popleft()] = i
    while Q: n += 1; feed[Q.popleft()] = n
    return feed

def flat_spans(h, k, band):
    """flat[j]: j lies in some k-block span with range <= band (spans with a gap are not flat)."""
    from numpy.lib.stride_tricks import sliding_window_view as sw
    n = len(h)
    if n < k: return np.zeros(n, bool)
    w = sw(np.asarray(h, float), k); r = w.max(1) - w.min(1)
    start = np.isfinite(r) & (r <= band)                      # span [s, s+k)
    c = np.concatenate([[0], np.cumsum(start)])
    j = np.arange(n); a = np.clip(j - k + 1, 0, len(start)); b = np.clip(j + 1, 0, len(start))
    return (c[b] - c[a]) > 0

def baseline_track(h, decl, m, tau_blocks=250):
    """Causal baseline level and noise (see module doc). Returns lvl, noise per block (NaN before the first declaration)."""
    n = len(h); lvl = np.full(n, np.nan); noi = np.full(n, np.nan); ds = sorted(int(d) for d in decl)
    a = 1.0 / tau_blocks; L = None; s = None
    for i in range(n):
        if ds and i == ds[0]:
            ds.pop(0); seg = np.asarray(h[i:i + 50], float); seg = seg[np.isfinite(seg)]
            if len(seg) >= 10:
                L = float(np.median(seg)); d = np.abs(np.diff(seg)); s = max(np.sqrt(np.pi) / 2 * d.mean(), 0.5)
        if L is None: continue
        lvl[i] = L; noi[i] = s
        x = h[i]
        if np.isfinite(x) and abs(x - L) <= m * s: L += a * (x - L)
    return lvl, noi

def u_unified(h, decl, k=50, band=24.0, m=16.0, off_baseline=True):
    h = np.asarray(h, float); n = len(h)
    flat = flat_spans(h, k, band)
    if off_baseline:
        lvl, noi = baseline_track(h, decl, m)
        off = np.isfinite(lvl) & np.isfinite(h) & (np.abs(h - lvl) > m * noi)
        withhold = flat & off
    else: withhold = flat
    fd = _first_decl(decl, n); withhold[:fd] = False
    keep = ~withhold
    feed = np.where(keep, np.minimum(np.arange(n) + (k - 1), n - 1), np.nan).astype(float)   # delay line of k-1 blocks
    return keep, feed
