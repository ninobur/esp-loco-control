"""Offline (non-causal) oracle segmentation of full sessions, for EVALUATION ONLY.
Cores: |med5(raw) - L| > 60 with peak >= 100, L = 4001-sample rolling median (1 kHz).
W: half-max width. Lobe exclusion margins from the passage analysis (x1.0)."""
import numpy as np
from scipy.ndimage import median_filter
MARG={'otto':(3.0,4.0),'toby':(3.5,3.0)}
def runs(mask):
    m=np.concatenate([[0],mask.astype(np.int8),[0]]); d=np.diff(m)
    return np.where(d==1)[0],np.where(d==-1)[0]
def load(loco):
    if loco=='otto':
        Z=np.load('otto_full.npz'); t=(Z['t_us']-Z['t_us'][0])/1000.0
        return dict(t=t,raw=Z['raw'].astype(float),pwm=Z['pwm'],base=Z['base'],mm=Z['mm'],ph=Z['ph'])
    Z=np.load('toby_full.npz'); t=(Z['t_ms']-Z['t_ms'][0]).astype(float)
    return dict(t=t,raw=Z['raw'].astype(float),pwm=Z['pwm'],base=Z['base'],act=Z['act'],pole=Z['pole'])
def cores(D, L=None):
    r=D['raw']; s5=median_filter(r,5,mode='nearest')
    if L is None: L=median_filter(r,4001,mode='nearest')
    dev=s5-L; a,b=runs(np.abs(dev)>60)
    # merge same-sign gaps < 30 samples
    A=[];B=[]
    for x,y in zip(a,b):
        if A and x-B[-1]<30 and np.sign(dev[x])==np.sign(dev[A[-1]]): B[-1]=y
        else: A.append(x);B.append(y)
    out=[]
    t=D['t']
    for x,y in zip(A,B):
        i=x+np.argmax(np.abs(dev[x:y])); pk=dev[i]
        if abs(pk)<100: continue
        sg=np.sign(pk); thr=0.5*abs(pk); lo=i; hi=i
        while lo>0 and sg*dev[lo-1]>thr: lo-=1
        while hi<len(r)-1 and sg*dev[hi+1]>thr: hi+=1
        out.append(dict(a=x,b=y,ipk=i,peak=pk,pol='N' if pk>0 else 'S',lo=lo,hi=hi,W=t[hi]-t[lo]+1))
    return out,L,dev
if __name__=='__main__':
    import sys, pickle
    for loco in ['otto','toby']:
        D=load(loco); C,L,dev=cores(D)
        W=np.array([c['W'] for c in C]); pk=np.array([abs(c['peak']) for c in C])
        print(loco,'cores',len(C),'W p5/p50/p95',np.percentile(W,[5,50,95]).round(0),'|peak| min/p1/p50',pk.min(),np.percentile(pk,1).round(0),np.median(pk))
        pickle.dump(dict(C=C,L=L),open(f'{loco}_oracle.pkl','wb'))
