import numpy as np, pandas as pd
I=pd.read_pickle('intervals.pkl')
out=[]
for loco,g in I.groupby('loco'):
    g=g.reset_index(drop=True)
    T=g.last100.values
    refs={'EWO C(k-1)':np.r_[np.nan,g.C.values[:-1]],
          'EWO gap-median(k-1) [lobes incl.]':np.r_[np.nan,g.gapmed.values[:-1]],
          'X22-like first100(k)':g.first100.values,
          'held: session first C':np.full(len(g),g.C.values[0]),
          'recorder operative baseline':g.base_next.values.astype(float)}
    print(f"\n{loco}: error = ref - ordinary level just before next core (last100 clean)  n={np.isfinite(T).sum()}")
    print(f"  {'reference':38s} p50|e| p95|e| p99|e|  max|e|   n>10  n>20  n>30")
    for name,v in refs.items():
        e=v-T; e=e[np.isfinite(e)]; a=np.abs(e)
        print(f"  {name:38s} {np.median(a):5.1f} {np.percentile(a,95):6.1f} {np.percentile(a,99):6.1f} {a.max():7.1f} {int((a>10).sum()):5d} {int((a>20).sum()):5d} {int((a>30).sum()):5d}")
    g['e_ewo']=refs['EWO C(k-1)']-T; g['e_x22']=refs['X22-like first100(k)']-T; g['e_gap']=refs['EWO gap-median(k-1) [lobes incl.]']-T
    out.append(g)
    big=g[np.abs(g.e_ewo)>8].copy()
    print('  EWO |e|>8 cases:'); 
    cols=['k','t','e_ewo','e_x22','e_gap','stop','gap_W','clean_n','C','last100','first100','pwm_next']+(['mm','ph'] if loco=='otto' else [])
    print(big[cols].round(1).to_string())
    prevstop=np.r_[False,g.stop.values[:-1]]
    for lab,m in [('no stop in k-1 or k',~(g.stop.values|prevstop)),('stop in k-1 or k',g.stop.values|prevstop)]:
        a=np.abs(g.e_ewo.values[m]); a=a[np.isfinite(a)]
        print(f"  EWO {lab}: n={len(a)} p50 {np.median(a):.1f} p99 {np.percentile(a,99):.1f} max {a.max():.1f}")
pd.concat(out).to_pickle('intervals_err.pkl')
