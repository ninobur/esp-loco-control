import numpy as np, pickle, pandas as pd
from oracle import load, MARG
rows=[]
for loco in ['otto','toby']:
    D=load(loco); O=pickle.load(open(f'{loco}_oracle.pkl','rb')); C=O['C']; t=D['t']; r=D['raw']; pwm=D['pwm']
    Lm,Tm=MARG[loco]
    for k in range(len(C)-1):
        a,b=C[k],C[k+1]
        z0=max(t[a['hi']]+Tm*a['W'],t[a['b']-1]); z1=min(t[b['lo']]-Lm*b['W'],t[b['a']])
        i0=np.searchsorted(t,z0,'right'); i1=np.searchsorted(t,z1,'left')
        g0=a['b']; g1=b['a']                           # whole gap between cores
        seg=r[i0:i1] if i1>i0 else np.array([])
        stop=(pwm[g0:g1]==0).any()
        R=dict(loco=loco,k=k,t=t[b['a']]/1000,gap_ms=t[g1]-t[g0],gap_W=(t[g1]-t[g0])/((a['W']+b['W'])/2),clean_n=len(seg),
               clean_W=(len(seg)/((a['W']+b['W'])/2)),stop=stop,Wa=a['W'],Wb=b['W'],pol_next=b['pol'],peak_next=b['peak'],
               C=np.median(seg) if len(seg)>=30 else np.nan,
               first100=np.median(seg[:100]) if len(seg)>=100 else np.nan,
               last100=np.median(seg[-100:]) if len(seg)>=100 else np.nan,
               MAD=np.median(np.abs(seg-np.median(seg))) if len(seg)>=30 else np.nan,
               gapmed=np.median(r[g0:g1]) if g1>g0 else np.nan,
               base_next=int(D['base'][b['a']]), pwm_next=int(pwm[b['a']]))
        if loco=='otto': R['mm']=int(D['mm'][b['a']]); R['ph']=int(D['ph'][b['a']])
        rows.append(R)
I=pd.DataFrame(rows); I.to_pickle('intervals.pkl')
pd.set_option('display.width',250)
for loco,g in I.groupby('loco'):
    print(loco,'intervals',len(g),'with stop',g.stop.sum(),'clean_n==0',(g.clean_n==0).sum(),'clean<100',(g.clean_n<100).sum())
    print('  gap_W p1/p5/p50/p95',np.percentile(g.gap_W,[1,5,50,95]).round(1),' clean_W p1/p5/p50',np.nanpercentile(g.clean_W,[1,5,50]).round(1))
    print('  clean_n p1/p5/p50',np.percentile(g.clean_n,[1,5,50]).round(0),' MAD p50/p95/max',np.nanpercentile(g.MAD,[50,95,100]))
