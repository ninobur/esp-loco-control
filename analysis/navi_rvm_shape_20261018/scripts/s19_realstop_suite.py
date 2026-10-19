"""Stop over a magnet with Otto's MEASURED stop tracks (realstop.py).
Each case: a recorded stretch (s18 targets, 12 CW + 6 CCW), a measured stop track
halting at a chosen point of the target ensemble, a 5 s declared stationary
start. Variants of which blocks are appended to the recognition waveform:
  all      every block (no pause)
  pwm15    the candidate as built: STOPPED = PWM < 15 is paused
  oracle   exactly the blocks in which the IR odometer shows motion (upper bound:
           perfect knowledge of stationarity; NOT implementable without IR)
  + any continuation mechanisms registered in MECHANISMS (added later).
Compared with the uninterrupted stretch: same recognitions (count, polarity,
centre within 2 blocks of the stretch coordinate) and the full MM sequence."""
import numpy as np, pandas as pd, json, sys
from common import *
import rvm_shape as R
import rvm_local as L
import stopgo as G
import realstop as RS
import s10_local_experiment as X

OUT = ROOT / 'results' / 'local_pause'
mj = json.load(open(ROOT / 'results' / 'local' / 'model_A.json'))
def gmm(c):
    g = R.GMM(len(c['w'])); g.w = np.array(c['w']); g.mu = np.array(c['mu']); g.S = np.array(c['S']); return g
Em, Nm = gmm(mj['E']), gmm(mj['N'])
from s11_nav_replay import Navigator
rng = np.random.default_rng(1019)
PRE = 250
NB, NA = 3, 5          # stretch: 6 magnets before the target, 8 after (the measured tracks are long)
truth = X.ens[(X.ens.source == 'OPENING') & ~X.ens.censored].sort_values('s_i').reset_index(drop=True)
pwm_all = np.load(D / 'block_pwm_boot1.npy')


def pick_targets(n, direction):
    out = []; last_t = -1e9
    for k in range(NB, len(truth) - NA - 1):
        if truth.dir[k] != direction: continue
        win = truth.iloc[k - NB:k + NA + 1]
        if (win.dir.nunique() != 1) or (win.fold.nunique() != 1): continue
        step = 1 if direction == 1 else -1
        if any((win.mm.iloc[i + 1] - win.mm.iloc[i]) % 171 != step % 171 for i in range(len(win) - 1)): continue
        a = int((truth.e_i[k - NB] + truth.s_i[k - NB + 1]) / 2); b = int(truth.e_i[k + NA]) + 100
        if not np.isfinite(X.h[a:b]).all() or (pwm_all[a:b] < 25).any(): continue
        if truth.s_i[k] - last_t < 4000: continue
        out.append((k, a, b)); last_t = truth.s_i[k]
        if len(out) == n: break
    return out


def recognize(H, C, keep):
    ids = np.where(keep)[0]
    det = L.run(L.Tracing(H[keep], C[keep]), Em, Nm, [(0, 'decl')])
    if not len(det): return det.assign(c_id=[])
    det['c_blk'] = np.interp(det.c.values, np.arange(len(ids)), ids)
    return det


def navigate(det, last, nxt, d):
    nav = Navigator(); nav.declare(last, nxt, d) if d > 0 else nav.declare(nxt, last, d)
    seq = []
    for r in det.itertuples():                  # rows are in acceptance order (the order the firmware emits them)
        st, mm, _ = nav.on_ensemble(int(r.pol)); seq.append(mm if st == 'MM' else st)
    return seq


MECHANISMS = {}          # name -> f(H, P, keep_pwm15) -> keep


def run_suite(variants, n_profiles=6, tag='realstop'):
    profs = RS.profiles(); step = max(1, len(profs) // n_profiles); profs = profs[::step][:n_profiles]
    print('profiles used', [round(p['stop_s']) for p in profs], flush=True)
    targets = pick_targets(12, 1) + pick_targets(6, 2)
    rows = []
    for k, a, b in targets:
        tk = truth.iloc[k]; d = 1 if tk.dir == 1 else -1
        h0 = X.h[a:b].astype(float); c0 = X.ccw[a:b]
        sig = RS.robust_noise(h0)
        mmpb = RS.stretch_mm_per_block(a, b)
        s, e = int(tk.s_i) - a, int(tk.e_i) - a
        pk = s + int(np.argmax(np.abs(h0[s:e + 1] - np.median(h0[:50]))))
        pos = dict(baseline=int((truth.e_i[k - 1] + truth.s_i[k]) / 2) - a, leading_lobe=s, rising=(s + pk) // 2, peak=pk,
                   falling=(pk + e) // 2, trailing_lobe=e + 2, after_completion=e + 25)
        inside = truth[(truth.s_i >= a) & (truth.e_i < b)]; expect = [int(m) for m in inside.mm]
        last, nxt = (int(truth.mm[k - NB]), int(truth.mm[k - NB + 1]))
        pre_h = h0[0] + RS.stationary_noise(PRE, rng)
        Hr = np.concatenate([pre_h, h0]); Cr = np.concatenate([np.repeat(c0[0], PRE), c0]); Or = np.concatenate([np.full(PRE, -1.0), np.arange(len(h0), dtype=float)])
        ref = recognize(Hr, Cr, np.ones(len(Hr), bool)); ref['orig'] = np.interp(ref.c_blk, np.arange(len(Or)), Or)
        for where, p_at in pos.items():
            for pi, prof in enumerate(profs):
                rp = RS.replay(h0, c0, p_at, prof, sig, rng, mmpb)
                if rp is None: continue
                H, C, P, O, S = rp
                H = np.concatenate([pre_h, H]); C = np.concatenate([np.repeat(c0[0], PRE), C]); P = np.concatenate([np.zeros(PRE, int), P])
                O = np.concatenate([np.full(PRE, -1.0), O]); S = np.concatenate([np.zeros(PRE, bool), S])
                k15 = G.admit_mask(P, [0])
                idx = np.arange(len(H)); paused = (~k15) & (idx >= PRE)
                first_p = idx[paused].min() if paused.any() else len(H)
                masks = dict(all=np.ones(len(H), bool), pwm15=k15, oracle=(~S) | (idx < PRE),
                             oracle_dep=k15 & ~(S & (idx > first_p)),            # perfect removal on the departure side only
                             oracle_arr=k15 & ~(S & (idx < first_p)))            # perfect removal on the arrival side only
                for name, f in MECHANISMS.items(): masks[name] = f(H, C, P, k15)
                # Manual cmd/stop ramps 31 ms/count: from Otto's measured stop PWM (~26) to 15 takes
                # ~0.34 s = 17 blocks. Model it by leaving only the last 17 arrival-side stationary
                # PWM>=15 blocks; the rest would already have been below 15 (paused).
                arr = np.where(S & (idx < first_p) & (P >= 15))[0]
                import continuation as K
                for keep_n in (17, 35, 70, 140):                     # stationary PWM>=15 arrival blocks left
                    fast = k15.copy(); fast[arr[:-keep_n] if len(arr) > keep_n else []] = False
                    masks[f'exit2_arr{keep_n}'] = K._rule(H, fast, 2) & fast
                masks['exit2_fast_stop'] = masks['exit2_arr17']
                masks['firmware'] = G.firmware_mask(H, P, [0])         # RvmPause.h as built (arrival mirror included)
                for vname in variants:
                    det = recognize(H, C, masks[vname])
                    got = np.interp(det.c_blk.values, np.arange(len(O)), O) if len(det) else np.array([])
                    ok = len(got) == len(ref) and all(np.min(np.abs(got - c)) <= 2 and det.pol.values[np.argmin(np.abs(got - c))] == ref.pol.values[i] for i, c in enumerate(ref.orig.values))
                    seq = navigate(det, last, nxt, d)
                    rows.append(dict(target_mm=int(tk.mm), dir=d, type=tk.type, where=where, profile=pi, stop_s=prof['stop_s'], variant=vname,
                                     n_ref=len(ref), n_got=len(got), invariant=bool(ok), mm_ok=bool(seq == expect),
                                     contradiction=bool('CONTRADICTION' in seq), appended_still=int((masks[vname] & S).sum()),
                                     dropped_moving=int((~masks[vname] & ~S & (np.arange(len(H)) >= PRE)).sum())))
        print(int(tk.mm), flush=True)
    r = pd.DataFrame(rows); r.to_csv(OUT / f'{tag}_suite.csv', index=False)
    s = r.groupby(['variant', 'where']).agg(cases=('invariant', 'size'), invariant=('invariant', 'sum'), mm_ok=('mm_ok', 'sum')).reset_index()
    json.dump(s.to_dict('records'), open(OUT / f'{tag}_suite_summary.json', 'w'), indent=1)
    print(s.pivot(index='where', columns='variant', values='invariant').to_string())
    print(s.pivot(index='where', columns='variant', values='mm_ok').to_string())
    return r


if __name__ == '__main__':
    import continuation as K
    MECHANISMS.update(envelope_exit=K.envelope_exit, envelope_exit2=K.envelope_exit2, envelope_both2=K.envelope_both2)
    which = sys.argv[1].split(',') if len(sys.argv) > 1 else ['all', 'pwm15', 'oracle', 'oracle_dep', 'oracle_arr', 'envelope_exit', 'envelope_exit2', 'envelope_both2']
    run_suite(which, tag=sys.argv[2] if len(sys.argv) > 2 else 'realstop')
