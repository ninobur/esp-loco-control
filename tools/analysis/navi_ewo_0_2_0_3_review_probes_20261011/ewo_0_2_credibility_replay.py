#!/usr/bin/env python3
"""Replay NAVI_EWO_0_2's observePulse credibility rule (cbdfe94) over October 9 Type-6
timestamps. Read-only review analysis. Usage: python3 -I this.py all_20261009.log
(gunzipped from field-records/logs/20261009_MM045_EVENING/ on codex/navi-cruise-stop-20261009).
Approximation: 0.2 resets credibility at each Hall encounter; spans of 31 pulses
(~one 300 mm marker interval) stand in for that. MQTT log omissions break consecutive
pairs, so on-locomotive flag rates would be equal or higher."""
import bisect,collections,json,sys
d={}; thr=[]
for line in open(sys.argv[1]):
    p=line.rstrip('\n').split('\t')
    if len(p)<3 or '9950011' not in p[1]: continue
    if p[1].endswith('state/throttle'):
        thr.append((p[0],int(p[2]) if p[2].strip().isdigit() else -1)); continue
    if not p[1].endswith('telem/ir_pulse'): continue
    try: j=json.loads(p[2])
    except ValueError: continue
    if j.get('completed_us') and j.get('boot')!='0000000000000000':
        d.setdefault(j['completed'],(p[0],j['completed_us']))
tt=[t for t,_ in thr]; tv=[v for _,v in thr]
def pwm(ts):
    i=bisect.bisect_right(tt,ts)-1; return tv[i] if i>=0 else -1
SESS=[('18:56','18:59','S0'),('19:03','19:13:55','RUN1'),('19:31','19:42','CLEAN'),('19:49','19:55','RUN2')]
def sess(ts):
    for lo,hi,n in SESS:
        if lo<=ts[11:]<=hi: return n
SPAN=31  # ~one 300 mm marker interval of 9.65 mm pulses; 0.2 resets at each Hall encounter
res=collections.defaultdict(lambda:[0,0,None])
base=susp=0; ppwm=None; prev=None; equiv=0; flagged=False; spanstart=None
for n in sorted(d):
    ts,us=d[n]; p=pwm(ts); s=sess(ts)
    if spanstart is None or n-spanstart>=SPAN:
        if spanstart is not None and s and p>0:
            r=res[s]; r[0]+=1; r[1]+=flagged
            if flagged and r[2] is None: r[2]=ts[11:23]
        spanstart=n; equiv=0; flagged=False
    if prev is None or n!=prev[0]+1 or p!=ppwm or p<=0:
        base=(us-prev[1]) if prev and n==prev[0]+1 else 0; susp=0
    else:
        iv=us-prev[1]
        if base:
            ratio=iv/base
            if susp>=1.5 and 1/1.4<=ratio<=1.4:
                equiv+=max(int(susp+0.5)-1,1); susp=0
                if equiv>=2: flagged=True
            elif ratio>=1.5: susp=ratio
            else: base=iv
        else: base=iv
    prev=(n,us); ppwm=p
for k in ['S0','RUN1','CLEAN','RUN2']:
    a,b,f=res[k]; print(f'{k:6s} spans={a:4d} questionable={b:4d} ({100*b/max(a,1):.0f}%) first={f}')
