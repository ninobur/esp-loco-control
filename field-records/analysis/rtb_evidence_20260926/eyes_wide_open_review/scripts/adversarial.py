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
base={}
for loco in ['otto','toby']:
    D,op=replay.run(loco,'anchored'); m,e,w,_=detail(loco,op,D=D); base[loco]=((m<0).sum(),e,w)
print('baseline anchored (miss,extra,wrongpol):',base)
print('\n== single bad reference injected after opening N (anchored). delta vs baseline; recovery = matched magnets after injection with |ref err|>5')
for loco,Ns in [('otto',[300,900]),('toby',[300,700])]:
    for N in Ns:
        for e0 in [30,45,60,80,120,-45,-60,-80,-120]:
            D,op=replay.run(loco,'anchored',inject=(N,e0)); m,ex,wp,errs=detail(loco,op,D=D)
            ks=sorted(k for k in errs if m[k]>N)
            bad=[k for k in ks if abs(errs[k])>5]
            # consecutive bad after injection
            run=0
            for k in ks:
                if abs(errs[k])>5: run+=1
                else: break
            print(f"  {loco} N={N} e0={e0:+4d}: miss {int((m<0).sum())-base[loco][0]:+d} extra {ex-base[loco][1]:+d} wrongpol {wp-base[loco][2]:+d} | magnets judged with |err|>5: {len(bad)} (first consecutive run {run}) | total openings {len(op)}")
