import os, numpy as np, pandas as pd, sys
import seg, segs
from seg import runs
# exclusion margins (in W from half-max edges), derived from pooled lobe profiles (settle.py)
MARG={'otto':(3.0,4.0),'toby':(3.5,3.0)}
def theilsen(t,y,maxn=400):
    if len(t)>maxn: idx=np.linspace(0,len(t)-1,maxn).astype(int); t=t[idx]; y=y[idx]
    i,j=np.triu_indices(len(t),1); dt=t[j]-t[i]; ok=dt>0
    return np.median((y[j]-y[i])[ok]/dt[ok])
def build(scale=1.0):
    out=[]
    for loco,name,d in segs.segments():
        ev,R0=seg.events(d); t=d.t.values; r=d.raw.values.astype(float)
        Ws=[e['W'] for e in ev if not e['partial']]
        Wref=np.median(Ws) if Ws else 100
        L,T=MARG[loco]; L*=scale; T*=scale
        ex=np.zeros(len(t),bool); zones=[]
        for e in ev:
            W=e['W'] if not e['partial'] else max(e['W'],Wref)
            z0=min(t[e['hm_lo']]-L*W,e['t_a']); z1=max(t[e['hm_hi']]+T*W,e['t_b'])
            if e['partial'] and e['a']==0: z0=-1
            if e['partial'] and e['b']>=len(t)-1: z1=1e9
            ex|=(t>=z0)&(t<=z1); zones.append((z0,z1,e,W))
        ivs=[]
        for a,b in runs(~ex):
            if t[b-1]-t[a]<30: continue
            prev=[z for z in zones if z[1]<=t[a]+1]; nxt=[z for z in zones if z[0]>=t[b-1]-1]
            ivs.append(dict(a=a,b=b,prev=prev[-1] if prev else None,next=nxt[0] if nxt else None))
        out.append(dict(loco=loco,name=name,d=d,ev=ev,ivs=ivs,ex=ex,Wref=Wref))
    return out
def ivstats(S,iv):
    d=S['d']; t=d.t.values[iv['a']:iv['b']]; r=d.raw.values[iv['a']:iv['b']].astype(float)
    n=len(r); med=np.median(r); mad=np.median(np.abs(r-med))
    p=np.percentile(r,[1,5,25,75,95,99])
    th=n//3; dmed=np.median(r[-th:])-np.median(r[:th]) if th>=20 else np.nan
    sl=theilsen(t/1000,r)
    # block medians (100 samples = one Otto comb period)
    nb=n//100; bm=np.array([np.median(r[i*100:(i+1)*100]) for i in range(nb)])
    step=np.max(np.abs(np.diff(bm))) if nb>=2 else np.nan
    brange=bm.max()-bm.min() if nb>=2 else np.nan
    # curvature: residual of block medians from their straight line (max |dev|)
    if nb>=3:
        x=np.arange(nb); c=np.polyfit(x,bm,1); curv=np.max(np.abs(bm-np.polyval(c,x)))
    else: curv=np.nan
    Wn=[z[3] for z in (iv['prev'],iv['next']) if z]; Wu=np.mean(Wn) if Wn else S['Wref']
    kind=('bounded' if iv['prev'] and iv['next'] else 'pre' if iv['next'] else 'post' if iv['prev'] else 'free')
    return dict(loco=S['loco'],seg=S['name'].split(':')[1][:24],kind=kind,t0=t[0],t1=t[-1],n=n,dur=t[-1]-t[0],len_W=(t[-1]-t[0])/Wu,
                pwm=int(np.median(d.pwm_actual.values[iv['a']:iv['b']])),median=med,MAD=mad,p5=p[1],p95=p[4],p1=p[0],p99=p[5],
                spread5_95=p[4]-p[1],dmed_thirds=dmed,slope_cps=sl,slope_perW=sl*Wu/1000,blk_range=brange,blk_maxstep=step,blk_curv=curv,
                rec_base=int(np.median(d.baseline.values[iv['a']:iv['b']])),
                next_pol=iv['next'][2]['pol'] if iv['next'] else '',prev_pol=iv['prev'][2]['pol'] if iv['prev'] else '')
if __name__=='__main__':
    sc=float(sys.argv[1]) if len(sys.argv)>1 else 1.0
    S=build(sc); rows=[ivstats(s,iv) for s in S for iv in s['ivs']]
    R=pd.DataFrame(rows); R.to_csv(os.path.join(seg.OUT,f'intervals_x{sc}.csv'),index=False)
    pd.set_option('display.width',300); pd.set_option('display.max_columns',40)
    print(R.round(2).to_string())
