"""Causal comparison of NAVI revision 5.1 (frozen, commit 5b7a518) and the 2026-10-16 whole-tracing
aligner (frozen, sha256 f3d5230e...), unchanged, on the same recordings, declarations and scoring.

  evaluate.py prepare <collection.json> <work>    tracings, 40 ms steps, declarations per dataset
  evaluate.py r51     <collection.json> <work>    revision 5.1 C++ firmware path (replay_open.cpp @ 5b7a518)
  evaluate.py align   <collection.json> <work> <dataset>   the aligner (frozen file, run as written)
  evaluate.py score   <collection.json> <work>    shared, model-independent scoring

Nothing here changes either model. Analysis only; no locomotive is touched."""
import sys, json, pathlib, subprocess, datetime, numpy as np, pandas as pd

NATIVE = np.dtype([('serial', '<u4'), ('us', '<u8'), ('adc', '<i2'), ('pwm', 'u1'), ('direction', 'u1')])
HERE = pathlib.Path(__file__).resolve().parent; REPO = HERE.parents[1]
ROUTE = np.loadtxt(REPO / 'analysis/navi_rvm_shape_20261018/scripts/route_polarity.txt', dtype=int)   # 1 = N
NMM = len(ROUTE)
PKG = pathlib.Path.home() / 'NGR/NGR-Files/navi-rvm-replay-20261016'            # 10-16 templates, speed models, map

# ----------------------------------------------------------------------------- inputs
def tracing20(native):
    """RvmTracing.h rule: 20 ms means of native reads; NaN if < 50 reads or a serial jump touches the block."""
    a = np.fromfile(native, dtype=NATIVE); us = a['us'].astype(np.int64); ser = a['serial'].astype(np.int64)
    fb = us[0] // 20000 * 20000; b = (us - fb) // 20000; nb = int(b[-1]) + 1
    n = np.bincount(b, minlength=nb); s = np.bincount(b, a['adc'].astype(float), nb)
    h = np.where(n > 0, s / np.maximum(n, 1), np.nan)
    j = np.where(np.diff(ser) != 1)[0]; bad = np.zeros(nb, bool); bad[b[j]] = True; bad[b[j + 1]] = True
    h[(n < 50) | bad] = np.nan
    pwm = np.zeros(nb, np.int16); np.maximum.at(pwm, b, a['pwm'].astype(np.int16))
    return h, fb + np.arange(nb) * 20000 + 10000, pwm, int(fb)

def steps40(native):
    """The 10-16 package's blocks.py + context.py rule (10 ms blocks, 40 ms steps); no IR (odo NaN)."""
    a = np.fromfile(native, dtype=NATIVE); us = a['us'].astype(np.int64); adc = a['adc'].astype(float)
    t0 = us[0] // 10000 * 10000; b = (us - t0) // 10000; nb = int(b[-1]) + 1
    n = np.bincount(b, minlength=nb); s = np.bincount(b, adc, nb); mean = np.where(n > 0, s / np.maximum(n, 1), np.nan)
    pwm = np.full(nb, -1, np.int16); pwm[b] = a['pwm']; dr = np.full(nb, -1, np.int8); dr[b] = a['direction']
    ser = a['serial'].astype(np.int64); jump = np.where(np.diff(ser) != 1)[0]
    gap = np.zeros(nb, bool); gap[b[jump]] = True; gap[b[jump + 1]] = True; gap |= n == 0
    tb = t0 + np.arange(nb) * 10000 + 5000
    STEP = 4; m = nb // STEP * STEP; sh = lambda x: x[:m].reshape(-1, STEP)
    cnt = sh(n); mm = sh(mean); ok = (cnt > 0) & ~sh(gap)
    w = np.where(ok, cnt, 0); feat = np.where(w.sum(1) > 0, np.nansum(np.where(ok, mm, 0) * w, 1) / np.maximum(w.sum(1), 1), np.nan)
    t = sh(tb)[:, 0] - 5000 + 20000
    return dict(t_us=t, hall=feat, valid_frac=ok.mean(1), pwm=sh(pwm).max(1), dir=sh(dr).max(1), odo_mm=np.full(len(t), np.nan))

def decls_from_log(tsv, boot, pi_offset_s, force_cw=False):
    """Operator declarations of this boot: the first DECLARED nav event of each declaration, placed on the
    locomotive clock by the boot's measured Pi offset (the firmware's own baseline_start_us is unusable in r5.1)."""
    out = []; cur = None; prev = None
    for l in open(tsv, errors='replace'):
        f = l.rstrip('\n').split('\t')
        if len(f) != 3: continue
        if f[1].endswith('/state/bootid'):
            try: cur = json.loads(f[2]).get('boot_id')
            except Exception: pass
        elif f[1].endswith('/state/nav') and cur == boot:
            try: d = json.loads(f[2])
            except Exception: continue
            key = (d.get('event'), d.get('mm'), d.get('target'), d.get('dir'))
            if d.get('event') == 'DECLARED' and key != prev:
                wall = datetime.datetime.strptime(f[0][:23], '%Y-%m-%dT%H:%M:%S.%f').timestamp()
                last, nxt, dr = int(d['mm']), int(d['target']), int(d['dir'])
                if force_cw and dr < 0: last, nxt, dr = last - 1, last, 1
                out.append(dict(us=int((wall - pi_offset_s) * 1e6), last=last, next=nxt, dir=dr))
            prev = key
    return out

def prepare(col, work):
    work.mkdir(parents=True, exist_ok=True); man = {}
    pdec = pd.read_csv(PKG / 'derived/declarations.csv')
    for ds in col:
        name = ds['name']; h, tb, pwm, fb = tracing20(ds['native'])
        if ds.get('pkg_steps'):                       # 10-15 boots: the package's own steps (with IR) and declarations
            st = dict(np.load(PKG / f"derived/steps_{ds['boot']}.npz"))
            d = pdec[pdec.boot == ds['boot']]; dec = []; dirn = 1
            for r in d.sort_values('t_us').itertuples():
                if r.topic.endswith('session_direction'): dirn = 1 if r.payload == 'CW' else -1
                elif r.topic.endswith('start_interval'):
                    a_, b_ = map(int, r.payload.split('-'))
                    dec.append(dict(us=int(r.t_us), last=a_ if dirn > 0 else b_, next=b_ if dirn > 0 else a_, dir=dirn))
        else:
            st = steps40(ds['native'])
            z = np.load(ds['npz'])
            if 'decl_idx' in z.files:                 # stitched run 9: declaration stored with its tracing
                dec = [dict(us=int(z['t_us'][int(i)]), last=int(l), next=int(x), dir=int(dd))
                       for i, l, x, dd in zip(z['decl_idx'], z['decl_last'], z['decl_next'], z['decl_dir'])]
            else:
                dec = decls_from_log(ds['tsv'], ds['boot'], float(z['pi_offset_s']), ds.get('force_cw', False))
        np.savez_compressed(work / f'{name}_trace.npz', h=h, t_us=tb, pwm=pwm, first_block_us=fb)
        np.savez_compressed(work / f'{name}_steps.npz', **st)
        json.dump(dec, open(work / f'{name}_decl.json', 'w'))
        man[name] = dict(blocks=len(h), seconds=round(len(h) * 0.02, 1), gaps=int(np.isnan(h).sum()), declarations=len(dec),
                         ir=bool(np.isfinite(st['odo_mm']).any()))
        print(name, man[name], flush=True)
    json.dump(man, open(work / 'manifest.json', 'w'), indent=1)

# ----------------------------------------------------------------------------- revision 5.1
def r51(col, work):
    """The firmware's navigation path at 5b7a518 (RvmTracing, RvmContinuity, RvmShape, RvmNavigator), via replay_open.cpp."""
    src = work / 'r51_src'; exe = work / 'r51_replay'
    if not exe.exists():
        src.mkdir(exist_ok=True)
        for f in ['RvmTracing.h', 'RvmContinuity.h', 'RvmShape.h', 'RvmShapeModel.h', 'RvmNavigator.h', 'RouteMap.h']:
            (src / f).write_bytes(subprocess.run(['git', '-C', str(REPO), 'show', f'5b7a518:firmware/test-programs/NAVI_RVM_AUTO_STATIONS_BUBBLE/{f}'], capture_output=True, check=True).stdout)
        (src / 'replay_open.cpp').write_bytes(subprocess.run(['git', '-C', str(REPO), 'show', '5b7a518:tests/navi_rvm_bubble/replay_open.cpp'], capture_output=True, check=True).stdout)
        subprocess.run(['c++', '-std=c++17', '-O2', f'-I{src}', str(src / 'replay_open.cpp'), '-o', str(exe)], check=True)
    for ds in col:
        name = ds['name']; tr = np.load(work / f'{name}_trace.npz'); fb = int(tr['first_block_us'])
        dec = json.load(open(work / f'{name}_decl.json')); od = work / f'r51_{name}'; od.mkdir(exist_ok=True)
        with open(od / 'decl.csv', 'w') as f:
            for d in dec: f.write(f"{max(0, round((d['us'] - fb) / 20000))},{d['last']},{d['next']},{d['dir']}\n")
        r = subprocess.run([str(exe), ds['native'], str(od / 'decl.csv'), str(od)], capture_output=True, text=True)
        print(name, r.stdout.strip().splitlines()[-1][:120] if r.stdout else r.stderr[:200], flush=True)

# ----------------------------------------------------------------------------- the 10-16 aligner
def align(col, work, name):
    """Run the frozen aligner exactly as written (aligner.run, declarations='all', templates CW_L1/CCW_L1)."""
    ds = next(d for d in col if d['name'] == name)
    aw = work / f'align_{name}'; (aw / 'scripts').mkdir(parents=True, exist_ok=True); (aw / 'derived').mkdir(exist_ok=True)
    (aw / 'scripts/aligner.py').write_bytes((REPO / 'analysis/navi_rvm_replay_20261016_frozen/aligner.py').read_bytes())
    (aw / 'scripts/speed_model.py').write_bytes((PKG / 'scripts/speed_model.py').read_bytes())
    for f in ['template_CW_L1.npy', 'template_CCW_L1.npy', 'speed_model_CW_L1.csv', 'speed_model_CCW_L1.csv', 'map_positions.csv']:
        (aw / 'derived' / f).write_bytes((PKG / 'derived' / f).read_bytes())
    boot = ds['boot']; st = np.load(work / f'{name}_steps.npz'); np.savez_compressed(aw / f'derived/steps_{boot}.npz', **st)
    dec = json.load(open(work / f'{name}_decl.json')); rows = []
    for d in dec:
        a_, b_ = (d['last'], d['next']) if d['dir'] > 0 else (d['next'], d['last'])
        rows.append((boot, d['us'] - 1, 'x/cmd/session_direction', 'CW' if d['dir'] > 0 else 'CCW'))
        rows.append((boot, d['us'], 'x/cmd/start_interval', f'{a_}-{b_}'))
    pd.DataFrame(rows, columns=['boot', 't_us', 'topic', 'payload']).to_csv(aw / 'derived/declarations.csv', index=False)
    drv = f"""import sys, numpy as np; sys.path.insert(0, r'{aw / 'scripts'}'); import aligner
o = aligner.run('{boot}', 'CW_L1', 'CCW_L1', declarations='all', label='eval')
n = len(o['t']); xr = np.full(n, np.nan)
for i in range(n):
    if o['seg'][i] >= 0 and np.isfinite(o['x'][i]): xr[i] = o['retro'](i, 25)
np.savez_compressed(r'{work / f'aligned_{name}.npz'}', t=o['t'], x=o['x'], xr=xr, seg=o['seg'], sgn=o['sgn'], pwm=o['pwm'])
print('done', n)"""
    py = str(pathlib.Path.home() / 'NGR/NGR-Files/hall-ground-20261015/.venv/bin/python')
    r = subprocess.run([py, '-c', drv], capture_output=True, text=True, cwd=str(aw))
    print(name, r.stdout.strip()[-200:], r.stderr.strip()[-400:], flush=True)

# ----------------------------------------------------------------------------- shared scoring
def raw_check(h, tb, t_us_centre):
    """Model-independent: the largest 20 ms excursion within +/-0.5 s of the claimed passage, against the median
    of +/-1.5 s. >= 70 counts: its sign (+1 N, -1 S). Otherwise 0 (no magnet seen there)."""
    i = int(np.clip(np.searchsorted(tb, t_us_centre), 0, len(h) - 1))
    w = h[max(0, i - 75):i + 76]; c = h[max(0, i - 25):i + 26]
    if np.isfinite(w).sum() < 20 or np.isfinite(c).sum() < 5: return None
    med = np.nanmedian(w); e = c - med; k = int(np.nanargmax(np.abs(e)))
    return int(np.sign(e[k])) if abs(e[k]) >= 70 else 0

def claims_r51(work, name, tb):
    od = work / f'r51_{name}'; nav = pd.read_csv(od / 'nav.csv'); ens = pd.read_csv(od / 'ensembles.csv').set_index('seq')
    rows = []
    for r in nav.itertuples():
        if r.event == 'MM':
            c = int(ens.loc[r.seq, 'c_id']); rows.append(dict(t_us=int(tb[min(c, len(tb) - 1)]), decided_us=int(tb[min(int(r.block), len(tb) - 1)]), mm=int(r.mm), kind='MM'))
        elif r.event == 'CONTRADICTION': rows.append(dict(t_us=int(tb[min(int(r.block), len(tb) - 1)]), decided_us=int(tb[min(int(r.block), len(tb) - 1)]), mm=-1, kind='CONTRADICTION'))
        elif r.event == 'DECLARE': rows.append(dict(t_us=int(tb[min(int(r.block), len(tb) - 1)]), decided_us=int(tb[min(int(r.block), len(tb) - 1)]), mm=int(r.mm), kind='DECLARE'))
    return pd.DataFrame(rows, columns=['t_us', 'decided_us', 'mm', 'kind'])

def claims_aligner(work, name, dec, lagged):
    """MM passages of the aligner: the first step at which its position passes the map coordinate of the next
    route MM (in travel direction), per declaration segment. Live estimate (lagged=False) or 1 s fixed lag."""
    z = np.load(work / f'aligned_{name}.npz'); t = z['t'] * 1e6; x = z['xr'] if lagged else z['x']; seg = z['seg']
    mp = pd.read_csv(PKG / 'derived/map_positions.csv').set_index('mm').map_mm.values; LAP = json.load(open(PKG / 'derived/map_meta.json'))['lap_mm']
    rows = []; lag = 25 if lagged else 0
    for s in sorted(set(seg[seg >= 0])):
        idx = np.where((seg == s) & np.isfinite(x))[0]
        if not len(idx): continue
        d = dec[s] if s < len(dec) else dec[-1]; dr = d['dir']; nxt = d['next']
        rows.append(dict(t_us=int(t[idx[0]]), decided_us=int(t[idx[0]]), mm=d['last'], kind='DECLARE'))
        # route distance from the declared last MM to each following MM, accumulated span by span
        cur, prevm, D = nxt, d['last'], 0.0
        D += ((mp[cur] - mp[prevm]) * dr) % LAP
        # progress is measured from the declared last MM's map coordinate (not from the first estimate)
        p = ((x[idx[0]] - mp[d['last']]) * dr + LAP / 2) % LAP - LAP / 2; prev = x[idx[0]]
        for i in idx[1:]:
            p += ((x[i] - prev) * dr + LAP / 2) % LAP - LAP / 2; prev = x[i]
            while p >= D:
                j = min(i + lag, len(t) - 1)
                rows.append(dict(t_us=int(t[i]), decided_us=int(t[j]), mm=int(cur), kind='MM'))
                prevm, cur = cur, (cur + dr) % NMM
                D += ((mp[cur] - mp[prevm]) * dr) % LAP
    return pd.DataFrame(rows, columns=['t_us', 'decided_us', 'mm', 'kind'])

def score(col, work):
    out = {}
    for ds in col:
        name = ds['name']; tr = np.load(work / f'{name}_trace.npz'); h, tb = tr['h'], tr['t_us']
        dec = json.load(open(work / f'{name}_decl.json')); res = dict(loco=ds['loco'], date=ds['date'], notes=ds.get('notes', ''))
        models = {}
        if (work / f'r51_{name}' / 'nav.csv').exists(): models['r51'] = claims_r51(work, name, tb)
        if (work / f'aligned_{name}.npz').exists():
            models['align_live'] = claims_aligner(work, name, dec, False); models['align_lag1s'] = claims_aligner(work, name, dec, True)
        for m, cl in models.items():
            mm = cl[cl.kind == 'MM']; sup = con = uns = 0; first_bad = None; checks = []
            for k, r in enumerate(mm.itertuples()):
                v = raw_check(h, tb, r.t_us); want = 1 if ROUTE[r.mm % NMM] == 1 else -1
                if v is None: checks.append('gap'); continue
                if v == 0: uns += 1; checks.append('none')
                elif v == want: sup += 1; checks.append('ok')
                else:
                    con += 1; checks.append('bad')
                    if first_bad is None: first_bad = k
            # operator truth points: each redeclaration vs the model's last claimed MM before it
            tp = []
            for d in dec[1:]:
                before = cl[(cl.decided_us < d['us']) & (cl.kind.isin(['MM', 'DECLARE']))]
                if not len(before): continue
                last = before.iloc[-1]; err = None
                if last.kind in ('MM', 'DECLARE'):
                    err = int(((d['last'] - last.mm) * d['dir'] + NMM // 2) % NMM - NMM // 2)
                tp.append(dict(declared_last=d['last'], model_last=int(last.mm), err_mm=err,
                               model_stopped=bool(((cl.kind == 'CONTRADICTION') & (cl.decided_us < d['us']) & (cl.decided_us > (dec[dec.index(d) - 1]['us']))).any())))
            res[m] = dict(claims=len(mm), supported=sup, contradicted=con, unsupported=uns, gap=checks.count('gap'),
                          first_contradicted_claim=first_bad, contradictions=int((cl.kind == 'CONTRADICTION').sum()), truth_points=tp)
        out[name] = res; print(name, json.dumps({k: (v if not isinstance(v, dict) else {kk: vv for kk, vv in v.items() if kk != 'truth_points'}) for k, v in res.items()}), flush=True)
    json.dump(out, open(work / 'scores.json', 'w'), indent=1, default=str)

if __name__ == '__main__':
    cmd, col, work = sys.argv[1], json.load(open(sys.argv[2])), pathlib.Path(sys.argv[3])
    if cmd == 'prepare': prepare(col, work)
    elif cmd == 'r51': r51(col, work)
    elif cmd == 'align': align(col, work, sys.argv[4])
    elif cmd == 'score': score(col, work)
