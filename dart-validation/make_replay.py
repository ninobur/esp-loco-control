from pathlib import Path
import sys,json,struct,hashlib
import numpy as np
root=Path(__file__).resolve().parents[1];workspace=root.parent
sys.path.insert(0,str(root/'tools'));import navi_sync_format as F
source=workspace/'hall-field-analysis-20260929/sources/otto_navi_sync_20260928_101705.nsr'
data=json.loads((workspace/'hall-field-analysis-20260929/continuous_hall_analysis.json').read_text())
events={e['event_serial']:e for e in data['events']}
print({i:events[i] for i in [253,254,255,256,257,424,425,426]})
cases=[('sep28_254_257',253,258),('sep28_425_426',424,427),('sep28_425_427_continued',424,427)]
rows={name:[] for name,_,_ in cases};latest={}
H=np.dtype([('dt','<u2'),('raw','<u2',(5,)),('median','<i2'),('pwm','u1'),('cmd','u1'),('flags','u1'),('reserved','u1')])
wirefmt='<HBBI'+'Q'*10+'IIQBBHH'
for _,record in F.iter_capture(str(source)):
 header,payload=F.parse_record(record)
 if header.rec_type==F.REC_IR:
  w=struct.unpack(wirefmt,F.parse_ir(payload)['wire']);t=header.t0_us
  for name,start,end in cases:
   if t<=events[start]['captured_us']:latest[name]=(t,w)
   elif t<=events[end]['captured_us']+(1000000 if name.endswith('continued') else 200000):rows[name].append((t,0,f'I {t} {w[3]} {w[4]} {w[5]} {w[7]} {w[17]}\n'))
 elif header.rec_type==F.REC_HALL:
  a=np.frombuffer(payload,H,header.n_items);times=header.t0_us+np.r_[0,np.cumsum(a['dt'][1:],dtype=np.int64)]
  for name,start,end in cases:
   mask=(times>events[start]['captured_us'])&(times<=events[end]['captured_us']+(1000000 if name.endswith('continued') else 200000))
   for t,item in zip(times[mask],a[mask]):
    t=int(t);direction=1 if int(item['flags'])&F.HALL_F_DIR_FWD else 2
    lines=''.join(f'H {t} {int(raw)} {int(item["pwm"])} {direction}\n' for raw in item['raw'])+f'F {t}\n'
    rows[name].append((t,1,lines))
manifest=[]
sourcehash=hashlib.sha256(source.read_bytes()).hexdigest()
for name,start,end in cases:
 t,w=latest[name];e=events[start];prefix=f'I {t} {w[3]} {w[4]} {w[5]} {w[7]} {w[17]}\nS {e["captured_us"]} {e["mm"]} 1 {w[16]} {e["base_open"]}\n'
 dst=root/'dart-validation'/f'{name}.txt';dst.write_text(prefix+''.join(r[2] for r in sorted(rows[name])))
 manifest.append(dict(case=name,source=str(source),source_sha256=sourcehash,seed_event=start,end_event=end,seed_mm=e['mm'],anchor_um=w[16],reference=e['base_open'],fixture_sha256=hashlib.sha256(dst.read_bytes()).hexdigest(),note='Historical identity/reference seed, original five ADC values per batch. Native timestamps unavailable; all five receive batch timestamp. No physical truth assertion.'))
(root/'dart-validation/replay-provenance.json').write_text(json.dumps(manifest,indent=2)+'\n')
print('Wrote native replay fixtures')
# Synthetic ADC envelope for the October 9 measured early-opening mechanism.
# The observation coordinate (26 pulses = 250.952 mm) is historical; waveform
# and subsequent movement are explicit test stimuli, not recovered field ADC.
lines=['I 10000 1 1 10000 0 6\n','S 10000 63 1 0 2000\n']
t=11000
for raw in [2000]*2:
 lines+=[''.join(f'H {t} {raw} 40 1\n' for _ in range(5)),f'F {t}\n'];t+=1000
lines+=[f'I {t} 2 1 {t} 26 6\n'];t+=1000
# Map MM064 polarity, from the frozen baseline map.
import re
route=(root/'firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/RouteMap.h').read_text()
poles=list(map(int,re.findall(r'\b[01]\b',route.split('ROUTE_POLARITY[ROUTE_N] = {')[1].split('};')[0])))
raw=2100 if poles[64] else 1900
for value in [raw,raw,2000,2000]:
 lines+=[''.join(f'H {t} {value} 40 1\n' for _ in range(5)),f'F {t}\n'];t+=1000
lines+=[f'I {t} 3 1 {t} 42 6\n'];t+=1000
lines+=[''.join(f'H {t} 2000 40 1\n' for _ in range(5)),f'F {t}\n']
(root/'dart-validation/oct09_early_synthetic.txt').write_text(''.join(lines))
