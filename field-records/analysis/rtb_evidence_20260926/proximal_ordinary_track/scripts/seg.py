"""Magnet-core segmentation for the proximal ordinary-track analysis.

Cores: contiguous |rolling-median-5(raw) - passage median| > 60 counts with peak >= 100.
The passage median is used only to locate cores; boundaries move <= ~20 ms for +/-15
counts of reference error. W = width at half the core peak (speed-free spatial unit).
"""
import pandas as pd, numpy as np, glob, json
from numpy.lib.stride_tricks import sliding_window_view as sw
import os
_PKG=os.environ.get('RTB_PKG',os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','..'))
P=os.path.join(_PKG,'passages','')
OUT=os.environ.get('RTB_OUT',os.path.join(os.path.dirname(os.path.abspath(__file__)),'..','results'))
os.makedirs(OUT,exist_ok=True)
MAN=json.load(open(os.path.join(_PKG,'manifest.json')))
meta={m['file'].split('/')[-1][:-4]:m for m in MAN['passages']}
def rmed(x,k):
    h=k//2; return np.median(sw(np.pad(x,h,mode='edge'),k),axis=1)
def runs(mask):
    m=np.concatenate([[0],mask.astype(int),[0]]); d=np.diff(m)
    return list(zip(np.where(d==1)[0],np.where(d==-1)[0]))  # [a,b)
CORE_T=60; MERGE=30; PEAK_MIN=100
def load(n):
    d=pd.read_csv(P+n+'.csv'); d['t']=(d.t_ms-d.t_ms.iloc[0]).astype(float); return d
def events(d, ref=None):
    r=d.raw.values.astype(float); R0=np.median(r) if ref is None else ref
    s5=rmed(r,5); dev=s5-R0
    rs=runs(np.abs(dev)>CORE_T)
    mg=[]
    for a,b in rs:
        if mg and a-mg[-1][1]<MERGE and np.sign(dev[a])==np.sign(dev[mg[-1][0]]): mg[-1]=(mg[-1][0],b)
        else: mg.append((a,b))
    ev=[]
    for a,b in mg:
        i=a+np.argmax(np.abs(dev[a:b])); pk=dev[i]
        if abs(pk)<PEAK_MIN: continue
        sgn=np.sign(pk); hm=sgn*dev>0.5*abs(pk)
        lo=i
        while lo>0 and hm[lo-1]: lo-=1
        hi=i
        while hi<len(r)-1 and hm[hi+1]: hi+=1
        t=d.t.values
        ev.append(dict(a=a,b=b,ipk=i,peak=pk,pol='N' if pk>0 else 'S',hm_lo=lo,hm_hi=hi,
                       W=t[hi]-t[lo]+1,c=(t[hi]+t[lo])/2,partial=(a==0 or b>=len(r)-1),
                       t_a=t[a],t_b=t[b-1]))
    return ev,R0
if __name__=='__main__':
    for f in sorted(glob.glob(P+'*.csv')):
        n=f.split('/')[-1][:-4]; d=load(n); ev,R0=events(d)
        evp,_=events(d,R0+15); evm,_=events(d,R0-15)
        print(n,'R0',R0)
        for e in ev:
            # sensitivity of boundaries
            def near(L): 
                c=[x for x in L if abs(x['c']-e['c'])<100]; return c[0] if c else None
            p,m=near(evp),near(evm)
            sh=[(p['t_a']-e['t_a'],p['t_b']-e['t_b']) if p else None,(m['t_a']-e['t_a'],m['t_b']-e['t_b']) if m else None]
            print(f"   {e['pol']} peak={e['peak']:+.0f} core=[{e['t_a']:.0f},{e['t_b']:.0f}] ({e['t_b']-e['t_a']:.0f}ms) W50={e['W']:.0f}ms c={e['c']:.0f} partial={e['partial']} pwm={d.pwm_actual.iloc[e['ipk']]} shift(+15,-15)={sh}")
