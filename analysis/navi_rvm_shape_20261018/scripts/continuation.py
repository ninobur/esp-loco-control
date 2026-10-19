"""Candidate STOP/GO continuation rules (investigation). Each returns the mask of
blocks appended to the recognition waveform, given the Hall blocks H and the
PWM-pause mask k15 (stopgo.admit_mask). Only H and the pause are used.

The stationary reference: while PWM < 15 Otto is certainly stationary (30/30
measured stops), so the Hall blocks of the pause ARE an observation of the
stationary state at the stop: its level and its own fluctuation.

envelope_exit:  after the pause, blocks are withheld while they lie inside the
                range [min, max] of the paused (stationary) blocks; the first
                block outside that range is where the pattern has resumed, and
                it and everything after it is appended.
envelope_exit2: the same, but the departure must be seen in two consecutive
                blocks on the same side (one-block look-ahead)."""
import numpy as np


def _rule(H, k15, need, arrival=False, declarations=()):
    """Same semantics as RvmPause.h: the stationary reference accumulates over
    every paused block of one stop, including re-stops before the pattern resumed.
    A declaration ends any stop: from it the declared stationary baseline is appended
    (startup, already in k15)."""
    keep = k15.copy(); n = len(H); i = 0; decl = set(int(d) for d in declarations)
    while i < n:
        if k15[i]: i += 1; continue
        lo, hi = np.inf, -np.inf; k = i; first_pause = i
        while k < n:
            if k in decl and k > i: break                 # operator declaration: startup, not a resumption
            if not k15[k]:                                # paused: observe the stationary state
                if np.isfinite(H[k]): lo, hi = min(lo, H[k]), max(hi, H[k])
                k += 1; continue
            if not np.isfinite(lo): break                 # nothing observed: no reference
            side = lambda x: 0 if not np.isfinite(x) or lo <= x <= hi else (1 if x > hi else -1)
            s0 = side(H[k])
            if s0 != 0 and all(k + m < n and k15[k + m] and (k + m not in decl) and side(H[k + m]) == s0 for m in range(need)): break
            keep[k] = False; k += 1                       # stationary repetition: withheld
        if arrival and np.isfinite(lo):                   # ANALYSIS ONLY (not in firmware): mirror on the arrival side
            side = lambda x: 0 if not np.isfinite(x) or lo <= x <= hi else (1 if x > hi else -1)
            m = first_pause - 1
            while m >= 0 and keep[m]:
                s0 = side(H[m])
                if s0 != 0 and all(m - q >= 0 and side(H[m - q]) == s0 for q in range(need)): break
                keep[m] = False; m -= 1
        i = k
    return keep


def envelope_exit(H, C, P, k15):
    return _rule(H, k15, 1)


def envelope_exit2(H, C, P, k15):
    return _rule(H, k15, 2)


def envelope_both2(H, C, P, k15):
    """ANALYSIS ONLY: departure rule plus its mirror image on the arrival side."""
    return _rule(H, k15, 2, arrival=True)
