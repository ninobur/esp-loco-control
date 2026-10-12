import sys, statistics as st
sys.path.insert(0,'scripts')
from rule185 import load_events, load_fixture
def run(segs,label):
    res={}
    for name,fn in (('prev x1.85',lambda iv,j: 1.85*iv[j-1]),('median5 x1.85',lambda iv,j: 1.85*st.median(iv[j-5:j])),('median5 x1.50',lambda iv,j: 1.5*st.median(iv[j-5:j]))):
        real=0; tot=det=0
        for seg in segs:
            iv=[seg[k][1]-seg[k-1][1] for k in range(1,len(seg))]
            for j in range(5,len(iv)):
                if iv[j]>fn(iv,j): real+=1
            flag=[False]*len(iv)
            for j in range(1,len(iv)): flag[j]=100*iv[j]>185*iv[j-1]
            for j in range(6,len(iv)-2):
                if any(flag[j-6:j+3]) or max(iv[j-6:j+3])>=500000: continue
                tot+=1
                m=list(iv[:j])+[iv[j]+iv[j+1]]
                det+= m[j]>fn(m,j)
        res[name]=(real,det,tot)
    print(f'== {label}')
    for k,(r,d,t) in res.items(): print(f'   {k:14s} real-data flags {r:5d} | synthetic merge detected {d}/{t} = {100*d/max(t,1):.1f}%')
run(load_fixture('data/oct09_healthy.txt.gz'),'oct09_healthy (19:31-19:41)')
run(load_fixture('data/oct10_121800.txt.gz'),'oct10_121800')
run(load_events('data/oct09_events.tsv'),'Oct 9 full evening')
