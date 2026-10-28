"""Per-boot summary of NAVI_WAVEFORM_0_1 MQTT logs: commands, warnings, every diag/navi_encounter row.
Usage: python3 session_summary.py <log> [<log> ...]"""
import json,sys,collections,glob
files=sys.argv[1:]
rows=[]
for f in files:
    for l in open(f):
        p=l.rstrip('\n').split('\t',2)
        if len(p)==3: rows.append(p)
rows.sort(key=lambda r:r[0])
boot=None; S=collections.OrderedDict()
for t,top,pl in rows:
    loco=top.split('/')[2]
    if top.endswith('state/bootid'):
        boot=(loco,json.loads(pl)['boot_id']); S.setdefault(boot,dict(t0=t,ev=[],cmd=[],last=t,maxpwm=0,warn=[]))
        continue
    if boot is None or boot[0]!=loco: continue
    s=S[boot]; s['last']=t
    if top.endswith('diag/navi_encounter'): s['ev'].append((t,json.loads(pl)))
    elif '/cmd/' in top and not top.endswith(('throttle','brake')): s['cmd'].append((t[11:19],top.split('/')[-1],pl))
    elif top.endswith('state/throttle'): s['maxpwm']=max(s['maxpwm'],int(pl))
    elif top.endswith('online'): s['cmd'].append((t[11:19],'online',pl))
    elif top.endswith('state/warning') and pl and 'THROTTLE REFUSED' not in pl: s['warn'].append((t[11:19],pl[:70]))
for (loco,b),s in S.items():
    c=collections.Counter(e['result'] for _,e in s['ev'])
    print('\n###',loco,b,s['t0'][11:19],'->',s['last'][11:19],'maxpwm',s['maxpwm'],dict(c))
    for x in s['cmd']: print('   ',x)
    for x in s['warn']: print('   W',x)
    for t,e in s['ev']:
        print('   %s %-15s mm=%-4s exp=%-3s %s/%s r=%.2f A=%6.1f pwm=%s seq=%s'%(t[11:23],e['result'],e['mm'],e['expected_mm'],e['expected_polarity'],e['polarity'],e.get('score_r',0),e.get('amplitude_adc',0),e['opening_pwm'],e.get('seq')))
