"""How often would the pattern-resumption rule (RvmPause.h) resume on Otto's REAL
stationary Hall while the train is still standing? For every recorded station
stop, the stationary 20 ms Hall blocks (IR-confirmed still) are split into a
'paused' part of n blocks (the reference range) followed by m 'still after GO'
blocks; a false resumption is two consecutive blocks outside the range on the
same side within those m. All start offsets inside each stop are used."""
import numpy as np, pandas as pd, json
from common import *
import realstop as RS
P = RS.profiles()
rows = []
for n in (50, 100, 135, 200, 300):
    for m in (165, 265, 400):
        trials = fails = 0
        for p in P:
            x = p['still_res'][p['still']]
            for s0 in range(0, len(x) - n - m, 25):
                ref = x[s0:s0 + n]; lo, hi = ref.min(), ref.max(); after = x[s0 + n:s0 + n + m]
                side = np.where(after > hi, 1, np.where(after < lo, -1, 0))
                pair = (side[:-1] != 0) & (side[:-1] == side[1:])
                trials += 1; fails += int(pair.any())
        rows.append(dict(pause_blocks=n, pause_s=n * 0.02, still_after_go_blocks=m, still_after_go_s=m * 0.02, trials=trials,
                         false_resumption_rate=fails / max(trials, 1)))
r = pd.DataFrame(rows); r.to_csv(ROOT / 'results' / 'local_pause' / 'false_resumption.csv', index=False)
print(r.to_string())
