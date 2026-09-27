import numpy as np, pickle, pandas as pd, sys
import replay
from oracle import load
def explain(loco, mode='anchored', **kw):
    D,op=replay.run(loco,mode,**kw); t=D['t']; pwm=D['pwm']
    O=pickle.load(open(f'{loco}_oracle.pkl','rb')); C=O['C']
    ca=np.array([c['a'] for c in C]); cb=np.array([c['b'] for c in C])
    matched=np.zeros(len(C),int); out=[]
    for idx,s,rf in op:
        k=np.searchsorted(ca,idx+300)-1
        ok = k>=0 and idx<=cb[k] and idx>=ca[k]-300
        if ok and not matched[k]:
            matched[k]=1
            if s!=(1 if C[k]['pol']=='N' else -1): out.append(('WRONGPOL',t[idx]/1000,int(pwm[idx]),rf,float(D['raw'][idx])))
        else: out.append(('EXTRA',t[idx]/1000,int(pwm[idx]),rf,float(D['raw'][idx])))
    for k in np.where(matched==0)[0]: out.append(('MISS',t[C[k]['a']]/1000,int(pwm[C[k]['a']]),np.nan,float(C[k]['peak'])))
    # data gaps near event (toby)
    for o in sorted(out,key=lambda z:z[1]):
        i=np.searchsorted(t,o[1]*1000); w=t[max(0,i-3000):i+3000]; gap=np.max(np.diff(w)) if len(w)>1 else 0
        print(f"  {o[0]:9s} t={o[1]:8.1f}s pwm={o[2]:3d} ref={o[3]:7.1f} raw/peak={o[4]:7.1f}  max_dt_nearby={gap:.0f}ms")
for loco,mode in [('otto','anchored'),('toby','anchored'),('otto','self')]:
    print(loco,mode); explain(loco,mode)
