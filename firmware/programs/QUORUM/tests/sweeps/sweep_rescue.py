#!/usr/bin/env python3
"""Sweep starts x lead-lengths for inputs that fire QUORUM_SUFFIX_RESCUE."""
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
    for k in (3,4,5,6,7):
        t=start; cmds=[f'dir CW',f'declare {start}','auto 1']
        for _ in range(k):
            t=(t+1)%N; cmds.append(ev(pol(t)))
        cmds.append(ev(pol(t),dt=1200))          # same-pole insertion, floor-safe
        for _ in range(20):
            t=(t+1)%N; cmds.append(ev(pol(t)))
        cmds.append('dump')
        out=subprocess.run([H],input='\n'.join(cmds),capture_output=True,text=True).stdout
        if 'QUORUM_SUFFIX_RESCUE' in out:
            terminal='NO_QUORUM' in out
            hits.append((start,k,terminal,'QUORUM_ADOPTED' in out))
print(f'{len(hits)} rescue hits')
for h in hits[:40]: print(h)
