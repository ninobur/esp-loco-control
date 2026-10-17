"""Frozen-parameter replay experiments. Scoring uses NAVI selections only after alignment."""
import sys, pathlib, json, copy, time, numpy as np, pandas as pd
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent)); import aligner, score
D = aligner.D; FR = json.load(open(D / 'FROZEN.json')); B1, B2 = '15549e9385faa4d5', '4d8083de79a0e299'
meta = score.meta; LAP = score.LAP; MANUAL_REVERSAL_S = 3778.736729  # cmd/direction 0: not modelled, excluded
R = {}

def run(boot, label, declarations='all', params=None, mask=None, odo_scale=None, cw='CW_L1', ccw='CCW_L1'):
    aligner.P.clear(); aligner.P.update(copy.deepcopy(FR['params']))
    if params: aligner.P.update(params)
    if odo_scale is not None:  # synthetic IR stress: systematic undermeasurement of the odometer (Hall untouched)
        z = dict(np.load(D / f'steps_{boot}.npz')); z['odo_mm'] = z['odo_mm'] * odo_scale
        np.savez_compressed(D / f'steps_{boot}_irx.npz', **z); sf = f'steps_{boot}_irx.npz'
    else: sf = None
    t0 = time.time(); o = aligner.run(boot, cw, ccw, steps_file=sf, declarations=declarations, mask=mask, label=label)
    o['_runtime_s'] = time.time() - t0
    return o

def scored(o, boot, cw='CW_L1'):
    df = score.score(o, boot, cw, 'CCW_L1')
    df = df[df.t < MANUAL_REVERSAL_S] if boot == B1 else df
    ins = [(meta[cw]['start_s'], meta[cw]['end_s']), (meta['CCW_L1']['start_s'], meta['CCW_L1']['end_s'])] if boot == B1 else []
    df['in_sample'] = False
    for a, b in ins: df.loc[(df.t >= a) & (df.t < b), 'in_sample'] = True
    return df

def summ(df):
    out = {}
    for name, d in [('out_of_sample', df[~df.in_sample]), ('in_sample', df[df.in_sample])]:
        if len(d): out[name] = score.summary(d)
    return out

def teleports(o, thr_mm=60):
    """Online incumbent changes larger than physically travelled in one step (alternative adopted by argmin)."""
    x = o['x']; s = o['seg']; ok = np.isfinite(x[1:]) & np.isfinite(x[:-1]) & (s[1:] == s[:-1]) & (s[1:] >= 0)
    d = np.abs(score.wrap(np.diff(x)))[ok]
    return dict(steps=int(ok.sum()), jumps_gt_60mm=int((d > thr_mm).sum()), jumps_gt_150mm=int((d > 150).sum()), max_jump_mm=float(d.max()))

if __name__ == '__main__':
    # ---- E1 / E8: cross-pass recognition, both templates; boot 2 holdout -----------------------------------
    for cw in ['CW_L1', 'CW_L2']:
        o = run(B1, f'x_e1_{cw}', cw=cw); df = scored(o, B1, cw); df.to_csv(D / f'xs_e1_{cw}.csv', index=False)
        R[f'E1_{cw}'] = dict(summary=summ(df), teleports=teleports(o), runtime_s=o['_runtime_s'])
        if cw == 'CW_L1': base_o, base_df = o, df
    o = run(B2, 'x_e8_boot2'); df = scored(o, B2); df.to_csv(D / 'xs_e8_boot2.csv', index=False)
    R['E8_boot2_holdout'] = dict(summary=summ(df), teleports=teleports(o))

    # ---- E2: strata (from the frozen base run) ------------------------------------------------------------
    bd = base_df[~base_df.in_sample].copy()
    bd['stratum'] = np.select([bd.pwm >= 105, bd.pwm >= 85, bd.pwm >= 55, bd.pwm >= 1], ['PWM>=105', 'PWM 85-104', 'PWM 55-84', 'PWM<55'], 'other')
    R['E2_strata'] = {k: score.summary(v) for k, v in bd.groupby('stratum')}
    R['E2_direction'] = {k: score.summary(v) for k, v in bd.groupby('dir')}

    # ---- E3: movement-context ablations ------------------------------------------------------------------
    for lab, p in [('no_PWM', dict(w_pwm=0.0)), ('no_IR', dict(w_ir=0.0)), ('neither', dict(w_pwm=0.0, w_ir=0.0)),
                   ('PWM_uncapped', dict(pwm_cap=1e9)), ('no_Hall', dict(w_hall=0.0))]:
        o = run(B1, f'x_e3_{lab}', params=p); df = scored(o, B1)
        R[f'E3_{lab}'] = dict(summary=summ(df), teleports=teleports(o))

    # ---- E1c: continuity without later declarations ---------------------------------------------------------
    o = run(B1, 'x_e1c_first', declarations='first'); df = scored(o, B1); df.to_csv(D / 'xs_e1c_first.csv', index=False)
    dec = pd.read_csv(D / 'declarations.csv'); dec = dec[(dec.boot == B1) & dec.topic.str.contains('start_interval')]
    mp = pd.read_csv(D / 'map_positions.csv').set_index('mm').map_mm
    chk = []
    for r in dec.itertuples():
        i = int(np.searchsorted(o['t'], r.t_us / 1e6)) - 1
        a, b = [int(v) for v in r.payload.split('-')]
        xa, xb = mp[a], mp[b]; xo = o['x'][i]
        lo_, hi_ = min(xa, xb), max(xa, xb)
        inside = (lo_ - 330 <= xo <= hi_ + 50) if np.isfinite(xo) else None
        chk.append(dict(t=r.t_us / 1e6, declared=r.payload, aligner_x=None if not np.isfinite(xo) else round(float(xo)),
                        declared_span=[lo_, hi_], offset_to_declared_mid=None if not np.isfinite(xo) else round(float(score.wrap(xo - (lo_ + hi_) / 2)))))
    R['E1c_first_declaration_only'] = dict(summary=summ(df), teleports=teleports(o), at_declarations=chk)
    R['E1c_segments'] = df.groupby('seg').apply(lambda d: dict(n=len(d), t0=round(d.t.min()), t1=round(d.t.max()),
                           wrong=int((d.err_retro.abs() > 150).sum()), median_err=round(float(d.err_retro.median()), 1))).to_dict()

    # ---- E4: IR stress (systematic undermeasurement; synthetic, Hall untouched) ----------------------------
    for sc in [0.95, 0.90, 0.80]:
        o = run(B1, f'x_e4_ir{sc}', odo_scale=sc); df = scored(o, B1)
        R[f'E4_IR_scaled_{sc}'] = dict(summary=summ(df), teleports=teleports(o))
    # real local IR shortfall edges, from the base run
    loc = base_df[base_df.mm.isin([19, 20, 121, 122]) & ~base_df.in_sample]
    R['E4_real_IR_shortfall_edges'] = loc[['t', 'mm', 'dir', 'err_online', 'err_retro']].round(1).to_dict('records')

    # ---- E7: Hall gaps over landmarks (no synthesis: samples removed, nothing inserted) ------------------------
    sel = pd.read_csv(D / 'selections_scoring_only.csv'); sel = sel[sel.boot == B1]
    z = np.load(D / f'steps_{B1}.npz'); t = z['t_us'] / 1e6
    for every in [4, 2]:
        m = np.zeros(len(t), bool); masked = []
        for j, r in enumerate(sel.sort_values('open_us').itertuples()):
            if j % every: continue
            a = r.open_us / 1e6 - 0.3; b = r.open_us / 1e6 + 1.2  # covers the whole ensemble at any recorded speed
            m |= (t >= a) & (t < b); masked.append(r.target)
        o = run(B1, f'x_e7_mask{every}', mask=m); df = scored(o, B1)
        df['masked'] = df.t.round(3).isin((sel.sort_values('open_us').open_us.iloc[::every] / 1e6).round(3))
        R[f'E7_mask_every_{every}'] = dict(masked_landmarks=len(masked), masked_fraction_of_steps=round(float(m.mean()), 3),
            summary=summ(df), on_masked=score.summary(df[df.masked]), on_unmasked=score.summary(df[~df.masked]), teleports=teleports(o))

    json.dump(R, open(D / 'experiments.json', 'w'), indent=1, default=str)
    print(json.dumps(R, indent=1, default=str))
