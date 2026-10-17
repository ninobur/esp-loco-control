import numpy as np,pandas as pd,json,sys
f,a,b,res=sys.argv[1],float(sys.argv[2]),float(sys.argv[3]),float(sys.argv[4])
o=np.load(f'derived/align_{f}.npz'); tm=json.load(open('derived/template_meta.json'))
t=o['t'];k=(t>a)&(t<b)
d=pd.DataFrame({kk:o[kk][k] for kk in ['t','x','dx','ve','vir','pwm','cost_local','h','alt_margin']})
d['sec']=(d.t/res).astype(int)*res
g=d.groupby('sec').agg(pwm=('pwm','median'),ve=('ve','mean'),vir=('vir','mean'),dx=('dx','sum'),x=('x','last'),cost=('cost_local','sum'),alt=('alt_margin','min'),hmin=('h','min'),hmax=('h','max'))
g['dx']=g.dx/res
print(g.round(0).to_string())
s=pd.read_csv('derived/selections_scoring_only.csv');s=s[(s.open_us>a*1e6)&(s.open_us<b*1e6)]
print(s[['open_us','target','pwm','pole','dir']].to_string())
