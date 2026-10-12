"""Characterise a spoke-period detector and its effect on MM-segment validity. Analysis only."""
import gzip, json, re, bisect, statistics as st, collections
ev=[l.rstrip('\n').split('\t') for l in open('data/oct09_events.tsv')]
flags=[]; gapflags=[]; stats=collections.Counter()
P_hist=[]; last=None
for r in ev:
    b,c,t,iv,pwm=r[1],int(r[3]),int(r[4]),int(r[5]),r[6]
    pw=0 if pwm=='None' else int(pwm)
    if last is None or last[0]!=b: last=(b,c,t); P_hist=[]; continue
    g=c-last[2-1] if False else c-last[1]; dt=t-last[2]
    P=st.median(P_hist[-5:]) if len(P_hist)>=3 else None
    if g==1:
        stats['single']+=1
        if P and pw>0 and iv<500000:
            k=iv/P; n=round(k)
            if k>=1.6 and n>=2 and abs(k-n)<=0.3: flags.append((c,r[0],k)); stats['flag_single']+=1
            else: P_hist.append(iv)
        elif iv<500000: P_hist.append(iv)
    elif 1<g<=4 and P and pw>0 and dt<500000*g:
        stats['gap_eval']+=1
        excess=dt/P-g   # spoke periods not accounted for by the count
        if excess>=0.6: gapflags.append((c,r[0],round(excess,2))); stats['flag_gap']+=1
        P_hist=[]  # re-baseline after a gap
    else:
        stats['unevaluable_gap']+=1; P_hist=[]
    last=(b,c,t)
print(dict(stats))
txt=open('code/RouteMap.h').read()
sp=[int(x) for x in re.search(r'ROUTE_SPACING_MM\[ROUTE_N\]\s*=\s*\{(.*?)\};',txt,re.S).group(1).replace('\n',' ').split(',') if x.strip()]
conf=[]
for line in gzip.open('data/all_20261009.log.gz','rt'):
    p=line.rstrip('\n').split('\t')
    if len(p)<3 or not p[1].endswith('nav/evidence'): continue
    d=json.loads(p[2])
    if d.get('event')=='TARGET_CONFIRMED': conf.append((p[0],d['mm'],d['dir'],d['opening_ir_um']))
def pos(lst): return sorted(x[0]*9652 for x in lst)
F=pos(flags); G=pos(gapflags); allf=sorted(F+G)
rows=[]
for x,y in zip(conf,conf[1:]):
    if y[2]!=x[2] or y[1]!=(x[1]+x[2])%171: continue
    meas=y[3]-x[3]; mapped=(sp[x[1]%171] if x[2]>0 else sp[(x[1]-1)%171])*1000
    if meas<=0: continue
    nf=bisect.bisect_left(allf,y[3])-bisect.bisect_left(allf,x[3])
    ng=bisect.bisect_left(G,y[3])-bisect.bisect_left(G,x[3])
    rows.append((meas/mapped,nf,ng))
def summ(v):
    v=sorted(v); q=lambda p:v[min(len(v)-1,int(p*len(v)))]
    return f"n={len(v)} median {st.median(v):.3f} p10 {q(.1):.3f} p90 {q(.9):.3f} share<0.95 {sum(x<0.95 for x in v)/len(v):.0%}"
print('VALID spans (no flag):   ',summ([r[0] for r in rows if r[1]==0]))
print('INVALID spans (>=1 flag):',summ([r[0] for r in rows if r[1]>0]))
print('  of which flagged only via gap rule:',summ([r[0] for r in rows if r[1]>0 and r[1]==r[2]]) if any(r[1]>0 and r[1]==r[2] for r in rows) else 'none')
print('share of spans invalidated:',f"{sum(r[1]>0 for r in rows)/len(rows):.0%}")
# Coverage: fraction of serials in span actually logged
seen=sorted(int(r[3]) for r in ev)
def coverage(a_um,b_um):
    a=a_um//9652; b=b_um//9652
    n=bisect.bisect_right(seen,b)-bisect.bisect_left(seen,a)
    return n/max(b-a+1,1)
rows2=[]
for x,y in zip(conf,conf[1:]):
    if y[2]!=x[2] or y[1]!=(x[1]+x[2])%171: continue
    meas=y[3]-x[3]; mapped=(sp[x[1]%171] if x[2]>0 else sp[(x[1]-1)%171])*1000
    if meas<=0: continue
    nf=bisect.bisect_left(allf,y[3])-bisect.bisect_left(allf,x[3])
    rows2.append((meas/mapped,nf,coverage(x[3],y[3])))
for lo in (0.95,1.0):
    v=[r for r in rows2 if r[2]>=lo]
    print(f'--- spans with >= {lo:.0%} of pulses logged:')
    print('  VALID  ',summ([r[0] for r in v if r[1]==0]))
    if any(r[1]>0 for r in v): print('  INVALID',summ([r[0] for r in v if r[1]>0]))
