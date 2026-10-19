"""Can the Hall tracing alone tell an ensemble that has stopped developing from one
still developing? Evidence only (no rule is proposed or implemented).
  stationarity_evidence.py <run8 native .npy> <out_dir>
For a span of tau seconds, the change the tracing shows is its range (max - min of the
20 ms block means). Compared:
  STATIONARY inside a magnet: run 8 stall plateaus (Otto still over MM109 for ~59 s
     and over MM023 for ~8 s, applied PWM 29-35), every tau-span inside them.
  STATIONARY on baseline: boot 1 station stops (IR odometer: no pulse), every tau-span.
  MOVING inside a magnet: boot 1 annotated ensemble extents where the IR odometer shows
     movement (consecutive pulses < 1 s apart, i.e. > 9.7 mm/s, around the whole span),
     and run 8 recognized ensemble cores outside the stalls. For each passage, the
     flattest tau-span (minimum range): how unchanged a MOVING magnet can look.
Also: the local noise scale the recognizer already estimates (sqrt(pi)/2 * mean |d|)."""
import sys, json, pathlib
import numpy as np, pandas as pd
HERE = pathlib.Path(__file__).resolve().parent; sys.path.insert(0, str(HERE.parent / 'scripts'))
from common import *

NPY, OUT = sys.argv[1], pathlib.Path(sys.argv[2]); OUT.mkdir(parents=True, exist_ok=True)
TAUS = [0.2, 0.5, 1.0, 2.0, 4.0, 8.0]
STALLS = [('MM109', 606.0, 665.0), ('MM023', 903.5, 911.5)]

def blocks_from_native(a):
    us = a['us'].astype(np.int64); o = np.argsort(us, kind='stable'); us = us[o]; adc = a['adc'][o].astype(float)
    B = 20000; t0 = us[0] // B * B; b = (us - t0) // B; nb = int(b[-1]) + 1
    n = np.bincount(b, minlength=nb); h = np.where(n >= 50, np.bincount(b, adc, nb) / np.maximum(n, 1), np.nan)
    pw = np.zeros(nb, np.int16); np.maximum.at(pw, b, a['pwm'][o].astype(np.int16))
    return (t0 + np.arange(nb) * B + B // 2) / 1e6, h, pw

def ranges(h, k):
    """range of every k-block span (NaN if the span holds a gap)."""
    if len(h) < k: return np.array([])
    w = np.lib.stride_tricks.sliding_window_view(h, k)
    r = w.max(1) - w.min(1); return r[np.isfinite(r)]

def noise(h):
    d = np.abs(np.diff(h)); d = d[np.isfinite(d)]; return float(np.sqrt(np.pi) / 2 * d.mean()) if len(d) else np.nan

res = dict(taus_s=TAUS)
# ---- run 8 ----
t8, h8, p8 = blocks_from_native(np.load(NPY))
stall = {}
for name, s0, s1 in STALLS:
    m = (t8 >= s0) & (t8 < s1); hs = h8[m]
    stall[name] = dict(seconds=round(s1 - s0, 1), pwm=[int(p8[m].min()), int(p8[m].max())], level_mean=round(float(np.nanmean(hs)), 1),
                       noise=round(noise(hs), 2),
                       range_by_tau={str(t): dict(median=round(float(np.median(r)), 1), p99=round(float(np.percentile(r, 99)), 1), max=round(float(r.max()), 1))
                                     for t in TAUS if len(r := ranges(hs, int(round(t / 0.02)))) })
res['run8_stall_plateaus'] = stall
# run 8 moving passages: recognized cores (from the replay output), outside the stalls
ens8 = pd.read_csv(HERE / 'run8_replay' / 'replay_actual_ensembles.csv')
B8 = 0.02; t0_8 = t8[0] - 0.01
mov8 = []
for r in ens8.itertuples():
    s_t, e_t = t0_8 + r.s_id * B8, t0_8 + (r.e_id + 1) * B8
    if any(not (e_t <= a_ or s_t >= b_) for _, a_, b_ in STALLS): continue
    m = (t8 >= s_t) & (t8 < e_t); hs = h8[m]
    row = dict(mid_s=round(r.mid_s, 2), core_s=round(e_t - s_t, 2), pwm=int(np.median(p8[m])))
    for t in TAUS:
        k = int(round(t / 0.02)); rr = ranges(hs, k); row[f'min_range_{t}'] = round(float(rr.min()), 1) if len(rr) else None
    mov8.append(row)
mov8 = pd.DataFrame(mov8); mov8.to_csv(OUT / 'run8_moving_cores.csv', index=False)

# ---- boot 1 with IR ----
z = np.load(D / 'tracing_boot1.npz'); h1 = z['hall']; tu = z['t_us'].astype(float)
ir = pd.read_csv(HG / 'ir_coordinate_source.csv'); ir = ir[ir.boot == BOOT1].sort_values('us')
pt = ir[ir.um.diff().fillna(1) != 0].us.values.astype(float)          # odometer pulse instants
k = np.searchsorted(pt, tu); prev_p = np.where(k > 0, pt[np.maximum(k - 1, 0)], -np.inf); next_p = np.where(k < len(pt), pt[np.minimum(k, len(pt) - 1)], np.inf)
moving = (next_p - prev_p) < 1.0e6          # inside an inter-pulse interval shorter than 1 s
still = (next_p - prev_p) > 3.0e6           # inside an inter-pulse interval longer than 3 s (stationary, conservatively)
pwm1 = np.load(D / 'block_pwm_boot1.npy')
ann = pd.read_csv(D / 'annotation_boot1.csv'); ann = ann[ann.source.isin(['OPENING', 'SEQUENCE'])]
idx = lambda u: np.clip(np.searchsorted(tu, u), 0, len(tu) - 1)
mov1 = []
for r in ann.itertuples():
    a_, b_ = idx(r.start_us), idx(r.end_us) + 1
    if b_ - a_ < 5 or not moving[a_:b_].all(): continue       # whole extent moving per IR
    hs = h1[a_:b_]
    if not np.isfinite(hs).all(): continue
    v = 9.652 / ((next_p[a_:b_] - prev_p[a_:b_]).max() / 1e6)  # slowest speed in the passage, mm/s (lower bound)
    row = dict(t_s=round(tu[a_] / 1e6, 2), extent_s=round((b_ - a_) * 0.02, 2), pwm_min=int(pwm1[a_:b_].min()), slowest_mm_s=round(v, 1))
    for t in TAUS:
        kk = int(round(t / 0.02)); rr = ranges(hs, kk); row[f'min_range_{t}'] = round(float(rr.min()), 1) if len(rr) else None
    mov1.append(row)
mov1 = pd.DataFrame(mov1); mov1.to_csv(OUT / 'boot1_moving_extents.csv', index=False)
# boot 1 stationary spans (any level)
segs = []; i = 0
while i < len(still):
    if still[i]:
        j = i
        while j + 1 < len(still) and still[j + 1]: j += 1
        if j - i + 1 >= 50: segs.append((i, j + 1))
        i = j + 1
    else: i += 1
st1 = {}
for t in TAUS:
    kk = int(round(t / 0.02)); rr = np.concatenate([ranges(h1[a_:b_], kk) for a_, b_ in segs] or [np.array([])])
    if len(rr): st1[str(t)] = dict(n=int(len(rr)), median=round(float(np.median(rr)), 1), p99=round(float(np.percentile(rr, 99)), 1), max=round(float(rr.max()), 1))
res['boot1_stationary'] = dict(segments=len(segs), seconds=round(sum(b_ - a_ for a_, b_ in segs) * 0.02, 1), range_by_tau=st1,
                               noise_median=round(float(np.median([noise(h1[a_:b_]) for a_, b_ in segs])), 2))

def summ(df):
    out = dict(n=len(df))
    for t in TAUS:
        c = df[f'min_range_{t}'].dropna()
        if len(c): out[str(t)] = dict(min=float(c.min()), p1=round(float(np.percentile(c, 1)), 1), p5=round(float(np.percentile(c, 5)), 1), median=float(c.median()))
    return out
res['boot1_moving_in_magnet_flattest_span'] = summ(mov1)
res['boot1_slowest_passages'] = mov1.sort_values('slowest_mm_s').head(8).to_dict('records')
res['run8_moving_in_magnet_flattest_span'] = summ(mov8)
res['run8_slowest_cores'] = mov8.sort_values('core_s', ascending=False).head(8).to_dict('records')
json.dump(res, open(OUT / 'stationarity_evidence.json', 'w'), indent=1, default=float)
print(json.dumps(res, indent=1, default=float))
