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
# lag-6 completeness check for the map fact (offsets vs true offsets outside set)
best=0;run=0
for i in range(2*N):
    if DNA[i%N]==DNA[(i+6)%N]: run+=1; best=max(best,run)
    else: run=0
print('lag 6 max run:',best)
for start in range(N):
    for k2 in (4,5,6):
        for v in (1,2,3):
            for w in (1,2,3):
                t=start; cmds=['dir CW',f'declare {start}','auto 1']
                for _ in range(5): t=(t+1)%N; cmds.append(ev(pol(t)))
                t=(t+2)%N; cmds.append(ev(pol(t),dt=4800))       # miss -> +1
                for _ in range(k2): t=(t+1)%N; cmds.append(ev(pol(t)))  # adopt +1
                for _ in range(v): t=(t+1)%N; cmds.append(ev(pol(t)))   # validating
                cmds.append(ev(pol(t),dt=1200))                  # insertion -> breaks validation
                for _ in range(w): t=(t+1)%N; cmds.append(ev(pol(t)))
                t=(t+2)%N; cmds.append(ev(pol(t),dt=4800))       # second miss -> true +1 again
                for _ in range(14): t=(t+1)%N; cmds.append(ev(pol(t)))
                cmds.append('dump')
                out=subprocess.run([H],input='\n'.join(cmds),capture_output=True,text=True).stdout
                if ('QUORUM_REOPENED' in out and 'HARD_BOUND' in out
                        and 'SUFFIX_RESCUE' not in out and 'SECOND_ADOPTION' not in out):
                    hits.append((start,k2,v,w))
print(len(hits),'candidate excluded-collision terminals')
print(hits[:20])
