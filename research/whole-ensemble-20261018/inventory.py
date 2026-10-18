"""Inventory includes excluded observations; recognizes no magnetic shapes."""
from pathlib import Path
import hashlib,json
import pandas as pd
import numpy as np
ROOT=Path(__file__).resolve().parent
OLD=ROOT.parent/'statistical-shape-20261018'
rows=[]; exclusion=[]
for p in sorted((ROOT/'data').glob('eligibility_*.csv.gz')):
    boot=p.name.split('_')[1].split('.')[0];mask=pd.read_csv(p)
    wave=OLD/f'data/hall20_{boot}.csv.gz';w=pd.read_csv(wave)
    rows.append(dict(boot=boot,portable_source=str(wave.relative_to(ROOT.parent)),source_sha256=hashlib.sha256(wave.read_bytes()).hexdigest(),
                     bins_20ms=len(w),valid_bins=int(w.valid.sum()),native_observations=int(w['count'].sum()),
                     eligible_bins=int(mask.eligible.sum()),excluded_bins=int((~mask.eligible).sum())))
    for partition in ('train','eval'):
        bad=(~mask.eligible)&(mask.partition==partition)
        edges=np.diff(np.r_[False,bad,False].astype(int));starts=np.flatnonzero(edges==1);ends=np.flatnonzero(edges==-1)
        for lo,hi in zip(starts,ends):
            exclusion.append(dict(boot=boot,partition=partition,start_us=int(lo*20000),end_us=int(hi*20000),seconds=float((hi-lo)*.02),
                                  status_reasons=';'.join(sorted(mask.movement_status.iloc[lo:hi].unique())),
                                  physically_confirmed_stationary=False))
pd.DataFrame(rows).to_csv(ROOT/'results/source_inventory.csv',index=False)
pd.DataFrame(exclusion).to_csv(ROOT/'results/excluded_contiguous_intervals.csv',index=False)
summary=pd.DataFrame(exclusion).groupby('partition').agg(intervals=('seconds','count'),seconds=('seconds','sum'))
summary.to_csv(ROOT/'results/excluded_summary.csv')
print(pd.DataFrame(rows).to_string(index=False));print(summary.to_string())
