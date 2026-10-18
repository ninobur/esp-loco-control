"""Freeze source-only morphology challenges before opening evaluation outputs."""
from pathlib import Path
import argparse,hashlib,json
import pandas as pd
import numpy as np
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
ROOT=Path(__file__).resolve().parent
ap=argparse.ArgumentParser(); ap.add_argument('--archive',type=Path,required=True); a=ap.parse_args()
src=a.archive/'navi-rvm-baseline-extent-20261017/derived/anatomical_cases.csv'
d=pd.read_csv(src); d=d[d.class_name=='within_ensemble_trough'].copy()
d.to_csv(ROOT/'sources/prior_lobe_morphology.csv',index=False)
(ROOT/'sources/morphology_manifest.json').write_text(json.dumps({'path':str(src),'sha256':hashlib.sha256(src.read_bytes()).hexdigest(),'meaning':'Prior waveform-morphology interpretation, not independently surveyed physical geography. No recognition predictions used.'},indent=2)+'\n')
wave=pd.read_csv(ROOT.parent/'statistical-shape-20261018/data/hall20_15549e9385faa4d5.csv.gz')
mask=pd.read_csv(ROOT/'data/eligibility_15549e9385faa4d5.csv.gz')
evalcases=d[((d.seed_us>=1840213501)&(d.seed_us<2745450837))|(d.seed_us>=3167014139)]
for i,row in enumerate(evalcases.itertuples(index=False)):
    lo=int(row.seed_us/20000)-100; hi=int(row.seed_us/20000)+100
    sub=wave.iloc[lo:hi]; t=sub.us/1e6
    fig,axes=plt.subplots(2,1,figsize=(14,6),sharex=True)
    for ax in axes:
        ax.plot(t,sub.adc,color='black',lw=.8)
        ax.axvline(row.seed_us/1e6,color='purple',label='prior within-ensemble undershoot landmark')
        invalid=~mask.eligible.iloc[lo:hi].astype(bool).to_numpy()
        ax.fill_between(t,0,1,where=invalid,transform=ax.get_xaxis_transform(),color='gray',alpha=.3)
        ax.set_ylabel('ADC')
    level=row.level_adc
    axes[1].set_ylim(level-30,level+40)
    axes[0].set_title(f'SOURCE ONLY {row.case_id}: shaded = excluded; no prediction shown')
    axes[0].legend(); axes[1].set_xlabel('seconds after boot');fig.tight_layout()
    fig.savefig(ROOT/f'figures/source_lobes_{i}.png',dpi=140);plt.close(fig)
print(evalcases[['case_id','seed_us']].to_string(index=False))
