import subprocess, sys, json
H=sys.argv[1]
DNA=[1,1,0,0,0,1,1,0,0,0,0,1,1,1,1,0,0,1,0,0,0,1,0,1,1,1,0,0,1,1,1,1,1,0,1,1,0,0,0,0,
     0,0,1,0,1,0,0,0,1,1,0,1,0,0,0,0,1,0,0,1,0,1,0,1,0,1,1,0,0,1,0,1,0,1,1,0,1,1,1,0,
     1,1,1,1,0,0,0,1,1,0,1,1,0,0,1,0,1,1,0,0,1,0,0,1,0,0,0,1,1,1,1,1,1,1,0,1,0,0,1,1,
     1,0,0,0,1,0,1,1,0,1,0,1,1,0,0,1,1,0,0,0,0,0,1,0,1,1,1,1,0,1,0,1,1,1,0,1,0,1,0,0,
     1,0,1,0,0,0,0,1,1,1,0]
N=171
def pol(m): return 'N' if DNA[m%N] else 'S'
def ev(p,dt,pk=180,du=175): return f'event {dt} {p} 90 90 {pk} {du} 0'
for s in range(N):
    b=(s+9)%N
    if pol((b+1)%N)==pol(b) or pol((b+2)%N)==pol((b+1)%N): continue
    t=s; cmds=['dir CW',f'declare {s}','auto 1']
    for _ in range(9): t=(t+1)%N; cmds.append(ev(pol(t),2400))
    p1='N' if pol(b)=='S' else 'S'
    cmds.append(ev(p1,480,pk=40,du=50))          # slow phantom 1: held
    cmds.append(ev(p1,450,pk=38,du=48))          # slow phantom 2: unfit witness
    t=(t+1)%N; cmds.append(ev(pol(t),1470))      # genuine b+1: remainder of 2400
    for _ in range(4): t=(t+1)%N; cmds.append(ev(pol(t),2400))
    cmds.append('dump')
    out=subprocess.run([H],input='\n'.join(cmds),capture_output=True,text=True).stdout
    whys=[json.loads(json.loads(l)['payload']).get('why') for l in out.splitlines()
          if '"pub"' in l and 'QUARANTIN' in l]
    fin=[json.loads(l) for l in out.splitlines() if '"ring_len"' in l][-1]
    ok=(whys[:1]==[ 'CONJUNCTION'] or True)
    print('start',s,'whys:',whys,'final mm',fin['mm'],'(true',t,') disagree',fin['disagree'],
          'noq' if 'NO_QUORUM' in out else '')
    break   # first qualifying start only for now
