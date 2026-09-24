"""Offline analysis of the 2026-09-23 final Toby R3 session. No network/control."""
import argparse, collections, datetime, gzip, json
from pathlib import Path

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('capture',nargs='?',type=Path,default=Path(__file__).parent/'all_20260923.log')
parser.add_argument('--out',type=Path,default=Path(__file__).parent)
args=parser.parse_args()
ROOT=args.out
ROOT.mkdir(parents=True,exist_ok=True)
if args.capture.resolve()==(ROOT/'20260923_toby_final_r3.log.gz').resolve():
    raise SystemExit('Choose a separate output directory; never overwrite input evidence')
START='2026-09-23T21:58:48.578'
rows=[]
opener=gzip.open if args.capture.suffix=='.gz' else open
with opener(args.capture,'rt') as f, gzip.open(ROOT/'20260923_toby_final_r3.log.gz','wt') as out:
    for line in f:
        parts=line.rstrip('\n').split('\t',2)
        if len(parts)!=3 or parts[0]<START: continue
        ts,topic,payload=parts
        if not (topic.startswith('ngr/loco/9950012/') or topic.startswith('ngr/dispatcher/')):continue
        out.write(line)
        try: value=json.loads(payload)
        except ValueError:value=payload
        rows.append((ts,topic,value))

def get(suffix):return [(t,v) for t,k,v in rows if k=='ngr/loco/9950012/'+suffix]
def counts(items,key):return dict(collections.Counter(v.get(key) for t,v in items))
nav=get('state/nav');health=get('diag/ir_health');ir=get('telem/ir');rec=get('diag/recovery');alerts=get('alert')
def changes(items,key):
    result=[];previous=object()
    for t,v in items:
        val=v.get(key) if isinstance(v,dict) else v
        if val!=previous:result.append([t,val]);previous=val
    return result
summary={
 'start':rows[0][0],'end':rows[-1][0],'rows':len(rows),
 'topics':dict(collections.Counter(k.split('9950012/')[-1] for _,k,_ in rows)),
 'boot':get('state/bootid'),'nav_count':len(nav),'nav_first':nav[:3],'nav_last':nav[-3:],
 'nav_rulings':counts(nav,'ruling'),'nav_evidence':counts(nav,'evidence'),'nav_states':counts(nav,'nav_state'),
 'correction_changes':changes(nav,'corrections'),'recovery_reasons':counts(rec,'reason'),
 'recovery_exceptions':[(t,v) for t,v in rec if v.get('reason') not in ('INCUMBENT_BEST','WARMUP','ISOLATED_DISAGREEMENT')],
 'nav_exceptions':[(t,v) for t,v in nav if v.get('ruling') not in ('ADVANCED','NONE')],
 'health_counts':counts(health,'health'),'readiness_counts':counts(health,'readiness'),
 'epoch_changes':changes(health,'epoch'),'health_last':health[-1:],
 'ir_reasons':counts(ir,'navi_speed_reason'),'raw_ir_reasons':counts(ir,'ir_speed_reason'),
 'ir_first':ir[:1],'ir_last':ir[-1:],'auto_changes':changes(get('state/auto'),''),
 'warning_changes':changes(get('state/warning'),''),'alerts_last':alerts[-1:],
 'commands':[(t,k,v) for t,k,v in rows if '/cmd/' in k],
}
(ROOT/'summary.json').write_text(json.dumps(summary,indent=2)+'\n')
print('Summary saved:', len(rows), 'rows')

import bisect, statistics
def sec(t):return datetime.datetime.fromisoformat(t).timestamp()
ats=[sec(t) for t,v in alerts]
def nearest_alert(t):
    x=sec(t);i=bisect.bisect_left(ats,x)
    choices=[j for j in (i-1,i) if 0<=j<len(ats)]
    j=min(choices,key=lambda j:abs(ats[j]-x))
    return alerts[j][1],abs(ats[j]-x)
stations=get('state/station')
ends=[];lastend=0
for t,v in health:
    if v['epoch_ends']>lastend:
        a,age=nearest_alert(t)
        ends.append(dict(time=t,epoch=v['epoch'],reason=v['epoch_reason'],health=v['health'],pwm=a['pwm'],mm=a['mm'],alert_offset_s=round(age,3)))
    lastend=v['epoch_ends']
zero=[];start=None
for t,a in alerts:
    if a['pwm']==0:
        if start is None:start=t
    elif start is not None:zero.append((start,t));start=None
if start:zero.append((start,alerts[-1][0]))
stops=[]
for a,b in zero:
    samples=[(t,v) for t,v in ir if a<=t<b]
    settled=[(t,v) for t,v in samples if sec(t)-sec(a)>=3]
    stops.append(dict(start=a,end=b,duration=round(sec(b)-sec(a),2),mm=nearest_alert(a)[0]['mm'],
        counts=counts(samples,'navi_speed_reason'),settled_counts=counts(settled,'navi_speed_reason'),
        pulse_range=[min((v['pulses'] for t,v in samples),default=None),max((v['pulses'] for t,v in samples),default=None)],
        first_stopped=next((t for t,v in samples if v['navi_speed_reason']=='STOPPED'),None)))
cmps=collections.defaultdict(list)
for t,v in get('nav/ir_compare'):cmps[v['event_serial']].append(v)
events=[(t,v) for t,v in nav if 'event_serial' in v]
assert [v['event_serial'] for t,v in events]==list(range(1,1222))
selected=[];rejections=[];missed=[];progress=0;prev=51
for t,v in events:
    progress+=(prev-v['mm'])%171;prev=v['mm']
    candidates=cmps[v['event_serial']]
    chosen=next((x for x in candidates if x['candidate_mm']==v['mm'] and x['steps']>0),None)
    if v['ruling'] in ('ADVANCED','MISSED_AND_ADVANCED') and chosen and chosen['mapped_mm'] and chosen['residual_mm'] is not None:
        selected.append(dict(time=t,event=v['event_serial'],mm=v['mm'],**{k:chosen[k] for k in ['steps','ir_nominal_mm','mapped_mm','residual_mm']}))
    if v['ruling']=='NON_LANDMARK_HALL':
        one=next((x for x in candidates if x['steps']==1),{})
        rejections.append(dict(time=t,event=v['event_serial'],mm=v['mm'],pwm=v['pwm_open'],candidate=one))
    if v['ruling']=='MISSED_AND_ADVANCED':missed.append(dict(time=t,event=v['event_serial'],mm=v['mm'],candidate=chosen))
res=[x['residual_mm']/x['mapped_mm']*100 for x in selected]
detail=dict(station_counts=dict(collections.Counter((v.get('station','')+':'+v.get('event','')) for t,v in stations)),
    epochs_ended=ends,stops=stops,progress_markers=progress,rejections=rejections,missed=missed,
    accepted_distance_comparisons=dict(n=len(res),median_signed_pct=statistics.median(res),
        median_absolute_pct=statistics.median(abs(x) for x in res),min_pct=min(res),max_pct=max(res)),
    compare_samples=selected,
    moving_unavailable=dict(collections.Counter(v['navi_speed_reason'] for t,v in ir if nearest_alert(t)[0]['pwm']>0 and not v['navi_speed_valid'])))
lap_counts=[s['pulse_range'][0] for s in stops if s['mm']==60]
lap_deltas=[b-a for a,b in zip(lap_counts,lap_counts[1:])]
detail['grillers_laps']={'stopped_counts':lap_counts,'pulse_deltas':lap_deltas,
    'nominal_mm':[round(n*9.652,3) for n in lap_deltas]}
detail['loopstat_last']=get('state/loopstat')[-1:]
detail['voltage_min']=min(float(v) for t,v in get('telem/voltage'))
detail['hall_speed_last']=get('telem/speed')[-1:]
(ROOT/'detail.json').write_text(json.dumps(detail,indent=2)+'\n')
print('Detail saved:',len(events),'consecutive Hall events;',progress,'marker steps;',len(stops)-1,'post-start zero-PWM episodes')
