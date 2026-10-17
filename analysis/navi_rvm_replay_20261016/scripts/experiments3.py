"""Window-size sensitivity (ESP32 memory) and unexpected-stationarity statistic. Frozen params otherwise."""
import sys, pathlib, json, numpy as np, pandas as pd
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent)); import aligner, score, experiments as X
D = aligner.D; B1 = X.B1; R = {}
for W in [1200, 800, 500]:
    o = X.run(B1, f'x_w{W}', params=dict(W=W)); df = X.scored(o, B1)
    R[f'W_{W}_cells_1mm'] = dict(summary=score.summary(df), teleports=X.teleports(o), runtime_s=round(o['_runtime_s'], 1))
o2 = X.run(X.B2, 'x_w800_b2', params=dict(W=800)); R['W_800_boot2'] = dict(summary=score.summary(X.scored(o2, X.B2)))
# unexpected stationarity: PWM model expects motion (>50 mm/s) but the committed path advances <10 mm in 1 s
b = np.load(D / 'align_x_e1_CW_L1.npz'); t = b['t']; x = b['x']; ve = b['ve']; s = b['seg']
df = pd.DataFrame({'t': t, 'x': x, 've': ve, 'seg': s}).dropna(); df['sec'] = df.t.astype(int)
g = df.groupby('sec').agg(x0=('x', 'first'), x1=('x', 'last'), ve=('ve', 'mean'))
g['adv'] = np.abs(score.wrap(g.x1 - g.x0)); flag = (g.ve > 50) & (g.adv < 10)
runs = flag.astype(int).groupby((flag != flag.shift()).cumsum()).agg(['sum', 'size'])
idx = flag[flag].index.to_series(); cl = idx.groupby((idx.diff() > 1).cumsum()).agg(['min', 'max', 'size'])
m = json.load(open(D / 'template_meta.json'))['CW_L2']
lap2 = cl[(cl['min'] >= m['start_s']) & (cl['max'] < m['end_s'])]
R['unexpected_stationarity'] = dict(longest_in_CW_lap2_s=int(lap2['size'].max()) if len(lap2) else 0,
    episodes_longer_than_lap2_max=cl[cl['size'] > (lap2['size'].max() if len(lap2) else 0)].to_dict('records'))
json.dump(R, open(D / 'experiments3.json', 'w'), indent=1, default=str); print(json.dumps(R, indent=1, default=str))
