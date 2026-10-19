"""Operating events in each recording (descriptive, for the report): PWM<15 stops (with the Hall level relative
to the baseline: on baseline or over a magnet), stalls (IR stationary while applied PWM >= 15), and throttle
reductions reversed before reaching zero (commanded < 15 withdrawn while applied PWM stayed >= 15).
  u05_events.py <boots_dir> <decl_events.tsv> <out.json> <dataset> ..."""
import sys, json, pathlib, numpy as np
HERE = pathlib.Path(__file__).resolve().parent; sys.path.insert(0, str(HERE)); sys.argv_saved = list(sys.argv)
BD, DEV, OUTF = pathlib.Path(sys.argv[1]), sys.argv[2], sys.argv[3]; names = sys.argv[4:]
sys.argv = [sys.argv[0], str(BD), DEV, '/tmp/u05_unused']   # reuse the loaders
import importlib.util
spec = importlib.util.spec_from_file_location('u04', HERE / 'u04_replay.py')
src = open(HERE / 'u04_replay.py').read(); src = src[:src.index('results = []')]
g = {'__file__': str(HERE / 'u04_replay.py')}; exec(compile(src, 'u04_replay.py', 'exec'), g)
def runs(mask):
    out = []; i = 0; n = len(mask)
    while i < n:
        if mask[i]:
            j = i
            while j + 1 < n and mask[j + 1]: j += 1
            out.append((i, j)); i = j + 1
        else: i += 1
    return out
res = {}
for nm in names:
    ds = g['load_boot1']() if nm == 'BOOT1' else g['load_nsr'](nm.split(':')[0])
    h, pwm, cmd, ir, t = ds['h'], np.asarray(ds['pwm']), np.asarray(ds['cmd']), ds['ir'], ds['t_us'] / 1e6
    fd = min(i for i, _, _ in ds['decl']) if ds['decl'] else len(h)
    base = np.nanmedian(h[fd:][pwm[fd:] >= 15]) if (pwm[fd:] >= 15).any() else np.nan
    stops = [(a, b) for a, b in runs(pwm < 15) if a >= fd and b - a >= 50]
    st = [dict(t=round(t[a], 1), s=round((b - a + 1) * 0.02, 1), offset=round(float(np.nanmedian(h[a:b + 1]) - base), 1)) for a, b in stops]
    stalls = [(a, b) for a, b in runs((ir == 0) & (pwm >= 15)) if a >= fd and b - a >= 50]
    sl = [dict(t=round(t[a], 1), s=round((b - a + 1) * 0.02, 1), offset=round(float(np.nanmedian(h[a:b + 1]) - base), 1), pwm=[int(pwm[a:b + 1].min()), int(pwm[a:b + 1].max())]) for a, b in stalls]
    rev = []
    if (cmd >= 0).all():
        for a, b in runs((cmd < 15) & (pwm >= 15)):
            if a >= fd and b + 1 < len(h) and cmd[b + 1] >= 15 and pwm[b + 1] >= 15: rev.append(dict(t=round(t[a], 1), s=round((b - a + 1) * 0.02, 1)))
    res[nm] = dict(baseline=round(float(base), 1), stops_pwm_lt15=st, stops_over_magnet=sum(abs(x['offset']) > 15 for x in st),
                   stalls_ir_stationary_pwm_ge15=sl, throttle_reductions_reversed=rev)
json.dump(res, open(OUTF, 'w'), indent=1); print(json.dumps({k: dict(stops=len(v['stops_pwm_lt15']), over_magnet=v['stops_over_magnet'], stalls=len(v['stalls_ir_stationary_pwm_ge15']), reversed=len(v['throttle_reductions_reversed'])) for k, v in res.items()}, indent=1))
