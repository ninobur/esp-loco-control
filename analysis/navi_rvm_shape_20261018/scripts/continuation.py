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


# ---------------------------------------------------------------------------
# Firmware reference (RvmPause.h, arrival mirror approved 2026-10-18).
# A sequential state machine identical to the C++, block by block.
HOLD_MAX = 512        # hold-back capacity: the arrival plateau must fit (measured max 291 blocks)
STOP_BELOW = 15


def commanded_stop_proxy(pwm):
    """Recordings carry applied PWM only. A commanded stop is taken to begin where
    the final monotone descent into a PWM<15 pause starts (manual throttle 0 / STOP
    set the target to 0 at that moment). Firmware uses its real commanded target."""
    pwm = np.asarray(pwm); out = np.zeros(len(pwm), bool)
    for i in range(1, len(pwm)):
        if pwm[i] < STOP_BELOW <= pwm[i - 1]:
            k = i
            while k > 0 and pwm[k - 1] >= pwm[k]: k -= 1        # back to the top of the descent
            while k < i and pwm[k + 1] == pwm[k]: k += 1        # the stop is commanded at the first decrease
            out[k:i] = True
    return out


def pause_sequence(H, P, stopcmd, declarations=(), hold_max=HOLD_MAX, need=2):
    """Returns keep: blocks appended to the recognition waveform (chronological)."""
    n = len(H); keep = np.zeros(n, bool); decl = set(int(d) for d in declarations)
    state = 'Running'; lo, hi = np.inf, -np.inf; nobs = 0
    held = []                                             # held-back block indices (oldest first)
    pend = None; pside = 0
    side = lambda x: 0 if (not np.isfinite(x)) or lo <= x <= hi else (1 if x > hi else -1)

    def release_all():
        for j in held: keep[j] = True
        held.clear()

    def trim_and_release():                               # mirror rule on the arrival side
        m = len(held) - 1
        while m >= 0:
            s0 = side(H[held[m]])
            if s0 != 0 and all(m - q >= 0 and side(H[held[m - q]]) == s0 for q in range(need)): break
            m -= 1
        del held[m + 1:]
        release_all()

    for i in range(n):
        if i in decl:                                     # declaration: held blocks are released to the old context first
            release_all(); pend = None; state = 'Startup'
        stopped = P[i] < STOP_BELOW
        if state == 'Startup':
            keep[i] = True
            if not stopped: state = 'Running'
            continue
        if state == 'Running':
            if stopped:
                state = 'Stopped'; lo, hi = np.inf, -np.inf; nobs = 0; pend = None
                if np.isfinite(H[i]): lo, hi, nobs = H[i], H[i], 1
                continue
            if stopcmd[i]:
                held.append(i)
                if len(held) > hold_max: keep[held.pop(0)] = True
            else:
                release_all(); keep[i] = True
            continue
        if state == 'Stopped':
            if stopped:
                if np.isfinite(H[i]): lo, hi, nobs = min(lo, H[i]), max(hi, H[i]), nobs + 1
                continue
            if nobs == 0:
                release_all(); state = 'Running'; keep[i] = True; continue
            trim_and_release(); state = 'Awaiting'; pend = None
        if state == 'Awaiting':
            if stopped:
                pend = None; state = 'Stopped'
                if np.isfinite(H[i]): lo, hi, nobs = min(lo, H[i]), max(hi, H[i]), nobs + 1
                continue
            s = side(H[i])
            if pend is not None:
                if s != 0 and s == pside:
                    keep[pend] = True; keep[i] = True; pend = None; state = 'Running'; continue
                pend = None
            if s != 0: pend, pside = i, s
    # end of data: blocks still held stay held, as on the locomotive (no GO followed)
    return keep
