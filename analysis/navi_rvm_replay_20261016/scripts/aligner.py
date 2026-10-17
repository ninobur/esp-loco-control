"""NAVI_RVM research aligner: online movement-constrained alignment (constrained DTW / Viterbi with a
single exported path) of 40 ms Hall steps against a distance-indexed reference landscape.

Research code only. Inputs are read-only recordings. NAVI selections are never read here.
Movement context:
  PWM  -> expected speed from the template pass's own PWM history table, lagged (PWM history);
          soft Huber penalty, no hard minimum or maximum (operator answer 6). PWM 0 -> freeze (0106).
  IR   -> smoothed odometer speed; saturating penalty (no veto, operator answer 4).
Gaps    -> no local evidence (cost 0); nothing is synthesized (operator answer 3).
"""
import numpy as np, pandas as pd, json, pathlib
ROOT = pathlib.Path(__file__).resolve().parents[1]; D = ROOT / 'derived'
G = 1.0; DT = 0.04

def huber(r, d=2.0):
    a = np.abs(r); return np.where(a <= d, 0.5 * a * a, d * (a - 0.5 * d))

class Landscape:
    def __init__(self, name):
        self.T = np.load(D / f'template_{name}.npy'); self.NC = len(self.T)
        g = np.gradient(self.T) / G
        base = pd.Series(np.concatenate([self.T[-600:], self.T, self.T[:600]])).rolling(601, center=True).median().values[600:-600]
        # expected between-pass variability: ordinary-track noise + position jitter x slope + amplitude share
        # measured lap-to-lap variability (CW_L1 vs CW_L2): ~0.5 counts on ordinary track, ~2.5 mm position jitter on flanks
        self.sigma = np.sqrt(P['sigma0'] ** 2 + (P['pos_jitter_mm'] * g) ** 2 + (P['amp_frac'] * (self.T - base)) ** 2)
        tab = pd.read_csv(D / f'speed_model_{name}.csv'); tab = tab[tab.n >= 20]
        self.model = {(int(r.ramp), int(r.bin)): (r.med, max((r.q90 - r.q10) / 2.56, 0.0)) for r in tab.itertuples()}
    def v_of_pwm(self, pwm, ramp):
        """Otto's recorded speed (median, spread) at this PWM bin and ramp state; nearest recorded bin otherwise."""
        if pwm <= 0: return 0.0, 0.0
        b = int(pwm // 5 * 5)
        for r in (ramp, 0, 1, -1):
            keys = [k for k in self.model if k[0] == r]
            if keys:
                kb = min(keys, key=lambda k: abs(k[1] - b))
                if abs(kb[1] - b) <= 10: return self.model[kb]
        return self.model[min(self.model, key=lambda k: abs(k[1] - b))]

P = dict(sigma0=1.5, pos_jitter_mm=2.5, amp_frac=0.02, init_margin_mm=50.0, pwm_cap=4.0, tau_pwm_s=0.6, sv_frac=0.05, sv_abs=8.0,
         si_frac=0.08, si_abs=12.0, ir_cap=4.0, w_pwm=1.0, w_ir=1.0, w_hall=1.0, kmax=30, beam=1e9, W=2400, hub_hall=3.0)

def run(boot, cw_name, ccw_name, declarations='all', mask=None, params=None, label='run', steps_file=None):
    """declarations: 'all' re-initialise at every declaration (operator authority);
       'first' only the first declaration of the boot (continuity test)."""
    if params: P.update(params)
    L = {'CW': Landscape(cw_name), 'CCW': Landscape(ccw_name)}; NC = L['CW'].NC
    z = np.load(D / (steps_file or f'steps_{boot}.npz')); t = z['t_us'] / 1e6; h = z['hall'].copy(); odo = z['odo_mm']
    # PWM is the commanded state; a recording gap (-1) is not PWM 0. Carry the last recorded command; Hall stays empty.
    pwm = pd.Series(np.where(z['pwm'] < 0, np.nan, z['pwm'])).ffill().fillna(0).values
    if mask is not None: h[mask] = np.nan
    import speed_model
    vir = speed_model.vir_of(t, odo); ramp = speed_model.ramp_state(pwm)
    mp = pd.read_csv(D / 'map_positions.csv').set_index('mm').map_mm
    dec = pd.read_csv(D / 'declarations.csv'); dec = dec[dec.boot == boot].sort_values('t_us')
    events = [(r.t_us / 1e6, r.topic.split('/')[-1], str(r.payload)) for r in dec.itertuples()]
    first_start = next(e for e in events if e[1] == 'start_interval')
    direction = 'CW'; sgn = 1
    n = len(t); out = {k: np.full(n, np.nan) for k in ['x', 'cost_local', 'beam_mm', 'alt_margin', 've', 'vir', 'dx']}
    seg = np.full(n, -1); bp = []; los = []; step_of = np.full(n, -1); best_abs = np.full(n, -10**9); sgn_arr = np.zeros(n, int)
    C = None; lo = 0; ve = 0.0; seg_id = -1; ei = 0; frozen = True
    k = np.arange(P['kmax'] + 1)
    for i in range(n):
        while ei < len(events) and events[ei][0] <= t[i]:
            et, kind, pl = events[ei]; ei += 1
            if kind == 'session_direction':
                direction = pl; sgn = 1 if pl == 'CW' else -1
                if C is not None: lo = -lo - (P['W'] - 1); C = C[::-1].copy()  # same map position, reversed progress
            elif kind == 'direction':
                pass  # manual reversal after the run; left as recorded, scored separately
            elif kind == 'start_interval' and (declarations == 'all' or (et, kind, pl) == first_start):
                a, b = [int(v) for v in pl.split('-')]
                xa, xb = mp[a], mp[b]
                if abs(xb - xa) > NC * G / 2: xb += NC * G if xb < xa else -NC * G
                pa, pb = sorted([sgn * xa, sgn * xb]); pa -= (pb - pa); pb += P['init_margin_mm']  # sensor may not yet be past the first declared magnet
                lo = int(np.floor(pa / G)) - P['W'] // 4; C = np.full(P['W'], np.inf)
                cells = (lo + np.arange(P['W'])) * G; C[(cells >= pa) & (cells <= pb)] = 0.0
                seg_id += 1; frozen = False
        if C is None: continue
        Ld = L[direction]
        cells = lo + np.arange(P['W']); tidx = (sgn * cells) % NC
        ve, vspread = Ld.v_of_pwm(pwm[i], ramp[i])
        if pwm[i] <= 0:  # 0106: no navigation authority; hold alignment, record only
            bp.append(np.zeros(P['W'], np.int8)); los.append(lo); seg[i] = seg_id; step_of[i] = len(bp) - 1
            best = int(np.argmin(C)); best_abs[i] = lo + best; sgn_arr[i] = sgn; out['x'][i] = sgn * (lo + best) * G % (NC * G); out['ve'][i] = ve; out['vir'][i] = vir[i]; out['dx'][i] = 0
            continue
        vk = k * G / DT
        sv = vspread + P['sv_frac'] * ve + P['sv_abs']; mot = P['w_pwm'] * np.minimum(P['pwm_cap'], huber((vk - ve) / sv))
        vi = max(vir[i], 0.0); si = P['si_frac'] * vi + P['si_abs']
        irp = P['w_ir'] * np.minimum(P['ir_cap'], huber((vk - vi) / si)) if np.isfinite(vir[i]) else 0 * vk
        trans = mot + irp
        cand = np.full((len(k), P['W']), np.inf)
        for j in k: cand[j, j:] = C[:P['W'] - j] + trans[j]
        arg = np.argmin(cand, 0); newC = cand[arg, np.arange(P['W'])]
        if np.isfinite(h[i]):
            r = (h[i] - Ld.T[tidx]) / Ld.sigma[tidx]; loc = P['w_hall'] * huber(r, P['hub_hall'])
        else: loc = np.zeros(P['W'])
        newC = newC + loc
        m = np.min(newC); newC -= m; newC[newC > P['beam']] = np.inf
        best = int(np.argmin(newC))
        fin = np.where(np.isfinite(newC))[0]
        out['beam_mm'][i] = (fin[-1] - fin[0]) * G if len(fin) else np.nan
        far = np.abs(np.arange(P['W']) - best) * G > 150
        out['alt_margin'][i] = np.min(newC[far]) if far.any() else np.inf
        out['cost_local'][i] = loc[best]; out['ve'][i] = ve; out['vir'][i] = vir[i]; out['dx'][i] = arg[best] * G
        bp.append(arg.astype(np.int8)); los.append(lo); seg[i] = seg_id; step_of[i] = len(bp) - 1
        C = newC
        if best < P['W'] // 6 or best > P['W'] * 2 // 3:  # recentre window on the incumbent
            sh = best - P['W'] // 4; C2 = np.full(P['W'], np.inf)
            if sh > 0: C2[:P['W'] - sh] = C[sh:]
            else: C2[-sh:] = C[:P['W'] + sh]
            C = C2; lo += sh
        out['x'][i] = sgn * (lo + int(np.argmin(C))) * G % (NC * G)
        best_abs[i] = lo + int(np.argmin(C)); sgn_arr[i] = sgn
    out['seg'] = seg; out['t'] = t; out['h'] = h; out['pwm'] = pwm
    out['best_abs'] = best_abs; out['sgn'] = sgn_arr
    np.savez_compressed(D / f'align_{label}.npz', **out)
    def retro(i, lag_steps):
        """Fixed-lag retrospective position at step i: backtrack from the incumbent at i+lag (same segment)."""
        j = i + lag_steps
        while j > i and (j >= n or seg[j] != seg[i] or step_of[j] < 0 or sgn_arr[j] != sgn_arr[i]): j -= 1
        a = best_abs[j]
        while j > i:
            s_ = step_of[j]; c = a - los[s_]
            a = a - int(bp[s_][c]) if 0 <= c < P['W'] else a
            j -= 1
            while j > i and step_of[j] < 0: j -= 1
        return sgn_arr[i] * a * G % (NC * G)
    out['retro'] = retro
    return out

if __name__ == '__main__':
    import sys, time
    t0 = time.time(); o = run('15549e9385faa4d5', 'CW_L1', 'CCW_L1', declarations=sys.argv[1] if len(sys.argv) > 1 else 'all', label='smoke')
    print('done', time.time() - t0, 's')
