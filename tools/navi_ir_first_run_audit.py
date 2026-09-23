#!/usr/bin/env python3
"""Read-only audit of the first paired NAVI_IR run; no inferred pulse repair."""
import collections
import datetime as dt
import json
from pathlib import Path
import sys
from zoneinfo import ZoneInfo

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ir_noon_analysis import parse_ir, stats, reception
from ir_movement_decode import REASONS

tz = ZoneInfo('America/Los_Angeles')
start = dt.datetime(2026, 9, 20, 13, 38, 26, tzinfo=tz).timestamp()
raw, movement, bad = parse_ir(sys.argv[1])
raw = [x for x in raw if x['t'] >= start]
movement = [x for x in movement if x['t'] >= start]
boot = 0x3B48EBDDC9F3895F
movement = [x for x in movement if x['boot'] == boot]
raw = [x for x in raw if x['boot'] == (boot & 0xffffffff)]
stamp = lambda t: dt.datetime.fromtimestamp(t, tz).isoformat(timespec='milliseconds')

out = {'scope': 'First paired run from declared 069-070 CW, receiver timestamp grouping',
       'corrupt_packets_full_capture': bad, 'sender_boot': f'{boot:016X}'}
periods = []
end = max([x['t'] for x in movement]+[start])
for a in range(int(start), int(end)+1, 10):
    b = a+10
    mm = [x for x in movement if a <= x['t'] < b]
    rr = [x for x in raw if a <= x['t'] < b]
    if not mm:
        continue
    adc = [v & 4095 for x in rr for v in x['samples']]
    keys = ('completed_pulses', 'unreliable_samples', 'saturated_samples', 'sample_gaps', 'open_aborts')
    periods.append({'start': stamp(mm[0]['t']), 'end': stamp(mm[-1]['t']),
        'samples': len(mm), 'reasons': dict(collections.Counter(REASONS[x['optical_reason']] for x in mm)),
        'span': stats([x['span'] for x in mm]), 'pulses_start': mm[0]['completed_pulses'],
        'pulses_end': mm[-1]['completed_pulses'],
        'counter_changes': {k:mm[-1][k]-mm[0][k] for k in keys},
        'raw_span': stats([x['span'] for x in rr]), 'raw_adc': stats(adc)})
out['periods'] = periods
out['raw_reception'] = reception(raw)
out['movement_reception'] = reception(movement)
if movement:
    out['movement_totals'] = {'start': stamp(movement[0]['t']), 'end': stamp(movement[-1]['t']),
        'pulses': [movement[0]['completed_pulses'], movement[-1]['completed_pulses']],
        'reasons': dict(collections.Counter(REASONS[x['optical_reason']] for x in movement))}

markers, compare, links, alerts, commands = [], [], [], [], []
for line in Path(sys.argv[2]).read_text(errors='replace').splitlines():
    p = line.split('\t', 2)
    if len(p) != 3 or '/9950012/' not in p[1]:
        continue
    t = dt.datetime.fromisoformat(p[0]).replace(tzinfo=tz).timestamp()
    if t < start:
        continue
    try:
        row = json.loads(p[2])
    except ValueError:
        row = p[2]
    if '/cmd/' in p[1]:
        commands.append({'time': p[0], 'topic': p[1], 'value': row})
    if not isinstance(row, dict):
        continue
    row['time'] = p[0]
    if p[1].endswith('/mm/marker'): markers.append(row)
    if p[1].endswith('/nav/ir_compare'): compare.append(row)
    if p[1].endswith('/diag/ir_link'): links.append(row)
    if p[1].endswith('/alert'): alerts.append(row)
out['commands'] = commands
out['hall_rulings'] = dict(collections.Counter(x['ruling'] for x in markers))
out['first_12_hall_observations'] = markers[:12]
out['first_12_interval_quality'] = [x for x in compare if x['steps']==1][:12]
out['latest_link'] = links[-1] if links else None
out['latest_alert'] = alerts[-1] if alerts else None
print(json.dumps(out, indent=2))
