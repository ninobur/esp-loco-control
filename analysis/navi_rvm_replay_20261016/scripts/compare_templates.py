import numpy as np, pandas as pd, json, pathlib, matplotlib; matplotlib.use('Agg'); import matplotlib.pyplot as plt
ROOT=pathlib.Path(__file__).resolve().parents[1]; D=ROOT/'derived'; G=1.0
T={k:np.load(D/f'template_{k}.npy') for k in ['CW_L1','CW_L2','CCW_L1']}
mp=pd.read_csv(D/'map_positions.csv'); NC=len(T['CW_L1'])
def lagscan(a,b,win=600,maxlag=150):
    out=[]
    for c in range(0,NC,win//2):
        i=(np.arange(c,c+win))%NC; best=None
        for L in range(-maxlag,maxlag+1):
            j=(i+L)%NC; r=np.mean(np.abs((a[i]-np.median(a[i]))-(b[j]-np.median(b[j]))))
            if best is None or r<best[1]: best=(L,r)
        amp=np.ptp(a[i]); out.append((c*G,best[0]*G,best[1],amp))
    return pd.DataFrame(out,columns=['x_mm','lag_mm','mad','ptp'])
res={}
for a,b in [('CW_L1','CW_L2'),('CW_L1','CCW_L1')]:
    df=lagscan(T[a],T[b]); s=df[df.ptp>100]
    res[f'{a}_vs_{b}']={'windows_with_structure':int(len(s)),'lag_mm_quantiles':s.lag_mm.quantile([.05,.25,.5,.75,.95]).round(1).tolist(),
        'mad_counts_quantiles':s.mad.quantile([.5,.9]).round(1).tolist(),'level_diff_median':float(np.median(T[a]-T[b]))}
    df.to_csv(D/f'lagscan_{a}_vs_{b}.csv',index=False)
print(json.dumps(res,indent=1)); json.dump(res,open(D/'template_comparison.json','w'),indent=1)
x=np.arange(NC)*G; m=mp.set_index('mm').map_mm
fig,ax=plt.subplots(2,1,figsize=(14,7))
for k,(lo,hi) in enumerate([(m[68]-200,m[76]+200),(m[78]-200,m[92]+200)]):
    sl=(x>=lo)&(x<=hi)
    for n,c in [('CW_L1','C0'),('CW_L2','C1'),('CCW_L1','C2')]: ax[k].plot(x[sl],T[n][sl],c,lw=1,label=n)
    for mm,v in m.items():
        if lo<=v<=hi: ax[k].axvline(v,color='0.8',lw=.5); ax[k].text(v,1700,str(mm),fontsize=7)
    ax[k].set_ylabel('Hall ADC (template)')
ax[0].legend(); ax[1].set_xlabel('map coordinate (mm), MM tick = surveyed magnet position')
fig.tight_layout(); fig.savefig(ROOT/'figures/templates_MM68_92.png',dpi=110)
