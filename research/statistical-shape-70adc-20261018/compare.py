"""Paired summaries and plots; never fits or modifies recognition models."""
import os,json
os.environ.setdefault('MPLCONFIGDIR','/private/tmp/ngr-statistical-shape-mpl')
import numpy as np
import pandas as pd
import matplotlib
matplotlib.use('Agg')
import matplotlib.pyplot as plt
from repeat import ROOT,ORIGINAL,raw_profiles,local_baseline,excursion

plt.rcParams.update({'font.size':10,'axes.spines.top':False,'axes.spines.right':False})

def boundaries(z):
    out={'n':len(z)}
    for key in ['start_error_ms','end_error_ms','midpoint_error_ms']:
        v=z[key].astype(float)
        out[key+'_median_signed']=float(v.median())
        out[key+'_median_absolute']=float(abs(v).median())
        out[key+'_mean_absolute']=float(abs(v).mean())
        out[key+'_p95_absolute']=float(abs(v).quantile(.95))
    return out

def main():
    summary=pd.read_csv(ROOT/'results/continuous_summary.csv')
    events=pd.read_csv(ROOT/'results/continuous_events.csv').fillna('')
    ann=pd.read_csv(ORIGINAL/'data/annotations_tail_0.001.csv').fillna('')
    spans=json.loads((ORIGINAL/'results/scan_spans.json').read_text())
    traces={b:pd.read_csv(ORIGINAL/f'data/hall20_{b}.csv.gz') for b in ann.boot.unique()}
    aggregates=[]
    keys=['MM_annotation_total','MM_annotation_unresolved','detections','matched','missed',
          'possible_lobe_splits','inside_BL','unassigned','BL_bins','BL_bins_as_MM',
          'candidate_windows','unresolved_windows','positive_shape_windows','positive_shape_below_70']
    for (focus,model,condition),z in summary.groupby(['focus','model','condition']):
        row={'focus':bool(focus),'model':model,'condition':condition,**{k:int(z[k].sum()) for k in keys}}
        row['matched_fraction_all_MM']=row['matched']/row['MM_annotation_total']
        row['BL_time_correct_fraction']=1-row['BL_bins_as_MM']/row['BL_bins']
        row['seconds']=float(((z.hi-z.lo)*.02).sum());aggregates.append(row)
    pd.DataFrame(aggregates).to_csv(ROOT/'results/aggregate.csv',index=False)

    paired=[]
    for span in spans:
        focus=span['id'].startswith('focus_')
        target=ann[ann.id.isin(span['ids'])&ann.kind.eq('MM')]
        for model in (['A','B_union'] if focus else ['A']):
            e=events[events.span.eq(span['id'])&events.model.eq(model)]
            lookups={c:{r['annotation_id']:r for r in e[e.condition.eq(c)&e.status.isin(['matched','missed'])].to_dict('records')} for c in ['original','70adc']}
            for r in target.to_dict('records'):
                row=dict(span=span['id'],boot=span['boot'],focus=focus,model=model,annotation_id=r['id'],mm=r['mm'],
                         annotation_reason=r['reason'],reference_lo=r['lo'],reference_hi=r['hi'])
                for c in ['original','70adc']:
                    p=lookups[c].get(r['id'])
                    row[c+'_status']='unresolved' if r['reason'] else p['status']
                    if p is not None and p['status']=='matched':
                        for k in ['lo','hi','score','excursion_adc','start_error_ms','end_error_ms','midpoint_error_ms']:
                            row[c+'_'+k]=p[k]
                paired.append(row)
    paired=pd.DataFrame(paired);paired.to_csv(ROOT/'results/annotation_comparison.csv',index=False)
    lost=paired[paired.original_status.eq('matched')&~paired['70adc_status'].eq('matched')]
    gained=paired[~paired.original_status.eq('matched')&paired['70adc_status'].eq('matched')]
    lost.to_csv(ROOT/'results/newly_lost_matches.csv',index=False)
    gained.to_csv(ROOT/'results/newly_gained_matches.csv',index=False)

    boundary_rows=[];paired_stats=[]
    for (focus,model),z in paired.groupby(['focus','model']):
        both=z[z.original_status.eq('matched')&z['70adc_status'].eq('matched')]
        paired_stats.append(dict(focus=bool(focus),model=model,common_matches=len(both),
            lost_matches=int((z.original_status.eq('matched')&~z['70adc_status'].eq('matched')).sum()),
            gained_matches=int((~z.original_status.eq('matched')&z['70adc_status'].eq('matched')).sum()),
            changed_boundaries=int(((both.original_lo!=both['70adc_lo'])|(both.original_hi!=both['70adc_hi'])).sum())))
        for condition in ['original','70adc']:
            e=events[events.focus.eq(focus)&events.model.eq(model)&events.condition.eq(condition)&events.status.eq('matched')]
            boundary_rows.append(dict(focus=bool(focus),model=model,condition=condition,scope='all_matches',**boundaries(e)))
            q=both[[condition+'_'+k for k in ['start_error_ms','end_error_ms','midpoint_error_ms']]].copy()
            q.columns=['start_error_ms','end_error_ms','midpoint_error_ms']
            boundary_rows.append(dict(focus=bool(focus),model=model,condition=condition,scope='common_annotations',**boundaries(q)))
    pd.DataFrame(boundary_rows).to_csv(ROOT/'results/boundaries.csv',index=False)
    pd.DataFrame(paired_stats).to_csv(ROOT/'results/paired_summary.csv',index=False)

    # Diagnostic excursions on intact reference/evaluation envelopes; not new labels or exclusions.
    diagnostic=[]
    for boot,z in ann[ann.reason.eq('')].groupby('boot'):
        t=traces[boot];y=t.adc.to_numpy();v=raw_profiles(y,z.lo,z.hi);b=local_baseline(v)
        for j,r in enumerate(z.to_dict('records')):
            signal=y[r['lo']:r['hi']]
            diagnostic.append(dict(id=r['id'],boot=boot,split=r['split'],kind=r['kind'],mm=r['mm'],
                baseline_adc=b[j],excursion_adc=float(excursion(signal.min(),signal.max(),b[j]))))
    diagnostic=pd.DataFrame(diagnostic);diagnostic.to_csv(ROOT/'results/envelope_excursions.csv',index=False)
    distributions=[]
    for (split,kind),z in diagnostic.groupby(['split','kind']):
        distributions.append(dict(split=split,kind=kind,n=len(z),below_70=int(z.excursion_adc.lt(70).sum()),
            quantiles=z.excursion_adc.quantile([0,.05,.5,.95,1]).tolist()))
    (ROOT/'results/comparison.json').write_text(json.dumps(dict(aggregate=aggregates,paired=paired_stats,
        boundaries=boundary_rows,envelope_excursions=distributions),indent=2)+'\n')

    # Exactly the same 13-s focused strip as the original report.
    span=next(s for s in spans if s['id']=='focus_1');t=traces[span['boot']]
    lo=int(span['lo']);hi=min(int(span['hi']),lo+650)
    fig,axes=plt.subplots(4,1,figsize=(14,11),sharex=True)
    for ax,(model,condition) in zip(axes,[('A','original'),('A','70adc'),('B_union','original'),('B_union','70adc')]):
        ax.plot((t.us.iloc[lo:hi]-t.us.iloc[lo])/1e6,t.adc.iloc[lo:hi],color='#222',lw=.8)
        for r in ann[ann.boot.eq(span['boot'])&ann.kind.eq('MM')&(ann.lo<hi)&(ann.hi>lo)&ann.reason.eq('')].to_dict('records'):
            a=max(lo,r['lo']);b=min(hi,r['hi']);ax.axvspan((a-lo)*.02,(b-lo)*.02,color='#478c5b',alpha=.18)
            ax.text(((a+b)/2-lo)*.02,ax.get_ylim()[1],f"MM{r['mm']:03}",ha='center',va='top',fontsize=8)
        e=events[events.span.eq(span['id'])&events.model.eq(model)&events.condition.eq(condition)&events.status.ne('missed')&(events.lo<hi)&(events.hi>lo)]
        bottom=ax.get_ylim()[0]
        for r in e.to_dict('records'):
            a=max(lo,r['lo']);b=min(hi,r['hi']);ax.plot([(a-lo)*.02,(b-lo)*.02],[bottom+4,bottom+4],color='#d35336',lw=3,solid_capstyle='butt')
        ax.set(title=f'{model} — {condition}: green = reference MM envelopes; red = accepted full windows',ylabel='ADC')
    axes[-1].set_xlabel('Seconds from the identical focused span start');fig.tight_layout()
    fig.savefig(ROOT/'figures/continuous_comparison.png',dpi=130,bbox_inches='tight');plt.close(fig)

    # Show boundary bias remaining after removing low-excursion detections.
    fig,axes=plt.subplots(1,3,figsize=(13,4))
    bd=pd.DataFrame(boundary_rows);bd=bd[bd.scope.eq('common_annotations')]
    groups=[(False,'A','A full'),(True,'A','A focus'),(True,'B_union','B focus')]
    for ax,key,title in zip(axes,['start_error_ms','end_error_ms','midpoint_error_ms'],['Beginning','End','Midpoint']):
        for offset,condition,color in [(-.18,'original','#777'),(.18,'70adc','#277f71')]:
            heights=[float(bd[bd.focus.eq(f)&bd.model.eq(m)&bd.condition.eq(condition)][key+'_median_absolute'].iloc[0]) for f,m,label in groups]
            ax.bar(np.arange(3)+offset,heights,width=.36,color=color,label=condition)
        ax.set_xticks(np.arange(3),[g[2] for g in groups]);ax.set(title=title,ylabel='Median absolute error (ms)');ax.legend()
    fig.suptitle('Same annotated ensembles matched under both conditions');fig.tight_layout()
    fig.savefig(ROOT/'figures/boundary_comparison.png',dpi=130,bbox_inches='tight');plt.close(fig)

    changed=paired[paired.original_status.eq('matched')&paired['70adc_status'].eq('matched')&
                   ((paired.original_lo!=paired['70adc_lo'])|(paired.original_hi!=paired['70adc_hi']))]
    weak=diagnostic[diagnostic.split.eq('train')&diagnostic.kind.eq('MM')&diagnostic.excursion_adc.lt(70)]
    fig,axes=plt.subplots(1,2,figsize=(13,4))
    r=changed.iloc[0];t=traces[r.boot];origin=int(r.reference_lo)
    lo=max(0,int(min(r.original_lo,r['70adc_lo']))-25);hi=min(len(t),int(max(r.original_hi,r['70adc_hi']))+25)
    axes[0].plot((t.us.iloc[lo:hi]-origin*20000)/1e6,t.adc.iloc[lo:hi],color='#222',lw=1)
    axes[0].axvspan(0,(r.reference_hi-origin)*.02,color='#478c5b',alpha=.18,label='Reference envelope')
    bottom=axes[0].get_ylim()[0]
    for condition,color,offset in [('original','#777',5),('70adc','#277f71',10)]:
        axes[0].plot([(r[condition+'_lo']-origin)*.02,(r[condition+'_hi']-origin)*.02],[bottom+offset]*2,
                     color=color,lw=4,label=condition)
    axes[0].set(title=f'Only changed matched boundary: MM{int(r.mm):03}',xlabel='Seconds from reference start',ylabel='ADC');axes[0].legend(fontsize=8)
    w=weak.iloc[0];r=ann[ann.id.eq(w.id)].iloc[0];t=traces[r.boot];origin=int(r.lo)
    lo=max(0,origin-25);hi=min(len(t),int(r.hi)+25)
    axes[1].plot((t.us.iloc[lo:hi]-origin*20000)/1e6,t.adc.iloc[lo:hi],color='#222',lw=1)
    axes[1].axvspan(0,(r.hi-origin)*.02,color='#d68035',alpha=.18,label='Existing training envelope')
    axes[1].axhline(w.baseline_adc,color='#277f71',ls=':',label='Local endpoint baseline')
    axes[1].set(title=f'MM{int(r.mm):03} training label: local excursion {w.excursion_adc:.2f} ADC',xlabel='Seconds from envelope start',ylabel='ADC');axes[1].legend(fontsize=8)
    fig.tight_layout();fig.savefig(ROOT/'figures/remaining_limitations.png',dpi=130,bbox_inches='tight');plt.close(fig)
    print(json.dumps(dict(aggregate=aggregates,paired=paired_stats,envelope_excursions=distributions),indent=2))

if __name__=='__main__':main()
