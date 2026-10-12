"""Evaluate I_i > 1.85*I_{i-1} on real completed-pulse timestamps. Read-only analysis."""
import gzip, sys, statistics as st
def load_fixture(path):
    segs=[]; cur=[]; pb=None; ps=None
    for line in gzip.open(path,'rt'):
        b,s,t,p,v=line.split(); s=int(s); t=int(t); p=int(p)
        if v!='1': cur=[]; pb=None; continue
        if b!=pb or s!=ps+1:
            if len(cur)>1: segs.append(cur)
            cur=[]
        cur.append((s,t,p,None)); pb,ps=b,s
    if len(cur)>1: segs.append(cur)
    return segs
def load_events(path):
    segs=[]; cur=[]; pb=None; ps=None
    for line in open(path):
        ts,b,seq,c,t,iv,p,a,_,_=line.rstrip('\n').split('\t')
        s=int(c); t=int(t); p=None if p=='None' else int(p)
        if b!=pb or s!=ps+1:
            if len(cur)>1: segs.append(cur)
            cur=[]
        cur.append((s,t,p,ts)); pb,ps=b,s
    if len(cur)>1: segs.append(cur)
    return segs
def analyse(segs, label, verbose):
    trip=0; cand=[]; ratios=[]
    for seg in segs:
        iv=[(seg[k][1]-seg[k-1][1], seg[k][2], seg[k-1][2], seg[k][3], k) for k in range(1,len(seg))]
        for j in range(1,len(iv)):
            prev,cur=iv[j-1],iv[j]
            trip+=1
            r=cur[0]/prev[0]; ratios.append(r)
            if 100*cur[0]>185*prev[0]:
                nxt=iv[j+1][0] if j+1<len(iv) else None
                pre=[x[0] for x in iv[max(0,j-5):j]]
                med=st.median(pre)
                gated = cur[1] and cur[1]==prev[1] and cur[2]==cur[1]
                cand.append(dict(t=cur[3],prev=prev[0],cur=cur[0],nxt=nxt,r=r,med5=med,pwm=cur[1],gated=bool(gated),
                    stop=cur[0]>=1_000_000, ret=(nxt is not None and abs(nxt-prev[0])<=0.25*prev[0])))
    n=len(cand); g=[c for c in cand if c['gated']]
    print(f"== {label}: interval pairs {trip}, candidates >185% {n}, PWM-gated (nonzero, unchanged) {len(g)}")
    for name,sub in (('all',cand),('gated',g)):
        print(f"   [{name}] stop/start (>=1 s) {sum(c['stop'] for c in sub)}; next returns within 25% of prior {sum(c['ret'] for c in sub)}; "
              f"cur<1.5x median5 (driven by short prior) {sum(c['cur']<1.5*c['med5'] for c in sub)}; cur>=1.85x median5 {sum(c['cur']>=1.85*c['med5'] for c in sub)}")
    rs=sorted(ratios)
    if rs:
        q=lambda p: rs[min(len(rs)-1,int(p*len(rs)))]
        print(f"   ratio I_i/I_(i-1): p50 {q(.5):.3f} p90 {q(.9):.3f} p99 {q(.99):.3f} p99.9 {q(.999):.3f} max {rs[-1]:.2f}; count in [1.5,1.85] {sum(1.5<=x<=1.85 for x in rs)}")
    if verbose:
        for c in g[:verbose]:
            print(f"     {c['t']} pwm {c['pwm']} prev {c['prev']//1000} ms cur {c['cur']//1000} ms next {None if c['nxt'] is None else c['nxt']//1000} ms ratio {c['r']:.2f} med5 {c['med5']/1000:.0f} ms")
    return cand
def merged(segs,label):
    # Characterisation only: merge two real adjacent intervals as one uncounted cycle would.
    tot=det=det_iso=0
    for seg in segs:
        iv=[(seg[k][1]-seg[k-1][1],seg[k][2],seg[k-1][2]) for k in range(1,len(seg))]
        for j in range(1,len(iv)-2):
            p=iv[j-1]
            if not(p[1] and all(iv[x][1]==p[1] for x in range(j-1,j+3))): continue
            if any(iv[x][0]>=500_000 for x in range(j-1,j+3)): continue
            tot+=1; m=iv[j][0]+iv[j+1][0]; nx=iv[j+2][0]
            if 100*m>185*p[0]:
                det+=1
                if abs(nx-p[0])<=0.25*p[0]: det_iso+=1
    print(f"   synthetic merge (NOT proof): eligible steady positions {tot}, merged ratio >1.85 {det} ({100*det/max(tot,1):.1f}%), also next within 25% {det_iso} ({100*det_iso/max(tot,1):.1f}%)")
if __name__=='__main__':
    kind,path,label=sys.argv[1:4]; v=int(sys.argv[4]) if len(sys.argv)>4 else 0
    segs=load_fixture(path) if kind=='fx' else load_events(path)
    analyse(segs,label,v); merged(segs,label)
