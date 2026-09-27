"""Q6: how much reference error can the next magnetic encounter tolerate under a
>=70 / 2-sample same-sign opening rule with electrical rearm (30 in-band samples)?
Reference = (ordinary level just before the magnet, oracle last100) + e.  EVALUATION ONLY."""
import numpy as np, pickle, pandas as pd
from numba import njit
from scipy.ndimage import median_filter
from oracle import load
@njit(cache=True)
def openings(x, ref, thr, rearm_n):
    # detector starts DISARMED (it has just opened on the previous core, at its peak)
    idx=np.empty(64,np.int64); sg=np.empty(64,np.int64); n=0
    armed=False; inband=0; prev=0
    for i in range(len(x)):
        d=x[i]-ref
        s= 1 if d>thr else (-1 if d<-thr else 0)
        if armed:
            if s!=0 and s==prev:
                if n<64: idx[n]=i; sg[n]=s; n+=1
                armed=False; inband=0
        else:
            if s==0:
                inband+=1
                if inband>=rearm_n: armed=True
            else: inband=0
        prev=s
    return idx[:n],sg[:n]
ES=np.arange(-160,161,2)
rows=[]
I=pd.read_pickle('intervals_err.pkl')
for loco in ['otto','toby']:
    D=load(loco); O=pickle.load(open(f'{loco}_oracle.pkl','rb')); C=O['C']; t=D['t']
    x=median_filter(D['raw'],3,mode='nearest')
    g=I[I.loco==loco].set_index('k')
    for k in range(1,len(C)-1):
        if k-1 not in g.index: continue
        T=g.loc[k-1,'last100']
        if not np.isfinite(T): continue
        if g.loc[k-1,'stop'] or g.loc[k,'stop']: continue   # stops analysed separately
        c=C[k]; p=C[k-1]; from oracle import MARG; Lm,Tm=MARG[loco]
        w0=p['ipk']; w1=c['b']
        seg=x[w0:w1]; a=c['a']-w0; b=c['b']-w0; Wn=c['W']
        tt=t[w0:w1]-t[w0]
        prev_trail_end=np.searchsorted(tt,(t[p['hi']]-t[w0])+Tm*p['W'])
        lead_start=np.searchsorted(tt,(t[c['lo']]-t[w0])-Lm*c['W'])
        core_ok_start=np.searchsorted(tt,(t[c['a']]-t[w0])-Wn)
        sgn=1 if c['pol']=='N' else -1
        res=[]
        for e in ES:
            ix,s=openings(seg,T+e,70.0,30)
            fl=set(); hit=False
            for j in range(len(ix)):
                i=ix[j]
                if i<prev_trail_end: fl.add('SPLIT_PREV_TRAIL')
                elif i<lead_start: fl.add('FALSE_ORDINARY')
                elif i<core_ok_start: fl.add('LEAD_WRONGPOL' if s[j]!=sgn else 'LEAD_SAMEPOL')
                elif s[j]!=sgn: fl.add('WRONGPOL_AT_CORE')
                else: hit=True
            # core covered? an ideal structure-aware recogniser still needs the core itself to exceed the threshold
            d=(seg[a:b]-(T+e))*sgn
            if not (d>70).sum()>=2: fl.add('MISS')
            res.append(fl)
        onset={}
        z=np.where(ES==0)[0][0]
        for side,rng in (('pos',range(z,len(ES))),('neg',range(z,-1,-1))):
            for i in rng:
                for f in res[i]:
                    onset.setdefault((side,f),abs(ES[i]))
        rows.append(dict(loco=loco,k=k,pol=c['pol'],peak=abs(c['peak']),W=c['W'],**{f'{sd}_{f}':v for (sd,f),v in onset.items()}))
        continue
        res=np.array(res); z=np.where(ES==0)[0][0]
        if res[z]!='OK':
            rows.append(dict(loco=loco,k=k,pol=c['pol'],peak=abs(c['peak']),W=c['W'],e_lo=np.nan,e_hi=np.nan,fail_lo=res[z],fail_hi=res[z],t=t[c['a']]/1000)); continue
        hi=z
        while hi+1<len(ES) and res[hi+1]=='OK': hi+=1
        lo=z
        while lo-1>=0 and res[lo-1]=='OK': lo-=1
        # sign convention: e_core = e * sgn  (positive = reference moved TOWARD the core direction)
        rows.append(dict(loco=loco,k=k,pol=c['pol'],peak=abs(c['peak']),W=c['W'],e_lo=ES[lo],e_hi=ES[hi],
                         fail_lo=res[lo-1] if lo>0 else 'none',fail_hi=res[hi+1] if hi+1<len(ES) else 'none',t=t[c['a']]/1000))
R=pd.DataFrame(rows); R.to_pickle('tolerance_types.pkl')
fails=['FALSE_ORDINARY','SPLIT_PREV_TRAIL','LEAD_WRONGPOL','WRONGPOL_AT_CORE','LEAD_SAMEPOL','MISS']
for loco,g in R.groupby('loco'):
    print(f"\n{loco} (n={len(g)} magnets, no stops). Onset |e| of each failure type; TOWARD = reference displaced toward the core's polarity")
    for pol,h in g.groupby('pol'):
        for lab,side in (('TOWARD',('pos' if pol=='N' else 'neg')),('AWAY  ',('neg' if pol=='N' else 'pos'))):
            parts=[]
            for f in fails:
                c=f'{side}_{f}'
                if c in h: v=h[c].dropna(); 
                else: v=pd.Series(dtype=float)
                if len(v): parts.append(f"{f}: min {v.min():.0f} p1 {np.percentile(v,1):.0f} p50 {v.median():.0f} (n{len(v)})")
            print(f"  {pol} {lab}  "+" | ".join(parts))
