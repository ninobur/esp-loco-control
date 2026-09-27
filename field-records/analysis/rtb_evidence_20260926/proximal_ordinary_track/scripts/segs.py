import pandas as pd, numpy as np, glob
from seg import rmed, runs, P, meta
import seg
def segments():
    out=[]
    for loco in ['otto','toby']:
        fs=sorted(glob.glob(P+loco+'_*.csv'))
        ds=[]
        for f in fs:
            n=f.split('/')[-1][:-4]; d=pd.read_csv(f); d['src']=n; ds.append(d)
        allr=pd.concat(ds).drop_duplicates('sample_seq').sort_values('sample_seq').reset_index(drop=True)
        # split where sample_seq jumps by > 50 (Toby has late/gaps; windows separate by far more)
        br=np.where(np.diff(allr.sample_seq.values)>50)[0]+1
        for i,(a,b) in enumerate(zip([0,*br],[*br,len(allr)])):
            part=allr.iloc[a:b].reset_index(drop=True)
            part['t']=(part.t_ms-part.t_ms.iloc[0]).astype(float)
            name=loco+':'+'+'.join(sorted(set(part.src)))
            out.append((loco,name,part))
    return out
if __name__=='__main__':
    for loco,name,d in segments():
        ev,R0=seg.events(d)
        print(f"{name}: {len(d)} samples {d.t.iloc[-1]:.0f} ms R0={R0} events={[(e['pol'],round(e['c']),round(e['W'])) for e in ev]}")
