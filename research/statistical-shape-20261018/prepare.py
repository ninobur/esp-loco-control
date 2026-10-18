"""Build portable Hall-only evidence; no operational imports or control outputs."""
from pathlib import Path
import argparse, hashlib, json
import numpy as np
import pandas as pd

ROOT = Path(__file__).resolve().parent
BOOT = '15549e9385faa4d5'
DT = 20_000
NATIVE = np.dtype([('serial','<u4'), ('us','<u8'), ('adc','<i2'),
                   ('pwm','u1'), ('direction','u1')])

def sha(path):
    h = hashlib.sha256()
    with open(path, 'rb') as f:
        for chunk in iter(lambda: f.read(8*1024*1024), b''):
            h.update(chunk)
    return h.hexdigest()

def split(boot, us):
    if boot != BOOT:
        return 'eval'
    return 'train' if us < 1840213501 or 2745450837 <= us < 3167014139 else 'eval'

def envelope(y, tail=.001):
    """Annotation convention, never called by continuous recognition."""
    k = max(1, len(y)//10)
    line = np.linspace(np.median(y[:k]), np.median(y[-k:]), len(y))
    energy = (y-line)**2
    if energy.sum() == 0:
        return None
    c = np.cumsum(energy)/energy.sum()
    return max(0, int(np.searchsorted(c, tail))-1), min(len(y), int(np.searchsorted(c, 1-tail))+2)

def build_annotations(traces, seeds, tail=.001):
    rows=[]
    seeds=seeds.sort_values(['boot','opening_us']).reset_index(drop=True)
    for (boot, direction), z in seeds.groupby(['boot','direction'], sort=False):
        z=z.sort_values('opening_us').to_dict('records')
        t=traces[boot]
        for i,r in enumerate(z):
            a=int(np.searchsorted(t.us, r['opening_us']))
            reason=''
            step=1 if direction=='CW' else -1
            prev=z[i-1] if i else None
            nxt=z[i+1] if i+1<len(z) else None
            if prev is None or nxt is None:
                reason='passage_edge'
            elif (int(r['mm'])-int(prev['mm']))%171 != step%171 or (int(nxt['mm'])-int(r['mm']))%171 != step%171:
                reason='nonconsecutive_annotation'
            else:
                # The intervening opposite-direction annotation defines a reversal,
                # even if separated direction-filtered labels happen to be consecutive.
                between=seeds[(seeds.boot==boot)&seeds.opening_us.between(prev['opening_us'],nxt['opening_us'])]
                if between.direction.nunique()!=1:
                    reason='direction_change'
            lo=max(0,a-1);hi=min(len(t),a+2)
            if not reason:
                lo=int(np.searchsorted(t.us, (prev['opening_us']+r['opening_us'])/2))
                hi=int(np.searchsorted(t.us, (nxt['opening_us']+r['opening_us'])/2))
                if hi-lo<3 or not t.valid.iloc[lo:hi].all():
                    reason='cell_recording_loss'
                else:
                    e=envelope(t.adc.iloc[lo:hi].to_numpy(),tail)
                    if e is None:
                        reason='constant_cell'
                    else:
                        lo,hi=lo+e[0],lo+e[1]
            partition=split(boot,r['opening_us'])
            if split(boot,int(t.us.iloc[lo])-DT//2)!=partition or split(boot,int(t.us.iloc[hi-1])+DT//2)!=partition:
                reason=reason or 'partition_boundary'
            rows.append(dict(id=f"MM_{boot}_{int(r['opening_serial'])}",boot=boot,
                kind='MM',mm=int(r['mm']),direction=direction,split=partition,
                opening_us=int(r['opening_us']),opening_serial=int(r['opening_serial']),
                prior_status=r['match_status'],lo=lo,hi=hi,reason=reason,
                annotation='provisional_energy_envelope'))
    mm=pd.DataFrame(rows).sort_values(['boot','opening_us'])
    out=mm.to_dict('records')
    for boot,z in mm.groupby('boot'):
        rec=z.to_dict('records')
        for r,nxt in zip(rec,rec[1:]):
            step=1 if r['direction']=='CW' else -1
            if nxt['direction']!=r['direction'] or (nxt['mm']-r['mm'])%171!=step%171:
                continue
            lo=r['hi'];hi=nxt['lo']
            reason=r['reason'] or nxt['reason']
            if hi-lo<2:
                reason=reason or 'no_resolved_baseline_complement'
                hi=max(lo+1,hi)
            if nxt['split']!=r['split']:
                reason=reason or 'partition_boundary'
            t=traces[boot]
            if not t.valid.iloc[lo:hi].all():
                reason=reason or 'baseline_recording_loss'
            out.append(dict(r,id=r['id'].replace('MM_','BL_'),kind='BL',lo=lo,hi=hi,reason=reason,
                            annotation='complement_between_provisional_envelopes'))
    df=pd.DataFrame(out).fillna('')
    for boot,ix in df.groupby('boot').groups.items():
        t=traces[boot]
        for i in ix:
            lo=int(df.at[i,'lo']);hi=min(len(t),int(df.at[i,'hi']))
            df.at[i,'hi']=hi
            q=t.iloc[lo:hi]
            df.at[i,'start_us']=int(q.us.iloc[0])-DT//2
            df.at[i,'end_us']=int(q.us.iloc[-1])+DT//2
            df.at[i,'first_native_row']=int(q.first_row.iloc[0])
            df.at[i,'end_native_row_exclusive']=int(q.end_row.iloc[-1])
            df.at[i,'first_serial']=int(q.first_serial.iloc[0])
            df.at[i,'last_serial']=int(q.last_serial.iloc[-1])
            finite=q.adc.dropna()
            df.at[i,'annotation_polarity']=(1 if abs(finite.max()-finite.median())>=abs(finite.min()-finite.median()) else -1) if len(finite) else 0
    return df.sort_values(['boot','start_us','kind']).reset_index(drop=True)

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--source-root',type=Path,required=True)
    args=ap.parse_args();src=args.source_root.resolve();prior=src/'hall-ground-20261015'
    integrity=json.loads((src/'navi-rvm-baseline-extent-20261017/derived/source_validation.json').read_text())
    manifest={'base_commit':'6aea978ae45e7164d176dfdda6d38ff41e8f272b','inputs':[],
        'native_dtype':NATIVE.descr,'bin_us':DT,'annotation_status':'unverified NAVI geography + research envelope convention'}
    traces={}
    for boot in ['4d8083de79a0e299', BOOT]:
        p=prior/f'derived/native_{boot}.bin';digest=sha(p)
        assert digest==integrity['arrays'][boot]['sha256']
        x=np.memmap(p,mode='r',dtype=NATIVE)
        times=x['us'].astype('int64');b=times//DT
        cut=np.r_[0,np.flatnonzero(np.diff(b))+1,len(x)]
        count=np.diff(cut);idx=b[cut[:-1]];n=int(b[-1])+1
        data=dict(us=np.arange(n,dtype='int64')*DT+DT//2,adc=np.full(n,np.nan),
                  count=np.zeros(n,dtype='int64'),first_row=np.full(n,-1),end_row=np.full(n,-1),
                  first_serial=np.full(n,-1),last_serial=np.full(n,-1),valid=np.zeros(n,dtype=bool))
        data['adc'][idx]=np.add.reduceat(x['adc'].astype(float),cut[:-1])/count
        data['count'][idx]=count;data['first_row'][idx]=cut[:-1];data['end_row'][idx]=cut[1:]
        data['first_serial'][idx]=x['serial'][cut[:-1]];data['last_serial'][idx]=x['serial'][cut[1:]-1]
        data['valid'][idx]=True
        gaps=np.flatnonzero((np.diff(times)>5000)|(np.diff(x['serial'].astype('int64'))!=1))
        for g in gaps:
            data['valid'][b[g]:b[g+1]+1]=False
        trace=pd.DataFrame(data);traces[boot]=trace
        out=ROOT/f'data/hall20_{boot}.csv.gz';trace.to_csv(out,index=False,float_format='%.12g',compression={'method':'gzip','mtime':0})
        manifest['inputs'].append(dict(path=str(p),sha256=digest,samples=len(x),first_us=int(times[0]),last_us=int(times[-1]),
            portable_file=str(out.relative_to(ROOT)),portable_sha256=sha(out),discontinuities=len(gaps)))
    p=prior/'derived/selected_matches.csv';s=pd.read_csv(p)
    cols=['boot','mm','direction','opening_serial','opening_us','match_status']
    s=s[cols].drop_duplicates(['boot','opening_serial','mm','direction'])
    s.to_csv(ROOT/'data/annotation_seeds.csv',index=False)
    manifest['inputs'].append(dict(path=str(p),sha256=sha(p),used_columns=cols,
        meaning='NAVI selected labels; location and selection bias, not physical truth'))
    for tail in [.001,.005,.01]:
        build_annotations(traces,s,tail).to_csv(ROOT/f'data/annotations_tail_{tail:g}.csv',index=False)
    batches=pd.read_csv(prior/'derived/batches.csv.gz')
    batches[batches.stream=='native'][['boot','offset','row_start','n','first_us','last_us']].to_csv(ROOT/'data/native_frame_index.csv.gz',index=False,compression={'method':'gzip','mtime':0})
    manifest['inputs'].append(dict(path=str(prior/'derived/batches.csv.gz'),sha256=sha(prior/'derived/batches.csv.gz')))
    nsr=src/'navi-ewo-dart-experiment-1/field-records/20261015_IR_WAVE_1_ACCURACY/sources/native.nsr'
    manifest['nsr']={'path':str(nsr),'sha256':sha(nsr),'previous_payload_verification':integrity}
    assert manifest['nsr']['sha256']==integrity['sha256']
    # Inventory nearby evidence without adding it to the October 15 models.
    candidates=[]
    for directory in ['otto-hall-repeatability-20261013','hall-field-analysis-20260929','waveforms','field_runs']:
        for p in sorted((src/directory).rglob('*')):
            if p.is_file() and p.suffix.lower() in ['.log','.gz','.csv','.nsr'] and '/tmp/' not in str(p):
                candidates.append(dict(path=str(p),bytes=p.stat().st_size,used=False,
                    reason='Different date/stream; not audited as equivalent native 20-ms evidence in this bounded experiment'))
    pd.DataFrame(candidates).to_csv(ROOT/'sources/additional_recordings.csv',index=False)
    (ROOT/'sources/input_manifest.json').write_text(json.dumps(manifest,indent=2)+'\n')
    print(json.dumps({'inputs':manifest['inputs'],'annotation_seeds':len(s)},indent=2))

if __name__=='__main__':main()
