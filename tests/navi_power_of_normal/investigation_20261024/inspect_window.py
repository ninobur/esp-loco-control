#!/usr/bin/env python3
"""Dump raw native Hall samples in a [t0,t1] microsecond window (one sample
printed per ~200ms) for direct visual inspection. Usage:
  python3 inspect_window.py <native.bin> <t0_us> <t1_us> [--full]
--full prints every record in the window instead of decimating to 200ms.
"""
import struct, sys

path = sys.argv[1]
t0, t1 = int(sys.argv[2]), int(sys.argv[3])
full = '--full' in sys.argv
rec_size = 16
with open(path, 'rb') as f:
    data = f.read()
n = len(data) // rec_size

def us_at(i):
    return struct.unpack_from('<IQhBB', data, i * rec_size)[1]

lo, hi = 0, n
while lo < hi:
    mid = (lo + hi) // 2
    if us_at(mid) < t0: lo = mid + 1
    else: hi = mid
i = lo
printed = 0
last_print = -10**9
start = i
while i < n and us_at(i) <= t1:
    serial, us, adc, pwm, direction = struct.unpack_from('<IQhBB', data, i * rec_size)
    if full or us - last_print >= 200_000:
        print(us, adc, pwm, direction)
        last_print = us
        printed += 1
    i += 1
print("printed", printed, "of", i - start, "records in window", file=sys.stderr)
