"""Feeding load of each mechanism (no recognizer): waiting-block backlog and how long it takes to drain.
F releases held blocks in bursts (hold-back after a commanded stop, up to HOLD_MAX 512); U passes every
block through a fixed delay line of k-1 blocks and never accumulates a backlog.
  u07_load.py <boots_dir> <decl_events.tsv> <out.json> <dataset> ..."""
import sys, json, pathlib, numpy as np
HERE = pathlib.Path(__file__).resolve().parent; sys.path.insert(0, str(HERE))
BD, DEV, OUTF = pathlib.Path(sys.argv[1]), sys.argv[2], sys.argv[3]; names = sys.argv[4:]
sys.argv = [sys.argv[0], str(BD), DEV, '/tmp/u07_unused']
src = open(HERE / 'u04_replay.py').read(); src = src[:src.index('results = []')]
g = {'__file__': str(HERE / 'u04_replay.py')}; exec(compile(src, 'u04_replay.py', 'exec'), g); M = g['M']
res = {}
for nm in names:
    ds = g['load_boot1']() if nm == 'BOOT1' else g['load_nsr'](nm.split(':')[0])
    dec = [i for i, _, _ in ds['decl']]
    keep, _, rel = M.f_current(ds['h'], ds['pwm'], ds['cmd'], dec)
    out = {}
    for pb in (1, 2):
        feed = M._drain(keep, rel, pb); n = len(keep)
        r = np.where(keep & np.isfinite(rel), rel, np.nan); f = feed
        # backlog(i) = released by i, not fed by i
        ev = np.zeros(n + 2)
        ok = np.isfinite(r) & np.isfinite(f)
        np.add.at(ev, np.clip(r[ok].astype(int), 0, n), 1); np.add.at(ev, np.clip(f[ok].astype(int), 0, n + 1), -1)
        bl = np.cumsum(ev)[:n]
        held_never = int((keep & ~np.isfinite(rel)).sum())
        delay = (f - np.arange(n))[ok]
        out[f'per_block_{pb}'] = dict(backlog_max=int(bl.max()), delay_max_s=round(float(delay.max()) * 0.02, 1), delay_p99_s=round(float(np.percentile(delay, 99)) * 0.02, 2))
    out['kept_never_fed_blocks'] = held_never
    res[nm] = out
json.dump(res, open(OUTF, 'w'), indent=1); print(json.dumps(res, indent=1))
