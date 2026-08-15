#!/usr/bin/env python3
"""Straddle sweep: miss opens an evaluation toward +1, an insertion mid-window
returns the true offset to 0 — the 17:21 board shape. Look for rescue."""
import subprocess, sys
H = sys.argv[1]
DNA = [1,1,0,0,0,1,1,0,0,0,0,1,1,1,1,0,0,1,0,0,0,1,0,1,1,1,0,0,1,1,1,1,1,0,1,1,0,0,0,0,
       0,0,1,0,1,0,0,0,1,1,0,1,0,0,0,0,1,0,0,1,0,1,0,1,0,1,1,0,0,1,0,1,0,1,1,0,1,1,1,0,
       1,1,1,1,0,0,0,1,1,0,1,1,0,0,1,0,1,1,0,0,1,0,0,1,0,0,0,1,1,1,1,1,1,1,0,1,0,0,1,1,
       1,0,0,0,1,0,1,1,0,1,0,1,1,0,0,1,1,0,0,0,0,0,1,0,1,1,1,1,0,1,0,1,1,1,0,1,0,1,0,0,
       1,0,1,0,0,0,0,1,1,1,0]
N=171
def pol(m): return 'N' if DNA[m%N] else 'S'
def ev(p,dt=2400,pk=180,du=175): return f'event {dt} {p} 90 90 {pk} {du} 0'
hits=[]
for start in range(N):
    for k2 in (2,3,4,5,6):
        t=start; cmds=['dir CW',f'declare {start}','auto 1']
        for _ in range(5):                       # clean lead
            t=(t+1)%N; cmds.append(ev(pol(t)))
        t=(t+2)%N                                # ONE marker missed
        cmds.append(ev(pol(t),dt=4800))
        for _ in range(k2):                      # eval opens, +1 accumulating
            t=(t+1)%N; cmds.append(ev(pol(t)))
        cmds.append(ev(pol(t),dt=1200))          # same-pole insertion: offset back to 0
        for _ in range(18):
            t=(t+1)%N; cmds.append(ev(pol(t)))
        cmds.append('dump')
        out=subprocess.run([H],input='\n'.join(cmds),capture_output=True,text=True).stdout
        if 'QUORUM_SUFFIX_RESCUE' in out:
            hits.append((start,k2,'NO_QUORUM' in out,'"state":"NORMAL"' in out.rsplit('{',1)[-1] or 'NORMAL' in out[-200:]))
print(f'{len(hits)} rescue hits')
for h in hits[:40]: print(h)
