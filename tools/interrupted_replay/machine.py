#!/usr/bin/env python3
"""Interrupted-traversal rule -- proposed state machine, replayed.
ANALYSIS ONLY. Reads captured waveforms. Modifies no firmware."""
import os, math

ENTRY_MARGIN=38; EXIT_MARGIN=25; EXIT_HOLD_MS=8
AMP_FLOOR=0.34; RESID_CEIL=0.13; GUARD_MS=200
SETTLE_BAND=8; SETTLE_MS=400          # the two new constants, Hall-only
BASE=os.path.expanduser("~/ngr-telemetry/waveforms")

def load(rel):
    h={};v=[]
    for line in open(os.path.join(BASE,rel)):
        line=line.strip()
        if line.startswith('#'): k,x=line[1:].split(',',1); h[k]=x
        elif line and not line.startswith('sample'): v.append(int(line.split(',')[1]))
    return h,v

def capture_stream(rel):
    h,v=load(rel); dec=int(h['decimation']); pol=int(h['polarity'])
    gain=round(int(h['peakCounts'])/float(h['amplitudeRatio']))
    sign=1 if pol else -1
    s=[]
    for x in v: s.extend([sign*x]*dec)
    return [0]*200+s+[0]*400, gain

def residual(seg):
    """Same shape as fitResidual(): least-squares-amplitude Gaussian over the
    arch above 20% of peak, normalised by peak."""
    y=[abs(x) for x in seg]
    if len(y)<8: return 1.0
    pk=max(y); 
    if pk<=0: return 1.0
    i=y.index(pk); th=0.2*pk
    a=i
    while a>0 and y[a-1]>=th: a-=1
    b=i
    while b<len(y)-1 and y[b+1]>=th: b+=1
    w=y[a:b+1]
    if len(w)<8: return 1.0
    n=len(w); tot=sum(w)
    c=sum(k*w[k] for k in range(n))/tot
    var=sum(w[k]*(k-c)**2 for k in range(n))/tot
    s=math.sqrt(max(var,1e-6))
    g=[math.exp(-0.5*((k-c)/s)**2) for k in range(n)]
    den=sum(x*x for x in g)
    A=sum(w[k]*g[k] for k in range(n))/den if den else 0
    err=sum((w[k]-A*g[k])**2 for k in range(n))/n
    return math.sqrt(err)/pk

class Machine:
    def __init__(self,gain): self.gain=gain; self.adv=[]; self.log=[]

    def interrupted_evidence(self,seg,label):
        """No shape. Entry into a changing field, amplitude, polarity, guard."""
        if not seg: return False,dict(label=label,reason="no changing field to judge")
        start=abs(seg[0]); pk=max(abs(x) for x in seg); growth=pk-start
        ratio=pk/self.gain; pole=1 if sum(seg)>=0 else 0
        contra=max((abs(x) for x in seg if (x>=0)!=(pole==1)),default=0)
        d=dict(label=label,start=start,peak=pk,growth=growth,ratio=round(ratio,3),
               pole="N" if pole else "S",contra=contra)
        if growth<ENTRY_MARGIN: d["reason"]="REJECT no entry into the field (growth %d < %d)"%(growth,ENTRY_MARGIN); return False,d
        if ratio<AMP_FLOOR:     d["reason"]="REJECT amplitude %.3f < floor %.2f"%(ratio,AMP_FLOOR); return False,d
        if contra>EXIT_MARGIN:  d["reason"]="REJECT contradictory excursion %d"%contra; return False,d
        d["reason"]="ACCEPT magnet established (shape not tested)"; return True,d

    def complete_evidence(self,seg,label):
        """The unchanged recognizer: amplitude AND Gaussian shape."""
        pk=max(abs(x) for x in seg); ratio=pk/self.gain; r=residual(seg)
        pole=1 if sum(seg)>=0 else 0
        d=dict(label=label,start=abs(seg[0]),peak=pk,growth=pk-abs(seg[0]),
               ratio=round(ratio,3),pole="N" if pole else "S",contra=0,resid=round(r,4))
        if ratio<AMP_FLOOR: d["reason"]="REJECT amplitude %.3f"%ratio; return False,d
        if r>RESID_CEIL:    d["reason"]="REJECT shape %.4f > %.2f"%(r,RESID_CEIL); return False,d
        d["reason"]="ACCEPT complete traversal (resid %.4f)"%r; return True,d

    def run(self,stream):
        st="TRAVERSING"; seg=None; open_t=0; ref=None; since=0; quiet=None; occ=0
        for t,d in enumerate(stream):
            mag=abs(d)
            if ref is None or abs(d-ref)>SETTLE_BAND: ref=d; since=t
            settled=(t-since)>=SETTLE_MS

            if st=="TRAVERSING":
                if seg is None:
                    if mag>=ENTRY_MARGIN: seg=[d]; open_t=t; quiet=None
                    continue
                seg.append(d)
                if settled:
                    pre=seg[:max(0,since-open_t)]           # dwell samples excluded
                    ok,info=self.interrupted_evidence(pre,"pre-stop")
                    self.log.append(info); occ=ref
                    if ok: self.adv.append("pre-stop"); st="COUNTED"
                    else:  st="PENDING"
                    seg=[d]; continue
                if mag<EXIT_MARGIN:
                    quiet=quiet if quiet is not None else t
                    if t-quiet>=EXIT_HOLD_MS:
                        ok,info=self.complete_evidence(seg,"traversal")
                        self.log.append(info)
                        if ok: self.adv.append("traversal")
                        seg=None; quiet=None; ref=None
                else: quiet=None
                continue

            # COUNTED / PENDING -- nothing counts until this field clears
            seg=(seg or []); seg.append(d)
            if mag<EXIT_MARGIN:
                quiet=quiet if quiet is not None else t
                if t-quiet>=EXIT_HOLD_MS:
                    if st=="PENDING":
                        ok,info=self.interrupted_evidence(seg,"departure")
                        self.log.append(info)
                        if ok: self.adv.append("departure")
                    else:
                        self.log.append(dict(label="departure",
                            reason="SUPPRESSED -- this field was already counted"))
                    st="TRAVERSING"; seg=None; quiet=None; ref=None
            else: quiet=None
        return self.adv

def report(title,stream,gain,want):
    m=Machine(gain); adv=m.run(stream)
    print("\n%s"%title)
    for d in m.log:
        if "start" in d:
            print("   %-9s start %4d peak %4d growth %4d ratio %.3f pole %s  %s"
                  %(d["label"],d["start"],d["peak"],d["growth"],d["ratio"],d["pole"],d["reason"]))
        else:
            print("   %-9s %s"%(d["label"],d["reason"]))
    n=len(adv); good=(n==want)
    print("   ADVANCES: %d   wanted %d   %s"%(n,want,"PASS" if good else "*** FAIL ***"))
    return good
