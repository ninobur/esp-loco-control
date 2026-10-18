"""Evidence only (no design input): at the recorded 5 s station stops of boot 1,
at what applied PWM did Otto's IR odometer stop advancing (arrival) and start
advancing again (departure)? Compared with the operator's STOPPED convention
(applied PWM < 25). IR is used here only to describe the recording; the firmware
never reads IR."""
import numpy as np, pandas as pd, json
from common import *
N = np.dtype([('serial','<u4'),('us','<u8'),('adc','<i2'),('pwm','u1'),('direction','u1')])
a = np.memmap(HG/f'native_{BOOT1}.bin', mode='r', dtype=N)
us = a['us'][::50].astype(np.int64); pw = a['pwm'][::50].astype(int)      # 100 Hz
ir = pd.read_csv(HG/'ir_coordinate_source.csv'); ir = ir[ir.boot == BOOT1].sort_values('us')
odo = np.interp(us, ir.us.values, ir.um.values / 1000.0)                # mm, step-held between snapshots
p0 = (pw == 0).astype(int); e = np.diff(np.concatenate([[0], p0, [0]])); ss = np.where(e == 1)[0]; ee = np.where(e == -1)[0]
rows = []
for i, j in zip(ss, ee):
    if (us[j-1]-us[i]) < 3e6 or (us[j-1]-us[i]) > 20e6: continue           # the 5 s dwell stops
    # arrival: last time the odometer advanced (>1 mm over 0.3 s) before the PWM-zero run ends
    k0 = max(0, i - 3000); seg = odo[k0:j]
    mv = np.where(seg[30:] - seg[:-30] > 1.0)[0]
    arr = k0 + mv[-1] + 30 if len(mv) else None
    # departure: first time after j the odometer has advanced 1 mm
    k1 = min(len(odo) - 1, j + 3000); after = np.where(odo[j:k1] - odo[j] > 1.0)[0]
    dep = j + after[0] if len(after) else None
    rows.append(dict(stop_s=us[i]/1e6, pwm_at_last_motion=int(pw[arr]) if arr is not None else None,
                     still_moving_after_pwm_below25_s=(us[arr] - us[np.where(pw[k0:j] < 25)[0][0] + k0]) / 1e6 if arr is not None and (pw[k0:j] < 25).any() else None,
                     pwm_at_first_motion=int(pw[dep]) if dep is not None else None,
                     moving_before_pwm25=(dep is not None and pw[dep] < 25),
                     stationary_s_with_pwm_ge25_on_departure=(us[dep] - us[j + np.where(pw[j:k1] >= 25)[0][0]]) / 1e6 if dep is not None and (pw[j:k1] >= 25).any() else None))
r = pd.DataFrame(rows); r.to_csv(ROOT/'results'/'local'/'stopped_threshold_evidence.csv', index=False)
summ = dict(stops=len(r), pwm_at_first_motion=r.pwm_at_first_motion.describe().to_dict(),
            stationary_s_with_pwm_ge25_on_departure=r.stationary_s_with_pwm_ge25_on_departure.describe().to_dict(),
            pwm_at_last_motion=r.pwm_at_last_motion.describe().to_dict(),
            still_moving_after_pwm_below25_s=r.still_moving_after_pwm_below25_s.describe().to_dict())
json.dump(summ, open(ROOT/'results'/'local'/'stopped_threshold_evidence.json', 'w'), indent=1, default=float)
print(r.to_string()); print(json.dumps(summ, indent=1, default=float))
