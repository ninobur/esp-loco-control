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
            out='OK'; hit=False
            for j in range(len(ix)):
                i=ix[j]
                if i<prev_trail_end: out='PREV_TRAIL_SPLIT'; break
                if i<lead_start: out='FALSE_ORDINARY'; break
                if i<core_ok_start: out='LEAD_WRONGPOL' if s[j]!=sgn else 'LEAD_SAMEPOL_EARLY'; break
                if s[j]!=sgn: out='WRONGPOL'; break
                if hit: out='EXTRA_IN_CORE'; break
                hit=True
            if out=='OK' and not hit: out='MISS'
            res.append(out)
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
R=pd.DataFrame(rows); R.to_pickle('tolerance.pkl')
for loco,g in R.groupby('loco'):
    bad=g[g.e_lo.isna()]
    print(f"\n{loco}: magnets tested {len(g)}; not OK even at e=0: {len(bad)} {bad.fail_lo.value_counts().to_dict()}")
    g=g.dropna(subset=['e_lo'])
    for pol,h in g.groupby('pol'):
        # toward-core = positive for N means ref higher -> e_hi side is ref toward core for N; for S ref lower (e_lo) is toward core
        tc = h.e_hi if pol=='N' else -h.e_lo      # tolerance with reference displaced TOWARD the core
        ac = -h.e_lo if pol=='N' else h.e_hi      # tolerance with reference displaced AWAY from the core (toward the lobe side)
        f_tc = h.fail_hi if pol=='N' else h.fail_lo
        f_ac = h.fail_lo if pol=='N' else h.fail_hi
        print(f"  {pol} n={len(h)}  ref displaced TOWARD core: tolerance min {tc.min():.0f} p1 {np.percentile(tc,1):.0f} p5 {np.percentile(tc,5):.0f} p50 {tc.median():.0f}  first failure {f_tc.value_counts().to_dict()}")
        print(f"  {pol} n={len(h)}  ref displaced AWAY from core: tolerance min {ac.min():.0f} p1 {np.percentile(ac,1):.0f} p5 {np.percentile(ac,5):.0f} p50 {ac.median():.0f}  first failure {f_ac.value_counts().to_dict()}")
