"""Post-hoc evaluation. Source adjudication is never imported by recognition."""
from pathlib import Path
import json
import numpy as np
import pandas as pd
ROOT=Path(__file__).resolve().parent
OLD=ROOT.parent/'statistical-shape-20261018'

def read(name): return json.loads((ROOT/f'results/{name}.json').read_text())
def save(name,rows): pd.DataFrame(rows).to_csv(ROOT/f'results/{name}.csv',index=False)
def at(obs,t,boot): return next((o for o in obs if o['boot']==boot and o['start_us']<=t*1e6<o['end_us']),None)
def distance(x,a,b): return max(a-x,0.,x-b)
def median(rows,key):
    vals=[r[key] for r in rows if r.get(key) is not None and np.isfinite(r[key])]
    return float(np.median(vals)) if vals else None

def units(obs):
    rows=[]
    for a,b,c in zip(obs[:-2],obs[1:-1],obs[2:]):
        if a['span_id']==c['span_id'] and [a['state'],b['state'],c['state']]==['mm','bl','mm']:
            rows.append(dict(id=f'{a["id"]}__{c["id"]}',left_mm=a['id'],bl=b['id'],right_mm=c['id'],
                             span_id=a['span_id'],closed=not(a['censored'] or c['censored']),
                             start_us=a['start_us'],end_us=c['end_us']))
    return rows

def morphology(name,obs,spans,review):
    rows=[]
    for r in review.itertuples(index=False):
        coverage=spans[(spans.boot==r.boot)&(spans.start_us<=r.start_lo_s*1e6)&(spans.end_us>=r.end_hi_s*1e6)]
        row=dict(case_id=r.case_id,ordinal=r.ordinal,boot=r.boot,mm_hypothesis=r.mm_hypothesis,
                 eligible=len(coverage)==1,principal_s=r.principal_s,source_polarity=r.polarity)
        if not len(coverage):
            row['outcome']='censored_by_eligibility';rows.append(row);continue
        p=at(obs,r.principal_s,r.boot)
        row.update(principal_state=p['state'],principal_object=p['id'],principal_censored=p['censored'])
        features=[r.principal_s,r.leading_s,r.lobe_s,r.recovery_s]
        owners=[at(obs,t,r.boot) for t in features]
        row['lobe_grouped']=p['state']=='mm' and not p['censored'] and all(o and o['id']==p['id'] for o in [owners[0],owners[2],owners[3]])
        row['all_landmarks_grouped']=p['state']=='mm' and not p['censored'] and all(o and o['id']==p['id'] for o in owners)
        neighbors=review[(review.case_id==r.case_id)&(review.ordinal!=r.ordinal)]
        row['merged_neighbor']=p['state']=='mm' and any(p['start_us']<=v*1e6<p['end_us'] for v in neighbors.principal_s)
        pieces=[o for o in obs if o['boot']==r.boot and o['state']=='mm' and o['start_us']<max(features)*1e6 and o['end_us']>min(features)*1e6]
        row['mm_pieces_in_ownership_core']=len(pieces)
        row['duplicate_or_split']=len(pieces)>1
        row['bounds_within_review']=False
        if p['state']=='mm':
            start=p['start_us']/1e6;end=p['end_us']/1e6
            row['bounds_within_review']=r.start_lo_s<=start<=r.start_hi_s and r.end_lo_s<=end<=r.end_hi_s
            row['start_error_outside_range_ms']=1000*distance(start,r.start_lo_s,r.start_hi_s)
            row['end_error_outside_range_ms']=1000*distance(end,r.end_lo_s,r.end_hi_s)
            row['midpoint_error_outside_range_ms']=1000*distance((start+end)/2,(r.start_lo_s+r.end_lo_s)/2,(r.start_hi_s+r.end_hi_s)/2)
            row['start_error_to_center_ms']=1000*(start-(r.start_lo_s+r.start_hi_s)/2)
            row['end_error_to_center_ms']=1000*(end-(r.end_lo_s+r.end_hi_s)/2)
            row['midpoint_error_to_center_ms']=1000*((start+end)/2-(r.start_lo_s+r.start_hi_s+r.end_lo_s+r.end_hi_s)/4)
            row['polarity_correct']=int(np.sign(p['principal_adc']))==r.polarity
            row['amplitude_adc']=abs(p['principal_adc']);row['breadth_s']=p['breadth_s']
        row['complete']=row['all_landmarks_grouped'] and row['bounds_within_review'] and not row['merged_neighbor'] and not row['duplicate_or_split']
        row['outcome']=('complete' if row['complete'] else 'principal_in_BL' if p['state']=='bl' else
                        'censored_MM' if p['censored'] else 'merged_neighbor' if row['merged_neighbor'] else
                        'fragmented_lobe_or_shoulder' if not row['all_landmarks_grouped'] else 'boundary_failure')
        rows.append(row)
    save(f'morphology_fields_{name}',rows)
    by={(r['case_id'],r['ordinal']):r for r in rows}; baselines=[]; checked=[]
    for case,group in review.groupby('case_id',sort=False):
        group=group.sort_values('ordinal')
        for left,right in zip(group.itertuples(index=False),list(group.itertuples(index=False))[1:]):
            a,b=by[(case,left.ordinal)],by[(case,right.ordinal)]
            lo,hi=left.end_hi_s,right.start_lo_s
            core_spans=spans[(spans.boot==left.boot)&(spans.start_us<=lo*1e6)&(spans.end_us>=hi*1e6)]
            intruders=[o for o in obs if o['boot']==left.boot and o['state']=='mm' and o['start_us']<hi*1e6 and o['end_us']>lo*1e6]
            contained=[o for o in intruders if lo*1e6<=o['start_us'] and o['end_us']<=hi*1e6]
            center=at(obs,(lo+hi)/2,left.boot)
            explicit=bool(center and center['state']=='bl' and center['start_us']<=lo*1e6 and center['end_us']>=hi*1e6)
            baseline=dict(case_id=case,left_ordinal=left.ordinal,boot=left.boot,core_start_s=lo,core_end_s=hi,
                          eligible=bool(len(core_spans)),intruding_MM=len(intruders),contained_false_MM=len(contained),
                          intruding_ids=';'.join(o['id'] for o in intruders),explicit_BL_covers_core=explicit,
                          bl_id=center['id'] if center and center['state']=='bl' else '')
            if explicit:
                baseline['start_error_outside_range_ms']=1000*distance(center['start_us']/1e6,left.end_lo_s,left.end_hi_s)
                baseline['end_error_outside_range_ms']=1000*distance(center['end_us']/1e6,right.start_lo_s,right.start_hi_s)
            baselines.append(baseline)
            valid=a['eligible'] and b['eligible'] and bool(len(core_spans))
            good=valid and a.get('complete',False) and b.get('complete',False) and explicit and not intruders
            checked.append(dict(case_id=case,left_ordinal=left.ordinal,eligible=valid,complete_unit=good,
                                left_mm=a.get('principal_object',''),bl=baseline['bl_id'],right_mm=b.get('principal_object','')))
    save(f'baseline_cores_{name}',baselines);save(f'morphology_units_{name}',checked)
    return rows,baselines,checked

def summarize(rows,baselines,checked,focus=False):
    if focus:
        cases={r['case_id'] for r in rows if 50<=r['mm_hypothesis']<=70}
        rows=[r for r in rows if r['case_id'] in cases]
        baselines=[r for r in baselines if r['case_id'] in cases];checked=[r for r in checked if r['case_id'] in cases]
    eligible=[r for r in rows if r['eligible']]; bc=[r for r in baselines if r['eligible']];un=[r for r in checked if r['eligible']]
    return dict(reviewed=len(rows),eligible_fields=len(eligible),censored_fields=len(rows)-len(eligible),
        complete=sum(r.get('complete',False) for r in eligible),not_completely_recognized=sum(not r.get('complete',False) for r in eligible),
        principal_in_BL=sum(r['principal_state']=='bl' for r in eligible),
        lobe_grouped=sum(r.get('lobe_grouped',False) for r in eligible),all_landmarks_grouped=sum(r.get('all_landmarks_grouped',False) for r in eligible),
        duplicate_or_split_fields=sum(r.get('duplicate_or_split',False) for r in eligible),
        polarity_n=sum('polarity_correct' in r for r in eligible),polarity_correct=sum(r.get('polarity_correct',False) for r in eligible),
        boundary_n=sum('start_error_outside_range_ms' in r for r in eligible),
        boundary_median_outside_range_ms={k:median(eligible,k+'_error_outside_range_ms') for k in ('start','end','midpoint')},
        baseline_cores=len(bc),baseline_cores_with_MM=sum(r['intruding_MM']>0 for r in bc),
        contained_false_MM=sum(r['contained_false_MM'] for r in bc),
        explicit_BL_core=sum(r['explicit_BL_covers_core'] for r in bc),
        baseline_boundary_n=sum('start_error_outside_range_ms' in r for r in bc),
        baseline_boundary_median_outside_range_ms={k:median(bc,k+'_error_outside_range_ms') for k in ('start','end')},
        units=len(un),complete_units=sum(r['complete_unit'] for r in un))

def main():
    spans=pd.read_csv(ROOT/'data/eligible_spans.csv');review=pd.read_csv(ROOT/'sources/morphology_adjudication.csv')
    train=read('training_final'); audit=[]
    for mm in range(171):
        exact=[o for o in train if o['state']=='mm' and not o['censored'] and o['annotation_mms']==str(mm)]
        audit.append(dict(mm=mm,unambiguous_closed_reference_count=len(exact),reference_ids=';'.join(o['id'] for o in exact)))
    save('all171_reference_audit',audit)
    seeds=pd.read_csv(OLD/'data/annotation_seeds.csv');allaccount=[];metrics={}
    polarity=json.loads((OLD/'sources/route_polarity.json').read_text())['polarity']
    masks={boot:pd.read_csv(ROOT/f'data/eligibility_{boot}.csv.gz') for boot in spans.boot.unique()}
    for name in ('A','B'):
        obs=read('evaluation_'+name); us=units(obs); save('units_'+name,us)
        rows,baselines,checked=morphology(name,obs,spans[spans.partition=='eval'],review)
        seedrows=[]
        for i,s in enumerate(seeds.itertuples(index=False)):
            partition='eval' if s.boot=='4d8083de79a0e299' or 1840213501<=s.opening_us<2745450837 or s.opening_us>=3167014139 else 'train'
            if partition!='eval': continue
            ix=int(s.opening_us//20000); mask=masks[s.boot]
            status=mask.iloc[ix].movement_status if ix<len(mask) else 'outside_source'
            p=at(obs,s.opening_us/1e6,s.boot)
            state='excluded' if p is None else p['state']+'_edge_censored' if p['censored'] else p['state']
            row=dict(model=name,seed_id=i,boot=s.boot,mm=s.mm,direction=s.direction,opening_us=s.opening_us,
                     eligibility_status=status,association_state=state,object_id='' if p is None else p['id'],
                     physical_completeness='unresolved_except_separately_adjudicated_morphology')
            if p and p['state']=='mm': row['map_polarity_correct']=(p['principal_adc']>0)==bool(polarity[int(s.mm)])
            seedrows.append(row)
        allaccount.extend(seedrows)
        mm=[o for o in obs if o['state']=='mm'];closed=[o for o in mm if not o['censored']]
        metrics[name]=dict(objects_MM=len(mm),closed_MM=len(closed),censored_MM=len(mm)-len(closed),BL_states=sum(o['state']=='bl' for o in obs),
            closed_unit_hypotheses=sum(u['closed'] for u in us),review_all=summarize(rows,baselines,checked),review_focus=summarize(rows,baselines,checked,True),
            annotation_associations=pd.Series([r['association_state'] for r in seedrows]).value_counts().to_dict(),
            focused_annotation_associations=pd.Series([r['association_state'] for r in seedrows if 50<=r['mm']<=70]).value_counts().to_dict(),
            provisional_map_polarity_n=sum('map_polarity_correct' in r for r in seedrows),
            provisional_map_polarity_correct=sum(r.get('map_polarity_correct',False) for r in seedrows),
            raw_model_amplitude_quantiles=np.quantile([abs(o['principal_adc']) for o in closed],[0,.25,.5,.75,1]).tolist(),
            breadth_seconds_quantiles=np.quantile([o['breadth_s'] for o in closed],[0,.25,.5,.75,1]).tolist())
    save('annotation_accounting',allaccount)
    metrics['reference_coverage']=dict(labels_with_unambiguous_reference=sum(r['unambiguous_closed_reference_count']>0 for r in audit),
        missing_labels=[r['mm'] for r in audit if not r['unambiguous_closed_reference_count']],
        single_reference_labels=[r['mm'] for r in audit if r['unambiguous_closed_reference_count']==1])
    (ROOT/'results/metrics.json').write_text(json.dumps(metrics,indent=2)+'\n')
    print(json.dumps(metrics,indent=2))

if __name__=='__main__': main()
