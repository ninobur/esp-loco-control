import subprocess, sys, json
H=sys.argv[1]
DNA=[1,1,0,0,0,1,1,0,0,0,0,1,1,1,1,0,0,1,0,0,0,1,0,1,1,1,0,0,1,1,1,1,1,0,1,1,0,0,0,0,
     0,0,1,0,1,0,0,0,1,1,0,1,0,0,0,0,1,0,0,1,0,1,0,1,0,1,1,0,0,1,0,1,0,1,1,0,1,1,1,0,
     1,1,1,1,0,0,0,1,1,0,1,1,0,0,1,0,1,1,0,0,1,0,0,1,0,0,0,1,1,1,1,1,1,1,0,1,0,0,1,1,
     1,0,0,0,1,0,1,1,0,1,0,1,1,0,0,1,1,0,0,0,0,0,1,0,1,1,1,1,0,1,0,1,1,1,0,1,0,1,0,0,
     1,0,1,0,0,0,0,1,1,1,0]
N=171; OFFS=[-1,0,1,2,3,4]
def pol(m): return 'N' if DNA[m%N] else 'S'
def ev(p,dt=2400): return f'event {dt} {p} 90 90 180 175 0'
def build(start,k2,v,w):
    t=start; cmds=['dir CW',f'declare {start}','auto 1']
    for _ in range(5): t=(t+1)%N; cmds.append(ev(pol(t)))
    t=(t+2)%N; cmds.append(ev(pol(t),dt=4800))
    for _ in range(k2): t=(t+1)%N; cmds.append(ev(pol(t)))
    for _ in range(v): t=(t+1)%N; cmds.append(ev(pol(t)))
    cmds.append(ev(pol(t),dt=1200))
    for _ in range(w): t=(t+1)%N; cmds.append(ev(pol(t)))
    t=(t+2)%N; cmds.append(ev(pol(t),dt=4800))
    for _ in range(14): t=(t+1)%N; cmds.append(ev(pol(t)))
    return cmds+['dump'], t
cmds,true_t=build(56,4,1,2)
out=subprocess.run([H],input='\n'.join(cmds),capture_output=True,text=True).stdout
polarity_in=[l.split()[2] for l in cmds if l.startswith('event')]
accepted=[]; ei=0; prev_mm=None
for l in out.splitlines():
    d=json.loads(l)
    if d.get('ev'):
        mm=d['mm']
        if prev_mm is not None and mm!=prev_mm: accepted.append((mm,polarity_in[ei]))
        prev_mm=mm; ei+=1
    elif 'pub' in d:
        p=json.loads(d['payload'])
        if p.get('event') in ('QUORUM_OPEN','QUORUM_ADOPTED','QUORUM_REOPENED','QUORUM_SUFFIX_RESCUE','NO_QUORUM','QUORUM_CLOSED'):
            print({k:v for k,v in p.items() if k in ('event','mm','offset','scores','reason','eval')})
ring=accepted[-12:]
print('ring (mm,pol):',ring)
for c,off in enumerate(OFFS):
    run=0
    for mm,p in reversed(ring):
        if p==pol(mm+off): run+=1
        else: break
    print(f'offset {off:+d}: suffix {run}')
print('true final mm:',true_t)
