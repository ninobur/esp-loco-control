"""Movement is used only to prepare eligible continuous research observations.

This module does not inspect ADC values and is not imported by recognition.
"""
from pathlib import Path
import argparse,hashlib,json
import numpy as np
import pandas as pd

ROOT=Path(__file__).resolve().parent
PREVIOUS=ROOT.parent/'statistical-shape-20261018'
DT=20000
BOOT_B='15549e9385faa4d5'

def sha(p):
    h=hashlib.sha256()
    with open(p,'rb') as f:
        for b in iter(lambda:f.read(8*1024*1024),b''):h.update(b)
    return h.hexdigest()

def partition(boot,us):
    us=np.asarray(us)
    if boot!=BOOT_B:return np.full(len(us),'eval',dtype=object)
    return np.where((us<1840213501)|((us>=2745450837)&(us<3167014139)),'train','eval')

def runs(values):
    cut=np.r_[0,np.flatnonzero(values[1:]!=values[:-1])+1,len(values)]
    return list(zip(cut[:-1],cut[1:]))

def main():
    ap=argparse.ArgumentParser();ap.add_argument('--archive',type=Path);args=ap.parse_args()
    if args.archive:
        archive=args.archive.resolve();prior=archive/'navi-ewo-dart-experiment-1/field-records'
        quality=prior/'20261016_HALL_GROUND_PHASE2/derived/ir_quality_source.csv'
        source=prior/'20261015_IR_WAVE_1_ACCURACY/derived/native_ir.jsonl'
        snapshots=[]
        for line in open(source):
            r=json.loads(line);w=r['wire']
            snapshots.append(dict(boot=r['boot'],us=r['rx_us'],pwm_actual=r['pwm_actual'],
                pwm_commanded=r['pwm_commanded'],ctx_flags=r['ctx_flags'],readiness=r['readiness'],
                completed_pulses=w['completed_pulses'],source_us=w['captured_us']))
        state=pd.DataFrame(snapshots).drop_duplicates(['boot','us'])
        q=pd.read_csv(quality,dtype={'irboot':str}).merge(state,on=['boot','us'],validate='one_to_one')
        q.to_csv(ROOT/'sources/movement_snapshots.csv.gz',index=False,compression={'method':'gzip','mtime':0})
    else:
        # Portable replay from the fully preserved source-state/quality table.
        q=pd.read_csv(ROOT/'sources/movement_snapshots.csv.gz',dtype={'irboot':str})
    pairs=[];span_rows=[];coverage=[];source_manifest=[]
    for boot,z in q.groupby('boot'):
        z=z.sort_values('us').reset_index(drop=True)
        trace=pd.read_csv(PREVIOUS/f'data/hall20_{boot}.csv.gz')
        classification=np.full(len(trace),'uncertain_no_bracket',dtype=object)
        boot_pairs=[]
        for i in range(len(z)-1):
            a=z.iloc[i];b=z.iloc[i+1];dt=int(b.us-a.us);du=int(b.um-a.um)
            transport=(0<dt<=250000 and a.irboot==b.irboot and 0<b.sequence-a.sequence<=3)
            clean=bool(transport and a.reason==6 and b.reason==6 and a.health_fault==0 and b.health_fault==0 and
                a.pitch_um==9652 and b.pitch_um==9652 and all(a[k]==b[k] for k in
                ['sample_gaps','unreliable','saturated','inferred_added','inferred_removed']))
            held=bool((int(a.ctx_flags)|int(b.ctx_flags))&(8|32))
            motor_off=bool(a.pwm_actual==0 and b.pwm_actual==0 and a.pwm_commanded==0 and b.pwm_commanded==0)
            if clean and du>0 and not held:status='supported_moving'
            elif held and du>0:status='uncertain_pulses_while_held'
            elif du==0 and held and motor_off:status='uncertain_stop_supported'
            elif du==0:status='uncertain_no_pulse_advance'
            else:status='uncertain_transport_or_optical_quality'
            lo=int(np.ceil(a.us/DT));hi=int(b.us//DT)
            if hi>lo:classification[lo:hi]=status
            boot_pairs.append(dict(boot=boot,start_us=int(a.us),end_us=int(b.us),duration_s=dt/1e6,
                status=status,du_um=du,transport_valid=bool(transport),tracking_clean=clean,
                held_or_estop=held,motor_off=motor_off,first_sequence=int(a.sequence),last_sequence=int(b.sequence),
                reason_first=int(a.reason),reason_last=int(b.reason),physical_stationarity_confirmed=False))
        # Completed pulses are quantized. A zero-increment bracket inside one
        # uninterrupted clean/nonheld tracking run can lie between wheel pulses.
        # Retain its actual ADC observations only when bracketed by real advances
        # in that same run; never bridge stale/quality/held/excluded intervals.
        trackable=np.array([p['tracking_clean'] and not p['held_or_estop'] and p['du_um']>=0 for p in boot_pairs])
        for a0,b0 in runs(trackable):
            if not trackable[a0]:continue
            positive=[j for j in range(a0,b0) if boot_pairs[j]['du_um']>0]
            if not positive:continue
            for j in range(positive[0],positive[-1]+1):
                if boot_pairs[j]['du_um']==0:boot_pairs[j]['status']='supported_moving_interpulse'
        pairs.extend(boot_pairs)
        # Adjacent snapshot brackets may leave a partial bin on a shared edge.
        # A bin is moving only when both its endpoints are supported by clean
        # moving brackets; a common bracket edge does not manufacture a gap.
        start=z.us.to_numpy()[:-1];end=z.us.to_numpy()[1:]
        statuses=np.array([p['status'] for p in boot_pairs],dtype=object)
        left=trace.us.to_numpy()-DT//2;right=left+DT
        il=np.searchsorted(start,left,side='right')-1;ir=np.searchsorted(start,right-1,side='right')-1
        valid=(il>=0)&(ir<len(statuses));cl=np.clip(il,0,len(statuses)-1);cr=np.clip(ir,0,len(statuses)-1)
        valid &= (left>=start[cl])&(right<=end[cr])
        for status in np.unique(statuses):
            other=np.r_[0,np.cumsum(statuses!=status)]
            same=valid&((other[cr+1]-other[cl])==0)
            classification[same]=status
        classification[valid&(statuses[cl]!=statuses[cr])]='uncertain_transition_bracket'
        moving_pair=np.isin(statuses,['supported_moving','supported_moving_interpulse'])
        uncertain_prefix=np.r_[0,np.cumsum(~moving_pair)]
        eligible=valid&((uncertain_prefix[cr+1]-uncertain_prefix[cl])==0)
        classification[eligible&(statuses[cl]!=statuses[cr])]='supported_moving_counts_and_interpulse'
        classification[~trace.valid.to_numpy()]='recording_loss'
        eligible &= trace.valid.to_numpy()
        part=partition(boot,trace.us.to_numpy())
        # Any source bin straddling a train/eval boundary is excluded.
        boundary=partition(boot,left)!=partition(boot,right-1)
        eligible[boundary]=False;classification[boundary]='partition_boundary'
        table=pd.DataFrame(dict(us=trace.us,eligible=eligible,movement_status=classification,partition=part))
        table.to_csv(ROOT/f'data/eligibility_{boot}.csv.gz',index=False,compression={'method':'gzip','mtime':0})
        labels=classification.astype(str)+'/'+part.astype(str)
        for lo,hi in runs(labels):
            coverage.append(dict(boot=boot,start_us=lo*DT,end_us=hi*DT,duration_s=(hi-lo)*.02,
                status=str(classification[lo]),partition=part[lo],eligible=bool(eligible[lo])))
        joint=np.where(eligible,part,'excluded')
        for lo,hi in runs(joint):
            if not eligible[lo]:continue
            span_rows.append(dict(id=f'{boot}_{lo}_{hi}',boot=boot,lo=int(lo),hi=int(hi),
                start_us=int(lo*DT),end_us=int(hi*DT),duration_s=(hi-lo)*.02,partition=part[lo]))
    pd.DataFrame(pairs).to_csv(ROOT/'sources/movement_brackets.csv.gz',index=False,compression={'method':'gzip','mtime':0})
    spans=pd.DataFrame(span_rows);spans.to_csv(ROOT/'data/eligible_spans.csv',index=False)
    cov=pd.DataFrame(coverage);cov.to_csv(ROOT/'results/eligibility_intervals.csv',index=False)
    totals=cov.groupby(['partition','status']).agg(intervals=('duration_s','size'),seconds=('duration_s','sum')).reset_index()
    totals.to_csv(ROOT/'results/eligibility_summary.csv',index=False)
    if args.archive:
        for p in [source,quality,archive/'hall-ground-20261015/derived/stop_context.csv']:
            source_manifest.append(dict(path=str(p),sha256=sha(p)))
        (ROOT/'sources/movement_manifest.json').write_text(json.dumps(dict(inputs=source_manifest,
            policy='Clean/nonheld tracking runs bracketed by actual completed-pulse advances. Interpulse continuity explicitly distinguished; trim before first/after last advance. No stale, held or optical-quality gap bridged. No PWM-only inference.',
            confirmed_stationary_intervals=0,
            confirmation_limitation='No independent source-clock-aligned physical stationary confirmation supplied yet. Stop-supported and stale/no-pulse intervals are conservatively uncertain, excluded.'),indent=2)+'\n')
    print(totals.to_string(index=False))
    print('Eligible spans:',len(spans),'longest seconds:',spans.duration_s.max())
    seeds=pd.read_csv(PREVIOUS/'data/annotation_seeds.csv');out=[]
    for boot,z in seeds.groupby('boot'):
        t=pd.read_csv(ROOT/f'data/eligibility_{boot}.csv.gz');idx=np.clip((z.opening_us.to_numpy()//DT).astype(int),0,len(t)-1)
        r=z.copy();r['eligible_at_opening']=t.eligible.to_numpy()[idx];r['partition']=t.partition.to_numpy()[idx]
        out.append(r)
    check=pd.concat(out);check.to_csv(ROOT/'results/seed_eligibility_diagnostic.csv',index=False)
    print(check[check.eligible_at_opening].groupby('partition').mm.agg(['size','nunique']).to_string())

if __name__=='__main__':main()
