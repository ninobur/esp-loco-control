"""One-condition, offline repeat of 804f1a2. Original artifacts are read-only."""
from pathlib import Path
import sys, json, hashlib
import numpy as np
import pandas as pd

ROOT=Path(__file__).resolve().parent
ORIGINAL=ROOT.parent/'statistical-shape-20261018'
sys.path.insert(0,str(ORIGINAL))
import evaluate as base

MIN_EXCURSION_ADC=70.0

def sha(path):
    h=hashlib.sha256()
    with open(path,'rb') as f:
        for block in iter(lambda:f.read(8*1024*1024),b''):h.update(block)
    return h.hexdigest()

def original_hashes():
    manifest=json.loads((ORIGINAL/'PACKAGE_MANIFEST.json').read_text())
    result={}
    for row in manifest['artifacts']:
        digest=sha(ORIGINAL/row['path'])
        assert digest==row['sha256'], f"Original changed: {row['path']}"
        result[row['path']]=digest
    result['PACKAGE_MANIFEST.json']=sha(ORIGINAL/'PACKAGE_MANIFEST.json')
    return result

def raw_profiles(y,lo,hi):
    """Exact unnormalized area-integration steps from original features()."""
    lo=np.atleast_1d(lo).astype(int);hi=np.atleast_1d(hi).astype(int)
    integral=np.r_[0,np.cumsum(np.nan_to_num(np.asarray(y,dtype=float)))]
    edges=lo[:,None]+(hi-lo)[:,None]*np.linspace(0,1,base.PHASE+1)
    values=np.interp(edges.ravel(),np.arange(len(integral)),integral).reshape(edges.shape)
    return np.diff(values,axis=1)/((hi-lo)/base.PHASE)[:,None]

def local_baseline(v):
    return (v[:,:6].mean(axis=1)+v[:,-6:].mean(axis=1))/2

def excursion(peak_min,peak_max,baseline):
    return np.maximum(abs(peak_max-baseline),abs(peak_min-baseline))

def select(w,trace_length,threshold=None):
    """Same greedy ordering/suppression; only the new eligibility conjunct differs."""
    eligible=np.ones(len(w),dtype=bool) if threshold is None else w[:,7]>=threshold
    occupied=np.zeros(trace_length,dtype=bool);chosen=[]
    # Filter first as in the original keep->concatenate->sort path. Stable ties retained.
    candidates=np.flatnonzero(eligible)
    for ix in candidates[np.argsort(-w[candidates,2],kind='stable')]:
        lo,hi=int(w[ix,0]),int(w[ix,1])
        if not occupied[lo:hi].any():
            chosen.append(ix);occupied[lo:hi]=True
    return np.array(chosen,dtype=int)

def scan(y,valid,start,end,model):
    loss=np.r_[0,np.cumsum(~valid)]
    windows=[];unresolved=0;total=0;feature_error=0.0
    for length in base.LENGTHS:
        lo=np.arange(start,end-length+1,5,dtype=int);hi=lo+length
        total+=len(lo)
        # Extrema are measured on full original 20-ms windows; no threshold contours.
        view=np.lib.stride_tricks.sliding_window_view(y[start:end],length)[::5]
        minimum=np.min(view,axis=1);maximum=np.max(view,axis=1)
        assert len(minimum)==len(lo)
        ok=(loss[hi]-loss[lo])==0
        unresolved+=int((~ok).sum())
        lo=lo[ok];hi=hi[ok];minimum=minimum[ok];maximum=maximum[ok]
        for a in range(0,len(lo),12000):
            l=lo[a:a+12000];h=hi[a:a+12000]
            x,p,rms=base.features(y,l,h);s=base.scores(x,model)
            v=raw_profiles(y,l,h);baseline=local_baseline(v)
            z,_,_=base.canonical(v)
            finite=np.isfinite(x)&np.isfinite(z)
            if finite.any():feature_error=max(feature_error,float(abs(x[finite]-z[finite]).max()))
            amp=excursion(minimum[a:a+12000],maximum[a:a+12000],baseline)
            unresolved+=int((~np.isfinite(s)).sum())
            keep=np.isfinite(s)&(s>0)
            if keep.any():
                windows.append(np.c_[l[keep],h[keep],s[keep],p[keep],baseline[keep],
                    minimum[a:a+12000][keep],maximum[a:a+12000][keep],amp[keep]])
    w=np.concatenate(windows) if windows else np.empty((0,8))
    old=select(w,len(y));new=select(w,len(y),MIN_EXCURSION_ADC)
    return w,old,new,dict(candidate_windows=total,unresolved_windows=unresolved,
        positive_shape_windows=len(w),positive_shape_below_70=int((w[:,7]<MIN_EXCURSION_ADC).sum()),
        full_profile_max_difference=feature_error)

def event_frame(w,indices,mm,bl,span,model,condition):
    selected=w[indices,:4]
    rows=base.match_events(selected,mm,bl)
    lookup={(int(w[i,0]),int(w[i,1])):i for i in indices}
    for r in rows:
        r.update(model=model,span=span['id'],boot=span['boot'],direction=span['direction'],
                 focus=span['id'].startswith('focus_'),condition=condition)
        if r['status']!='missed':
            i=lookup[(r['lo'],r['hi'])]
            r.update(baseline_adc=w[i,4],minimum_adc=w[i,5],maximum_adc=w[i,6],excursion_adc=w[i,7])
    return rows

def main():
    before=original_hashes()
    models=json.loads((ORIGINAL/'models/models.json').read_text())
    assert sha(ORIGINAL/'models/models.json')=='1ae49f93c7569560e62a816a9793598fbf0e8311f51bd0fb3464ff0791d7d490'
    spans=json.loads((ORIGINAL/'results/scan_spans.json').read_text())
    ann=pd.read_csv(ORIGINAL/'data/annotations_tail_0.001.csv').fillna('')
    traces={b:pd.read_csv(ORIGINAL/f'data/hall20_{b}.csv.gz') for b in ann.boot.unique()}
    events=[];summaries=[];audits=[]
    for span in spans:
        boot=span['boot'];t=traces[boot];lo=int(span['lo']);hi=int(span['hi'])
        target=ann[(ann.id.isin(span['ids']))&ann.kind.eq('MM')]
        mm=target[target.reason.eq('')].to_dict('records')
        bl=ann[ann.boot.eq(boot)&ann.kind.eq('BL')&(ann.lo>=lo)&(ann.hi<=hi)&ann.reason.eq('')].to_dict('records')
        focus=span['id'].startswith('focus_')
        for model in (['A','B_union'] if focus else ['A']):
            w,old,new,stats=scan(t.adc.to_numpy(),t.valid.to_numpy(),lo,hi,models['A' if model=='A' else 'B'])
            audit=pd.DataFrame(w,columns=['lo','hi','score','polarity','baseline_adc','minimum_adc','maximum_adc','excursion_adc'])
            audit['span']=span['id'];audit['model']=model;audit['boot']=boot
            audit['passes_70']=audit.excursion_adc>=MIN_EXCURSION_ADC
            audit['selected_original']=False;audit.loc[old,'selected_original']=True
            audit['selected_70adc']=False;audit.loc[new,'selected_70adc']=True
            audits.append(audit)
            for condition,indices in [('original',old),('70adc',new)]:
                rows=event_frame(w,indices,mm,bl,span,model,condition);events.extend(rows)
                detected=np.zeros(hi-lo,dtype=bool)
                for i in indices:detected[int(w[i,0])-lo:int(w[i,1])-lo]=True
                bltotal=sum(b['hi']-b['lo'] for b in bl)
                blerr=sum(detected[b['lo']-lo:b['hi']-lo].sum() for b in bl)
                summaries.append(dict(condition=condition,model=model,span=span['id'],boot=boot,focus=focus,
                    direction=span['direction'],lo=lo,hi=hi,MM_annotation_total=len(target),
                    MM_annotation_unresolved=int(target.reason.ne('').sum()),detections=len(indices),
                    matched=sum(r['status']=='matched' for r in rows),missed=sum(r['status']=='missed' for r in rows),
                    possible_lobe_splits=sum(r['status']=='possible_lobe_split' for r in rows),
                    inside_BL=sum(r['status']=='inside_BL' for r in rows),unassigned=sum(r['status']=='unassigned' for r in rows),
                    BL_bins=bltotal,BL_bins_as_MM=int(blerr),**stats))
            print(f"{model} {span['id']}: original {len(old)}; 70 ADC {len(new)}",flush=True)
    pd.DataFrame(events).to_csv(ROOT/'results/continuous_events.csv',index=False)
    summary=pd.DataFrame(summaries);summary.to_csv(ROOT/'results/continuous_summary.csv',index=False)
    pd.concat(audits,ignore_index=True).to_csv(ROOT/'results/candidate_audit.csv.gz',index=False,
        compression={'method':'gzip','mtime':0})
    # Exact original controls include per-event matching, boundaries, scores and order.
    original_events=pd.read_csv(ORIGINAL/'results/continuous_events.csv')
    replay=pd.DataFrame(events);replay=replay[replay.condition.eq('original')].reset_index(drop=True)
    replay[original_events.columns].to_csv(ROOT/'results/original_events_replay.csv',index=False)
    assert sha(ROOT/'results/original_events_replay.csv')==sha(ORIGINAL/'results/continuous_events.csv')
    original_summary=pd.read_csv(ORIGINAL/'results/continuous_summary.csv')
    replay=summary[summary.condition.eq('original')].reset_index(drop=True)
    replay[original_summary.columns].to_csv(ROOT/'results/original_summary_replay.csv',index=False)
    assert sha(ROOT/'results/original_summary_replay.csv')==sha(ORIGINAL/'results/continuous_summary.csv')
    assert original_hashes()==before
    checks=dict(status='PASS',base_commit='804f1a2d9306d7c2d19c9ee1be69be4a5ca2fc30',
        minimum_excursion_adc=MIN_EXCURSION_ADC,original_artifacts_unchanged=len(before),
        original_events_exactly_reproduced=len(original_events),original_summary_rows_exactly_reproduced=len(original_summary),
        model_sha256=sha(ORIGINAL/'models/models.json'),models_refitted=False,
        full_profile_max_difference=float(summary.full_profile_max_difference.max()))
    (ROOT/'results/validation.json').write_text(json.dumps(checks,indent=2)+'\n')
    (ROOT/'results/original_hashes.json').write_text(json.dumps(before,indent=2)+'\n')
    print(json.dumps(checks,indent=2))

if __name__=='__main__':main()
