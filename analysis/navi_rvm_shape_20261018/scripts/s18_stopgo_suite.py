"""STOP/GO continuity suite (assignment 2026-10-18 section 5).
Real recorded boot-1 stretches (moving throughout, no gaps) are given a declared
stationary starting baseline (5 s at PWM 0) and then commanded stops are
inserted: held Hall value (+ the stretch's own baseline noise) with PWM 0.
Stationary blocks (the declared start and every stop) use Otto's REAL recorded
stationary Hall fluctuation (realstop.stationary_noise).
Every case runs through BOTH the Python reference (stopgo + rvm_local) and the
firmware headers (tests/navi_rvm_shape/blocks_replay.cpp), and is compared with
the uninterrupted run of the same stretch:
  invariant = same number of recognitions, same polarity, same centres (mapped
  back to the recorded stretch, within 1 block) and the same MM sequence.
Ideal stops: the stop is exactly the PWM<25 interval.
Measured-departure / arrival variants use s16's evidence at the current STOPPED
boundary: how long Otto stays still after PWM reaches it (median and max), and
the longest he kept moving after PWM fell below it. These are reported separately; nothing was tuned to them.
Zero-shift variant: the Hall level is offset after a 30 s stop (characterization)."""
import numpy as np, pandas as pd, json, subprocess, tempfile, pathlib
from common import *
import rvm_shape as R
import rvm_local as L
import stopgo as G
import realstop as RS
import s10_local_experiment as X

OUT = ROOT/'results'/'local_pause'; REPO = ROOT.parents[1]
mj = json.load(open(ROOT/'results'/'local'/'model_A.json'))
def gmm(c):
    g = R.GMM(len(c['w'])); g.w = np.array(c['w']); g.mu = np.array(c['mu']); g.S = np.array(c['S']); return g
Em, Nm = gmm(mj['E']), gmm(mj['N'])
pwm_all = np.load(D/'block_pwm_boot1.npy')
tmp = pathlib.Path(tempfile.mkdtemp(prefix='rvm_stopgo_'))
SK = REPO/'firmware'/'test-programs'/'NAVI_RVM_SHAPE_MANUAL'
CPP = tmp/'blocks_replay'
subprocess.run(['c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror', f'-I{SK}', str(REPO/'tests'/'navi_rvm_shape'/'blocks_replay.cpp'), '-o', str(CPP)], check=True)
rng = np.random.default_rng(1018)
PRE = 250                                         # 5 s declared stationary baseline
# Measured on Otto at the current STOPPED boundary (s16 output, same threshold as stopgo.py):
EV = json.load(open(ROOT/'results'/'local'/'stopped_threshold_evidence.json'))
assert EV['threshold'] == G.STOPPED_BELOW_PWM
TAIL_MED = round(EV['stationary_s_with_pwm_ge_th_on_departure']['50%'], 2)   # still stationary at PWM >= boundary
TAIL_MAX = round(EV['stationary_s_with_pwm_ge_th_on_departure']['max'], 2)
ARRIVE_MAX = round(max(EV['still_moving_after_pwm_below_th_s']['max'], 0.02), 2)  # still moving below the boundary

truth = X.ens[(X.ens.source == 'OPENING') & ~X.ens.censored].sort_values('s_i').reset_index(drop=True)

def pick_targets(n=12, direction=1):
    out = []; last_t = -1e9
    for k in range(3, len(truth) - 6):
        if truth.dir[k] != direction: continue
        win = truth.iloc[k - 3:k + 6]
        if (win.dir.nunique() != 1) or (win.fold.nunique() != 1): continue
        d = int(win.dir.iloc[0]); step = 1 if d == 1 else -1
        if any((win.mm.iloc[i + 1] - win.mm.iloc[i]) % 171 != step % 171 for i in range(len(win) - 1)): continue
        a = int((truth.e_i[k - 3] + truth.s_i[k - 2]) / 2); b = int(truth.e_i[k + 5]) + 100
        if not np.isfinite(X.h[a:b]).all() or (pwm_all[a:b] < 25).any(): continue
        if truth.s_i[k] - last_t < 4000: continue                    # spread over the evening
        out.append((k, a, b)); last_t = truth.s_i[k]
        if len(out) == n: break
    return out

def run_py(h, ccw, pwm):
    keep = G.resumption_mask(h, pwm, [0]); C = G.Compressed(h, ccw, keep)
    det = L.run(L.Tracing(C.h, C.ccw), Em, Nm, [(0, 'decl')])
    if not len(det): return pd.DataFrame(columns=['c_id', 'c', 'pol'])
    det['c_id'] = [int(C.ids[min(int(np.floor(c)), len(C.ids) - 1)]) for c in det.c]
    det['c'] = C.to_ids(det.c)
    return det

def run_cpp(h, ccw, pwm, last, nxt, d):
    f = tmp/'blocks.csv'
    with open(f, 'w') as fh:
        for v, c, p in zip(h, ccw, pwm): fh.write('%s,%d,%d\n' % ('nan' if not np.isfinite(v) else '%.6f' % v, 2 if c else 1, p))
    out = subprocess.run([str(CPP), str(f), '0', str(last), str(nxt), str(d)], capture_output=True, text=True, check=True).stdout
    rows = [l.split(',') for l in out.strip().splitlines() if l]
    return pd.DataFrame([dict(c_id=int(r[1]), pol=int(r[5]), result=int(r[7]), mm=int(r[8])) for r in rows], columns=['c_id', 'pol', 'result', 'mm'])

def build(a, b, stops, depart_tail=0.0, arrive=0.0, shift=None):
    """stops: list of (orig_pos, seconds). Returns h, ccw, pwm, orig (recorded-stretch coordinate per block)."""
    h0 = X.h[a:b].astype(float); c0 = X.ccw[a:b]; n0 = len(h0)
    sig = RS.robust_noise(h0)
    H = [h0[0] + RS.stationary_noise(PRE, rng)]; C = [np.repeat(c0[0], PRE)]; P = [np.zeros(PRE, int)]; O = [np.full(PRE, -1.0)]
    pwm0 = np.full(n0, 90); h0 = h0.copy()
    cut = sorted(stops); prev = 0; offset = 0.0
    for pos, sec in cut:
        if arrive:                                                     # still rolling while PWM already < 25
            pwm0[max(prev, pos - int(arrive * 50)):pos] = G.STOPPED_BELOW_PWM - 5
        H.append(h0[prev:pos] + offset); C.append(c0[prev:pos]); P.append(pwm0[prev:pos]); O.append(np.arange(prev, pos, dtype=float))
        n = int(sec * 50); held = h0[pos] + offset
        H.append(held + RS.stationary_noise(n, rng)); C.append(np.repeat(c0[pos], n)); P.append(np.zeros(n, int)); O.append(np.full(n, pos - 0.5))
        if depart_tail:                                                # stationary although PWM >= 25
            m = int(depart_tail * 50)
            H.append(held + RS.stationary_noise(m, rng)); C.append(np.repeat(c0[pos], m)); P.append(np.full(m, G.STOPPED_BELOW_PWM + 5)); O.append(np.full(m, pos - 0.5))
        if shift is not None: offset += shift
        prev = pos
    H.append(h0[prev:] + offset); C.append(c0[prev:]); P.append(pwm0[prev:]); O.append(np.arange(prev, n0, dtype=float))
    return np.concatenate(H), np.concatenate(C), np.concatenate(P), np.concatenate(O)

def compare(ref, got, orig):
    gc = np.interp(got.c.values, np.arange(len(orig)), orig) if len(got) else np.array([])
    rc = ref['c_orig'].values
    matched = sum(1 for i, c in enumerate(rc) if len(gc) and np.min(np.abs(gc - c)) <= 1 and got.pol.values[np.argmin(np.abs(gc - c))] == ref.pol.values[i])
    return dict(n_ref=len(rc), n_got=len(gc), matched=int(matched), invariant=bool(len(gc) == len(rc) and matched == len(rc)),
                max_shift_blocks=float(max((np.min(np.abs(gc - c)) for c in rc), default=0)) if len(gc) else None)

rows = []
targets = pick_targets(12, 1) + pick_targets(6, 2)          # 12 CW, then 6 CCW added for direction coverage
for k, a, b in targets:
    tk = truth.iloc[k]; d = 1 if tk.dir == 1 else -1
    last, nxt = int(truth.mm[k - 3]), int(truth.mm[k - 2])
    s, e = int(tk.s_i) - a, int(tk.e_i) - a
    seg = X.h[a + s:a + e + 1]; pk = s + int(np.argmax(np.abs(seg - np.median(X.h[a:a + 50]))))
    pos = dict(baseline=int((truth.e_i[k - 1] + truth.s_i[k]) / 2) - a, leading_lobe=s, rising=(s + pk) // 2, peak=pk,
               falling=(pk + e) // 2, trailing_lobe=e + 2, after_completion=e + 25)
    inside = truth[(truth.s_i >= a) & (truth.e_i < b)]               # every recorded ensemble in the stretch
    expect_mm = [int(m) for m in inside.mm]
    hh, cc, pp, oo = build(a, b, [])
    ref = run_py(hh, cc, pp); ref['c_orig'] = np.interp(ref.c.values, np.arange(len(oo)), oo)
    refc = run_cpp(hh, cc, pp, last, nxt, d)
    base = dict(target_mm=int(tk.mm), dir=d, type=tk.type)
    rows.append(dict(base, case='uninterrupted', where='-', hold_s=0, n=len(ref), cpp_equal=bool(list(refc.c_id) == list(ref.c_id)),
                     mm_sequence_ok=bool(list(refc[refc.result == 1].mm) == expect_mm[:int((refc.result == 1).sum())] and (refc.result == 1).sum() == len(expect_mm)),
                     invariant=True, matched=len(ref), n_ref=len(ref), n_got=len(ref), max_shift_blocks=0.0))
    def case(name, where, stops, **kw):
        h2, c2, p2, o2 = build(a, b, stops, **kw)
        py = run_py(h2, c2, p2); cp = run_cpp(h2, c2, p2, last, nxt, d)
        r = compare(ref, py, o2)
        mmseq = list(cp[cp.result == 1].mm)
        rows.append(dict(base, case=name, where=where, hold_s=sum(x[1] for x in stops), cpp_equal=bool(list(cp.c_id) == list(py.c_id)),
                         mm_sequence_ok=bool(mmseq == expect_mm and (cp.result == 2).sum() == 0), contradictions=int((cp.result == 2).sum()), **r, **{k_: v for k_, v in kw.items()}))
    for where, p in pos.items():
        for sec in (1, 3, 5, 10, 30, 300):
            case('stop', where, [(p, sec)])
    for where in ('peak', 'baseline'):
        case('long_stop', where, [(pos[where], 1800)])
    for sec in (2, 10):
        case('repeated_stop_go', 'rising+peak+falling', [(pos['rising'], sec), (pos['peak'], sec), (pos['falling'], sec)])
    case('repeated_stop_go', 'five_1s_stops', [(pos['leading_lobe'], 1), (pos['rising'], 1), (pos['peak'], 1), (pos['falling'], 1), (pos['trailing_lobe'], 1)])
    for where in ('rising', 'peak', 'falling', 'baseline'):
        for tail in (TAIL_MED, TAIL_MAX):
            case('measured_departure_tail', where, [(pos[where], 5)], depart_tail=tail)
        case('measured_arrival_creep', where, [(pos[where], 5)], arrive=ARRIVE_MAX)
        case('measured_arrival_and_departure', where, [(pos[where], 5)], depart_tail=TAIL_MED, arrive=ARRIVE_MAX)
    for where in ('peak', 'baseline'):
        for sh in (2.0, 5.0, 10.0):
            case('zero_shift_after_30s', where, [(pos[where], 30)], shift=sh)
    print(int(tk.mm), 'done', flush=True)

r = pd.DataFrame(rows); r.to_csv(OUT/'stopgo_suite.csv', index=False)
g = r.groupby(['case', 'where'] + (['depart_tail'] if 'depart_tail' in r else []), dropna=False)
summ = r.groupby(['case', 'where']).agg(cases=('target_mm', 'size'), invariant=('invariant', 'sum'), mm_sequence_ok=('mm_sequence_ok', 'sum'),
                                        cpp_equal=('cpp_equal', 'sum')).reset_index()
for col in ('depart_tail', 'arrive', 'shift'):
    if col not in r: r[col] = np.nan
detail = r[r.case.str.startswith(('measured', 'zero'))].groupby(['case', 'where', 'depart_tail', 'arrive', 'shift'], dropna=False).agg(
    cases=('target_mm', 'size'), invariant=('invariant', 'sum'), mm_sequence_ok=('mm_sequence_ok', 'sum'), contradictions=('contradictions', 'sum')).reset_index()
json.dump(dict(targets=[int(truth.mm[k]) for k, _, _ in targets], by_case=summ.to_dict('records'), measured_and_shift=detail.to_dict('records'),
               ideal_all_invariant=bool(r[r.case.isin(['uninterrupted', 'stop', 'long_stop', 'repeated_stop_go'])].invariant.all()),
               ideal_all_mm_ok=bool(r[r.case.isin(['uninterrupted', 'stop', 'long_stop', 'repeated_stop_go'])].mm_sequence_ok.all()),
               cpp_python_equal_all=bool(r.cpp_equal.all())),
          open(OUT/'stopgo_suite_summary.json', 'w'), indent=1, default=float)
print(summ.to_string()); print(detail.to_string())
print('ideal invariant all:', r[r.case.isin(['uninterrupted', 'stop', 'long_stop', 'repeated_stop_go'])].invariant.all(), ' cpp==python all:', r.cpp_equal.all())
