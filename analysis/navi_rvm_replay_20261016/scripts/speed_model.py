"""Otto's historical speed given PWM history, learned from a template pass only:
IR odometer speed (1 s smoothing) conditioned on PWM bin (5 units) and ramp state (PWM change over the last 0.5 s)."""
import numpy as np, pandas as pd, json, pathlib
ROOT=pathlib.Path(__file__).resolve().parents[1]; D=ROOT/'derived'
def ramp_state(pwm, lag=12):
    p=np.asarray(pwm,float); prev=np.concatenate([np.full(lag,p[0]),p[:-lag]]); d=p-prev
    return np.where(d>1,1,np.where(d<-1,-1,0))
def vir_of(t,odo,win=25):
    o=pd.Series(odo).interpolate(limit_direction='both').values
    h=win//2; v=np.full(len(o),np.nan); v[h:-h]=(o[2*h:]-o[:-2*h])/(t[2*h:]-t[:-2*h]); return pd.Series(v).bfill().ffill().values
if __name__=='__main__':
    meta=json.load(open(D/'template_meta.json')); B='15549e9385faa4d5'
    z=np.load(D/f'steps_{B}.npz'); t=z['t_us']/1e6; pwm=pd.Series(np.where(z['pwm']<0,np.nan,z['pwm'])).ffill().fillna(0).values; v=vir_of(t,z['odo_mm']); rs=ramp_state(pwm)
    for name,m in meta.items():
        k=(t>=m['start_s'])&(t<m['end_s'])&(pwm>=0)
        df=pd.DataFrame({'bin':(pwm[k]//5*5).astype(int),'ramp':rs[k],'v':v[k]})
        tab=df.groupby(['ramp','bin']).v.agg(n='size',med='median',q10=lambda s:s.quantile(.1),q90=lambda s:s.quantile(.9)).reset_index()
        tab.to_csv(D/f'speed_model_{name}.csv',index=False)
        print(name); print(tab[tab.n>=20].round(0).to_string())
