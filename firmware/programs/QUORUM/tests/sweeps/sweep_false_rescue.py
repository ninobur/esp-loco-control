import subprocess, sys
H=sys.argv[1]
DNA=[1,1,0,0,0,1,1,0,0,0,0,1,1,1,1,0,0,1,0,0,0,1,0,1,1,1,0,0,1,1,1,1,1,0,1,1,0,0,0,0,
     0,0,1,0,1,0,0,0,1,1,0,1,0,0,0,0,1,0,0,1,0,1,0,1,0,1,1,0,0,1,0,1,0,1,1,0,1,1,1,0,
     1,1,1,1,0,0,0,1,1,0,1,1,0,0,1,0,1,1,0,0,1,0,0,1,0,0,0,1,1,1,1,1,1,1,0,1,0,0,1,1,
     1,0,0,0,1,0,1,1,0,1,0,1,1,0,0,1,1,0,0,0,0,0,1,0,1,1,1,1,0,1,0,1,1,1,0,1,0,1,0,0,
     1,0,1,0,0,0,0,1,1,1,0]
N=171
def pol(m): return 'N' if DNA[m%N] else 'S'
def ev(p,dt=2400): return f'event {dt} {p} 90 90 180 175 0'
hits=[]
for start in range(N):
    for lead in (5,6,7):
        # phantom pair: odometer runs 2 AHEAD, true offset -2 (outside fence)
        t=start; cmds=['dir CW',f'declare {start}','auto 1']
        for _ in range(lead): t=(t+1)%N; cmds.append(ev(pol(t)))
        cmds.append(ev(pol(t),dt=1200)); cmds.append(ev(pol(t),dt=1200))
        for _ in range(20): t=(t+1)%N; cmds.append(ev(pol(t)))
        out=subprocess.run([H],input='\n'.join(cmds),capture_output=True,text=True).stdout
        if 'SUFFIX_RESCUE' in out:
            hits.append((start,lead,'REOPENED' in out,'NO_QUORUM' in out))
print('false-rescue exposures:',len(hits))
for h in hits[:20]: print(h)
