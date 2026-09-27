import os, numpy as np, pandas as pd
import seg, segs
from seg import rmed
rows=[]; bins=np.arange(0,8.01,0.25); pooled={}
for loco,name,d in segs.segments():
    ev,R0=seg.events(d); t=d.t.values; r=d.raw.values.astype(float); s101=rmed(r,101); s41=rmed(r,41)
    for k,e in enumerate(ev):
        sg=1 if e['pol']=='N' else -1
        for side in ['lead','trail']:
            if side=='lead': edge=t[e['hm_lo']]; lim=ev[k-1]['t_b'] if k>0 else 0.0; nbr=k>0
            else: edge=t[e['hm_hi']]; lim=ev[k+1]['t_a'] if k+1<len(ev) else t[-1]; nbr=k+1<len(ev)
            x_all=np.abs(t-edge)
            inside=((t<edge)&(t>=lim)) if side=='lead' else ((t>edge)&(t<=lim))
            nb_far=np.abs(t-lim)>=(300 if nbr else 0)
            fm=inside&(x_all>=400)&nb_far
            if fm.sum()<100: continue
            F=np.median(r[fm]); sel=inside&nb_far
            order=np.argsort(x_all[sel]); x=x_all[sel][order]; dv=((s101[sel]-F)*sg)[order]; dv41=((s41[sel]-F)*sg)[order]
            j=np.argmin(dv41[x<4*e['W']]) if (x<4*e['W']).any() else 0
            after=np.where((np.arange(len(x))>j)&(np.abs(dv)<=2))[0]
            st=x[after[0]] if len(after) else np.nan
            rows.append(dict(loco=loco,pol=e['pol'],side=side,pwm=int(d.pwm_actual.iloc[e['ipk']]),W=e['W'],lobe=dv41[j],lobe_x_ms=x[j],settle_ms=st,settle_W=st/e['W'],seg=name.split(':')[1][:18]))
            xb=np.digitize(x/e['W'],bins)
            for b in np.unique(xb):
                if b<len(bins): pooled.setdefault((loco,side,bins[b-1]),[]).append(np.median(dv41[xb==b]))
R=pd.DataFrame(rows); R.to_csv(os.path.join(seg.OUT,'lobes.csv'),index=False)
pd.DataFrame([dict(loco=k[0],side=k[1],x_W=k[2],n=len(v),median=np.median(v),p10=np.percentile(v,10),p90=np.percentile(v,90),abs_p90=np.percentile(np.abs(v),90)) for k,v in sorted(pooled.items())]).to_csv(os.path.join(seg.OUT,'lobe_profile_pooled.csv'),index=False)
pd.set_option('display.width',200)
print(R.round(2).to_string())
print()
for (loco,side),g in R.groupby(['loco','side']):
    g=g[g.pwm>0]
    print(f"{loco} {side}: n={len(g)} lobe amp median={g.lobe.median():.1f} [{g.lobe.min():.0f},{g.lobe.max():.0f}]  lobe_x_W med={ (g.lobe_x_ms/g.W).median():.2f}  settle_W med={g.settle_W.median():.2f} p90={g.settle_W.quantile(.9):.2f} max={g.settle_W.max():.2f} | settle_ms med={g.settle_ms.median():.0f} max={g.settle_ms.max():.0f}  corr(settle_ms,W)={np.corrcoef(g.settle_ms,g.W)[0,1]:.2f}  CV_ms={g.settle_ms.std()/g.settle_ms.mean():.2f} CV_W={g.settle_W.std()/g.settle_W.mean():.2f}")
print("\npooled binned profile (median over events of bin-median s41-F, core-sign; lobe negative) and p90 |.|")
for loco in ['otto','toby']:
  for side in ['lead','trail']:
    line=[]
    for b in bins[:-1]:
        v=pooled.get((loco,side,b))
        if v and len(v)>=4: line.append(f"{b:.2f}:{np.median(v):+.1f}/{np.percentile(np.abs(v),90):.1f}(n{len(v)})")
    print(loco,side); print('  '+'  '.join(line))
