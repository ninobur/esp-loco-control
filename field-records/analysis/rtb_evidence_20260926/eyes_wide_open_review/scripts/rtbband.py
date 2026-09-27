import numpy as np, pickle, pandas as pd
import replay, adversarial_lib as A
for loco in ['otto','toby']:
    for band,n in [(70,30),(40,100),(25,100),(15,100)]:
        for step in [None,(2000.0*1000,-45),(2000.0*1000,+45)] if loco=='otto' else [None]:
            st=[]
            D,op=replay.run(loco,'anchored',rtb_band=band,rtb_n=n,stuck=st,step=step)
            m,ex,wp,errs=A.detail(loco,op,step=step,D=D)
            t=D['t']; dur=np.array([(t[c]-t[o])/1000 for o,c in st])
            print(f"{loco} RTB band ±{band} x{n} step={step and (step[0]/1000,step[1])}: openings {len(op)} miss {int((m<0).sum())} extra {ex} wrongpol {wp} | encounter open-time p50 {np.median(dur):.2f}s p99 {np.percentile(dur,99):.1f}s max {dur.max():.1f}s", flush=True)
