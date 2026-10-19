"""General field-run analysis for NAVI_RVM_SHAPE_MANUAL (Pi run-logger logs).
  analyze_run.py <run.log> <out_dir>
For every boot in the log:
- recognitions (diag/rvm_ensemble), STOP/GO transitions, loopstat health;
- longest exact run of the recognized polarity sequence against the route map
  read CW and CCW (each 10-magnet window of the route is unique, so a long run
  identifies the magnets passed independently of the navigator);
- each MM claim (result MM) compared with the route-derived identity, where the
  route-derived identity is taken from the longest run in the claimed direction.
Read only."""
import json, sys, pathlib, numpy as np, pandas as pd
HERE = pathlib.Path(__file__).resolve().parent
LOG, OUT = sys.argv[1], pathlib.Path(sys.argv[2]); OUT.mkdir(parents=True, exist_ok=True)
R = np.loadtxt(HERE.parent / 'scripts' / 'route_polarity.txt', dtype=int); sgn = np.where(R == 1, 1, -1)
rows = [l.rstrip('\n').split('\t') for l in open(LOG, encoding='utf-8', errors='replace')]
L = pd.DataFrame([r[:3] for r in rows if len(r) >= 3], columns=['t', 'topic', 'payload'])
L['topic'] = L.topic.str.replace('ngr/loco/9950011/', '', regex=False)
def get(tp):
    out = []
    for t, p in L[L.topic == tp][['t', 'payload']].values:
        try: out.append(dict(json.loads(p), wall=t))
        except Exception: pass
    return pd.DataFrame(out)
E, SG, LS, NV = get('diag/rvm_ensemble'), get('diag/rvm_stopgo'), get('state/loopstat'), get('state/nav')
summary = dict(log=LOG, lines=len(L), first=L.t.iloc[0], last=L.t.iloc[-1], boots={})
def longest(obs, step):
    best = (0, 0, 0)
    for i in range(len(obs)):
        for s in range(171):
            k = 0
            while i + k < len(obs) and obs[i + k] == sgn[(s + step * k) % 171]: k += 1
            if k > best[0]: best = (k, i, s)
    return best
for boot, e in (E.groupby('boot') if len(E) else []):
    e = e.reset_index(drop=True); obs = np.where(e.polarity == 'N', 1, -1)
    cw, ccw = longest(obs, 1), longest(obs, -1)
    step = 1 if cw[0] >= ccw[0] else -1; run = cw if step == 1 else ccw
    e['route_mm'] = [((run[2] + step * (i - run[1])) % 171) for i in range(len(e))]   # valid inside/around the run
    in_run = (e.index >= run[1]) & (e.index < run[1] + run[0])
    claims = e[e.result == 'MM'].copy()
    claims['route_agrees'] = claims.mm.astype(int) == claims.route_mm
    claims['in_run'] = in_run[claims.index]
    e.to_csv(OUT / f'ensembles_{boot}.csv', index=False)
    sg = SG[SG.boot == boot] if len(SG) else SG
    ls = LS  # loopstat carries no boot id; boots are separated by time in practice
    summary['boots'][boot] = dict(
        recognitions=len(e), results=e.result.value_counts().to_dict(),
        longest_cw=dict(run=cw[0], at=cw[1], start_mm=cw[2]), longest_ccw=dict(run=ccw[0], at=ccw[1], start_mm=ccw[2]),
        travel='CW' if step == 1 else 'CCW',
        mm_claims=len(claims), claims_agreeing_with_route=int(claims.route_agrees.sum()),
        claims_disagreeing=claims[~claims.route_agrees][['seq', 'wall', 'mm', 'route_mm']].to_dict('records'),
        contradictions=e[e.result == 'CONTRADICTION'][['seq', 'wall', 'expected_mm', 'expected_polarity', 'polarity', 'route_mm']].to_dict('records'),
        first=e.wall.iloc[0], last=e.wall.iloc[-1],
        stopgo=sg[['wall', 'transition', 'last_mm', 'expected_mm', 'candidate_held', 'arrival_blocks_trimmed', 'withheld_after_go_total',
                   'stationary_low', 'stationary_high']].to_dict('records') if len(sg) else [])
if len(LS):
    LS['wall'] = pd.to_datetime(LS.wall)
    h = dict(heap_min=int(LS.heap_min.min()), heap_free_min=int(LS.heap_free.min()), block_q_drop=int(LS.block_q_drop.max()),
             nav_event_drop=int(LS.nav_event_drop.max()), rvm_push_max_us=int(LS.rvm_push_max_us.max()),
             pub_drop=int(LS.pub_drop.max()), cmd_drop=int(LS.cmd_drop.max()),
             nsr_native_drop=int(LS.nsr_native_drop.max()), nsr_udp_fail=int(LS.nsr_udp_fail.max()))
    for k in ('rvm_backlog_max', 'rvm_stack_free'):
        if k in LS: h[k] = int(LS[k].max()) if k == 'rvm_backlog_max' else int(LS[LS[k] > 0][k].min()) if (LS[k] > 0).any() else None
    h['block_drop_times'] = LS[LS.block_q_drop.diff().fillna(0) > 0][['wall', 'block_q_drop', 'rvm_push_max_us']].astype(str).to_dict('records')
    summary['health'] = h
summary['warnings'] = L[L.topic == 'state/warning'][['t', 'payload']].drop_duplicates('payload').values.tolist()
summary['session_dirs'] = NV.session_dir.value_counts().to_dict() if len(NV) else {}
v = pd.to_numeric(L[L.topic == 'telem/voltage'].payload, errors='coerce'); summary['voltage'] = [float(v.min()), float(v.max())] if len(v) else None
json.dump(summary, open(OUT / 'summary.json', 'w'), indent=1, default=str)
print(json.dumps(summary, indent=1, default=str)[:6000])
