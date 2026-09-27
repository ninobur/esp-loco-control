"""Causal replay of an EYES_WIDE_OPEN-style chain.  DIAGNOSTIC ONLY - not operational code.
Opening: >=70 vs frozen ref, 2 consecutive same-sign (median-of-3) samples. Encounter closes (rearm) after
30 consecutive in-band samples (a stand-in; EWO's RTB is undefined). Interval j = samples between opening j and
opening j+1. When encounter j+1's core has completed (its first return in-band), interval j is segmented
retrospectively and its reference is used from the NEXT opening onward (one completed interval of separation).
Segmentation: all regions with |med5 - A| > 60 are magnetic; each widened by lobe margins (W-scaled, from the
passage study); A = frozen ref ('anchored') or the interval's own median ('self'). Median of the rest -> ref.
If < minN samples remain, the old ref is kept."""
import numpy as np, pickle, pandas as pd, sys
from numba import njit
from scipy.ndimage import median_filter
from oracle import load, MARG
@njit(cache=True)
def step_detect(x, i0, ref, thr, rearm_n):
    """scan from i0 (armed) until an opening; return (open_idx, sign) or (-1,0)"""
    prev=0
    for i in range(i0,len(x)):
        d=x[i]-ref; s=1 if d>thr else (-1 if d<-thr else 0)
        if s!=0 and s==prev: return i,s
        prev=s
    return -1,0
@njit(cache=True)
def scan_close(x, i0, ref, thr, rearm_n):
    inb=0
    for i in range(i0,len(x)):
        if abs(x[i]-ref)<=thr:
            inb+=1
            if inb>=rearm_n: return i
        else: inb=0
    return len(x)-1
def segment(r5, t, lo, hi, A, loco, minN, pwm=None, pwm_floor=None):
    seg=r5[lo:hi]; tt=t[lo:hi]
    if A is None: A=np.median(seg)
    dev=seg-A; mag=np.abs(dev)>60
    keep=np.ones(len(seg),bool)
    if mag.any():
        m=np.concatenate([[0],mag.astype(np.int8),[0]]); d=np.diff(m); st=np.where(d==1)[0]; en=np.where(d==-1)[0]
        Lm,Tm=MARG[loco]
        for a,b in zip(st,en):
            i=a+np.argmax(np.abs(dev[a:b])); pk=dev[i]; sg=np.sign(pk); thr=0.5*abs(pk)
            l=i; h=i
            while l>0 and sg*dev[l-1]>thr: l-=1
            while h<len(seg)-1 and sg*dev[h+1]>thr: h+=1
            W=tt[h]-tt[l]+1
            keep&=~((tt>=tt[l]-Lm*W)&(tt<=tt[h]+Tm*W))
            keep[a:b]=False
    if pwm is not None: keep&=(pwm[lo:hi]>=pwm_floor)
    if keep.sum()<minN: return None, int(keep.sum())
    return float(np.median(seg[keep])), int(keep.sum())
def run(loco, mode='anchored', minN=100, inject=None, step=None, pwm_floor=None, ref0=None, rtb_band=70.0, rtb_n=30, stuck=None):
    D=load(loco); t=D['t']; raw=D['raw'].copy()
    if step is not None:
        ts,delta=step; raw[t>=ts]+=delta
    x=median_filter(raw,3,mode='nearest'); r5=median_filter(raw,5,mode='nearest')
    ref=float(np.median(raw[:2000])) if ref0 is None else ref0     # boot: first 2 s
    opens=[]; refs=[]; kept=[]; i=0; pending=None; j=0; fallbacks=[]
    while True:
        o,s=step_detect(x,i,ref,70.0,30)
        if o<0: break
        opens.append((o,s,ref))
        c=scan_close(x,o,ref,rtb_band,rtb_n)
        if stuck is not None: stuck.append((o,c))
        # interval between previous opening and this one is now complete (its closing core = this encounter)
        if len(opens)>=2:
            lo=opens[-2][0]; hi=c                   # include this encounter's core for W estimation
            A=None if mode=='self' else ref
            nr,n=segment(r5,t,lo,hi,A,loco,minN,D['pwm'] if pwm_floor else None,pwm_floor)
            if nr is None and mode=='hybrid':
                # anchored found no ordinary track: re-derive from the interval itself, cruise samples only
                nr,n=segment(r5,t,lo,hi,None,loco,minN,D['pwm'],70)
                if nr is not None: fallbacks.append(o)
            kept.append(n)
            if nr is not None: ref=nr
        if inject is not None and len(opens)==inject[0]: ref+=inject[1]
        i=c+1
    run.fallbacks=fallbacks
    return D,opens
def score(loco, opens, D=None):
    O=pickle.load(open(f'{loco}_oracle.pkl','rb')); C=O['C']
    I=pd.read_pickle('intervals.pkl'); I=I[I.loco==loco].set_index('k')
    oi=np.array([o[0] for o in opens]); os_=np.array([o[1] for o in opens]); orf=np.array([o[2] for o in opens])
    ca=np.array([c['a'] for c in C]); cb=np.array([c['b'] for c in C]); cp=np.array([1 if c['pol']=='N' else -1 for c in C])
    # attribute each opening to the oracle core whose [a-300, b] contains it
    matched=np.zeros(len(C),int); wrongpol=0; extra=0; refe=[]
    for idx,s,rf in zip(oi,os_,orf):
        k=np.searchsorted(ca,idx+300)-1
        if k>=0 and idx<=cb[k] and idx>=ca[k]-300:
            if matched[k]: extra+=1
            else:
                matched[k]=1
                if s!=cp[k]: wrongpol+=1
                if k-1 in I.index and np.isfinite(I.loc[k-1,'last100']): refe.append(rf-I.loc[k-1,'last100'])
        else: extra+=1
    refe=np.abs(np.array(refe))
    return dict(oracle=len(C),openings=len(oi),missed=int((matched==0).sum()),extra=extra,wrongpol=wrongpol,
                ref_p50=np.median(refe),ref_p99=np.percentile(refe,99),ref_max=refe.max(),n_gt30=int((refe>30).sum()))
if __name__=='__main__':
    res=[]
    for loco in ['otto','toby']:
        for mode in ['anchored','self']:
            for pf in [None, 30]:
                D,op=run(loco,mode,pwm_floor=pf); sc=score(loco,op); sc.update(loco=loco,mode=mode,pwm_floor=pf,scenario='baseline'); res.append(sc); print(sc,flush=True)
    pd.DataFrame(res).to_pickle('replay_base.pkl')
