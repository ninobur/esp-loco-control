#!/usr/bin/env python3
"""Shadow-replay matrix: run extracted per-boot session replays through
variant harness binaries and tabulate navigation decisions.

Reproduces the table in docs/QUORUM_SHADOW_REPLAY_REPORT.md (CODEX finding 4:
the result must be reproducible from the repo). Recipe:

  1. Build variants (from tests/):
       clang++ -std=c++17 -Wno-format -I shim -I .. -o /tmp/qh_BASE harness.cpp
       ... -DQ_TIMING_MEASURED -o /tmp/qh_T ...
       ... -DQ_TIMING_MEASURED -DQ_OFFSETS_SYMMETRIC -o /tmp/qh_TO ...
     Toby-profile variants: shadow LocoConfig.h in a scratch dir that includes
     LL_LocoConfig_9950012.h, and put that dir FIRST on the include path
     (-I <scratch> -I shim -I ..). Replaying a locomotive under the other's
     profile silently applies the wrong quarantine floor (found the hard way).
  2. Extract sessions from the committed capture
     field-records/logs/20260820_morning_session.log:
       python3 extract_session.py --capture <log> --loco 995001X \
           --name <n> --outdir <dir>
  3. Edit S/CELLS below to point at your dir, then run this file.

Decision streams land beside the summary as dec_<replay>_<variant>.json so
per-episode trajectories can be diffed event by event.
"""
import sys, json, itertools, collections
sys.path.insert(0,'/Users/davidbrown/esp-loco-control/firmware/QUORUM/tests')
import qrun
S='/private/tmp/claude-501/-Users-davidbrown-esp-loco-control/006c3f8b-8700-43cb-a77e-a224e517bd40/scratchpad'
CELLS=[('toby_ms_s02','/tmp/qhT_'),('otto_ms_s05','/tmp/qh_'),('otto_ms_s01','/tmp/qh_'),
       ('otto_ms_s02','/tmp/qh_'),('otto_ms_s04','/tmp/qh_')]
VAR=['BASE','T','O','TO']
def navs(rows):
    out=[]
    for r in rows:
        if not isinstance(r,dict): continue
        if r.get('pub','').endswith('/state/nav'):
            try: d=json.loads(r['payload'])
            except Exception: continue
            out.append(d)
    return out
def summarise(evs):
    c=collections.Counter(e.get('event') for e in evs)
    longest=0; cur=0
    for e in evs:
        ev=e.get('event')
        if ev=='PHANTOM_REJECTED': cur+=1; longest=max(longest,cur)
        elif ev in ('AGREE','DISAGREE','DECLARED','QUORUM_ADOPTED'): cur=0
    adopts=[]
    for e in evs:
        if e.get('event')=='QUORUM_ADOPTED':
            v=e.get('viable') or []
            adopts.append((e.get('mm'), v[0] if v else None))
    noq=[e.get('mm') for e in evs if e.get('event')=='NO_QUORUM']
    return c,longest,adopts,noq
res={}
for (rp,pre),bn in itertools.product(CELLS,VAR):
    rows=qrun.run_file(pre+bn, f'{S}/replays/{rp}.replay')
    evs=navs(rows)
    json.dump(evs, open(f'{S}/dec_{rp}_{bn}.json','w'))
    c,longest,adopts,noq=summarise(evs)
    res.setdefault(rp,{})[bn]={'dis':c['DISAGREE'],'agree':c['AGREE'],'rej':c['PHANTOM_REJECTED'],
        'maxrun':longest,'opens':c['QUORUM_OPEN'],'adopts':adopts,'noq':noq}
    print("%-14s %-4s dis=%-4d rej=%-3d maxrun=%-2d opens=%-2d noq=%s adopts=%s"%(
        rp,bn,c['DISAGREE'],c['PHANTOM_REJECTED'],longest,c['QUORUM_OPEN'],noq,adopts[:8]),flush=True)
json.dump(res,open(f'{S}/matrix3.json','w'),indent=1)
print("MATRIX3 COMPLETE")
