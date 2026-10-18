"""Reproducible construction and blinded ADC replay. Geography is evaluated post hoc."""
import os
os.environ.setdefault('OPENBLAS_NUM_THREADS','1')
os.environ.setdefault('OMP_NUM_THREADS','1')
from pathlib import Path
import argparse, json, time, hashlib
import numpy as np
import pandas as pd
from functional import K, PHI, basis, infer, make_prior, canonical

ROOT=Path(__file__).resolve().parent
OLD=ROOT.parent/'statistical-shape-20261018'

def dump(path,obj):
    path.write_text(json.dumps(obj,indent=2,allow_nan=False)+'\n')

def load_data():
    spans=pd.read_csv(ROOT/'data/eligible_spans.csv')
    seeds=pd.read_csv(OLD/'data/annotation_seeds.csv')
    waves={b:pd.read_csv(OLD/f'data/hall20_{b}.csv.gz') for b in spans.boot.unique()}
    return spans,seeds,waves

def objects_for(span,wave,result,model,iteration,seed):
    out=[]; ss=seed[(seed.boot==span.boot)&(seed.opening_us>=span.start_us)&(seed.opening_us<span.end_us)]
    for j,o in enumerate(result['objects']):
        lo=span.lo+o['lo']; hi=span.lo+o['hi']; start=lo*20000; end=hi*20000
        matches=ss[(ss.opening_us>=start)&(ss.opening_us<end)]
        fit=basis((np.arange(o['hi']-o['lo'])+.5)/(o['hi']-o['lo']),len(o['mean']))@o['mean']
        peak=int(np.argmax(np.abs(fit)))
        ids=matches.mm.astype(int).tolist()
        out.append(dict(id=f'{model}_{iteration}_{span.id}_{j}',span_id=span.id,boot=span.boot,partition=span.partition,
          model=model,iteration=iteration,state=o['state'],lo=lo,hi=hi,start_us=start,end_us=end,
          midpoint_us=(start+end)/2,censored=o['censored'],annotation_mms=';'.join(map(str,ids)),
          annotation_count=len(ids),principal_adc=float(fit[peak]),breadth_s=(end-start)/1e6,
          residual_rms=result['residual_rms'],mean=o['mean'].tolist(),cov=o['cov'].tolist()))
    return out

def replay(spans,waves,seeds,model,name,iteration,plots=False):
    rows=[]; begun=time.monotonic()
    for i,span in enumerate(spans.itertuples(index=False)):
        y=waves[span.boot].adc.iloc[span.lo:span.hi].to_numpy(float)
        result=infer(y,model)
        obs=objects_for(span,waves[span.boot],result,name,iteration,seeds); rows.extend(obs)
        if plots and i<6: plot_span(span,y,result,name,i)
        print(f'{name} update={iteration} {i+1}/{len(spans)} n={len(y)} mm={sum(o["state"]=="mm" for o in obs)} elapsed={time.monotonic()-begun:.1f}',flush=True)
    return rows

def moments(rows,state,default):
    selected=[o for o in rows if o['state']==state and not o['censored']]
    if not selected: return default,0
    means=[]; covs=[]
    for o in selected:
        m,c=np.array(o['mean']),np.array(o['cov'])
        if state=='mm': m,c=canonical(m,c)
        means.append(m); covs.append(c)
    M=np.array(means); mu=M.mean(axis=0)
    cov=np.mean(covs,axis=0)+(M-mu).T@(M-mu)/len(M)
    # Fixed empirical-Bayes diagonal shrinkage: K pseudo-observations.
    alpha=len(mu)/(len(M)+len(mu)); cov=(1-alpha)*cov+alpha*np.diag(np.diag(cov))
    cov+=np.eye(len(mu))*np.finfo(float).eps*max(float(np.trace(cov)),1.)
    return make_prior(mu,cov),len(M)

def initial(spans,waves):
    ys=[waves[s.boot].adc.iloc[s.lo:s.hi].to_numpy(float) for s in spans.itertuples(index=False)]
    diff=np.concatenate([np.diff(y) for y in ys if len(y)>1])
    sigma=float(1.4826*np.median(np.abs(diff-np.median(diff)))/np.sqrt(2))
    sigma=max(sigma,np.finfo(float).eps)
    centered=np.concatenate([y-np.median(y) for y in ys])
    amp=float(np.sqrt(np.mean(centered**2)))
    curv=[]
    for y in ys:
        m=np.array([np.median(y[j:j+64]) for j in range(0,len(y)-63,64)])
        if len(m)>2: curv.extend(np.diff(m,2)/64**2)
    curv=np.array(curv); scale=float(1.4826*np.median(np.abs(curv-np.median(curv))))
    strength=1/max(scale**2,np.finfo(float).eps)
    return dict(sigma=sigma,background_strength=strength,
      mm=make_prior(np.r_[amp,np.zeros(K-1)],np.diag(amp**2/(1+np.arange(K))**4)),
      bl=make_prior(np.zeros(3),np.diag((3*sigma)**2/(1+np.arange(3))**4)),
      initialization=dict(amp_rms=amp,background_second_difference_scale=scale))

def conditional(rows,model):
    reference=[]; mm_means=[]; bl_means=[]
    for mm in range(50,71):
        mmobs=[o for o in rows if o['state']=='mm' and not o['censored'] and o['annotation_mms']==str(mm)]
        blobs=[]
        for o in mmobs:
            same=[v for v in rows if v['span_id']==o['span_id']]
            j=next(i for i,v in enumerate(same) if v['id']==o['id'])
            blobs.extend(v for v in same[max(0,j-1):j+2] if v['state']=='bl' and not v['censored'])
        reference.append(dict(mm=mm,magnetic_n=len(mmobs),baseline_n=len(blobs),
                              magnetic_ids=';'.join(o['id'] for o in mmobs),baseline_ids=';'.join(o['id'] for o in blobs)))
        if mmobs:
            means=[canonical(np.array(o['mean']),np.array(o['cov']))[0] for o in mmobs]
            # One pooled-distribution pseudo-observation, fixed before evaluation.
            mm_means.append((np.sum(means,axis=0)+np.array(model['mm']['mean']))/(len(means)+1))
        if blobs: bl_means.append((np.sum([o['mean'] for o in blobs],axis=0)+np.array(model['bl']['mean']))/(len(blobs)+1))
    if not mm_means or not bl_means: raise RuntimeError('No geographic reference constructed; cannot evaluate Model B')
    b=dict(model); b['mm']=dict(model['mm'],means=np.array(mm_means).tolist()); b['bl']=dict(model['bl'],means=np.array(bl_means).tolist())
    return b,reference

def save_rows(path,rows):
    dump(path.with_suffix('.json'),rows)
    pd.DataFrame([{k:v for k,v in o.items() if k not in ('mean','cov')} for o in rows]).to_csv(path.with_suffix('.csv'),index=False)

def plot_span(s,y,r,name,i):
    import matplotlib
    matplotlib.use('Agg')
    import matplotlib.pyplot as plt
    x=(np.arange(len(y))+s.lo)*.02
    fig,axs=plt.subplots(2,1,figsize=(14,6),sharex=True)
    axs[0].plot(x,y,color='black',lw=.6,label='raw 20 ms ADC')
    axs[0].plot(x,r['baseline'],color='green',lw=1,label='shared background')
    axs[0].plot(x,r['baseline']+r['field'],color='orange',lw=1,label='whole-state fit')
    for o in r['objects']:
        color='red' if o['state']=='mm' else 'blue'
        axs[0].axvspan((o['lo']+s.lo)*.02,(o['hi']+s.lo)*.02,color=color,alpha=.1)
    axs[1].plot(x,y-r['baseline'],color='black',lw=.6)
    axs[1].plot(x,r['field'],color='orange',lw=1)
    axs[1].set_ylim(-30,30); axs[1].set_ylabel('ADC relative to inferred background; ±30 detail')
    axs[0].legend(); axs[0].set_ylabel('ADC'); axs[1].set_xlabel('source seconds')
    axs[0].set_title(f'{name}: {s.boot} {s.start_us/1e6:.2f}–{s.end_us/1e6:.2f}; red MM, blue BL; edges censored')
    fig.tight_layout(); fig.savefig(ROOT/f'figures/{name}_{i:02d}.png',dpi=130); plt.close(fig)

def main():
    ap=argparse.ArgumentParser(); ap.add_argument('stage',choices=['train','evaluate']); args=ap.parse_args()
    spans,seeds,waves=load_data(); train=spans[spans.partition=='train']; evaluation=spans[spans.partition=='eval']
    if args.stage=='train':
        model=initial(train,waves); dump(ROOT/'models/initial.json',model)
        for iteration in range(3):
            rows=replay(train,waves,seeds,model,'A_train',iteration)
            save_rows(ROOT/f'results/training_{iteration}',rows)
            model['mm'],nm=moments(rows,'mm',model['mm']); model['bl'],nb=moments(rows,'bl',model['bl'])
            model['training_counts']=dict(mm=nm,bl=nb,update=iteration)
            dump(ROOT/f'models/training_{iteration}.json',model)
        # The final update is replayed for provenance, no further parameter update.
        rows=replay(train,waves,seeds,model,'A_train',3,True); save_rows(ROOT/'results/training_final',rows)
        dump(ROOT/'models/A.json',model); b,refs=conditional(rows,model); dump(ROOT/'models/B.json',b)
        pd.DataFrame(refs).to_csv(ROOT/'results/geographic_references.csv',index=False)
    else:
        for name in ('A','B'):
            model=json.loads((ROOT/f'models/{name}.json').read_text())
            rows=replay(evaluation,waves,seeds,model,name,0,True)
            save_rows(ROOT/f'results/evaluation_{name}',rows)
        dump(ROOT/'results/model_freeze.json',{n:hashlib.sha256((ROOT/f'models/{n}.json').read_bytes()).hexdigest() for n in ('A','B')})

if __name__=='__main__': main()
