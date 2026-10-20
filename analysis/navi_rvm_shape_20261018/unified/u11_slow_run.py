"""Slow run (2026-10-19 17:43-18:03, NAVI_RVM_CONTINUITY_MANUAL on Otto, PWM mostly 30-35): what the
firmware's continuity withheld, against IR movement truth, and how fast moving magnet passages develop.
  u11_slow_run.py <boots_dir> <boot_id> <run_log> <out.json>
Withheld intervals come from the firmware's own diag/rvm_stopgo events (DEVELOPMENT_STOPPED block_us =
first withheld block, DEVELOPMENT_RESUMED block_us = first block passed again). Passages are the
firmware's diag/rvm_ensemble cores [start_us, end_us]. IR = u02 reference (research only)."""
import sys, json, numpy as np
BD, BID, LOG, OUTF = sys.argv[1:5]
z = np.load(f'{BD}/boot_{BID}.npz'); t = z['t_us'].astype(np.int64); h = z['hall']; pwm = z['pwm']; ir = np.load(f'{BD}/ir_{BID}.npy')
rows = [l.rstrip('\n').split('\t', 2) for l in open(LOG, errors='replace')]
ev = [json.loads(p) for _, tp, p in rows if tp.endswith('/diag/rvm_stopgo')]
ens = [json.loads(p) for _, tp, p in rows if tp.endswith('/diag/rvm_ensemble')]
ev = [e for e in ev if e.get('boot') == BID]; ens = [e for e in ens if e.get('boot') == BID]
idx = lambda us: int(np.clip(np.searchsorted(t, us), 0, len(t) - 1))
W = np.zeros(len(t), bool); start = None
for e in ev:
    if e['transition'] == 'DEVELOPMENT_STOPPED': start = idx(e['block_us'])
    elif e['transition'] == 'DEVELOPMENT_RESUMED' and start is not None: W[start:idx(e['block_us'])] = True; start = None
a0, a1 = idx(ens[0]['start_us']) - 500, idx(ens[-1]['end_us']) + 500          # the run's span
span = np.zeros(len(t), bool); span[a0:a1] = True
mv = (ir == 1) & span; st = (ir == 0) & span
res = dict(run_seconds=round(span.sum() * 0.02, 1), ir_moving_s=round(mv.sum() * 0.02, 1), ir_stationary_s=round(st.sum() * 0.02, 1),
           withheld_s=round((W & span).sum() * 0.02, 1), withheld_while_moving_s=round((W & mv).sum() * 0.02, 1),
           withheld_while_stationary_s=round((W & st).sum() * 0.02, 1), pwm_while_moving=dict(zip(['p5', 'median', 'p95'], np.percentile(pwm[mv], [5, 50, 95]).round(0).tolist())))
# moving blocks withheld: where in the magnet (level relative to the passage's own extreme)
P = []
for e in ens:
    s, en = idx(e['start_us']), idx(e['end_us']) + 1
    hs = h[s:en]; m = mv[s:en]
    if en - s < 10 or not np.isfinite(hs).all(): continue
    k = 50; rr = [np.ptp(hs[i:i + k]) for i in range(0, len(hs) - k + 1)] if len(hs) >= k else []
    P.append(dict(mm=e.get('mm'), core_s=round((en - s) * 0.02, 2), pwm=int(np.median(pwm[s:en])), ir_moving_frac=round(float(m.mean()), 2),
                  withheld_frac=round(float(W[s:en].mean()), 2), withheld_while_moving_frac=round(float((W[s:en] & m).mean()), 2),
                  min_1s_range=round(float(min(rr)), 1) if rr else None, llr=e['llr']))
mvP = [p for p in P if p['ir_moving_frac'] >= 0.95]
res['passages'] = len(P); res['passages_moving_throughout'] = len(mvP)
if mvP:
    r1 = [p['min_1s_range'] for p in mvP if p['min_1s_range'] is not None]
    res['moving_passages_min_1s_range'] = dict(n=len(r1), min=min(r1) if r1 else None, p5=float(np.percentile(r1, 5)) if r1 else None, median=float(np.median(r1)) if r1 else None,
                                               at_or_below_band_24=sum(x <= 24 for x in r1))
    res['moving_passages_withheld_frac'] = dict(median=float(np.median([p['withheld_while_moving_frac'] for p in mvP])), max=max(p['withheld_while_moving_frac'] for p in mvP))
    res['moving_passages_llr_min'] = min(p['llr'] for p in mvP)
res['slowest_moving_passages'] = sorted(mvP, key=lambda p: -p['core_s'])[:8]
json.dump(dict(res, all_passages=P), open(OUTF, 'w'), indent=1); print(json.dumps(res, indent=1))
