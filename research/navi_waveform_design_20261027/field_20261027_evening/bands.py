#!/usr/bin/env python3
"""Recognition by applied-PWM band, by phase, and for the first magnet after each standstill, from
results.json written by when_it_fails.py. Also the inter-magnet time around each fast miss, as a
speed check. Usage: python3 bands.py results.json"""
import collections
import json
import sys

r = json.load(open(sys.argv[1]))
P = r['passages']
bands = [(0, 30), (30, 35), (35, 40), (40, 45), (45, 60), (60, 90), (90, 91), (91, 120), (120, 150), (150, 175), (175, 201)]
print('band            passages recognized missed')
for lo, hi in bands:
    s = [p for p in P if lo <= p['pwm'] < hi]
    if s:
        print('PWM %3d-%3d   %5d %5d %5d   missed: %s' % (lo, hi - 1, len(s), sum(p['recognized'] for p in s), sum(not p['recognized'] for p in s),
              [('MM%03d' % p['mm'], p['pwm'], p['phase']) for p in s if not p['recognized']]))
print()
for ph in ('accelerating', 'steady', 'decelerating'):
    s = [p for p in P if p['phase'] == ph]
    print(ph, len(s), 'recognized', sum(p['recognized'] for p in s))
print()
rec = [p for p in P if p['recognized']]
print('lowest recognized PWM', sorted((p['pwm'], 'MM%03d' % p['mm'], p['phase']) for p in rec)[:8])
print('highest recognized PWM', sorted((p['pwm'], 'MM%03d' % p['mm'], p['phase']) for p in rec)[-8:])
print()
print('first magnet after each standstill (>= 1 s at PWM 0):')
prev = None
for p in sorted(P, key=lambda p: p['t']):
    if p['s_since_standstill'] is not None and (prev is None or p['s_since_standstill'] < p['t'] - prev['t']):
        print('   t %.1f MM%03d pwm %d %s %.1f s after standstill -> %s' % (p['t'], p['mm'], p['pwm'], p['phase'], p['s_since_standstill'], 'recognized' if p['recognized'] else 'MISSED'))
    prev = p
print()
srt = sorted(P, key=lambda p: p['t'])
for i, p in enumerate(srt):
    if not p['recognized'] and p['pwm'] > 90:
        nb = [round(srt[j]['t'] - srt[j - 1]['t'], 3) for j in range(max(i - 2, 1), min(i + 3, len(srt)))]
        print('fast miss MM%03d t %.3f pwm %d: inter-magnet times around it %s, r/A %s' % (p['mm'], p['t'], p['pwm'], nb, p['best_at_width']['240']))
