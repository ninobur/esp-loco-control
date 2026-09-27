import numpy as np, pickle, pandas as pd
from scipy.ndimage import median_filter
from oracle import load, MARG
import replay
for loco in ['otto','toby']:
    D=load(loco); t=D['t']; r5=median_filter(D['raw'],5,mode='nearest')
    C=pickle.load(open(f'{loco}_oracle.pkl','rb'))['C']
    I=pd.read_pickle('intervals.pkl'); I=I[I.loco==loco].set_index('k')
    Lm,Tm=MARG[loco]; ea=[]; eb=[]; ec=[]
    for k in range(1,len(C)-2,3):
        if I.loc[k-1,'stop'] or I.loc[k,'stop'] or k+1 not in I.index: continue
        truth=I.loc[k,'last100']
        if not np.isfinite(truth): continue
        lo=C[k-1]['a']; hi=C[k+1]['b']          # encounter k-1 .. encounter k+1, magnet k missed
        ref=I.loc[k-2,'last100'] if k-2 in I.index else truth
        a,_=replay.segment(r5,t,lo,hi,ref,loco,100)          # masks ALL magnetic structure (anchored)
        # boundary-only: mask only first/last encounters (+ lobes), keep the missed magnet k in 'ordinary'
        tt=t[lo:hi]; seg=r5[lo:hi]; keep=np.ones(len(seg),bool)
        for c in (C[k-1],C[k+1]):
            keep&=~((tt>=t[c['lo']]-Lm*c['W'])&(tt<=t[c['hi']]+Tm*c['W']))
        b=np.median(seg[keep])
        # boundary-only but magnet k removed by threshold only (no lobe margins)
        keep2=keep&(np.abs(seg-ref)<=60); c2=np.median(seg[keep2])
        ea.append(a-truth); eb.append(b-truth); ec.append(c2-truth)
    for lab,e in [('mask all structure (anchored)',ea),('mask boundary encounters only',eb),('boundary + threshold, no lobe margins',ec)]:
        e=np.abs(np.array(e)); print(f"{loco} missed-MM merged interval, {lab:40s} n={len(e)} p50 {np.median(e):.1f} p99 {np.percentile(e,99):.1f} max {e.max():.1f}")
