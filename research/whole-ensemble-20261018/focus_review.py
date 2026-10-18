from pathlib import Path
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
r=Path(__file__).resolve().parent
for i,(boot,t) in enumerate([('4d8083de79a0e299',261.9),('15549e9385faa4d5',2064.45),('15549e9385faa4d5',2485.38),('15549e9385faa4d5',3334.27)]):
 w=pd.read_csv(r.parent/f'statistical-shape-20261018/data/hall20_{boot}.csv.gz');m=pd.read_csv(r/f'data/eligibility_{boot}.csv.gz')
 lo=int((t-2.05)/.02);hi=int((t+2.05)/.02);w=w.iloc[lo:hi];x=w.us/1e6
 f,ax=plt.subplots(2,1,figsize=(15,6),sharex=True)
 for a in ax:
  a.plot(x,w.adc,color='black',lw=.8); a.fill_between(x,0,1,where=~m.eligible.iloc[lo:hi].astype(bool).to_numpy(),transform=a.get_xaxis_transform(),alpha=.3,color='gray');a.set_ylabel('ADC')
 mid=w.adc.median();ax[1].set_ylim(mid-25,mid+25);ax[0].set_title(f'SOURCE ONLY focus {i}: {boot}; gray excluded; no predictions');ax[1].set_xlabel('seconds after boot');f.tight_layout();f.savefig(r/f'figures/source_focus_{i}.png',dpi=140);plt.close(f)
