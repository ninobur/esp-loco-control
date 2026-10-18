"""Source-context plots for reference coverage; no detector or class decisions."""
from pathlib import Path
import os
os.environ.setdefault('MPLCONFIGDIR','/private/tmp/ngr-whole-mpl')
import numpy as np,pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
ROOT=Path(__file__).resolve().parent
OLD=ROOT.parent/'statistical-shape-20261018'

def main():
    seeds=pd.read_csv(ROOT/'results/seed_eligibility_diagnostic.csv')
    traces={b:pd.read_csv(OLD/f'data/hall20_{b}.csv.gz') for b in seeds.boot.unique()}
    masks={b:pd.read_csv(ROOT/f'data/eligibility_{b}.csv.gz') for b in seeds.boot.unique()}
    chosen=seeds[seeds.partition.eq('train')&seeds.mm.isin([15,16,63,108,157])].sort_values(['mm','opening_us'])
    for page,start in enumerate(range(0,len(chosen),6)):
        fig,axes=plt.subplots(3,2,figsize=(15,10))
        for ax,(_,r) in zip(axes.flat,chosen.iloc[start:start+6].iterrows()):
            t=traces[r.boot];mask=masks[r.boot].eligible.to_numpy();lo=max(0,int(r.opening_us//20000)-150);hi=min(len(t),int(r.opening_us//20000)+200)
            ax.plot(t.us.iloc[lo:hi]/1e6,t.adc.iloc[lo:hi],color='#333',lw=.8)
            top,bottom=ax.get_ylim()[1],ax.get_ylim()[0]
            ax.fill_between(t.us.iloc[lo:hi]/1e6,bottom,top,where=mask[lo:hi],color='#4c9965',alpha=.18,step='mid')
            ax.axvline(r.opening_us/1e6,color='#a44',ls=':')
            ax.set(title=f'MM{r.mm:03} {r.direction}; opening {r.opening_us/1e6:.3f}s; {r.boot[-4:]}',xlabel='Boot seconds',ylabel='ADC')
        fig.suptitle('Green = movement-supported, valid observation bins; unshaded = excluded/uncertain')
        fig.tight_layout();fig.savefig(ROOT/f'figures/reference_coverage_{page}.png',dpi=130);plt.close(fig)
    chosen.to_csv(ROOT/'results/reference_coverage_review.csv',index=False)

if __name__=='__main__':main()
