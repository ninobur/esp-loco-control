"""Replay each recording through the recognizer with each continuity mechanism and score it.
  u04_replay.py <boots_dir> <decl_events.tsv> <out_dir> <dataset> [<dataset> ...] [--grid]
dataset: BOOT1 (2026-10-15 boot 1, local derived data) or an Otto boot id from <boots_dir>,
optionally suffixed ':CW' to replace the field declaration's direction (Oct 18 counterfactual).
Recognizer = rvm_local (Python reference of the firmware, parity 164/164 on run 8), model_A.json.
Analysis only."""
import sys, json, time, pathlib, numpy as np, pandas as pd
HERE = pathlib.Path(__file__).resolve().parent; sys.path.insert(0, str(HERE.parent / 'scripts')); sys.path.insert(0, str(HERE))
from common import *
import rvm_shape as R, rvm_local as L, stopgo as G, u03_mech as M
BD, DEV, OUT = pathlib.Path(sys.argv[1]), sys.argv[2], pathlib.Path(sys.argv[3]); OUT.mkdir(parents=True, exist_ok=True)
GRID = '--grid' in sys.argv; DATASETS = [a for a in sys.argv[4:] if not a.startswith('--')]
# last magnet the Hall sensor certainly passed, from the operator's accounts (docs/NAVI_RVM_SHAPE_MANUAL_FIELD_RUN_20261018.md)
TRUTH_END = {'05D9583227F92717': 38, 'B05694E24BFC6579': 126, '8F9B71ED5DAF40BB': 33, 'EFD0225ADC42E524': 23,
             'FA9D779D889CC1F9': 23, 'B29BDF0E37C0CDA1': 23, 'RUN9': 22}   # RUN9: Hall at rest atop MM023 (David), so MM022 is the last completed magnet
ROUTE = np.loadtxt(HERE.parent / 'scripts' / 'route_polarity.txt', dtype=int); RSGN = np.where(ROUTE == 1, 1, -1); NMM = len(ROUTE)
mj = json.load(open(ROOT / 'results/local/model_A.json'))
def gmm(c):
    g = R.GMM(len(c['w'])); g.w = np.array(c['w']); g.mu = np.array(c['mu']); g.S = np.array(c['S']); return g
Em, Nm = gmm(mj['E']), gmm(mj['N'])

# ---------------- datasets ----------------
def load_boot1():
    z = np.load(D / 'tracing_boot1.npz'); h = z['hall'].astype(float); t_us = z['t_us'].astype(np.int64); ccw = z['dir'] == 2
    pwm = np.load(D / 'block_pwm_boot1.npy')[:len(h)]; cmd = np.full(len(h), -1)
    idx = lambda us: (np.asarray(us) - t_us[0]) / 20000
    dd = pd.read_csv(RP / 'declarations.csv'); dd = dd[dd.boot == BOOT1]
    st = dd[dd.topic.str.contains('start_interval')]; sd = dd[dd.topic.str.contains('session_direction')]
    decl = []
    for r in st.itertuples():
        a_, b_ = map(int, r.payload.split('-')); d = 1 if sd[sd.t_us <= r.t_us].payload.iloc[-1] == 'CW' else -1
        decl.append((int(np.ceil(idx(r.t_us))), (a_, b_) if d > 0 else (b_, a_), d))
    # IR odometer reference (boot 1): moving if inside an inter-pulse interval < 1 s, stationary if > 3 s
    ir = pd.read_csv(HG / 'ir_coordinate_source.csv'); ir = ir[ir.boot == BOOT1].sort_values('us')
    pt = ir[ir.um.diff().fillna(1) != 0].us.values.astype(float); k = np.searchsorted(pt, t_us)
    gap = np.where((k > 0) & (k < len(pt)), pt[np.minimum(k, len(pt) - 1)] - pt[np.maximum(k - 1, 0)], np.inf)
    irs = np.where(gap < 1e6, 1, np.where(gap > 3e6, 0, -1)).astype(np.int8)
    ann = pd.read_csv(D / 'annotation_boot1.csv'); ann['s_i'] = np.round(idx(ann.start_us)).astype(int); ann['e_i'] = np.round(idx(ann.end_us)).astype(int)
    return dict(name='BOOT1', h=h, t_us=t_us, ccw=ccw, pwm=pwm, cmd=cmd, decl=decl, ir=irs, ann=ann)

def load_nsr(bid, force_cw=False):
    z = np.load(BD / f'boot_{bid}.npz'); h = z['hall'].astype(float); t_us = z['t_us'].astype(np.int64)
    decl = []
    for l in open(DEV, errors='replace'):
        f = l.rstrip('\n').split('\t')
        if len(f) == 3 and f[1].endswith('/state/nav'):
            d = json.loads(f[2])
            if d.get('event') == 'DECLARED':
                # the nav event belongs to this boot if its baseline block lies in this tracing and it is reported after boot
                pass
    ev = []
    cur = None
    for l in open(DEV, errors='replace'):
        f = l.rstrip('\n').split('\t')
        if len(f) != 3: continue
        if f[1].endswith('/state/bootid'): cur = json.loads(f[2]).get('boot_id')
        elif f[1].endswith('/state/nav') and cur == bid:
            d = json.loads(f[2])
            if d.get('event') == 'DECLARED': ev.append((d['baseline_start_us'], d['mm'], d['target'], d['dir']))
    for us, last, nxt, dr in sorted(set(ev)):
        i = int(np.argmin(np.abs(t_us - us)))
        if force_cw and dr < 0: last, nxt, dr = last - 1, last, 1   # Oct 18: declared CCW last 42 next 41; travel was CW from MM042: CW last 41, next 42
        decl.append((i, (last, nxt), dr))
    if 'decl_idx' in z.files:                              # stitched run (u08): declaration stored with the tracing
        decl = [(int(i), (int(l), int(x)), int(d)) for i, l, x, d in zip(z['decl_idx'], z['decl_last'], z['decl_next'], z['decl_dir'])]
    ccw = np.zeros(len(h), bool)
    if decl and decl[0][2] < 0: ccw[decl[0][0]:] = True
    irp = BD / f'ir_{bid}.npy'; irs = np.load(irp) if irp.exists() else np.full(len(h), -1, np.int8)
    return dict(name=bid + (':CW' if force_cw else ''), h=h, t_us=t_us, ccw=ccw, pwm=z['pwm'], cmd=z['cmd'], decl=decl, ir=irs, ann=None)

# ---------------- recognition + navigation ----------------
def recognize(ds, keep):
    C = G.Compressed(ds['h'], ds['ccw'], keep)
    starts = [(C.start_of(i), f'decl{n}') for n, (i, _, _) in enumerate(ds['decl'])]
    det = L.run(L.Tracing(C.h, C.ccw), Em, Nm, starts) if starts else pd.DataFrame()
    if len(det):
        for col in ('c', 's', 'e', 'bl_start'): det[col + '_id'] = C.to_ids(det[col])
        det['decided_id'] = C.to_ids(det.groupby('seg').decided.cummax())
        det = det.sort_values('decided_id', kind='stable').reset_index(drop=True)
    return det

def navigate(ds, det):
    out = []; state = None; nxt = None; dr = 0; di = 0; decl = sorted(ds['decl'])
    events = [(i, 0, n) for n, (i, _, _) in enumerate(decl)] + [(r.decided_id, 1, k) for k, r in enumerate(det.itertuples())]
    for t, kind, n in sorted(events):
        if kind == 0:
            _, (last, nx), dr = decl[n]; nxt = nx; state = 'TRACKING'; continue
        pol = int(det.pol.iloc[n])
        if state != 'TRACKING': out.append((n, 'NO_CLAIM', None)); continue
        if pol == RSGN[nxt % NMM]: out.append((n, 'MM', nxt % NMM)); nxt = (nxt + dr) % NMM
        else: out.append((n, 'CONTRADICTION', nxt % NMM)); state = 'CONTRADICTION'
    return out

def align(pols, start_mm, dr):
    """Edit alignment of recognized polarities to the route from start_mm in direction dr: matches,
    misses (route magnets skipped), extras (recognitions not on the route). Route end free (up to 3 laps),
    all recognitions consumed. Among equal-cost alignments the walk matches as early as possible, so a
    miss is placed at the latest consistent position (the navigator's view: claims before a miss stand)."""
    m = len(pols); L_ = min(3 * NMM, m + 60); route = np.array([RSGN[(start_mm + dr * k) % NMM] for k in range(L_)])
    INF = 10 ** 9; G_ = np.full((m + 1, L_ + 1), INF, np.int64); G_[m, :] = 0   # cost-to-go, free route end
    for i in range(m - 1, -1, -1):
        for j in range(L_, -1, -1):
            best = G_[i + 1, j] + 1                                               # extra
            if j < L_:
                best = min(best, G_[i, j + 1] + 1)                                # miss
                if pols[i] == route[j]: best = min(best, G_[i + 1, j + 1])        # match
            G_[i, j] = best
    i = j = 0; ident = [None] * m; missed = []; extras = []
    while i < m:
        if j < L_ and pols[i] == route[j] and G_[i, j] == G_[i + 1, j + 1]: ident[i] = (start_mm + dr * j) % NMM; i += 1; j += 1
        elif G_[i, j] == G_[i + 1, j] + 1: extras.append(i); i += 1
        else: missed.append((start_mm + dr * j) % NMM); j += 1
    return ident, missed, extras, j

# ---------------- scoring ----------------
def score(ds, name, keep, feed, det, nav, t_run):
    h = ds['h']; n = len(h); fd = min(i for i, _, _ in ds['decl']) if ds['decl'] else n
    res = dict(mechanism=name, kept=int(keep[fd:].sum()), withheld=int((~keep[fd:]).sum()), recognitions=len(det), runtime_s=round(t_run, 1))
    if len(det):
        res['llr_min'] = round(float(det.llr.min()), 2); res['llr_p5'] = round(float(det.llr.quantile(.05)), 2)
        lat = (feed[np.clip(np.round(det.decided_id).astype(int), 0, n - 1)] - det.decided_id.values) * 0.02
        res['report_delay_s'] = dict(median=round(float(np.nanmedian(lat)), 2), p95=round(float(np.nanpercentile(lat, 95)), 2), max=round(float(np.nanmax(lat)), 2),
                                    never=int(np.isnan(lat).sum()))
        dec_i = np.clip(np.round(det.decided_id).astype(int), 0, n - 1)
        cmd = np.asarray(ds['cmd']); pw = np.asarray(ds['pwm'])
        if (cmd >= 0).all():
            dz = (cmd[dec_i] < 15) & (pw[dec_i] >= 15)
            res['report_delay_in_decel_s'] = dict(n=int(dz.sum()), median=round(float(np.nanmedian(lat[dz])), 2) if dz.any() else None,
                                                  max=round(float(np.nanmax(lat[dz])), 2) if dz.any() else None, never=int(np.isnan(lat[dz]).sum()))
        res['decide_after_end_s'] = dict(median=round(float(np.median(det.decided_id - det.e_id) * 0.02), 2), max=round(float((det.decided_id - det.e_id).max() * 0.02), 2))
    ev = pd.Series([e for _, e, _ in nav]).value_counts().to_dict() if nav else {}
    res['nav'] = ev
    # route alignment (identity independent of the navigator)
    if ds['decl'] and len(det):
        i0, (last, nxt), dr = sorted(ds['decl'])[0]
        ident, missed, extras, _ = align([int(p) for p in det.pol], nxt, dr)
        tc = ds['t_us'][np.clip(np.round(det.c_id).astype(int), 0, n - 1)] / 1e6
        res['route'] = dict(start=f'MM{nxt:03d}', matched=sum(x is not None for x in ident), missed=[f'MM{x:03d}' for x in missed],
                            extras=[round(float(tc[k]), 1) for k in extras])
        bid = ds['name'].split(':')[0]
        if bid in TRUTH_END:
            lastid = max((x for x in ident if x is not None), key=lambda x: ((x - nxt) * dr) % NMM, default=None)
            span = ((TRUTH_END[bid] - nxt) * dr) % NMM + 1                       # magnets nxt..end inclusive (single lap)
            got = ((lastid - nxt) * dr) % NMM + 1 if lastid is not None else 0
            res['route']['truth_end'] = f'MM{TRUTH_END[bid]:03d}'
            res['route']['tail_missed'] = [f'MM{(nxt + dr * q) % NMM:03d}' for q in range(got, span)] if len(ident) < 2 * NMM else []
        claims = [(k, mm) for k, e, mm in nav if e == 'MM']
        res['claims_correct'] = sum(1 for k, mm in claims if ident[k] == mm); res['claims_wrong'] = sum(1 for k, mm in claims if ident[k] != mm)
        res['claims_duplicate'] = len(claims) - len({mm for _, mm in claims}) if len(claims) <= NMM else None
    # IR reference: withheld blocks while the IR shows movement; stationary-in-magnet blocks kept
    ir = ds['ir']; base = np.nanmedian(h[fd:][ir[fd:] == 1]) if (ir[fd:] == 1).any() else np.nanmedian(h[fd:])
    post = np.arange(n) >= fd
    res['ir'] = dict(moving_blocks=int(((ir == 1) & post).sum()), withheld_while_moving=int(((ir == 1) & ~keep & post).sum()),
                     stationary_off_baseline_blocks=int(((ir == 0) & (np.abs(h - base) > 15) & post).sum()),
                     stationary_off_baseline_kept=int(((ir == 0) & (np.abs(h - base) > 15) & keep & post).sum()))
    # boot-1 annotation truth
    if ds['ann'] is not None and len(det):
        tr = ds['ann'][ds['ann'].source.isin(['OPENING', 'SEQUENCE'])]; tr = tr[tr.s_i >= fd]
        c = det.c_id.values; nd = np.array([((c >= s - 5) & (c <= e + 5)).sum() for s, e in zip(tr.s_i, tr.e_i)])
        allann = ds['ann'][ds['ann'].s_i >= fd]
        tol = np.where(allann.source.eq('NO_EXTENT'), 15, 5)
        matched = np.zeros(len(c), bool)
        for s, e, t in zip(allann.s_i, allann.e_i, tol): matched |= (c >= s - t) & (c <= e + t)
        res['truth'] = dict(ensembles=len(tr), once=int((nd == 1).sum()), missed=int((nd == 0).sum()), split=int((nd > 1).sum()),
                            unmatched_detections=int((~matched).sum()))
    return res

def run_one(ds, name, keep, feed):
    t0 = time.time(); det = recognize(ds, keep); nav = navigate(ds, det) if len(det) else []
    r = score(ds, name, keep, feed, det, nav, time.time() - t0)
    det.assign(nav_event=[e for _, e, _ in nav] if nav else [], nav_mm=[mm for _, _, mm in nav] if nav else []).to_csv(OUT / f"det_{ds['name'].replace(':', '_')}_{name}.csv", index=False)
    print(json.dumps(r), flush=True); return r

results = []
for dname in DATASETS:
    ds = load_boot1() if dname == 'BOOT1' else load_nsr(dname.split(':')[0], dname.endswith(':CW'))
    if not ds['decl']: print('no declaration:', dname); continue
    dec = [i for i, _, _ in ds['decl']]
    mechs = [('R0',) + M.r0(ds['h']), ('F',) + M.f_current(ds['h'], ds['pwm'], ds['cmd'], dec)[:2],
             ('U1_k50_b24_m16',) + M.u_unified(ds['h'], dec, 50, 24, 16, True), ('U0_k50_b24',) + M.u_unified(ds['h'], dec, 50, 24, 16, False)]
    if GRID:
        for k in (25, 50, 100):
            for b in (8, 16, 24, 32):
                for m in (8, 16, 32):
                    if (k, b, m) != (50, 24, 16): mechs.append((f'U1_k{k}_b{b}_m{m}',) + M.u_unified(ds['h'], dec, k, b, m, True))
    for name, keep, feed in mechs:
        r = run_one(ds, name, keep, feed); r['dataset'] = ds['name']; results.append(r)
    json.dump(results, open(OUT / f"results_{'_'.join(d.replace(':', '_') for d in DATASETS)}{'_grid' if GRID else ''}.json", 'w'), indent=1, default=str)
