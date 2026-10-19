"""Per-stall view for one recording: for each IR-stationary stretch with applied PWM >= 15 (a stall or
powered standstill), the share of its blocks each mechanism withholds, with its level offset.
  u09_per_stall.py <boots_dir> <events.json> <dataset> <decl_block> <out.csv>"""
import sys, json, pathlib, numpy as np, pandas as pd
HERE = pathlib.Path(__file__).resolve().parent; sys.path.insert(0, str(HERE)); sys.path.insert(0, str(HERE.parent / 'scripts'))
import u03_mech as M
BD, EV, NAME, DECL, OUT = pathlib.Path(sys.argv[1]), sys.argv[2], sys.argv[3], int(sys.argv[4]), sys.argv[5]
z = np.load(BD / f'boot_{NAME}.npz'); h = z['hall']; t = z['t_us'] / 1e6; pwm = z['pwm']; cmd = z['cmd']
mechs = {'F': M.f_current(h, pwm, cmd, [DECL])[0]}
for k, b, m in ((50, 24, 16), (50, 24, 32), (50, 24, 8), (100, 24, 16), (25, 24, 16)):
    mechs[f'U1_k{k}_b{b}_m{m}'] = M.u_unified(h, [DECL], k, b, m, True)[0]
lvl, noi = M.baseline_track(h, [DECL], 16)
rows = []
for s in json.load(open(EV))[NAME]['stalls_ir_stationary_pwm_ge15']:
    a = int(np.searchsorted(t, s['t'])); b = a + int(round(s['s'] / 0.02))
    r = dict(t=s['t'], seconds=s['s'], offset_vs_run_baseline=s['offset'], offset_vs_tracker=round(float(np.nanmedian(h[a:b]) - np.nanmedian(lvl[a:b])), 1),
             noise=round(float(np.nanmedian(noi[a:b])), 2), pwm=f"{s['pwm'][0]}-{s['pwm'][1]}")
    for nm, keep in mechs.items(): r[nm + '_withheld_%'] = round(100 * float((~keep[a:b]).mean()), 0)
    rows.append(r)
df = pd.DataFrame(rows); df.to_csv(OUT, index=False); pd.set_option('display.width', 250); print(df.to_string(index=False))
