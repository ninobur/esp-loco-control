"""Independent, offline functional-profile comparison. No tracking or warping."""
from pathlib import Path
import json, os
os.environ.setdefault('MPLCONFIGDIR','/private/tmp/ngr-statistical-shape-mpl')
import numpy as np
import pandas as pd
from scipy.special import logsumexp
from scipy.stats import rankdata
from prepare import ROOT, BOOT, DT

PHASE=64
LENGTHS=[8,12,18,27,40,60,90,135,202,303,454,681,1022]

def canonical(v):
    """All phase samples retained; return shape, sign and centered RMS."""
    v=np.atleast_2d(v).astype(float)
    edge=(v[:,:6].mean(axis=1)+v[:,-6:].mean(axis=1))/2
    deviation=v-edge[:,None]
    principal=deviation[np.arange(len(v)),np.argmax(abs(deviation),axis=1)]
    pole=np.where(principal>=0,1,-1)
    centered=v-v.mean(axis=1)[:,None]
    scale=np.sqrt(np.mean(centered**2,axis=1))
    z=np.divide(centered,scale[:,None],out=np.full_like(v,np.nan),where=scale[:,None]>0)*pole[:,None]
    return z,pole,scale

def features(y,lo,hi):
    """Area integration: every input bin contributes, even if longer than 64."""
    lo=np.atleast_1d(lo).astype(int);hi=np.atleast_1d(hi).astype(int)
    integral=np.r_[0,np.cumsum(np.nan_to_num(np.asarray(y,dtype=float)))]
    edges=lo[:,None]+(hi-lo)[:,None]*np.linspace(0,1,PHASE+1)
    values=np.interp(edges.ravel(),np.arange(len(integral)),integral).reshape(edges.shape)
    v=np.diff(values,axis=1)/((hi-lo)/PHASE)[:,None]
    return canonical(v)

def fit(x,pooled):
    # Reversal augmentation represents either traversal direction statistically.
    augmented=np.r_[x,x[:,::-1]]
    return dict(mean=augmented.mean(axis=0).tolist(),
                variance=(.5*augmented.var(axis=0)+.5*pooled+.05).tolist(),
                reference_intervals=len(x),augmented_profiles=len(augmented))

def logdensity(x,model):
    m=np.array(model['mean']);v=np.array(model['variance'])
    return -.5*np.sum(np.log(2*np.pi*v)+(x-m)**2/v,axis=1)

def scores(x,models,geo=None):
    if geo is not None:
        if str(int(geo)) not in models:
            return np.full(len(x),np.nan)
        refs=[models[str(int(geo))]]
    else:
        refs=list(models.values())
    mm=np.array([logdensity(x,m['MM']) for m in refs])
    bl=np.array([logdensity(x,m['BL']) for m in refs])
    # Equal weight mixture across reference families, identical in A and B.
    return (logsumexp(mm,axis=0)-logsumexp(bl,axis=0))/PHASE

def decision(s):
    return np.where(~np.isfinite(s)|(s==0),'unresolved',np.where(s>0,'MM','BL'))

def auc(labels,score):
    labels=np.asarray(labels)=='MM';score=np.asarray(score)
    yes=labels.sum();no=len(labels)-yes
    return None if yes==0 or no==0 else float((rankdata(score)[labels].sum()-yes*(yes+1)/2)/(yes*no))

def metric(z):
    good=z[z.decision!='unresolved']
    n=len(z);mm=z.kind=='MM';bl=~mm
    return dict(n=n,MM=int(mm.sum()),BL=int(bl.sum()),resolved=len(good),unresolved=n-len(good),
        MM_as_MM=int((mm&(z.decision=='MM')).sum()),MM_as_BL=int((mm&(z.decision=='BL')).sum()),
        BL_as_BL=int((bl&(z.decision=='BL')).sum()),BL_as_MM=int((bl&(z.decision=='MM')).sum()),
        MM_unresolved=int((mm&(z.decision=='unresolved')).sum()),BL_unresolved=int((bl&(z.decision=='unresolved')).sum()),
        correct_all=float((z.kind==z.decision).sum()/n) if n else None,
        auc_resolved=auc(good.kind,good.score) if len(good) else None)

def scan(y,valid,start,end,model):
    """Finite multiscale full-window proposals; greedy disjoint evidence selection."""
    loss=np.r_[0,np.cumsum(~valid)]
    windows=[];unresolved=0;total=0
    for length in LENGTHS:
        lo=np.arange(start,end-length+1,5,dtype=int);hi=lo+length
        total+=len(lo)
        ok=(loss[hi]-loss[lo])==0
        unresolved+=int((~ok).sum());lo=lo[ok];hi=hi[ok]
        for a in range(0,len(lo),12000):
            l=lo[a:a+12000];h=hi[a:a+12000]
            x,p,rms=features(y,l,h);s=scores(x,model)
            unresolved+=int((~np.isfinite(s)).sum())
            keep=np.isfinite(s)&(s>0)
            if keep.any():
                windows.append(np.c_[l[keep],h[keep],s[keep],p[keep]])
    if not windows:
        return np.empty((0,4)),dict(candidate_windows=total,unresolved_windows=unresolved)
    w=np.concatenate(windows);occupied=np.zeros(len(y),dtype=bool);chosen=[]
    for ix in np.argsort(-w[:,2],kind='stable'):
        a,b=int(w[ix,0]),int(w[ix,1])
        if not occupied[a:b].any():
            chosen.append(w[ix]);occupied[a:b]=True
    return np.array(chosen),dict(candidate_windows=total,unresolved_windows=unresolved)

def spans(annotations,focus=False):
    m=annotations[(annotations.kind=='MM')&(annotations.split=='eval')].sort_values(['boot','opening_us'])
    groups=[];group=[]
    for r in m.to_dict('records'):
        eligible=not focus or 50<=r['mm']<=70
        contiguous=False
        if group:
            p=group[-1];step=1 if r['direction']=='CW' else -1
            contiguous=(r['boot']==p['boot'] and r['direction']==p['direction'] and
                        (r['mm']-p['mm'])%171==step%171)
        if group and (not eligible or not contiguous):
            groups.append(group);group=[]
        if eligible:group.append(r)
    if group:groups.append(group)
    # Include unresolved edge cells in the denominator but avoid fabricating span extents.
    return [dict(id=f"{'focus' if focus else 'all'}_{i}",boot=g[0]['boot'],direction=g[0]['direction'],
                 lo=min(r['lo'] for r in g),hi=max(r['hi'] for r in g),ids=[r['id'] for r in g]) for i,g in enumerate(groups)]

def match_events(detections,mm,bl):
    candidates=[]
    for di,d in enumerate(detections):
        for mi,m in enumerate(mm):
            intersection=max(0,min(d[1],m['hi'])-max(d[0],m['lo']))
            union=max(d[1],m['hi'])-min(d[0],m['lo'])
            if union and intersection/union>=.3:
                candidates.append((intersection/union,di,mi))
    paired={};used=set()
    for overlap,di,mi in sorted(candidates,reverse=True):
        if di not in paired and mi not in used:
            paired[di]=(mi,overlap);used.add(mi)
    rows=[]
    for di,d in enumerate(detections):
        if di in paired:
            mi,overlap=paired[di];m=mm[mi]
            rows.append(dict(lo=int(d[0]),hi=int(d[1]),score=d[2],polarity=int(d[3]),status='matched',annotation_id=m['id'],
                mm=m['mm'],iou=overlap,start_error_ms=(d[0]-m['lo'])*20,end_error_ms=(d[1]-m['hi'])*20,
                midpoint_error_ms=((d[0]+d[1]-m['lo']-m['hi'])/2)*20,
                polarity_agrees=int(d[3])==int(m['annotation_polarity'])))
        else:
            overlaps=[m for m in mm if min(d[1],m['hi'])>max(d[0],m['lo'])]
            in_bl=any(d[0]>=b['lo'] and d[1]<=b['hi'] for b in bl)
            split=any(m['id'] in {mm[k]['id'] for k in used} for m in overlaps)
            status='possible_lobe_split' if split else 'inside_BL' if in_bl else 'unassigned'
            rows.append(dict(lo=int(d[0]),hi=int(d[1]),score=d[2],polarity=int(d[3]),status=status,annotation_id='',mm=None))
    for mi,m in enumerate(mm):
        if mi not in used:
            rows.append(dict(lo=m['lo'],hi=m['hi'],score=None,polarity=None,status='missed',annotation_id=m['id'],mm=m['mm']))
    return rows

def main():
    traces={p.stem.split('_')[-1].replace('.csv',''):pd.read_csv(p) for p in (ROOT/'data').glob('hall20_*.csv.gz')}
    ann=pd.read_csv(ROOT/'data/annotations_tail_0.001.csv').fillna('')
    X=np.full((len(ann),PHASE),np.nan);pol=np.zeros(len(ann));rms=np.full(len(ann),np.nan)
    for boot,ix in ann.groupby('boot').groups.items():
        x,p,s=features(traces[boot].adc.to_numpy(),ann.loc[ix,'lo'],ann.loc[ix,'hi'])
        X[ix]=x;pol[ix]=p;rms[ix]=s
    bad=(ann.reason!='').to_numpy()|~np.isfinite(X).all(axis=1)
    # Masked gaps have no interpretable amplitude or principal-polarity estimate.
    rms[bad]=np.nan;pol[bad]=0
    train=(ann.split=='train').to_numpy()&~bad
    pooled={k:np.r_[X[train&(ann.kind==k)],X[train&(ann.kind==k),::-1]].var(axis=0) for k in ['MM','BL']}
    def construct(mask):
        result={}
        for k in ['MM','BL']:
            ix=train&mask&(ann.kind==k).to_numpy()
            if not ix.any():return None
            result[k]=fit(X[ix],pooled[k])
        return result
    A={'general':construct(np.ones(len(ann),dtype=bool))}
    B={str(mm):construct((ann.mm==mm).to_numpy()) for mm in range(50,71)}
    B={k:v for k,v in B.items() if v is not None}
    models=dict(A=A,B=B,phase_bins=64,variance_shrinkage=.5,variance_ridge=.05,scale_bins=LENGTHS,
                reference_ids=ann.loc[train,'id'].tolist())
    (ROOT/'models/models.json').write_text(json.dumps(models,indent=2)+'\n')
    # Save the frozen training model before accessing evaluation scores.
    eval_ix=np.flatnonzero((ann.split=='eval').to_numpy())
    result=[]
    for name,model in [('A',A),('B_oracle',B),('B_union',B)]:
        ix=eval_ix if name=='A' else eval_ix[ann.loc[eval_ix,'mm'].between(50,70)]
        s=scores(X[ix],model) if name!='B_oracle' else np.array([scores(X[i:i+1],model,ann.at[i,'mm'])[0] for i in ix])
        s[bad[ix]]=np.nan
        out=ann.loc[ix].copy();out['model']=name;out['score']=s;out['decision']=decision(s)
        out['profile_polarity']=pol[ix];out['rms_adc']=rms[ix];out['width_ms']=(out.hi-out.lo)*20
        out['polarity_agrees']=pol[ix]==out.annotation_polarity
        result.append(out)
    decisions=pd.concat(result,ignore_index=True)
    decisions.to_csv(ROOT/'results/interval_decisions.csv',index=False)
    summaries={}
    for (model,scope),z in decisions.assign(scope='all_available').groupby(['model','scope']):
        summaries[f'{model}/{scope}']=metric(z)
    a=decisions[decisions.model=='A']
    summaries['A/focus']=metric(a[a.mm.between(50,70)])
    summaries['A/outside_focus']=metric(a[~a.mm.between(50,70)])
    for name,z in decisions.groupby('model'):
        for boot,q in z.groupby('boot'):summaries[f'{name}/boot_{boot}']=metric(q)
        for direction,q in z.groupby('direction'):summaries[f'{name}/{direction}']=metric(q)
        complete=z[(z.boot==BOOT)&z.opening_us.between(1840213501,2257049782,inclusive='left')]
        summaries[f'{name}/CW_second_complete']=metric(complete)
    (ROOT/'results/interval_summary.json').write_text(json.dumps(summaries,indent=2)+'\n')
    coverage=ann.assign(usable=~bad).groupby(['split','kind','mm']).agg(total=('id','size'),usable=('usable','sum')).reset_index()
    coverage.to_csv(ROOT/'results/coverage.csv',index=False)
    shape_rows=[]
    for name,z in decisions.groupby('model'):
        for kind,q in z[z.decision!='unresolved'].groupby('kind'):
            shape_rows.append(dict(model=name,kind=kind,n=len(q),**{f'q{int(p*100):02}':float(q.score.quantile(p)) for p in [0,.05,.25,.5,.75,.95,1]}))
    pd.DataFrame(shape_rows).to_csv(ROOT/'results/score_distributions.csv',index=False)
    # Observed width/amplitude strata use training quartiles only, after classification.
    variation=[]
    for quantity,values in [('rms_adc',rms),('width_ms',(ann.hi-ann.lo).to_numpy()*20)]:
        cuts=np.quantile(values[train&(ann.kind=='MM')],[.25,.5,.75])
        for name,z in decisions[decisions.kind=='MM'].groupby('model'):
            strata=np.searchsorted(cuts,z[quantity],side='right')
            strata[(z.decision=='unresolved').to_numpy()]=-1
            for j,q in z.groupby(strata):
                variation.append(dict(model=name,quantity=quantity,stratum=int(j),cutpoints=cuts.tolist(),**metric(q)))
    (ROOT/'results/variation.json').write_text(json.dumps(variation,indent=2)+'\n')
    # Annotation-envelope sensitivity; unchanged models and decision rule.
    sensitivity=[]
    for tail in [.001,.005,.01]:
        alt=pd.read_csv(ROOT/f'data/annotations_tail_{tail:g}.csv').fillna('')
        for name,model in [('A',A),('B_union',B)]:
            for boot,z in alt[alt.split=='eval'].groupby('boot'):
                if name!='A':z=z[z.mm.between(50,70)]
                x,_,_=features(traces[boot].adc.to_numpy(),z.lo,z.hi);s=scores(x,model)
                s[(z.reason!='').to_numpy()]=np.nan
                z=z.copy();z['score']=s;z['decision']=decision(s)
                sensitivity.append(dict(tail=tail,model=name,boot=boot,**metric(z)))
    (ROOT/'results/boundary_sensitivity.json').write_text(json.dumps(sensitivity,indent=2)+'\n')
    # Independent continuous localization. No annotations passed to scan().
    scan_rows=[];scan_summary=[];span_rows=[]
    full=[]
    for boot,t in traces.items():
        clock_ranges=[(0,len(t)*DT)] if boot!=BOOT else [(1840213501,2745450837),(3167014139,len(t)*DT)]
        for start_us,end_us in clock_ranges:
            lo=int(np.ceil(start_us/DT));hi=int(end_us//DT)
            ids=ann[(ann.boot==boot)&(ann.kind=='MM')&(ann.split=='eval')&
                    (ann.opening_us>=start_us)&(ann.opening_us<end_us)].id.tolist()
            full.append(dict(id=f'full_{boot}_{lo}',boot=boot,direction='mixed',lo=lo,hi=hi,ids=ids))
    for focus in [False,True]:
        for span in (spans(ann,True) if focus else full):
            boot=span['boot'];t=traces[boot];lo=int(span['lo']);hi=int(span['hi'])
            target=ann[(ann.id.isin(span['ids']))&(ann.kind=='MM')]
            mm=target[target.reason==''].to_dict('records')
            bl=ann[(ann.boot==boot)&(ann.kind=='BL')&(ann.lo>=lo)&(ann.hi<=hi)&(ann.reason=='')].to_dict('records')
            span_rows.append(span)
            for name,model in ([('A',A),('B_union',B)] if focus else [('A',A)]):
                d,stats=scan(t.adc.to_numpy(),t.valid.to_numpy(),lo,hi,model)
                rows=match_events(d,mm,bl)
                for r in rows:r.update(model=name,span=span['id'],boot=boot,direction=span['direction'],focus=focus)
                scan_rows.extend(rows)
                # BL sample occupancy and complete interval classification are different metrics.
                detected=np.zeros(hi-lo,dtype=bool)
                for dd in d:detected[int(dd[0])-lo:int(dd[1])-lo]=True
                bltotal=sum(b['hi']-b['lo'] for b in bl)
                blerr=sum(detected[b['lo']-lo:b['hi']-lo].sum() for b in bl)
                scan_summary.append(dict(model=name,span=span['id'],boot=boot,focus=focus,direction=span['direction'],
                    lo=lo,hi=hi,MM_annotation_total=len(target),MM_annotation_unresolved=int((target.reason!='').sum()),
                    detections=len(d),matched=sum(r['status']=='matched' for r in rows),
                    missed=sum(r['status']=='missed' for r in rows),
                    possible_lobe_splits=sum(r['status']=='possible_lobe_split' for r in rows),
                    inside_BL=sum(r['status']=='inside_BL' for r in rows),unassigned=sum(r['status']=='unassigned' for r in rows),
                    BL_bins=bltotal,BL_bins_as_MM=int(blerr),**stats))
                print(f"Scanned {name} {span['id']}: {len(d)} detections / {len(mm)} usable annotation envelopes",flush=True)
    pd.DataFrame(scan_rows).to_csv(ROOT/'results/continuous_events.csv',index=False)
    pd.DataFrame(scan_summary).to_csv(ROOT/'results/continuous_summary.csv',index=False)
    (ROOT/'results/scan_spans.json').write_text(json.dumps(span_rows,indent=2)+'\n')
    # Arithmetic invariance checks; never fit synthetic observations.
    q=X[train][:20];check={}
    v=q*7+1900;z,_,_=canonical(v)
    check['positive_gain_and_offset_max_error']=float(abs(z-q).max())
    z,_,_=canonical(-q*3+1900);check['negative_gain_shape_max_error']=float(abs(z-q).max())
    y=np.r_[np.zeros(13),np.sin(np.arange(71)*.13),np.zeros(9)]
    z,_,_=features(y,[0],[len(y)]);zz,_,_=features(np.repeat(y,3),[0],[3*len(y)])
    check['uniform_width_resampling_max_error']=float(abs(z-zz).max())
    impact=[]
    for i in range(len(y)):
        altered=y.copy();altered[i]+=.2
        v,_,_=features(altered,[0],[len(y)]);impact.append(float(abs(v-z).max()))
    check['every_source_bin_affects_profile']=min(impact)>0
    check['all_171_reference_MM_present']=bool(set(ann.loc[train&(ann.kind=='MM'),'mm'])==set(range(171)))
    check['all_21_B_models_present']=len(B)==21
    (ROOT/'results/checks.json').write_text(json.dumps(check,indent=2)+'\n')
    print(json.dumps(summaries,indent=2))

if __name__=='__main__':main()
