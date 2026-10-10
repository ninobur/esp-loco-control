#!/usr/bin/env python3
"""Reproduce the numbers in docs/NAVI_EWO_DNA_IR_CREDIBILITY_REVIEW_20261010.md.

Read-only analysis. Inputs (both on branch codex/navi-cruise-stop-20261009, 85a2cf2):
  LOG  field-records/logs/20261009_MM045_EVENING/all_20261009.log (gunzipped)
  MAP  firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/RouteMap.h
Usage: python3 -I reproduce.py LOG MAP
Nothing here is a proposed firmware algorithm; thresholds are analysis probes.
"""
import bisect, collections, json, random, re, statistics, sys

LOG, MAP = sys.argv[1], sys.argv[2]
src = open(MAP).read()
def arr(name):
    body = re.search(name + r'\[ROUTE_N\] = \{(.*?)\};', src, re.S).group(1)
    return [int(x) for x in body.replace('\n', '').split(',') if x.strip()]
POL, SPAN = arr('ROUTE_POLARITY'), arr('ROUTE_SPACING_MM')
N = len(POL)

# ---------------------------------------------------------------- parse log
pulses, events, throttle = {}, [], []
seen = set()
for line in open(LOG):
    p = line.rstrip('\n').split('\t')
    if len(p) < 3 or '9950011' not in p[1]:
        continue
    ts, topic, body = p
    if topic.endswith('state/throttle'):
        throttle.append((ts, int(body) if body.strip().isdigit() else -1)); continue
    if not (topic.endswith('telem/ir_pulse') or topic.endswith('mm/marker')
            or topic.endswith('nav/evidence')):
        continue
    try: j = json.loads(body)
    except ValueError: continue
    if topic.endswith('ir_pulse'):
        if j.get('completed_us') and j.get('boot') != '0000000000000000':
            pulses.setdefault(j['completed'], (ts, j['completed_us']))
    else:
        k = (j.get('event'), j.get('consumption_id'), j.get('hall_serial'))
        if k not in seen:
            seen.add(k); events.append((ts, j))
tt = [t for t, _ in throttle]
def pwm(ts):
    i = bisect.bisect_right(tt, ts) - 1
    return throttle[i][1] if i >= 0 else -1
SESS = [('18:56', '18:59', 'S0 18:56-18:58 (pre-reseat)'),
        ('19:03', '19:13:55', 'RUN1'), ('19:31', '19:42', 'CLEAN 19:31-19:41'),
        ('19:49', '19:55', 'RUN2')]
def sess(ts):
    for lo, hi, n in SESS:
        if lo <= ts[11:] <= hi: return n
    return None

# ------------------------------------------- 1. confirmations / misses by session
print('== 1. NAVI outcomes by session')
c = collections.defaultdict(collections.Counter)
for ts, j in events:
    s = sess(ts)
    if s and j['event'] in ('TARGET_CONFIRMED', 'MISSED_MAGNET'): c[s][j['event']] += 1
for _, _, n in SESS: print(f'  {n:32s} {dict(c[n])}')

# ------------------------------------------- 2. consecutive pulse-interval probe
print('== 2. Type-6 consecutive completion intervals (TX completed_us)')
ns = sorted(pulses)
pairs = [(pulses[a + 1][0], a + 1, (pulses[a + 1][1] - pulses[a][1]) / 1000)
         for a in ns if a + 1 in pulses]
longs = []
for i, (ts, n, dt) in enumerate(pairs):
    if pwm(ts) <= 0: continue
    m = statistics.median(x[2] for x in pairs[max(0, i - 8):i + 9])
    if m <= 150 and dt >= 2.2 * m: longs.append((ts, n, dt / m))
by = collections.defaultdict(list)
for ts, n, r in longs:
    if sess(ts): by[sess(ts)].append((ts, n, r))
for _, _, name in SESS:
    L = by[name]
    tot = sum(1 for ts, n, dt in pairs if sess(ts) == name and pwm(ts) > 0)
    streaks, cur = [], []
    for e in L:
        if cur and e[1] - cur[-1][1] == 8: cur.append(e)
        else:
            if cur: streaks.append(cur)
            cur = [e]
    if cur: streaks.append(cur)
    mx = max((r for ts, n, r in L if pwm(ts) >= 60), default=0)
    f4 = [s[3][0][11:23] for s in streaks if len(s) >= 4][:1]
    print(f'  {name:32s} intervals={tot:5d} long(>=2.2x local median)={len(L):4d} '
          f'({100*len(L)/max(tot,1):.1f}%) max_ratio@PWM>=60={mx:.1f} '
          f'longest_8count_streak={max((len(s) for s in streaks), default=0)} first_streak>=4={f4}')
print('  RUN1 onset counts 5923..5926 TX ms:',
      [(n, round((pulses[n][1] - pulses[5923][1]) / 1000)) for n in (5923, 5925, 5926) if n in pulses])

# ------------------------------------------- 3. support offsets after run-1 onset
print('== 3. RUN1 target-polarity supports after onset, relative to MM095 anchor')
def mapped(a, b):
    t, i = 0, a
    while i != b: t += SPAN[i]; i = (i + 1) % N
    return t
anchor = None
for ts, j in events:
    if not ('19:05:20' <= ts[11:] <= '19:05:27'): continue
    if j['event'] == 'TARGET_CONFIRMED' and j['mm'] == 95: anchor = j['opening_ir_um']
    if j['event'] in ('SPATIAL_COLLECT', 'SPATIAL_REFERENCE', 'MISSED_MAGNET') or (
            j['event'] == 'HALL_SUPPORT' and anchor):
        extra = ''
        if j['event'] == 'HALL_SUPPORT':
            tr = (j['ir_um'] - anchor) / 1000; mp = mapped(95, j['target'])
            extra = f'target {j["target"]} IR {tr:.0f} mm vs mapped {mp} (diff {tr-mp:+.0f})'
        print(f'  {ts[11:23]} {j["event"]:18s} mm={j["mm"]} {extra}')

# ------------------------------------------- 4. map DNA statistics
print('== 4. Polarity DNA of the 171-marker map')
for k in range(6, 11):
    cnt = collections.Counter(tuple(POL[(i + j) % N] for j in range(k)) for i in range(N))
    uniq = sum(1 for i in range(N) if cnt[tuple(POL[(i + j) % N] for j in range(k))] == 1)
    print(f'  window {k:2d}: {uniq:3d}/171 positions unique, worst ambiguity {max(cnt.values())}')
random.seed(1)
for q in (0.0, 0.05, 0.2):
    out = {}
    for s in (1, -1, 2, -2):
        L = []
        for _ in range(20000):
            i, k = random.randrange(N), 0
            while k <= 60:
                k += 1
                if random.random() < q: continue
                if POL[(i + k) % N] != POL[(i + s + k) % N]: break
            L.append(k)
        L.sort(); out[s] = (L[len(L) // 2], L[int(.9 * len(L))], L[-1])
    print(f'  slip contradiction, Hall miss rate {q}: (median,p90,max) markers {out}')
