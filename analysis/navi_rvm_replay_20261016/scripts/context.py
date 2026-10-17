"""40 ms observation steps + context. Hall features come only from native blocks.
IR odometer (um, unsigned) interpolated in controller time between Type-5 snapshots.
NAVI selections and declarations are exported separately: selections are used ONLY for scoring."""
import numpy as np, pandas as pd, json, pathlib
ROOT=pathlib.Path(__file__).resolve().parents[1]; NG=ROOT.parent
IRW=NG/'navi-ewo-dart-experiment-1/field-records/20261015_IR_WAVE_1_ACCURACY/derived'
ir=pd.read_csv(NG/'hall-ground-20261015/derived/ir_coordinate_source.csv')
sel=pd.DataFrame(json.load(open(IRW/'selected_records.json')))
cmd=pd.read_csv(IRW/'session_commands.csv')
iv=pd.read_csv(IRW/'interval_discrepancies.csv')
# map positions from mapped edge lengths (CW numbering increasing); MM170 -> MM0 wraps
edges={}
for r in iv.itertuples():
    a,b=(r.from_mm,r.to_mm) if r.direction=='CW' else (r.to_mm,r.from_mm)
    edges[(a,b)]=r.mapped_mm
pos={0:0.0}
for k in range(0,170): pos[k+1]=pos[k]+edges.get((k,k+1),np.nan)
L=pos[170]+edges.get((170,0),np.nan)  # MM0..MM170 = 171 magnets
mp=pd.DataFrame({'mm':list(pos),'map_mm':list(pos.values())})
pole=sel.groupby('target').pole.agg(lambda s:s.mode().iloc[0]).rename('pole')
mp=mp.join(pole,on='mm'); mp.to_csv(ROOT/'derived/map_positions.csv',index=False)
print('map lap',L,'missing edges',int(mp.map_mm.isna().sum()))
json.dump({'lap_mm':L},open(ROOT/'derived/map_meta.json','w'))
STEP=4
for boot in ['15549e9385faa4d5','4d8083de79a0e299']:
    z=np.load(ROOT/f'derived/blocks_{boot}.npz'); n=len(z['n'])//STEP*STEP
    sh=lambda k:z[k][:n].reshape(-1,STEP)
    cnt=sh('n'); m=sh('mean'); ok=(cnt>0)&~sh('gap')
    w=np.where(ok,cnt,0); feat=np.where(w.sum(1)>0,np.nansum(np.where(ok,m,0)*w,1)/np.maximum(w.sum(1),1),np.nan)
    t=sh('t_us')[:,0]-5000+20000
    pwm=sh('pwm').max(1); dr=sh('dir').max(1)
    b=ir[ir.boot==boot].sort_values('us'); odo=np.interp(t,b.us.values,b.um.values/1000.0,left=np.nan,right=np.nan)
    np.savez_compressed(ROOT/f'derived/steps_{boot}.npz',t_us=t,hall=feat,valid_frac=ok.mean(1),pwm=pwm,dir=dr,odo_mm=odo,
        hall_min=np.nanmin(np.where(ok,sh('min'),np.nan),1),hall_max=np.nanmax(np.where(ok,sh('max'),np.nan),1))
    print(boot,len(t),'steps; invalid',int(np.isnan(feat).sum()))
s=sel[['boot','open_us','target','previous_mm','pole','dir','pwm']].copy(); s.to_csv(ROOT/'derived/selections_scoring_only.csv',index=False)
c=cmd[cmd.topic.str.contains('start_interval|session_direction|/direction')][['boot','t_us','topic','payload']]; c.to_csv(ROOT/'derived/declarations.csv',index=False); print(c.to_string())
