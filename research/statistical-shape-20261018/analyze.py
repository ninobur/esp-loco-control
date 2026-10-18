"""Post-evaluation evidence tables and plots; does not change either model."""
import json, os
os.environ.setdefault('MPLCONFIGDIR','/private/tmp/ngr-statistical-shape-mpl')
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from evaluate import ROOT, features, canonical, scores, metric
from prepare import BOOT, DT

plt.rcParams.update({'font.size':10,'axes.spines.top':False,'axes.spines.right':False,'figure.dpi':120})

def save(fig,name):
    fig.savefig(ROOT/f'figures/{name}.png',bbox_inches='tight');plt.close(fig)

def main():
    models=json.loads((ROOT/'models/models.json').read_text());A=models['A'];B=models['B']
    ann=pd.read_csv(ROOT/'data/annotations_tail_0.001.csv').fillna('')
    d=pd.read_csv(ROOT/'results/interval_decisions.csv').fillna('')
    events=pd.read_csv(ROOT/'results/continuous_events.csv').fillna('')
    scans=pd.read_csv(ROOT/'results/continuous_summary.csv')
    traces={b:pd.read_csv(ROOT/f'data/hall20_{b}.csv.gz') for b in ann.boot.unique()}
    types=json.loads((ROOT/'sources/magnet_types.json').read_text())
    poles=json.loads((ROOT/'sources/route_polarity.json').read_text())['polarity']
    extra={}
    # Correlated observations are resampled by MM, with paired A/B decisions.
    a=d[(d.model=='A')&d.mm.between(50,70)].set_index('id')
    b=d[d.model=='B_oracle'].set_index('id').loc[a.index]
    paired=pd.DataFrame({'mm':a.mm,'A_correct':a.kind==a.decision,'B_correct':b.kind==b.decision})
    paired['delta']=paired.B_correct.astype(int)-paired.A_correct.astype(int)
    paired.to_csv(ROOT/'results/paired_focus.csv')
    groups=[z.delta.to_numpy() for _,z in paired.groupby('mm')]
    rng=np.random.default_rng(20261018);boot=[]
    for _ in range(10000):
        selected=[groups[i] for i in rng.integers(0,len(groups),len(groups))]
        boot.append(np.concatenate(selected).mean())
    extra['paired_focus']={'B_minus_A_correct_fraction':float(paired.delta.mean()),
        'cluster_by_MM_percentile_interval_95':np.quantile(boot,[.025,.975]).tolist(),
        'bootstrap_replicates':10000,'B_only_correct':int((paired.delta==1).sum()),
        'A_only_correct':int((paired.delta==-1).sum()),'meaning':'Exploratory uncertainty across 21 correlated geographic clusters; not independent field trials'}
    type_rows=[]
    for name,z in d.groupby('model'):
        for typ,q in z.groupby(z.mm.map(lambda m:types[str(int(m))])):
            type_rows.append(dict(model=name,magnet_type=typ,**metric(q)))
    pd.DataFrame(type_rows).to_csv(ROOT/'results/type_strata.csv',index=False)
    # Declared map and observed sign are compared only after all decisions.
    m=d[(d.model=='A')&(d.kind=='MM')&(d.decision!='unresolved')].copy()
    m['declared_pole']=m.mm.map(lambda v:1 if poles[int(v)] else -1)
    m['map_sign_agrees']=m.profile_polarity==m.declared_pole
    m[['id','mm','direction','profile_polarity','annotation_polarity','declared_pole','polarity_agrees','map_sign_agrees']].to_csv(ROOT/'results/polarity.csv',index=False)
    extra['polarity']={'resolved_envelopes':len(m),'profile_vs_envelope_agree':int(m.polarity_agrees.sum()),
        'profile_positive_vs_map_North_agree':int(m.map_sign_agrees.sum()),
        'meaning':'ADC sign contingency with declared map; not independent polarity ground truth'}
    boundaries=[]
    for (focus,name),z in events[events.status=='matched'].groupby(['focus','model']):
        row=dict(focus=bool(focus),model=name,n=len(z),polarity_agrees=int(z.polarity_agrees.astype(bool).sum()))
        for k in ['start_error_ms','end_error_ms','midpoint_error_ms']:
            v=z[k].astype(float)
            row[k+'_median']=float(v.median());row[k+'_median_absolute']=float(abs(v).median());row[k+'_p95_absolute']=float(abs(v).quantile(.95))
        boundaries.append(row)
    extra['boundaries']=boundaries
    extra['continuous_aggregate']=[]
    for (focus,name),z in scans.groupby(['focus','model']):
        keys=['MM_annotation_total','MM_annotation_unresolved','detections','matched','missed','possible_lobe_splits','inside_BL','unassigned','BL_bins','BL_bins_as_MM','candidate_windows','unresolved_windows']
        row=dict(focus=bool(focus),model=name,**{k:int(z[k].sum()) for k in keys})
        row['matched_fraction_all_MM']=row['matched']/row['MM_annotation_total']
        row['BL_time_correct_fraction']=1-row['BL_bins_as_MM']/row['BL_bins']
        row['evaluation_seconds']=float(((z.hi-z.lo)*.02).sum())
        extra['continuous_aggregate'].append(row)
    # Quantify support and width without claiming measured physical speed.
    ranges=[]
    for (kind,dec),q in d[d.model=='A'].groupby(['kind','decision']):
        row=dict(kind=kind,decision=dec,n=len(q))
        for k in ['rms_adc','width_ms']:
            row[k+'_quantiles']=pd.to_numeric(q[k],errors='coerce').quantile([0,.05,.5,.95,1]).tolist() if dec!='unresolved' else None
        ranges.append(row)
    extra['observed_variation']=ranges
    # Descriptive family difference at one geographic vote per training marker.
    train=ann[(ann.split=='train')&(ann.kind=='MM')&(ann.reason=='')]
    profiles=[];ids=[]
    for _,r in train.iterrows():
        x,_,_=features(traces[r.boot].adc.to_numpy(),[r.lo],[r.hi]);profiles.append((x[0]+x[0,::-1])/2);ids.append(r.mm)
    z=pd.DataFrame(profiles).assign(mm=ids).groupby('mm').mean()
    disk=np.array([types[str(i)]=='disk' for i in z.index]);means=z.to_numpy()
    stat=lambda mask:float(np.mean((means[mask].mean(axis=0)-means[~mask].mean(axis=0))**2))
    observed=stat(disk);permuted=np.array([stat(rng.permutation(disk)) for _ in range(5000)])
    extra['type_shape_association']={'disk_markers':int(disk.sum()),'bar_markers':int((~disk).sum()),
        'mean_phase_squared_difference':observed,'label_permutation_p':float((1+(permuted>=observed).sum())/5001),
        'caution':'Descriptive geographic association, installation/sensor/direction confounded; not proof that separate recognition families are necessary'}
    (ROOT/'results/analysis.json').write_text(json.dumps(extra,indent=2)+'\n')

    fig,axes=plt.subplots(1,3,figsize=(14,3.7))
    for kind,color in [('MM','#b74728'),('BL','#216d90')]:
        model=A['general'][kind];mu=np.array(model['mean']);sd=np.sqrt(model['variance']);phase=np.linspace(0,1,len(mu))
        axes[0].plot(phase,mu,color=color,label=kind);axes[0].fill_between(phase,mu-sd,mu+sd,color=color,alpha=.15)
    axes[0].set(title='A: mean ± model SD',xlabel='Uniform phase',ylabel='Canonical RMS units');axes[0].legend()
    for kind,color in [('MM','#b74728'),('BL','#216d90')]:
        q=d[(d.model=='A')&(d.kind==kind)&d.decision.ne('unresolved')]
        axes[1].hist(q.score,bins=45,density=True,histtype='step',color=color,label=kind)
    axes[1].axvline(0,color='k',ls=':');axes[1].set(title='Prepared evaluation intervals',xlabel='MM minus BL log density / 64',ylabel='Density');axes[1].legend()
    for mask,label,color in [(disk,'Disk','#8662b4'),(~disk,'Bar','#328065')]:
        mu=means[mask].mean(axis=0);sd=means[mask].std(axis=0)
        axes[2].plot(phase,mu,label=label,color=color);axes[2].fill_between(phase,mu-sd,mu+sd,color=color,alpha=.15)
    axes[2].set(title='Training: one vote per MM',xlabel='Uniform phase',ylabel='Canonical RMS units');axes[2].legend()
    save(fig,'models_and_separation')

    # Deterministically selected success/failure examples, with both wave and profile.
    q=d[d.model=='A']
    selections=[q[(q.kind=='MM')&(q.decision=='MM')&q.mm.eq(50)].iloc[0],
                q[(q.kind=='MM')&(q.decision=='MM')&q.mm.eq(81)].iloc[0],
                q[(q.kind=='MM')&(q.decision=='BL')&q.mm.eq(65)].iloc[0],
                q[(q.kind=='BL')&(q.decision=='MM')].iloc[0]]
    cases=[]
    fig,axes=plt.subplots(4,2,figsize=(13,12))
    for k,r in enumerate(selections):
        t=traces[r.boot];lo=int(r.lo);hi=int(r.hi);pad=max(25,(hi-lo)//3);a=max(0,lo-pad);b=min(len(t),hi+pad)
        axes[k,0].plot((t.us.iloc[a:b]-t.us.iloc[lo])/1e6,t.adc.iloc[a:b],color='#333',lw=.8)
        axes[k,0].axvspan(-.01,(hi-lo)*.02-.01,color='#d68035',alpha=.17)
        axes[k,0].set(title=f"{r.kind} MM{int(r.mm):03} {r.direction} → {r.decision}; score {float(r.score):.3f}",xlabel='Seconds from envelope first bin center',ylabel='ADC')
        x,p,_=features(t.adc.to_numpy(),[lo],[hi]);phase=np.linspace(0,1,64)
        axes[k,1].plot(phase,x[0],color='#111',label='Observed full profile')
        for kind,color in [('MM','#b74728'),('BL','#216d90')]:
            axes[k,1].plot(phase,A['general'][kind]['mean'],color=color,label=f'A {kind} mean',alpha=.8)
        axes[k,1].set(xlabel='Uniform phase',ylabel='Canonical RMS units',title=f"{r.boot[-4:]}: {float(r.start_us)/1e6:.3f}–{float(r.end_us)/1e6:.3f} s")
        if k==0:axes[k,1].legend(fontsize=8)
        cases.append(dict(id=r.id,boot=r.boot,mm=int(r.mm),kind=r.kind,decision=r.decision,start_us=r.start_us,end_us=r.end_us,
                          first_serial=r.first_serial,last_serial=r.last_serial,score=r.score))
    fig.tight_layout();save(fig,'successes_and_failures')
    pd.DataFrame(cases).to_csv(ROOT/'results/plot_cases.csv',index=False)

    fig,axes=plt.subplots(1,2,figsize=(12,4))
    for ax,mm in zip(axes,[63,64]):
        r=d[(d.model=='A')&(d.kind=='MM')&d.mm.eq(mm)&d.decision.eq('BL')].iloc[0]
        t=traces[r.boot];x,_,_=features(t.adc.to_numpy(),[int(r.lo)],[int(r.hi)])
        phase=np.linspace(0,1,64);ax.plot(phase,x[0],color='#111',label='Observed interval')
        for label,model,color in [('A',A['general']['MM'],'#bb5137'),('B',B[str(mm)]['MM'],'#4275aa')]:
            mu=np.array(model['mean']);sd=np.sqrt(model['variance'])
            ax.plot(phase,mu,color=color,label=f'{label} MM mean');ax.fill_between(phase,mu-sd,mu+sd,color=color,alpha=.12)
        oracle=d[(d.model=='B_oracle')&d.id.eq(r.id)].iloc[0]
        ax.set(title=f'MM{mm:03}: A score {float(r.score):.3f}; B {float(oracle.score):.3f}',
               xlabel='Uniform phase',ylabel='Canonical RMS units');ax.legend(fontsize=8)
    fig.tight_layout();save(fig,'geographic_comparison')

    # Continuous A/B failure on the same focused waveform around MM050–MM052.
    span=next(s for s in json.loads((ROOT/'results/scan_spans.json').read_text()) if s['id']=='focus_1')
    t=traces[span['boot']];lo=int(span['lo']);hi=min(int(span['hi']),lo+650)
    fig,axes=plt.subplots(2,1,figsize=(14,7),sharex=True)
    for ax,name in zip(axes,['A','B_union']):
        ax.plot((t.us.iloc[lo:hi]-t.us.iloc[lo])/1e6,t.adc.iloc[lo:hi],color='#222',lw=.8)
        for _,r in ann[(ann.boot==span['boot'])&(ann.kind=='MM')&(ann.lo<hi)&(ann.hi>lo)&(ann.reason=='')].iterrows():
            a=max(lo,int(r.lo));b=min(hi,int(r.hi));ax.axvspan((a-lo)*.02,(b-lo)*.02,color='#478c5b',alpha=.18)
            ax.text(((a+b)/2-lo)*.02,ax.get_ylim()[1],f'MM{r.mm:03}',ha='center',va='top',fontsize=8)
        ev=events[(events.span==span['id'])&(events.model==name)&events.status.ne('missed')&(events.lo<hi)&(events.hi>lo)]
        bottom=ax.get_ylim()[0]
        for _,r in ev.iterrows():
            a=max(lo,int(r.lo));b=min(hi,int(r.hi));ax.plot([(a-lo)*.02,(b-lo)*.02],[bottom+4,bottom+4],color='#d35336',lw=3,solid_capstyle='butt')
        ax.set(title=f'{name}: green = provisional MM envelopes; red bars = recognized intervals',ylabel='ADC')
    axes[-1].set_xlabel('Seconds from focused span start');fig.tight_layout();save(fig,'continuous_failure')

    fig,axes=plt.subplots(1,2,figsize=(12,4))
    for ax,k,label in zip(axes,['width_ms','rms_adc'],['Envelope duration (ms; not speed)','Envelope centered RMS (ADC)']):
        q=d[(d.model=='A')&(d.kind=='MM')&(d.decision!='unresolved')]
        for dec,color in [('MM','#267c55'),('BL','#c64d32')]:
            v=q[q.decision==dec];ax.scatter(v[k],v.score,s=13,alpha=.5,label=f'Classified {dec}',c=color)
        ax.axhline(0,color='k',ls=':');ax.set(xlabel=label,ylabel='MM minus BL log density / 64');ax.legend()
        if k=='width_ms':ax.set_xscale('log')
    fig.tight_layout();save(fig,'variation')

    # Auditable reference atlas: every usable training contour, no cherry-picking.
    fig,axes=plt.subplots(12,15,figsize=(24,19))
    for mm,ax in enumerate(axes.flat):
        if mm>=171:ax.axis('off');continue
        rows=train[train.mm==mm]
        for _,r in rows.iterrows():
            x,_,_=features(traces[r.boot].adc.to_numpy(),[r.lo],[r.hi]);ax.plot(np.linspace(0,1,64),x[0],lw=.55,alpha=.6)
        ax.set_title(f'{mm:03} {types[str(mm)]} n={len(rows)}',fontsize=7);ax.set_xticks([]);ax.set_yticks([])
    fig.suptitle('All 171 reference labels: entire provisional intervals, before reversal augmentation',fontsize=16)
    fig.tight_layout(rect=(0,0,1,.98));save(fig,'all_171_reference_atlas')
    print(json.dumps(extra,indent=2))

if __name__=='__main__':main()
