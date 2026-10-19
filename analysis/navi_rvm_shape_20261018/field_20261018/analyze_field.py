"""Field evidence, 2026-10-18 evening: Otto, NAVI_RVM_SHAPE_MANUAL 30271a6.
Source: Pi run log /Users/davidbrown/Downloads/9950011_20261018_210522.log (David),
sha256 in docs/NAVI_RVM_SHAPE_MANUAL_FIELD_RUN_20261018.md. Read only.
1. Extracts diag/rvm_ensemble, diag/rvm_stopgo and state/loopstat.
2. Compares the sequence of recognized principal polarities with the route
   polarity map read CCW (declared) and CW, and polarity inverted: longest exact
   run. Every 10-magnet polarity window of the route is unique, so a long exact
   run identifies which magnets were passed, independently of the navigator.
3. Places each STOP/GO transition relative to the recognized ensembles."""
import json, sys, numpy as np, pandas as pd, pathlib
HERE = pathlib.Path(__file__).resolve().parent
LOG = sys.argv[1] if len(sys.argv) > 1 else '/Users/davidbrown/Downloads/9950011_20261018_210522.log'
R = np.loadtxt(HERE.parent / 'scripts' / 'route_polarity.txt', dtype=int); sgn = np.where(R == 1, 1, -1)
rows = [l.rstrip('\n').split('\t') for l in open(LOG, encoding='utf-8', errors='replace')]
L = pd.DataFrame([r for r in rows if len(r) >= 3], columns=['t', 'topic', 'payload'])
L['topic'] = L.topic.str.replace('ngr/loco/9950011/', '', regex=False)
get = lambda tp: pd.DataFrame([dict(json.loads(p), wall=t) for t, p in L[L.topic == tp][['t', 'payload']].values])
e, sg, ls = get('diag/rvm_ensemble'), get('diag/rvm_stopgo'), get('state/loopstat')
obs = np.where(e.polarity == 'N', 1, -1); n = len(obs)
res = {}
for name, step, inv in [('CCW', -1, 1), ('CW', 1, 1), ('CCW_inverted', -1, -1), ('CW_inverted', 1, -1)]:
    best = (0, 0, 0)
    for i in range(n):
        for s in range(171):
            k = 0
            while i + k < n and obs[i + k] * inv == sgn[(s + step * k) % 171]: k += 1
            if k > best[0]: best = (k, i, s)
    res[name] = dict(longest_exact_run=best[0], at_recognition=best[1], starting_mm=best[2])
e['mm_if_cw_from_042'] = [(42 + i) % 171 for i in range(n)]
stops = []
for r in sg.itertuples():
    prev = e[e.end_us < r.block_us].tail(1); nxt = e[e.start_us > r.block_us].head(1)
    inside = e[(e.start_us <= r.block_us) & (e.end_us >= r.block_us)]
    stops.append(dict(wall=r.wall, transition=r.transition, inside_ensemble=inside.mm_if_cw_from_042.tolist(),
                      prev_mm=int(prev.mm_if_cw_from_042.iloc[0]) if len(prev) else None,
                      s_after_prev=round((r.block_us - prev.end_us.iloc[0]) / 1e6, 2) if len(prev) else None,
                      next_mm=int(nxt.mm_if_cw_from_042.iloc[0]) if len(nxt) else None,
                      arrival_trimmed=int(r.arrival_blocks_trimmed)))
out = dict(recognitions=n, results=e.result.value_counts().to_dict(), polarity_alignment=res, stops=stops,
           health=dict(heap_min=int(ls.heap_min.min()), heap_free_min=int(ls.heap_free.min()), block_q_drop=int(ls.block_q_drop.max()),
                       nav_event_drop=int(ls.nav_event_drop.max()), rvm_push_max_us=int(ls.rvm_push_max_us.max()),
                       rvm_stack_free_min=int(ls[ls.rvm_stack_free > 0].rvm_stack_free.min()), pub_drop=int(ls.pub_drop.max()),
                       nsr_native_drop=int(ls.nsr_native_drop.max()), nsr_udp_fail=int(ls.nsr_udp_fail.max())),
           block_drop_times=ls[ls.block_q_drop.diff().fillna(0) > 0][['wall', 'block_q_drop', 'rvm_push_max_us']].to_dict('records'))
json.dump(out, open(HERE / 'field_summary.json', 'w'), indent=1, default=str)
print(json.dumps(out, indent=1, default=str))
