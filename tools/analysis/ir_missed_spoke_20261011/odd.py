import sys, statistics as st
sys.path.insert(0,'scripts')
from rule185 import load_events, load_fixture
segs=load_events('data/oct09_events.tsv')
odd=[]; mult=[]
for seg in segs:
    iv=[(seg[k][1]-seg[k-1][1],seg[k][2],seg[k][3],seg[k][0]) for k in range(1,len(seg))]
    for j in range(1,len(iv)):
        p,c=iv[j-1],iv[j]
        if 100*c[0]<=185*p[0]: continue
        pre=[x[0] for x in iv[max(0,j-5):j]]; med=st.median(pre); nxt=iv[j+1][0] if j+1<len(iv) else None
        m=c[0]/med; mult.append(round(m*2)/2)
        if not(nxt is not None and abs(nxt-p[0])<=0.25*p[0]) or c[0]<1.85*med:
            odd.append((c[2],c[1],[x//1000 for x in pre],p[0]//1000,c[0]//1000,None if nxt is None else nxt//1000,round(m,2)))
import collections
print('cur/median5 rounded to 0.5:',sorted(collections.Counter(mult).items()))
print('non-isolated or <1.85x median5:',len(odd))
for o in odd: print(' ',o[0][11:23],'pwm',o[1],'pre',o[2],'prev',o[3],'cur',o[4],'next',o[5],'cur/med5',o[6])
# synthetic sensitivity excluding positions touching an existing candidate
tot=det=0
for seg in segs:
    iv=[seg[k][1]-seg[k-1][1] for k in range(1,len(seg))]
    flag=[False]+[100*iv[j]>185*iv[j-1] for j in range(1,len(iv))]
    for j in range(6,len(iv)-2):
        if any(flag[j-6:j+3]) or max(iv[j-6:j+3])>=500000: continue
        tot+=1; det+=100*(iv[j]+iv[j+1])>185*iv[j-1]
print(f'synthetic merge on clean steady positions (characterisation, NOT proof): {det}/{tot} = {100*det/tot:.2f}% exceed 185%')
