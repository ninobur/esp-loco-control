"""Firmware verification of NAVI_RVM_CONTINUITY_MANUAL on the recordings.
  u10_firmware_parity.py <out_dir> <dataset_spec> ...
dataset_spec = NAME|native.bin|boots_dir|decl_events.tsv[|CW]
  NAME BOOT1 uses the local boot-1 data; RUN9 is the stitched run-9 tracing (its native file is built here).
For each recording:
  * C++: tests/navi_rvm_continuity/replay.cpp (exactly the firmware headers) on the native Hall stream;
  * Python: u03_mech.u_unified (K=50, band 24, m=16) + rvm_local (the evaluated reference);
  * parity: withheld blocks, recognitions (block id + polarity + LLR), navigator events;
  * outcome (from the C++ firmware path): recognitions, MM claims, contradictions, route misses and extras
    (u04 alignment and operator truth ends), lowest LLR, worst recognizer step on the host.
Analysis only; nothing touches Otto."""
import sys, json, subprocess, tempfile, pathlib, numpy as np, pandas as pd
HERE = pathlib.Path(__file__).resolve().parent; REPO = HERE.parents[2]
SK = REPO / 'firmware/test-programs/NAVI_RVM_CONTINUITY_MANUAL'
OUT = pathlib.Path(sys.argv[1]); OUT.mkdir(parents=True, exist_ok=True)
tmp = pathlib.Path(tempfile.mkdtemp(prefix='cont_parity_'))
subprocess.run(['c++', '-std=c++17', '-O2', '-Wall', '-Wextra', '-Werror', f'-I{SK}', str(REPO / 'tests/navi_rvm_continuity/replay.cpp'), '-o', str(tmp / 'replay')], check=True)
NATIVE = np.dtype([('serial', '<u4'), ('us', '<u8'), ('adc', '<i2'), ('pwm', 'u1'), ('direction', 'u1')])
def fw_tracing(path):
    """The RvmTracing.h rule on a native file: 20 ms blocks from the first read; NaN if < 50 reads or a serial jump touches it."""
    a = np.fromfile(path, dtype=NATIVE); us = a['us'].astype(np.int64); ser = a['serial'].astype(np.int64)
    b = us // 20000 - us[0] // 20000; nb = int(b.max()) + 1
    n = np.bincount(b, minlength=nb); s = np.bincount(b, a['adc'].astype(float), nb)
    h = np.where(n > 0, s / np.maximum(n, 1), np.nan)
    j = np.where(np.diff(ser) != 1)[0]; bad = np.zeros(nb, bool); bad[b[j]] = True; bad[b[j + 1]] = True
    h[(n < 50) | bad] = np.nan; return h
summary = []
for spec in sys.argv[2:]:
    parts = spec.split('|'); name, native, bd, dev = parts[:4]; force_cw = len(parts) > 4 and parts[4] == 'CW'
    sys.argv = [sys.argv[0], bd, dev, str(tmp / 'u04_unused')]
    src = open(HERE / 'u04_replay.py').read(); src = src[:src.index('results = []')]
    g = {'__file__': str(HERE / 'u04_replay.py')}; exec(compile(src, 'u04_replay.py', 'exec'), g); M = g['M']
    ds = g['load_boot1']() if name == 'BOOT1' else g['load_nsr'](name, force_cw)
    nat = native
    if name == 'RUN9':                               # joined boots: the Python reference re-blocks the joined reads as the firmware does
        hh = fw_tracing(native); ds['h'] = np.r_[hh, np.full(max(0, len(ds['h']) - len(hh)), np.nan)][:len(ds['h'])]
    if force_cw:                                     # counterfactual CW declaration: the direction byte must say CW too
        a = np.fromfile(native, dtype=NATIVE); a['direction'] = np.where(a['direction'] == 2, 1, a['direction']); nat = str(tmp / f'{name}_cw.bin'); a.tofile(nat)
    dec = sorted(ds['decl']); di = [i for i, _, _ in dec]
    (tmp / 'decl.csv').write_text(''.join(f'{i},{l},{x},{d}\n' for i, (l, x), d in dec))
    od = tmp / (name + ('_CW' if force_cw else '')); od.mkdir()
    r = subprocess.run([str(tmp / 'replay'), nat, str(tmp / 'decl.csv'), str(od)], capture_output=True, text=True, check=True)
    st = json.loads(r.stdout.strip().splitlines()[-1])
    assert st['first_block_us'] == int(ds['t_us'][0]) - 10000, (st['first_block_us'], ds['t_us'][0])
    ce = pd.read_csv(od / 'ensembles.csv'); cn = pd.read_csv(od / 'nav.csv')
    cw = np.fromfile(od / 'withheld.bin', dtype=np.uint8).astype(bool)
    keep, feed = M.u_unified(ds['h'], di, 50, 24.0, 16.0, True)
    pw = ~keep; n = min(len(pw), len(cw))
    py = g['recognize'](ds, keep)
    same_n = len(py) == len(ce)
    par = dict(blocks=int(len(pw)), cpp_blocks=int(len(cw)), withheld_python=int(pw.sum()), withheld_cpp=int(cw.sum()),
               withheld_blocks_differing=int((pw[:n] != cw[:n]).sum()) + abs(len(pw) - len(cw)),
               recognitions_python=len(py), recognitions_cpp=len(ce),
               same_block_id=int((np.floor(py.c_id).astype(int).values == ce.c_id.values).sum()) if same_n else None,
               same_polarity=int((py.pol.values == ce.pol.values).sum()) if same_n else None,
               max_llr_diff=float(np.abs(py.llr.values - ce.llr.values).max()) if same_n and len(ce) else None)
    # navigator events, Python vs C++
    pn = g['navigate'](ds, py) if len(py) else []
    cev = cn[cn.event != 'DECLARE']
    par['nav_events_equal'] = bool(len(pn) == len(cev) and all(e == c for (_, e, _), c in zip(pn, cev.event)) and
                                   all((mm if mm is not None else -1) == int(c) for (_, e, mm), c in zip(pn, cev.mm) if e == 'MM'))
    # outcome from the C++ firmware path
    dsc = dict(ds); det = ce.copy(); det['decided_id'] = det['decided_id'].astype(float)
    nav = g['navigate'](ds, det.assign(c_id=det.c_id.astype(float), e_id=det.e_id.astype(float)))
    res = g['score'](ds, 'FIRMWARE', ~np.r_[cw, np.zeros(max(0, len(ds['h']) - len(cw)), bool)][:len(ds['h'])],
                     np.where(np.r_[~cw, np.ones(max(0, len(ds['h']) - len(cw)), bool)][:len(ds['h'])], np.arange(len(ds['h'])) + 49, np.nan).astype(float),
                     det.assign(c_id=det.c_id.astype(float), e_id=det.e_id.astype(float)), nav, 0.0)
    row = dict(dataset=name + (':CW' if force_cw else ''), parity=par, firmware=res, host_runtime=st)
    summary.append(row); print(json.dumps(row, default=str), flush=True)
    ce.to_csv(OUT / f"fw_ensembles_{row['dataset'].replace(':', '_')}.csv", index=False); cn.to_csv(OUT / f"fw_nav_{row['dataset'].replace(':', '_')}.csv", index=False)
json.dump(summary, open(OUT / 'firmware_parity.json', 'w'), indent=1, default=str)
