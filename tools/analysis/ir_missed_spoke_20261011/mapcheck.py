"""Counted IR distance vs mapped interval between consecutive TARGET_CONFIRMED events, split by 185% candidates in the span."""
import gzip, json, re, sys, bisect, statistics as st
txt=open('code/RouteMap.h').read()
sp=[int(x) for x in re.search(r'ROUTE_SPACING_MM\[ROUTE_N\]\s*=\s*\{(.*?)\};',txt,re.S).group(1).replace('\n',' ').split(',') if x.strip()]
N=171
def span(mm,d): return sp[mm%N] if d>0 else sp[(mm-1)%N]
conf=[]
for line in gzip.open('data/all_20261009.log.gz','rt'):
    p=line.rstrip('\n').split('\t')
    if len(p)<3 or not p[1].endswith('nav/evidence'): continue
    d=json.loads(p[2])
    if d.get('event')=='TARGET_CONFIRMED': conf.append((p[0],d['mm'],d['dir'],d['opening_ir_um']))
ev=[l.rstrip('\n').split('\t') for l in open('data/oct09_events.tsv')]
# candidate pulse counts (completed) and their positions in um = completed*9652
cands=[]
for a,b,c in zip(ev,ev[1:],ev[2:]):
    if a[1]==b[1]==c[1] and int(b[3])==int(a[3])+1 and int(c[3])==int(b[3])+1:
        if 100*int(c[5])>185*int(b[5]): cands.append(int(c[3])*9652)
cands.sort()
rows=[]
for x,y in zip(conf,conf[1:]):
    if y[2]!=x[2] or y[1]!=(x[1]+x[2])%N: continue
    meas=y[3]-x[3]; mapped=span(x[1],x[2])*1000
    if meas<=0: continue
    n=bisect.bisect_left(cands,y[3])-bisect.bisect_left(cands,x[3])
    rows.append((y[0],x[1],y[1],meas/mapped,n))
clean=[r[3] for r in rows if r[4]==0]; hit=[r for r in rows if r[4]>0]
print('adjacent confirmed intervals',len(rows))
print(f'no 185% candidate in span: n={len(clean)} median measured/mapped {st.median(clean):.3f}')
for k in (1,2,3):
    s=[r[3] for r in hit if r[4]==k]
    if s: print(f'{k} candidate(s) in span: n={len(s)} median {st.median(s):.3f}')
s=[r[3] for r in hit if r[4]>=4]
if s: print(f'>=4 candidates: n={len(s)} median {st.median(s):.3f}')
# within healthy window
hw=[r for r in rows if '19:31'<=r[0][11:16]<'19:42']
print('healthy window 19:31-19:41:', 'clean median %.3f n=%d'%(st.median([r[3] for r in hw if r[4]==0]),sum(r[4]==0 for r in hw)), '| with candidates median %.3f n=%d'%(st.median([r[3] for r in hw if r[4]>0]),sum(r[4]>0 for r in hw)))
for r in hw[:40]: print('  ',r[0][11:23],f'MM{r[1]:03d}->{r[2]:03d}',f'{r[3]:.3f}','cands',r[4])
