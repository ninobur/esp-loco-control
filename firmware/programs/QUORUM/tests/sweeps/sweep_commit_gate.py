import subprocess, sys, json
NEW, PRE = sys.argv[1], sys.argv[2]
DNA=[1,1,0,0,0,1,1,0,0,0,0,1,1,1,1,0,0,1,0,0,0,1,0,1,1,1,0,0,1,1,1,1,1,0,1,1,0,0,0,0,
     0,0,1,0,1,0,0,0,1,1,0,1,0,0,0,0,1,0,0,1,0,1,0,1,0,1,1,0,0,1,0,1,0,1,1,0,1,1,1,0,
     1,1,1,1,0,0,0,1,1,0,1,1,0,0,1,0,1,1,0,0,1,0,0,1,0,0,0,1,1,1,1,1,1,1,0,1,0,0,1,1,
     1,0,0,0,1,0,1,1,0,1,0,1,1,0,0,1,1,0,0,0,0,0,1,0,1,1,1,1,0,1,0,1,1,1,0,1,0,1,0,0,
     1,0,1,0,0,0,0,1,1,1,0]
N=171
def pol(m): return 'N' if DNA[m%N] else 'S'
def ev(p,dt,pk=180,du=175,pwm=60): return f'event {dt} {p} {pwm} {pwm} {pk} {du} 0'
def build(start):
    t=start; cmds=['dir CW',f'declare {start}','auto 1']
    for _ in range(9): t=(t+1)%N; cmds.append(ev(pol(t),2200))
    b=t
    t=(t+1)%N; cmds.append(ev(pol(t),800,pk=40,du=60))   # genuine, damning credentials
    t=(t+1)%N; cmds.append(ev(pol(t),2200))              # the witness
    for _ in range(3): t=(t+1)%N; cmds.append(ev(pol(t),2200))
    return cmds+['dump'], t
def run(H,cmds): return subprocess.run([H],input='\n'.join(cmds),capture_output=True,text=True).stdout
hits=[]
for s in range(N):
    b=(s+9)%N
    if pol((b+1)%N)==pol(b) or pol((b+2)%N)==pol((b+1)%N): continue
    cmds,t=build(s)
    pre=run(PRE,cmds); new=run(NEW,cmds)
    pre_bug = ('QUARANTINE_COMMITTED' in pre and 'PHANTOM_REJECTED' in pre)
    new_ok  = ('QUARANTINE_COMMITTED' in new and 'PHANTOM_REJECTED' not in new
               and 'NO_QUORUM' not in new and f'"mm":{t},' in new.rsplit(chr(10),2)[-2]+new.rsplit(chr(10),2)[-1])
    if pre_bug and new_ok:
        finpre=[json.loads(l) for l in pre.splitlines() if '"ring_len"' in l][-1]
        finnew=[json.loads(l) for l in new.splitlines() if '"ring_len"' in l][-1]
        hits.append((s,t,finpre['mm'],finpre['disagree'],finnew['mm'],finnew['disagree']))
print(len(hits),'load-bearing starts (start, true_mm, pre_mm, pre_dis, new_mm, new_dis)')
for h in hits[:12]: print(h)
