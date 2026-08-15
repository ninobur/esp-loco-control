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

# --- exact run of the winning fixture (start 36, k2 2) ---
t=36; cmds=['dir CW','declare 36','auto 1']
for _ in range(5): t=(t+1)%N; cmds.append(ev(pol(t)))
t=(t+2)%N; cmds.append(ev(pol(t),dt=4800))
for _ in range(2): t=(t+1)%N; cmds.append(ev(pol(t)))
cmds.append(ev(pol(t),dt=1200))
for _ in range(18): t=(t+1)%N; cmds.append(ev(pol(t)))
cmds.append('dump')
out=subprocess.run([H],input='\n'.join(cmds),capture_output=True,text=True).stdout
for l in out.splitlines():
    if any(k in l for k in ('SUFFIX','ADOPTED','REOPENED','NO_QUORUM','TIED','OPEN')) or '"state"' in l and 'mm' in l and 'ring_len' in l:
        pass
decs=[json.loads(l) for l in out.splitlines() if l.startswith('{')]
for d in decs:
    if 'pub' in d:
        p=json.loads(d['payload'])
        if p.get('event') in ('QUORUM_OPEN','QUORUM_TIED','QUORUM_SUFFIX_RESCUE','QUORUM_ADOPTED','QUORUM_CLOSED','QUORUM_REOPENED','NO_QUORUM'):
            print({k:v for k,v in p.items() if k in ('event','mm','scores','leader','margin','suffix','rescue_offset','offset','eval')})
    elif 'ring_len' in d and 'ev' not in d:
        print('FINAL', {k:d[k] for k in ('state','mm','agree','disagree','auto')})
print('true final mm =', t)

# --- map fact: can TWO offsets ever share a clean 7-suffix? ---
# Two candidates c1<c2 both fit 7 consecutive ring entries iff DNA agrees with
# itself at lag (c2-c1) over 7 consecutive positions. Candidate offsets span
# -1..4 -> lags 1..5.
worst={}
for lag in range(1,6):
    best=0; run=0
    for i in range(2*N):
        if DNA[i%N]==DNA[(i+lag)%N]: run+=1; best=max(best,run)
        else: run=0
    worst[lag]=best
print('max same-polarity agreement run by lag:', worst)
