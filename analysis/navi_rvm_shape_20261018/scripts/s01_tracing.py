"""20 ms Hall tracing from the native ADC stream: non-overlapping 20 ms means.
No smoothing, no interpolation. A block touching a serial jump or holding no
samples is NaN (gap)."""
import numpy as np
from common import *
N = np.dtype([('serial','<u4'),('us','<u8'),('adc','<i2'),('pwm','u1'),('direction','u1')])
a = np.memmap(HG/f'native_{BOOT1}.bin', mode='r', dtype=N)
us = a['us'].astype(np.int64); adc = a['adc'].astype(np.float64); ser = a['serial'].astype(np.int64)
B = DT_MS*1000; t0 = us[0]//B*B; b = (us-t0)//B; nb = int(b[-1])+1
n = np.bincount(b, minlength=nb); s = np.bincount(b, adc, nb)
h = np.where(n > 0, s/np.maximum(n, 1), np.nan)
jump = np.where(np.diff(ser) != 1)[0]
gap = n < 0.5*np.median(n[n > 0]); gap[b[jump]] = True; gap[b[jump+1]] = True
h[gap] = np.nan
dr = np.zeros(nb, np.int8); dr[b] = a['direction']   # declared direction byte, used only to orient CCW windows
t_us = t0 + np.arange(nb)*B + B//2
np.savez_compressed(D/'tracing_boot1.npz', t_us=t_us, hall=h, n=n, dir=dr)
print(f'{nb} blocks, {np.isnan(h).sum()} gap blocks, {len(jump)} serial jumps, median samples/block {np.median(n[n>0])}')
