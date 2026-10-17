"""Reference landscape templates from single recorded passes, on a 2 mm grid of the map coordinate.
Placement uses the pass's IR odometer scaled so the pass length equals the map lap length, anchored
once at the pass start (the one label used, like a declaration). The geographic map is not modified."""
import numpy as np, pandas as pd, json, pathlib
ROOT=pathlib.Path(__file__).resolve().parents[1]; D=ROOT/'derived'
mp=pd.read_csv(D/'map_positions.csv'); iv=pd.read_csv(ROOT.parent/'navi-ewo-dart-experiment-1/field-records/20261015_IR_WAVE_1_ACCURACY/derived/interval_discrepancies.csv')
LAP=json.load(open(D/'map_meta.json'))['lap_mm']
assert mp.map_mm.notna().all()

sel=pd.read_csv(D/'selections_scoring_only.csv'); B='15549e9385faa4d5'; sel=sel[sel.boot==B]
z=np.load(D/f'steps_{B}.npz'); t=z['t_us']/1e6; h=z['hall']; odo=z['odo_mm']; pwm=z['pwm']
G=1.0; NC=int(round(LAP/G))
passes={'CW_L1':(1420.658776,1840.213501,+1),'CW_L2':(1840.213501,2257.049782,+1),'CCW_L1':(2745.450837,3167.014139,-1)}
meta={}
for name,(a,b,sg) in passes.items():
    s0=sel.iloc[(sel.open_us/1e6-a).abs().argmin()]; mm0=int(s0.target); x0=float(mp.set_index('mm').map_mm[mm0])
    m=(t>=a)&(t<b)&np.isfinite(h)&np.isfinite(odo)
    o0=np.interp(a,t,odo); o1=np.interp(b,t,odo); sc=LAP/(o1-o0)
    x=(x0+sg*(odo[m]-o0)*sc)%LAP; c=(x/G).astype(int)%NC
    df=pd.DataFrame({'c':c,'h':h[m]}); med=df.groupby('c').h.median()
    T=np.full(NC,np.nan); T[med.index]=med.values
    filled=np.isfinite(T); idx=np.arange(NC)
    T=np.interp(idx,idx[filled],T[filled],period=NC)
    k=np.exp(-0.5*(np.arange(-12,13)*G/4.0)**2); k/=k.sum()
    Ts=np.convolve(np.concatenate([T[-12:],T,T[:12]]),k,'valid')
    np.save(D/f'template_{name}.npy',Ts)
    so=sel[(sel.open_us/1e6>=a)&(sel.open_us/1e6<b)]
    xo=(x0+sg*(np.interp(so.open_us.values/1e6,t,odo)-o0)*sc)%LAP
    opening={int(k):float(v) for k,v in zip(so.target,xo)}
    meta[name]={'opening_coord_mm':opening,'start_s':a,'end_s':b,'anchor_mm':mm0,'anchor_map_mm':x0,'odo_scale':sc,'odo_lap_mm':o1-o0,'cells_observed_frac':float(filled.mean())}
    # speed history table: IR speed over 1 s windows where PWM constant, from this template pass only
    tt=t[(t>=a)&(t<b)]; oo=odo[(t>=a)&(t<b)]; pp=pwm[(t>=a)&(t<b)]
    W=25; v=(oo[W:]-oo[:-W])/(tt[W:]-tt[:-W]); const=np.array([pp[i:i+W+1].min()==pp[i:i+W+1].max() for i in range(len(pp)-W)])
    tab=pd.DataFrame({'pwm':pp[:-W][const],'v':v[const]}).groupby('pwm').v.describe(percentiles=[.05,.5,.95])
    tab=tab[tab['count']>=25]; tab.to_csv(D/f'speed_table_{name}.csv'); print(name,meta[name]); print(tab.round(0).to_string())
json.dump(meta,open(D/'template_meta.json','w'),indent=1)
