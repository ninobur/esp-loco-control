import subprocess, sys, json
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
    for k2 in (2,3,4,5,6):
        t=start; cmds=['dir CCW',f'declare {start}','auto 1']
        for _ in range(5): t=(t-1)%N; cmds.append(ev(pol(t)))
        t=(t-2)%N; cmds.append(ev(pol(t),dt=4800))
        for _ in range(k2): t=(t-1)%N; cmds.append(ev(pol(t)))
        cmds.append(ev(pol(t),dt=1200))
        for _ in range(18): t=(t-1)%N; cmds.append(ev(pol(t)))
        cmds.append('dump')
        out=subprocess.run([H],input='\n'.join(cmds),capture_output=True,text=True).stdout
        if 'QUORUM_SUFFIX_RESCUE' not in out or 'NO_QUORUM' in out: continue
        fin=[json.loads(l) for l in out.splitlines() if '"ring_len"' in l][-1]
        if fin['state']=='NORMAL' and fin['dir']=='CCW' and fin['mm']==t:
            wraps = start-25 < 0   # window walked below 0
            hits.append((start,k2,t,wraps))
print(len(hits),'strict CCW hits (start,k2,true_mm,crosses_wrap)')
for h in hits[:25]: print(h)
