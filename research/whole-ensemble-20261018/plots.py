"""Published waveform, lobe/boundary, and variation diagnostics. No inference gates."""
from pathlib import Path
import json
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from functional import basis
ROOT=Path(__file__).resolve().parent
OLD=ROOT.parent/'statistical-shape-20261018'


def main():
    review=pd.read_csv(ROOT/'sources/morphology_adjudication.csv')
    spans=pd.read_csv(ROOT/'data/eligible_spans.csv')
    waves={b:pd.read_csv(OLD/f'data/hall20_{b}.csv.gz') for b in review.boot.unique()}
    sources=[]
    for r in review.itertuples(index=False):
        w=waves[r.boot];lo=int(r.start_lo_s/.02);hi=int(r.end_hi_s/.02)
        bg=float(np.median(np.r_[w.adc.iloc[max(0,lo-5):lo],w.adc.iloc[hi:hi+5]]))
        raw=w.adc.iloc[lo:hi].to_numpy();peak=int(np.argmax(np.abs(raw-bg)));principal=float(raw[peak])
        sources.append(dict(case_id=r.case_id,ordinal=r.ordinal,source_principal_adc=principal-bg,
                            source_extremum_s=float(w.us.iloc[lo+peak]/1e6),
                            source_breadth_s=(r.end_lo_s+r.end_hi_s-r.start_lo_s-r.start_hi_s)/2,
                            breadth_lo_s=r.end_lo_s-r.start_hi_s,breadth_hi_s=r.end_hi_s-r.start_lo_s))
    sources=pd.DataFrame(sources);sources.to_csv(ROOT/'results/source_variation.csv',index=False)
    for name in ('A','B'):
        if not (ROOT/f'results/evaluation_{name}.csv').exists(): continue
        obs=pd.read_csv(ROOT/f'results/evaluation_{name}.csv');fits=np.load(ROOT/f'results/fits_{name}_0.npz')
        result=pd.read_csv(ROOT/f'results/morphology_fields_{name}.csv')
        checks=[]
        for r in result.merge(sources,on=['case_id','ordinal']).itertuples(index=False):
            owner=obs[(obs.boot==r.boot)&(obs.start_us<=r.source_extremum_s*1e6)&(obs.end_us>r.source_extremum_s*1e6)]
            checks.append(dict(case_id=r.case_id,ordinal=r.ordinal,eligible=r.eligible,source_extremum_s=r.source_extremum_s,
                               extremum_in_same_object_as_frozen_landmark=bool(len(owner)==1 and owner.iloc[0].id==r.principal_object)))
        pd.DataFrame(checks).to_csv(ROOT/f'results/source_peak_ownership_{name}.csv',index=False)
        for case in ['A_M081_CW_10838585','A_M090_CCW_16243685','focus_A','focus_B3']:
            rr=review[review.case_id==case];boot=rr.iloc[0].boot;w=waves[boot]
            lo=int((rr.start_lo_s.min()-.15)/.02);hi=int((rr.end_hi_s.max()+.15)/.02)
            x=w.us.iloc[lo:hi].to_numpy()/1e6;y=w.adc.iloc[lo:hi].to_numpy();b=np.full(len(x),np.nan);f=b.copy()
            for s in spans[(spans.boot==boot)&(spans.partition=='eval')&(spans.lo<hi)&(spans.hi>lo)].itertuples(index=False):
                left=max(s.lo,lo);right=min(s.hi,hi)
                b[left-lo:right-lo]=fits[s.id+'__baseline'][left-s.lo:right-s.lo]
                f[left-lo:right-lo]=fits[s.id+'__field'][left-s.lo:right-s.lo]
            fig,axes=plt.subplots(2,1,figsize=(15,7),sharex=True)
            axes[0].plot(x,y,color='black',lw=.8,label='source 20-ms mean')
            axes[0].plot(x,b,color='green',lw=1,label='inferred shared background')
            axes[0].plot(x,b+f,color='orange',lw=1,label='whole-state fit')
            axes[1].plot(x,y,color='black',lw=.8)
            axes[1].plot(x,b+f,color='orange',lw=1)
            mid=np.median(y);axes[1].set_ylim(mid-25,mid+25)
            for ax in axes:
                for o in obs[(obs.boot==boot)&(obs.lo<hi)&(obs.hi>lo)].itertuples(index=False):
                    ax.axvspan(max(lo,o.lo)*.02,min(hi,o.hi)*.02,color='red' if o.state=='mm' else 'blue',alpha=.10)
                for r in rr.itertuples(index=False):
                    ax.axvspan(r.start_lo_s,r.start_hi_s,color='green',alpha=.25)
                    ax.axvspan(r.end_lo_s,r.end_hi_s,color='green',alpha=.25)
                    ax.axvline(r.lobe_s,color='purple',ls=':',lw=.8)
                ax.set_ylabel('ADC')
            fig.suptitle(f'{name} {case}: red MM, blue explicit BL; green source boundary ranges; purple secondary feature',fontsize=11)
            fig.legend(*axes[0].get_legend_handles_labels(),loc='upper center',bbox_to_anchor=(.5,.96),ncol=3)
            axes[1].set_xlabel('source seconds; bottom panel magnifies baseline-scale morphology')
            fig.tight_layout(rect=[0,0,1,.90]);fig.savefig(ROOT/f'figures/review_{name}_{case}.png',dpi=140);plt.close(fig)
    # Statistical full-function portraits, including coefficient correlations.
    fig,axes=plt.subplots(2,3,figsize=(15,8))
    u=np.linspace(0,1,201)
    for row,name in enumerate(('A','B')):
        model=json.loads((ROOT/f'models/{name}.json').read_text())
        for col,key in enumerate(('mm','bl')):
            prior=model[key];mu=np.array(prior['mean']);cov=np.array(prior['cov']);phi=basis(u,len(mu));mean=phi@mu
            sd=np.sqrt(np.einsum('ij,jk,ik->i',phi,cov,phi))
            axes[row,col].fill_between(u,mean-2*sd,mean+2*sd,color='gray',alpha=.25,label='pooled mean ±2 SD')
            axes[row,col].plot(u,mean,color='black')
            if prior.get('means') is not None:
                for m in prior['means']: axes[row,col].plot(u,phi@np.array(m),lw=.5,alpha=.6)
            axes[row,col].set(title=f'{name}: {key.upper()} whole-function prior',xlabel='uniform whole-state phase',ylabel='ADC from shared background')
        cov=np.array(model['mm']['cov']);sd=np.sqrt(np.diag(cov));corr=cov/sd[:,None]/sd[None,:]
        im=axes[row,2].imshow(corr,vmin=-1,vmax=1,cmap='coolwarm');axes[row,2].set_title(f'{name}: full MM coefficient correlations');fig.colorbar(im,ax=axes[row,2])
    fig.tight_layout();fig.savefig(ROOT/'figures/statistical_references.png',dpi=140);plt.close(fig)
    fig,axes=plt.subplots(1,2,figsize=(13,5),sharex=True,sharey=True)
    for ax,name in zip(axes,('A','B')):
        if not (ROOT/f'results/morphology_fields_{name}.csv').exists():
            ax.set_visible(False);continue
        result=pd.read_csv(ROOT/f'results/morphology_fields_{name}.csv').merge(sources,on=['case_id','ordinal'])
        for outcome,group in result.groupby('outcome'):
            ax.scatter(group.source_breadth_s,group.source_principal_adc.abs(),label=outcome,alpha=.7)
        ax.set(title=f'{name}: source strength and breadth, reviewed cohort',xlabel='source whole-field breadth (s)',ylabel='source principal excursion (ADC)');ax.legend(fontsize=7)
    fig.tight_layout();fig.savefig(ROOT/'figures/reviewed_variation.png',dpi=140);plt.close(fig)
if __name__=='__main__':main()
