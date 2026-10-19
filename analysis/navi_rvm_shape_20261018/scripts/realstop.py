"""Stop-over-a-magnet cases built from Otto's MEASURED station stops (boot 1).

For each recorded 5 s station stop we take the IR odometer track (position vs
time) and the applied PWM over [last cruise ... first cruise again]. Its speed
is normalized by the cruise speed before the deceleration, so the track is
expressed in 'cruise blocks' of travel. A recorded magnet stretch (recorded at
cruise) is then replayed along that track: the Hall value at time t is the
recorded stretch at the position the track has reached, placed so that the
train halts at a chosen point of the ensemble. Where the track moves slower
than cruise, interpolation repeats near-identical values, so fresh baseline
noise (the stretch's own block noise) is added there; while the train is
stationary, Otto's REAL recorded stationary Hall fluctuation at that stop is used.

IR is used ONLY to construct these test cases and the oracle (truly stationary
blocks). Nothing in the recognizer, the pause or the continuation rule reads IR."""
import numpy as np, pandas as pd
from common import *

_ir = None


def profiles(min_cruise_s=2.0):
    """Measured stop tracks: list of dict(d=normalized travel in cruise blocks per
    20 ms block, pwm=applied PWM per block, stop=index of the first fully stopped block)."""
    global _ir
    z = np.load(D / 'tracing_boot1.npz'); t = z['t_us']
    pwm = np.load(D / 'block_pwm_boot1.npy')
    if _ir is None:
        ir = pd.read_csv(HG / 'ir_coordinate_source.csv'); _ir = ir[ir.boot == BOOT1].sort_values('us')
    ir = _ir; ch = ir[ir.um.diff().fillna(1) != 0]                # odometer pulse instants (9.652 mm each)
    pt, px = ch.us.values.astype(float), ch.um.values / 1000.0
    ev = pd.read_csv(ROOT / 'results' / 'local' / 'stopped_threshold_evidence.csv')
    out = []
    for s in ev.stop_s.values:
        i = int((s * 1e6 - t[0]) / 20000)                         # first PWM-0 block of this stop
        flat = lambda k, step: all(pwm[k + step * m] == pwm[k] for m in range(1, 50))
        a = i
        while a > 50 and pwm[a - 1] >= pwm[a] and not flat(a, -1): a -= 1   # back up the deceleration ramp
        while i < len(pwm) - 1 and pwm[i] > 0: i += 1                      # first block whose every read is PWM 0
        j = i
        while j < len(pwm) - 1 and pwm[j] == 0: j += 1                       # end of the PWM-0 dwell
        b = j
        while b < len(pwm) - 51 and pwm[b + 1] >= pwm[b] and not flat(b, 1): b += 1  # up the acceleration ramp
        a -= int(min_cruise_s * 50); b += int(min_cruise_s * 50)
        tb = t[a:b].astype(float)
        # the stop gap: last pulse before the dwell, first pulse after it
        k1 = np.searchsorted(pt, t[i]) - 1; k2 = np.searchsorted(pt, t[j])
        if k1 < 2 or k2 >= len(pt) - 2 or pt[k1] < tb[0] or pt[k2] > tb[-1]: continue
        da = pt[k1] - pt[k1 - 1]; dd = pt[k2 + 1] - pt[k2]           # last arrival / first departure pulse intervals
        s0, s1 = pt[k1] + da, pt[k2] - dd                             # stationary between these instants
        if s1 <= s0: continue
        xm = px[k1] + 0.5 * (px[k2] - px[k1])                         # half a pulse rolled in, half rolled out
        kt = np.concatenate([pt[:k1 + 1], [s0, s1], pt[k2:]]); kx = np.concatenate([px[:k1 + 1], [xm, xm], px[k2:]])
        x = np.interp(tb, kt, kx)
        v = np.diff(np.concatenate([x, [np.interp(tb[-1] + 20000, kt, kx)]]))     # mm per block
        vc = np.median(v[:int(min_cruise_s * 50)])
        if not np.isfinite(vc) or vc <= 0: continue
        d = np.cumsum(np.concatenate([[0], v[:-1]])) / vc               # travel in cruise blocks
        still = (tb > s0) & (tb < s1)
        hb = z['hall'][a:b].astype(float)                               # the recorded (baseline) Hall at this stop
        res = np.where(still, hb - np.nanmedian(hb[still]), 0.0)        # Otto's real stationary fluctuation
        res = np.nan_to_num(res)
        out.append(dict(stop_s=float(s), d=d, pwm=pwm[a:b].astype(int), still=still, v=v / vc, x_mm=x - xm, v_mm=v,
                        still_res=res, still_s=float((s1 - s0) / 1e6), still_pwm_ge15_s=float((still & (pwm[a:b] >= 15)).sum() * 0.02)))
    return out


def stretch_mm_per_block(a, b):
    """Recorded cruise speed of boot-1 blocks [a, b) from the IR odometer (description only)."""
    z = np.load(D / 'tracing_boot1.npz'); t = z['t_us']
    x = np.interp([t[a], t[b - 1]], _ir.us.values, _ir.um.values / 1000.0)
    return abs(x[1] - x[0]) / (b - 1 - a)


def replay(h_stretch, ccw_stretch, stop_at, prof, sig, rng, mm_per_block):
    """Replay the stretch along the measured track (real millimetres) so the train
    halts at stretch block `stop_at`. Only the part of the track slower than the
    stretch's own cruise speed is used (deceleration, stop, acceleration); before
    and after it the stretch runs at its recorded cruise.
    Returns hall, ccw, pwm, orig (stretch coordinate), still (oracle) or None."""
    vm = prof['v_mm']; st = prof['still']; h0 = int(np.argmax(st)); h1 = len(st) - 1 - int(np.argmax(st[::-1]))
    fast = vm >= mm_per_block
    pre = np.where(fast[:h0])[0]; post = np.where(fast[h1:])[0]
    i0 = (pre[-1] + 1) if len(pre) else 0; i1 = (h1 + post[0]) if len(post) else len(vm)
    x = prof['x_mm'][i0:i1]
    pos = stop_at + x / mm_per_block                              # stretch coordinate of each block
    lo = int(np.floor(pos[0])); hi = int(np.ceil(pos[-1]))
    if lo < 1 or hi >= len(h_stretch) - 1: return None
    hall = np.interp(pos, np.arange(len(h_stretch)), h_stretch)
    slow = np.concatenate([[True], np.diff(pos) < 0.999])
    pw = prof['pwm'][i0:i1]; still = st[i0:i1]
    # stationary blocks: Otto's own recorded stationary fluctuation at this stop;
    # slow (moving) blocks: Gaussian noise of the stretch's own block noise
    hall = hall + np.where(still, prof['still_res'][i0:i1], np.where(slow, rng.normal(0, sig, len(hall)), 0.0))
    H = np.concatenate([h_stretch[:lo], hall, h_stretch[hi + 1:]])
    C = np.concatenate([ccw_stretch[:lo], np.repeat(ccw_stretch[stop_at], len(hall)), ccw_stretch[hi + 1:]])
    P = np.concatenate([np.full(lo, max(int(pw[0]), 15)), pw, np.full(len(h_stretch) - hi - 1, max(int(pw[-1]), 15))])
    O = np.concatenate([np.arange(lo, dtype=float), pos, np.arange(hi + 1, len(h_stretch), dtype=float)])
    S = np.concatenate([np.zeros(lo, bool), still, np.zeros(len(h_stretch) - hi - 1, bool)])
    return H, C, P, O, S


def robust_noise(h):
    """Block noise of a stretch that a magnet cannot inflate: 1.4826 * MAD of the
    20 ms differences / sqrt(2)."""
    d = np.diff(np.asarray(h, float)); d = d[np.isfinite(d)]
    return float(1.4826 * np.median(np.abs(d - np.median(d))) / np.sqrt(2))


_res = None
def stationary_noise(n, rng):
    """A contiguous piece of Otto's REAL recorded stationary Hall fluctuation
    (20 ms blocks, IR-confirmed still, median removed), length n."""
    global _res
    if _res is None:
        _res = [p['still_res'][p['still']] for p in profiles()]
    seg = _res[rng.integers(len(_res))]
    if n <= len(seg):
        o = rng.integers(len(seg) - n + 1); return seg[o:o + n].copy()
    return np.resize(seg, n)
