#!/usr/bin/env python3
"""Reproduce the September 20 noon mixed-light run audit; no pulse repairs."""
import argparse
import binascii
import bisect
import collections
import datetime as dt
import json
import re
from zoneinfo import ZoneInfo

from ir_movement_decode import FIELDS, REASONS, WIRE
from ir_scope_espnow_analyze import HDR, SAMPLES

TZ = ZoneInfo('America/Los_Angeles')
NOTES = [
    ('Start annotation (following interval spans mixed-light loop)', 1789931526.581727),
    ('Mostly sun starts', 1789931761.695502),
    ('Very sunny', 1789931810.755317),
    ('Shade starts', 1789931874.108704),
    ('Mostly sun starts again', 1789931967.622318),
    ('Complete lap reported', 1789931977.599300),
    ('Final stop annotation', 1789932202.051559),
]

def stamp(t):
    return dt.datetime.fromtimestamp(t, TZ).isoformat(timespec='milliseconds')

def quant(v, p):
    v = sorted(v)
    return v[int((len(v)-1)*p)] if v else None

def stats(v):
    return dict(n=len(v), minimum=min(v) if v else None,
                p05=quant(v,.05), median=quant(v,.5), p95=quant(v,.95),
                maximum=max(v) if v else None)

def parse_ir(path):
    raw, motion = [], []
    bad = collections.Counter()
    with open(path, errors='replace') as source:
        for line in source:
            s = line.split()
            if len(s) != 7 or s[1] != 'RX':
                continue
            try:
                t, rssi, data = float(s[0]), int(s[3]), bytes.fromhex(s[6])
                if len(data) != int(s[4]) or binascii.crc_hqx(data,65535) != int(s[5],16):
                    bad['transport'] += 1
                    continue
            except ValueError:
                bad['malformed'] += 1
                continue
            if data[:4] not in (b'RI\x01\x01', b'RI\x01\x05'):
                continue
            if binascii.crc_hqx(data[:-2],65535) != int.from_bytes(data[-2:],'little'):
                bad['payload'] += 1
                continue
            if data[3] == 1 and len(data) == 250:
                h = HDR.unpack_from(data)
                samples = SAMPLES.unpack_from(data, HDR.size)[:h[6]]
                raw.append(dict(t=t, rssi=rssi, boot=h[3], seq=h[4], sample=h[5],
                    count=h[6], span=h[9]-h[8], missed=h[13], drops=h[14],
                    senderr=h[15], pulses=h[16], samples=samples))
            elif data[3] == 5 and len(data) == 110:
                x = dict(zip(FIELDS, WIRE.unpack(data)))
                x.update(t=t, rssi=rssi, boot=x['boot_id'], seq=x['sequence'])
                motion.append(x)
    return raw, motion, dict(bad)

def reception(rows):
    if not rows:
        return {}
    groups = collections.defaultdict(list)
    for x in rows:
        groups[x['boot']].append(x)
    expected = sum(max(x['seq'] for x in g)-min(x['seq'] for x in g)+1 for g in groups.values())
    received = sum(len({x['seq'] for x in g}) for g in groups.values())
    return dict(received=received, expected=expected,
                loss_pct=100*(1-received/expected), rssi=stats([x['rssi'] for x in rows]))

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('ir'); p.add_argument('hall'); p.add_argument('route_header')
    args = p.parse_args()
    with open(args.route_header) as f:
        initializer = re.search(r'ROUTE_SPACING_MM\[ROUTE_N\]\s*=\s*\{([^}]+)\}', f.read()).group(1)
    spacing = [int(v.strip()) for v in initializer.split(',') if v.strip()]
    assert len(spacing) == 171 and sum(spacing) == 52150
    raw, motion, bad = parse_ir(args.ir)
    # Only the repaired-car noon sender session; never mix yesterday's evidence.
    raw = [x for x in raw if x['boot'] == 0xe4410597]
    motion = [x for x in motion if x['boot'] & 0xffffffff == 0xe4410597]
    assert len({x['boot'] for x in motion}) == 1
    offsets = [x['t']-x['captured_us']/1e6 for x in motion]
    offset = quant(offsets,.05)
    for x in motion:
        x['aligned_t'] = offset+x['captured_us']/1e6
    motion.sort(key=lambda x:x['aligned_t'])
    times = [x['aligned_t'] for x in motion]

    def near(t):
        i = bisect.bisect_left(times,t)
        return min(motion[max(0,i-1):min(len(motion),i+1)], key=lambda x:abs(x['aligned_t']-t))

    alerts, markers, events, metadata = [], [], [], None
    with open(args.hall, errors='replace') as source:
        for line in source:
            s = line.rstrip().split('\t',2)
            if len(s)!=3 or '/9950012/' not in s[1]:
                continue
            try:
                t = dt.datetime.fromisoformat(s[0]).replace(tzinfo=TZ).timestamp()
                d = json.loads(s[2])
            except ValueError:
                continue
            if not NOTES[0][1]-180 <= t <= NOTES[-1][1]+60:
                continue
            if s[1].endswith('/alert'):
                alerts.append(dict(t=t, **d))
            elif s[1].endswith('/diag/wave_meta'):
                metadata = (t,d)
            elif s[1].endswith('/mm/marker'):
                markers.append(dict(t=t,**d))
                if d.get('ruling') == 'ADVANCED' and d.get('why') == 'MAGNET' and metadata and 0<=t-metadata[0]<1:
                    events.append(dict(receipt=t, close_ms=metadata[1]['close_ms'], **d))
                metadata = None
    for e in events:
        local = [x['t']-x['uptime_ms']/1000 for x in alerts if abs(x['t']-e['receipt'])<30]
        e['t'] = e['close_ms']/1000+quant(local,.05)
        x = near(e['t'])
        e.update(ir_count=x['completed_pulses'], ir_seq=x['sequence'],
                 ir_offset_ms=(x['aligned_t']-e['t'])*1000,
                 unreliable=x['unreliable_samples'], reason=REASONS[x['optical_reason']])
        i = bisect.bisect_left(times,e['t'])
        lo,hi = motion[max(0,i-1)],motion[min(len(motion)-1,i)]
        e['bracket_counts'] = [lo['completed_pulses'],hi['completed_pulses']]
        e['bracket_seconds'] = hi['aligned_t']-lo['aligned_t']
    events.sort(key=lambda x:x['t'])
    transitions=[]
    for x in alerts:
        state = (x.get('nav_state'),x.get('powered'),x.get('dir'))
        if not transitions or state != transitions[-1]['state']:
            transitions.append(dict(time=stamp(x['t']),t=x['t'],state=state,mm=x['mm']))
    annotations=[]
    for label,t in NOTES:
        before = [x for x in events if x['t']<=t]
        after = [x for x in events if x['t']>t]
        annotations.append(dict(label=label,time=stamp(t),previous_mm=before[-1]['mm'] if before else None,
            previous_time=stamp(before[-1]['t']) if before else None,
            next_mm=after[0]['mm'] if after else None,next_time=stamp(after[0]['t']) if after else None))
    phases=[]
    for (label,a),(_,b) in zip(NOTES,NOTES[1:]):
        rr=[x for x in raw if a<=x['t']<b]
        mm=[x for x in motion if a<=x['aligned_t']<b]
        values=[v&4095 for x in rr for v in x['samples']]
        result=dict(label=label,start=stamp(a),end=stamp(b),raw=reception(rr),movement=reception(mm),
            span=stats([x['span'] for x in rr]),adc=stats(values),
            adc_rail_samples=sum(v==0 or v==4095 for v in values))
        if mm:
            result['cumulative_delta']={k:mm[-1][k]-mm[0][k] for k in ('completed_pulses','unreliable_samples','saturated_samples','sample_gaps','open_aborts')}
            result['seconds']=(mm[-1]['captured_us']-mm[0]['captured_us'])/1e6
            result['reasons']=dict(collections.Counter(REASONS[x['optical_reason']] for x in mm))
        phases.append(result)
    intervals=[]
    for a,b in zip(events,events[1:]):
        if a['dir']!='CW' or b['dir']!='CW' or (b['mm']-a['mm'])%171!=1:
            continue
        if max(abs(a['ir_offset_ms']),abs(b['ir_offset_ms']))>150:
            continue
        count=b['ir_count']-a['ir_count']
        distance=spacing[a['mm']]
        intervals.append(dict(start=stamp(a['t']),start_mm=a['mm'],end_mm=b['mm'],seconds=b['t']-a['t'],
            pulses=count,map_mm=distance,nominal_mm=count*9.652,ratio=count*9.652/distance,
            unreliable_delta=b['unreliable']-a['unreliable'],offset_ms=max(abs(a['ir_offset_ms']),abs(b['ir_offset_ms']))))
    laps=[]
    anchors=[(i,x) for i,x in enumerate(events) if x['mm']==41 and x['dir']=='CW']
    for (ia,a),(ib,b) in zip(anchors,anchors[1:]):
        valid=ib-ia==171 and all((y['mm']-x['mm'])%171==1 and y['dir']==x['dir']=='CW' for x,y in zip(events[ia:ib],events[ia+1:ib+1]))
        if valid:
            count=b['ir_count']-a['ir_count']
            laps.append(dict(start=stamp(a['t']),end=stamp(b['t']),pulses=count,nominal_mm=count*9.652,
                map_mm=sum(spacing),ratio=count*9.652/sum(spacing),endpoint_offset_ms=max(abs(a['ir_offset_ms']),abs(b['ir_offset_ms'])),
                unreliable_delta=b['unreliable']-a['unreliable']))
    # Labels are inferred route zones, excluding several markers at each boundary.
    # The first annotation-to-sun window spans a whole loop: it is NOT all shade.
    event_times = [x['t'] for x in events]
    def zone_at(t):
        i = bisect.bisect_right(event_times,t)-1
        if i<0 or i>=len(events)-1 or events[i+1]['t']-events[i]['t']>10:
            return 'unknown'
        mm = events[i]['mm']
        return 'sun' if 70<=mm<=155 else 'shade' if mm>=164 or mm<=61 else 'boundary'
    zones={}
    for label in ('sun','shade','boundary'):
        rr=[x for x in raw if zone_at(x['t'])==label]
        mm=[x for x in motion if zone_at(x['aligned_t'])==label]
        # Sum individual contiguous route visits, never count intervening zones as loss.
        def visits(rows,sequence_rows):
            runs=[];g=[]
            for x in sequence_rows:
                z=zone_at(x.get('aligned_t',x['t']))
                if z==label:g.append(x)
                elif g:runs.append(g);g=[]
            if g:runs.append(g)
            rs=[reception(g) for g in runs]
            expected=sum(x['expected'] for x in rs);received=sum(x['received'] for x in rs)
            return dict(received=received,expected=expected,loss_pct=100*(1-received/expected),
                rssi=stats([x['rssi'] for x in rows]))
        values=[v&4095 for x in rr for v in x['samples']]
        zones[label]=dict(raw=visits(rr,raw),movement=visits(mm,motion),
            span=stats([x['span'] for x in rr]),adc=stats(values),rail_samples=sum(v==0 or v==4095 for v in values),
            reasons=dict(collections.Counter(REASONS[x['optical_reason']] for x in mm)))
    a,b=events[0],events[-1]
    assert len(events)==513 and all((y['mm']-x['mm'])%171==1 and x['dir']==y['dir']=='CW' for x,y in zip(events,events[1:]))
    full_mm=sum(spacing[x['mm']] for x in events[:-1])
    full_count=b['ir_count']-a['ir_count']
    full=dict(start=stamp(a['t']),end=stamp(b['t']),start_mm=a['mm'],end_mm=b['mm'],intervals=len(events)-1,
        pulses=full_count,map_mm=full_mm,nominal_mm=full_count*9.652,ratio=full_count*9.652/full_mm,
        count_bracket=[b['bracket_counts'][0]-a['bracket_counts'][1],b['bracket_counts'][1]-a['bracket_counts'][0]],
        endpoint_offset_ms=[a['ir_offset_ms'],b['ir_offset_ms']])
    active=[x for x in motion if a['t']<=x['aligned_t']<=b['t']]
    full['diagnostic_deltas']={k:active[-1][k]-active[0][k] for k in ('completed_pulses','unreliable_samples','saturated_samples','sample_gaps','open_aborts')}
    full['reasons']=dict(collections.Counter(REASONS[x['optical_reason']] for x in active))
    active_raw=[x for x in raw if a['t']<=x['t']<=b['t']]
    full['raw_reception']=reception(active_raw)
    full['movement_reception']=reception(active)
    full['raw_counter_deltas']={k:active_raw[-1][k]-active_raw[0][k] for k in ('missed','drops','senderr')}
    # Rise-to-rise timing is audited only across consecutive received sample indices.
    periods=[];period_runs=[];run=[];previous_sample=None;previous_rise=None
    for x in active_raw:
        for i,v in enumerate(x['samples']):
            sample=x['sample']+i
            if previous_sample is None or sample!=previous_sample+1:
                if run:period_runs.append(run)
                run=[];previous_rise=None
            if v&0x2000:
                if previous_rise is not None:
                    periods.append(sample-previous_rise)
                    run.append((sample,sample-previous_rise))
                previous_rise=sample
            previous_sample=sample
    full['continuous_rise_period_ms']=stats(periods)
    full['periods_under_20ms']=sum(v<20 for v in periods)
    if run:period_runs.append(run)
    tested=0;candidates=[]
    for run in period_runs:
        for i in range(2,len(run)-2):
            neighbors=[run[j][1] for j in (i-2,i-1,i+1,i+2)]
            reference=quant(neighbors,.5)
            if max(neighbors)/min(neighbors)>1.3:continue
            tested+=1
            ratio=run[i][1]/reference
            if ratio<.6 or ratio>1.6:
                candidates.append(dict(sample=run[i][0],period_ms=run[i][1],reference_ms=reference,ratio=ratio))
    full['isolated_period_screen']=dict(tested=tested,candidates=candidates,
        rule='Four contiguous neighboring periods within 30%; candidate below 0.6x or above 1.6x lower median. Diagnostic only, no corrections.')
    # Equal, long route blocks reduce count quantization and timestamp uncertainty.
    blocks=[]
    for start,end in ((70,155),(164,61),(41,65),(0,40)):
        pending=None
        for i,e in enumerate(events):
            if e['mm']==start:pending=(i,e)
            if e['mm']==end and pending:
                j,x=pending
                if i-j==(end-start)%171:
                    dist=sum(spacing[k['mm']] for k in events[j:i]);count=e['ir_count']-x['ir_count']
                    blocks.append(dict(start_mm=start,end_mm=end,start=stamp(x['t']),end=stamp(e['t']),
                        pulses=count,map_mm=dist,ratio=count*9.652/dist,
                        endpoint_offset_ms=max(abs(e['ir_offset_ms']),abs(x['ir_offset_ms'])),
                        count_bracket=[e['bracket_counts'][0]-x['bracket_counts'][1],e['bracket_counts'][1]-x['bracket_counts'][0]]))
                pending=None
    out=dict(sources=vars(args),corrupt_packets=bad,clock_offset_spread_p05_p95_s=quant(offsets,.95)-quant(offsets,.05),
        raw_first=stamp(raw[0]['t']),raw_last=stamp(raw[-1]['t']),transitions=transitions,annotations=annotations,
        marker_rulings=dict(collections.Counter(x.get('ruling') for x in markers)),phases=phases,laps=laps,
        inferred_route_zones=zones,full=full,blocks=blocks,
        interval_summary=dict(n=len(intervals),ratio=stats([x['ratio'] for x in intervals]),
            unreliable_intervals=sum(x['unreliable_delta']>0 for x in intervals)),intervals=intervals,
        events=[dict(time=stamp(x['t']),mm=x['mm'],trust=x.get('trust'),count=x['ir_count'],offset_ms=x['ir_offset_ms']) for x in events])
    print(json.dumps(out,indent=2))

if __name__=='__main__':
    main()
