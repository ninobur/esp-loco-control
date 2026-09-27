import sys, numpy as np, json
sys.path.insert(0,'tools'); sys.path.insert(0,'.')
import xhr_format as X
import extract_passages as E
from pathlib import Path
# ---- Otto
SID=0xC3B93D0B
seq=[];tus=[];raw=[];pwm=[];fl=[];base=[];mm=[];ph=[];pc=[]
rul=[]; seen=set(); phases={}
for _r,data in X.iter_capture('otto.xhr'):
    try: hdr,pl=X.parse_record(data)
    except X.BadRecord: continue
    if hdr.session_id!=SID: continue
    if hdr.rec_type==X.REC_SAMPLES:
        if hdr.first_sample_seq in seen: continue
        seen.add(hdr.first_sample_seq)
        for s in X.iter_samples(hdr,pl):
            seq.append(s['sample_seq']);tus.append(s['t_us']);raw.append(s['raw']);pwm.append(s['pwm_actual']);pc.append(s['pwm_commanded']);fl.append(s['flags'])
            base.append(hdr.baseline);mm.append(hdr.nav_mm);ph.append(hdr.st_phase); phases[hdr.st_phase]=hdr.phase_name
    elif hdr.rec_type==X.REC_RULING:
        r=X.parse_ruling(pl); rul.append({k:(v if isinstance(v,(int,float,str)) else str(v)) for k,v in r.items()})
o=np.argsort(seq)
np.savez_compressed('otto_full.npz',seq=np.array(seq)[o],t_us=np.array(tus,dtype=np.int64)[o],raw=np.array(raw)[o],pwm=np.array(pwm)[o],pwmc=np.array(pc)[o],flags=np.array(fl)[o],base=np.array(base)[o],mm=np.array(mm)[o],ph=np.array(ph)[o])
json.dump({'rulings':rul,'phases':{str(k):v for k,v in phases.items()}},open('otto_meta.json','w'))
print('otto samples',len(seq),'unique',len(set(seq)),'rulings',len(rul), 'seq range',min(seq),max(seq))
# ---- Toby
TSID=None
sid_counts={}
for _r,f,rt,n,pl in E.iter_qt(Path('toby.qtcap')):
    sid_counts[f[5]]=sid_counts.get(f[5],0)+(n if rt==E.QT_SAMPLE_REC else 0)
print('qt sessions',{hex(k):v for k,v in sid_counts.items()})
TSID=0xD7651658
cols={k:[] for k in ['seq','t_ms','raw','base','pwm','pwmc','dir','act','pole','estop','late','pkn','pks']}
dec=[]; seen=set()
for _r,f,rt,n,pl in E.iter_qt(Path('toby.qtcap')):
    if f[5]!=TSID: continue
    if rt==E.QT_SAMPLE_REC:
        if f[7] in seen: continue
        seen.add(f[7]); t=f[8]
        for i in range(n):
            s=E.qt_sample(f,pl,i)
            if i: t+=s['dt_ms']
            cols['seq'].append(s['sample_seq']);cols['t_ms'].append(t);cols['raw'].append(s['raw']);cols['base'].append(s['baseline'])
            cols['pwm'].append(s['pwm_actual']);cols['pwmc'].append(s['pwm_commanded']);cols['dir'].append(s['dir']);cols['act'].append(s['event_active'])
            cols['pole'].append(s['event_pole']);cols['estop'].append(s['estop']);cols['late'].append(s['late']);cols['pkn'].append(s['peak_n']);cols['pks'].append(s['peak_s'])
    elif rt==E.QT_DECISION_REC and len(pl)>=E.QT_DECISION.size:
        d=E.unwrap_qt_decision(pl[:E.QT_DECISION.size]); dec.append({k:(v if isinstance(v,(int,float,str)) else str(v)) for k,v in d.items()})
o=np.argsort(cols['seq'])
np.savez_compressed('toby_full.npz',**{k:np.array(v)[o] for k,v in cols.items()})
json.dump({'decisions':dec},open('toby_meta.json','w'))
print('toby samples',len(cols['seq']),'decisions',len(dec),'t range',min(cols['t_ms']),max(cols['t_ms']))
