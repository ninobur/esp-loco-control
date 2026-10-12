import gzip, json, sys
src, out = sys.argv[1], sys.argv[2]
pwm = None; auto=None; seen=set(); n=0
with gzip.open(src, 'rt') as f, open(out,'w') as o:
    for line in f:
        p = line.rstrip('\n').split('\t')
        if len(p) < 3: continue
        ts, topic, body = p
        if topic.endswith('state/loopstat'):
            try:
                d=json.loads(body); pwm = d.get('pwm'); auto=d.get('auto')
            except Exception: pass
        elif topic.endswith('telem/ir_pulse'):
            try: d = json.loads(body)
            except Exception: continue
            if d.get('kind')!='EVENT' or not d.get('event_valid'): continue
            k=(d['boot'],d['seq'])
            if k in seen: continue
            seen.add(k); n+=1
            o.write('\t'.join(map(str,(ts,d['boot'],d['seq'],d['completed'],d['completed_us'],d.get('interval_us'),pwm,auto,d.get('reason'),d.get('span'))))+'\n')
print(n)
