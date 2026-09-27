import numpy as np, pickle, pandas as pd
import replay
from oracle import load
def detail(loco, op, step=None, D=None):
    O=pickle.load(open(f'{loco}_oracle.pkl','rb')); C=O['C']; t=D['t']
    I=pd.read_pickle('intervals.pkl'); I=I[I.loco==loco].set_index('k')
    ca=np.array([c['a'] for c in C]); cb=np.array([c['b'] for c in C])
    matched=np.full(len(C),-1); extra=0; wp=0; errs={}
    for n,(idx,s,rf) in enumerate(op):
        k=np.searchsorted(ca,idx+300)-1
        if k>=0 and idx<=cb[k] and idx>=ca[k]-300 and matched[k]<0:
            matched[k]=n
            if s!=(1 if C[k]['pol']=='N' else -1): wp+=1
            if k-1 in I.index and np.isfinite(I.loc[k-1,'last100']):
                tr=I.loc[k-1,'last100']+(step[1] if step and t[ca[k]]>=step[0] else 0)
                errs[k]=rf-tr
        else: extra+=1
    return matched, extra, wp, errs
