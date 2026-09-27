import numpy as np, replay, adversarial_lib as A, sys
loco=sys.argv[1]; cases=[('baseline',{})]
cases+=[(f'inject {e:+d} @300',dict(inject=(300,e))) for e in (60,120,-80)]
cases+=[(f'boot ref truth{d:+d}',dict(ref0=(1934.0 if loco=='otto' else 1795.0)+d)) for d in (45,100,200,-200)]
cases+=[(f'step {d:+d} @2000s',dict(step=(2000e3,d))) for d in (60,120,-120)]
for mode in ['anchored','hybrid','self']:
    for name,kw in cases:
        if mode=='anchored' and name.startswith('inject'): continue   # already measured
        D,op=replay.run(loco,mode,**kw); m,ex,wp,errs=A.detail(loco,op,step=kw.get('step'),D=D)
        fb=len(replay.run.fallbacks)
        print(f"{loco} {mode:8s} {name:22s} miss {int((m<0).sum()):4d} extra {ex:5d} wrongpol {wp:4d} | |ref err|>10 on {sum(abs(v)>10 for v in errs.values()):4d} magnets | fallbacks {fb}",flush=True)
