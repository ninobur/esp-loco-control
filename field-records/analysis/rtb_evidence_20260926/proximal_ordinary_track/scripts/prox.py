import os, numpy as np, pandas as pd, sys
import clean, seg
sc=float(sys.argv[1]) if len(sys.argv)>1 else 1.0
S=clean.build(sc)
Ns=[5,10,20,30,50,75,100,150,200,300,400,600]
# ---- 1. sliding stationarity: median of N preceding clean samples vs median of next 100 clean samples, same interval
SL=[]
print("== sliding: |median(prev N) - median(next 100)|, all clean intervals (counts): p50 / p95 / max  [windows]")
for loco in ['otto','toby']:
    for N in Ns:
        errs=[]
        for s in S:
            if s['loco']!=loco: continue
            r=s['d'].raw.values.astype(float)
            for iv in s['ivs']:
                a,b=iv['a'],iv['b']
                for i in range(a+N,b-100+1,5):
                    errs.append(np.median(r[i-N:i])-np.median(r[i:i+100]))
        e=np.abs(errs)
        if len(e): SL.append(dict(loco=loco,N=N,p50=np.median(e),p95=np.percentile(e,95),max=e.max(),windows=len(e)))
        if len(e): print(f"  {loco} N={N:4d}: {np.median(e):.1f} / {np.percentile(e,95):.1f} / {e.max():.1f}  [{len(e)}]")
pd.DataFrame(SL).to_csv(os.path.join(seg.OUT,f'sliding_x{sc}.csv'),index=False)
# ---- 2. per-magnet: proximal pre window vs interval, vs post-magnet ordinary level, vs held refs
rows=[]
otto_early_held=1934.0  # placeholder, replaced below by first clean interval of first otto segment
first={}
for s in S:
    r=s['d'].raw.values.astype(float)
    if s['ivs'] and s['loco'] not in first: first[s['loco']]=np.median(r[s['ivs'][0]['a']:s['ivs'][0]['b']])
for s in S:
    r=s['d'].raw.values.astype(float); t=s['d'].t.values
    seg_first=np.median(r[s['ivs'][0]['a']:s['ivs'][0]['b']]) if s['ivs'] else np.nan
    for k,iv in enumerate(s['ivs']):
        if not iv['next']: continue
        z0,z1,e,W=iv['next']
        post=[v for v in s['ivs'] if v['prev'] is iv['next']]
        pre=r[iv['a']:iv['b']]; M=np.median(pre)
        Q=np.median(r[post[0]['a']:post[0]['b']]) if post else np.nan
        Qs=r[post[0]['a']:post[0]['b']] if post else None
        row=dict(loco=s['loco'],seg=s['name'].split(':')[1][:22],pol=e['pol'],pwm=int(s['d'].pwm_actual.values[e['ipk']]),W=W,pre_n=len(pre),pre_W=len(pre)/W,
                 M_pre=M,Q_post=Q,post_n=len(Qs) if Qs is not None else 0,seg_first=seg_first,loco_first=first[s['loco']],rec_base=int(s['d'].baseline.values[e['ipk']]),
                 is_first_iv=(k==0))
        for N in Ns:
            if len(pre)>=N:
                P=np.median(pre[-N:]); row[f'P{N}']=P
        # naive: window ending at departure proxy (core start) minus D*W, length N=100 and N=50
        for D in [0,0.5,1,1.5,2,2.5,3,3.5]:
            te=e['t_a']-D*W; m=(t<te); idx=np.where(m)[0]
            for N in (50,100):
                if len(idx)>=N:
                    w=r[idx[-N:]]
                    sg=1 if e['pol']=='N' else -1
                    row[f'naive_N{N}_D{D}']=(np.median(w)-M)*sg
        rows.append(row)
R=pd.DataFrame(rows); R.to_csv(os.path.join(seg.OUT,f'prox_x{sc}.csv'),index=False)
pd.set_option('display.width',300); pd.set_option('display.max_columns',60)
print(R[['loco','seg','pol','pwm','W','pre_n','pre_W','M_pre','Q_post','post_n','seg_first','rec_base']+[f'P{N}' for N in (10,30,100,300)]].round(1).to_string())
print("\n== proximal P_N vs own-interval median M_pre and vs post-magnet level Q (|diff|: p50/max) ")
for loco,g in R.groupby('loco'):
    for N in Ns:
        c=f'P{N}'
        if c not in g: continue
        gg=g.dropna(subset=[c]); a=np.abs(gg[c]-gg.M_pre); q=gg.dropna(subset=['Q_post']); b=np.abs(q[c]-q.Q_post)
        print(f"  {loco} N={N:4d} (n={len(gg)},{len(q)}): vs M_pre {a.median():.1f}/{a.max():.1f}   vs Q_post {b.median():.1f}/{b.max():.1f}")
    q=g.dropna(subset=['Q_post'])
    print(f"  {loco} M_pre vs Q_post: p50 {np.abs(q.M_pre-q.Q_post).median():.1f} max {np.abs(q.M_pre-q.Q_post).max():.1f}   signed: {list((q.Q_post-q.M_pre).round(1))}")
    print(f"  {loco} held seg_first vs Q_post: p50 {np.abs(q.seg_first-q.Q_post).median():.1f} max {np.abs(q.seg_first-q.Q_post).max():.1f}")
    print(f"  {loco} held loco_first vs Q_post: p50 {np.abs(q.loco_first-q.Q_post).median():.1f} max {np.abs(q.loco_first-q.Q_post).max():.1f}")
    print(f"  {loco} recorder baseline vs Q_post: p50 {np.abs(q.rec_base-q.Q_post).median():.1f} max {np.abs(q.rec_base-q.Q_post).max():.1f}")
print("\n== naive proximal window ending at departure proxy minus D*W; bias vs M_pre in core-sign units (negative = toward leading lobe) median [min,max]")
for loco,g in R.groupby('loco'):
    for N in (50,100):
        line=[]
        for D in [0,0.5,1,1.5,2,2.5,3,3.5]:
            c=f'naive_N{N}_D{D}'; v=g[c].dropna()
            line.append(f"D={D}:{v.median():+.1f}[{v.min():+.0f},{v.max():+.0f}]n{len(v)}")
        print(f"  {loco} N={N}: "+'  '.join(line))
