# Baseline sampling timing for a magnet-only navigator

**Question.** Measured from the start of the previous magnet's signal, when may
a simplified magnet-only navigator begin sampling the line for a new baseline?

**Source.** `xhr_20260916_191904.xhr`, X18 Lowline continuous Hall recorder,
Otto (loco 9950011), evening of 2026-09-16. Analysed session **`C3B93D0B`**
only. md5 `6f78f5de8cb9116a26e62362baf1e09e`, 39,339,450 bytes, on the Pi at
`/home/david/NGR/hall_records/`.

**Reproduce.**

```bash
python3 tools/xhr_baseline_timing.py xhr_20260916_191904.xhr --session C3B93D0B --all
```

---

## 0. What the capture will and will not support

### Capture integrity — no gaps

| | session `94E30E44` (earlier) | session **`C3B93D0B`** (analysed) |
|---|---|---|
| samples | 995,400 | **3,537,200** |
| span | 995.4 s | **3537.2 s (59 min)** |
| SAMPLES datagrams missing | **2,124 (212.4 s of trace absent)** | **0** |
| RULING / STATUS missing | 2 / 211 | 0 / 0 |
| on-board ring drops | 0 | 0 |

The two sessions are a reboot apart and are **not joined**: sample sequence,
batch sequence and `millis()` all restart at zero. Session `94E30E44` is the
earlier bench session and is excluded from every number below — it is also the
one with the 212 s hole, so nothing here is measured across missing evidence.

Within `C3B93D0B` the batch sequence runs 1..35372 with no gap, no duplicate and
no reorder, and the sample sequence is contiguous throughout. Timing:

- mean tick 1000.000 µs, p99 1681 µs, p99.99 2704 µs
- **4 samples** in the whole hour exceed 5 ms (5.4, 7.6, 9.7, 63.1 ms),
  **0.09 s in total — 0.002 % of the run**

The 63 ms outlier is a single event. Every window reported below is measured on
a continuous 1 kHz stream.

*(A local snapshot at `/tmp/xhr_ccw_stall_snapshot.xhr` is 38,642,656 bytes —
**truncated**, 696,794 bytes short of the Pi file, missing the last 80.6 s of
the session. It was not used.)*

### What the recording cannot establish

This is one evening's run. The line level was **exceptionally stable**: across
36 minutes of CW running the offline reference line stayed inside 1931–1936
counts, and across the CCW block inside 1899–1904, with no trend. The rule
proposed below is therefore validated against **line noise and magnet tails, not
against baseline drift** — because there was essentially none to validate
against. Morning and midday thermal drift are untested here, and the one real
level change in the recording was not drift at all (see §6).

---

## 1. What a magnet looks like in raw counts

1,547 events were detected from raw as sustained ±70-count departures. X18
independently issued 1,548 rulings in the same session — an agreement that is a
cross-check, not an input.

| | p1 | p50 | p95 | p99 | max |
|---|---|---|---|---|---|
| peak amplitude (counts) | 134 | 181 | — | — | 283 |
| ≥70 span (ms) | — | 111 | 206 | 328 | 744 |
| start-to-start (ms) | — | 1202 | 2248 | — | (30 over 3 s = stops) |

The **weakest magnet in the hour peaked at 125 counts**. Against a 70-count
threshold that is a 55-count margin, and it is what makes a few counts of
baseline error harmless.

Line noise between magnets is ±10 to ±17 counts peak-to-peak.

### The tail is a shelf, not a decay

After the ≥70 span closes, the signal does not decay smoothly back to the line.
It sits on an **opposite-sign shelf of 15–18 counts** — the magnet's return flux
— for a further 100–200 ms. At MM90, the worst place on the route, that shelf is
a flat −15 to −17 counts from 140 ms to 300 ms after the magnet's start:

```
ms after start   deviation from line      (MM90 -> MM91, t=719.6 s, CW)
       40        +167 .. +180             magnet body
      100         +47 ..  +63
      130         -10 ..  +11             span closes
      160         -17 ..  -15   <-- shelf begins
      250         -15 ..  -10   <-- still there
      300          -9 ..   -5
      400          -5 ..   -2   <-- line
```

This matters because it sets a **floor**: no amount of waiting inside one marker
interval gets the bias below about ±11 counts, and waiting from 160 ms to 400 ms
only improves the worst case from 17 counts to 8.

The worst tails are **location-specific and repeatable**, not noise. The five
longest ordinary tails in the hour are all at MM90 (t = 92, 302, 511, 719, 926,
1616 s), and the next group is MM65–MM78 — the Grillers grade, where the
locomotive is slowest.

---

## 2. When has the previous magnet subsided? Two anchors, one event

Criterion: `|raw − line|` stays under TOL for 30 consecutive ms. Measured from
the **start** of the previous magnet's signal, ordinary running only:

| TOL | p50 | p95 | p99 | max |
|---|---|---|---|---|
| ±10 | 152 | 293 | 351 | **405** |
| ±15 | 139 | 177 | 198 | **256** |
| ±20 | 134 | 171 | 186 | **207** |

Across every non-stop class at ±15 counts:

| class | n | p50 | p95 | p99 | max |
|---|---|---|---|---|---|
| ordinary | 1288 | 139 | 177 | 198 | 256 |
| throttle-change | 41 | 152 | 302 | 385 | 431 |
| station | 187 | 203 | 307 | 375 | **476** |
| *stop* | *30* | *385* | *917* | *1052* | *1081* |

**A from-start fixed time must therefore be 476 ms** to cover the non-stop
envelope. Now the same settling event, measured instead from the moment the
magnet's own ≥70 span closed:

| class | n | p50 | p95 | p99 | max |
|---|---|---|---|---|---|
| ordinary | 1288 | 30 | 46 | 69 | 157 |
| throttle-change | 41 | 34 | 64 | 78 | **80** |
| station | 187 | 46 | 80 | 91 | **109** |

**The constant collapses from 476 ms to about 110 ms.** Almost all of the
spread in the from-start measurement was the magnet's own passage duration —
which the navigator does not have to predict, because it watches it happen.

---

## 3. Safe sampling windows

A window is *safe* when it starts at or after the previous ≥70 span closes, ends
at or before the next 70-count departure, and its median is within 20 counts of
the line the next departure will be judged against.

100 ms window, offsets in ms after the previous magnet's **start**:

| class | dir | n | earliest p50 | earliest max | latest (min) | band width (min) | no safe window |
|---|---|---|---|---|---|---|---|
| ordinary | CW | 1165 | 107 | 166 | 572 | 505 | 0 |
| ordinary | CCW | 123 | 114 | 149 | 868 | 775 | 0 |
| throttle-change | CW | 35 | 113 | 366 | 864 | 780 | 0 |
| throttle-change | CCW | 6 | 195 | 262 | 1284 | 1145 | **1** |
| station | CW | 134 | 158 | 307 | 1054 | 950 | 0 |
| station | CCW | 53 | 157 | 406 | 913 | 815 | 0 |

**Every interval in the hour has a safe window except one** — t = 2422.8 s, the
onset of the CCW stall, where the locomotive came to rest on a magnet.

The narrowest safe band anywhere in ordinary running is **505 ms wide**. For the
200 ms window the proposed rule actually uses, the narrowest band is 405 ms.
There is no shortage of room; the question is only where to put the window.

### Bias vs. offset (100 ms window, ordinary running)

| T (ms after previous start) | p1 | p50 | p99 | worst |
|---|---|---|---|---|
| 120 | −25.6 | 0.0 | +31.6 | **65.0** |
| 140 | −14.0 | 0.0 | +11.0 | 43.0 |
| **160** | −14.0 | 0.0 | +10.0 | **16.0** |
| 200 | −13.1 | −1.0 | +10.0 | 14.0 |
| 300 | −9.7 | 0.0 | +4.2 | 12.0 |
| 500 | −3.0 | 0.0 | +2.0 | 5.0 |

There is a cliff between 140 and 160 ms — that is the window clearing the magnet
body. After it, the curve is nearly flat, because what remains is the shelf.

### The window may not be pushed late either

The 100 ms immediately *preceding* a departure is itself contaminated: measured
against the rolling reference it is off by p5/p95 = −14/+15.5 counts, worst +80.
The approaching magnet has a **leading lobe**: the signal is last quiet within
±15 counts **70 ms (p99), 82 ms (max)** before the 70-count departure, and
within ±10 counts **118 ms (p99), 144 ms (max)** before it. A baseline taken
right up against the next magnet is biased *toward* that magnet — the worst
possible direction, since it raises the threshold the magnet must clear.

---

## 4. Speed, and why the rule does not need it

Speed *is* available causally: the elapsed time between the two preceding magnet
starts, with `RouteMap.h` spacing for that span (mean 304 mm, min 280, max 355).
Over ordinary running that gives 204–303 mm/s (p5–p95), median 257 mm/s, and
carried one marker forward it predicts the next marker time to within about
**±13 %** (p5..p95; worst case 69 %).

The tail is a **distance**, not a time — it scales inversely with speed
(corr(tail, speed) = −0.76):

| | p50 | p95 | p99 | max | **cv** |
|---|---|---|---|---|---|
| tail as time (ms) | 138 | 177 | 198 | 256 | 0.142 |
| tail as distance (mm) | 36 | 40 | 45 | 71 | **0.093** |
| tail as fraction of prior interval | 0.118 | 0.133 | 0.146 | 0.237 | 0.094 |

Ranking the four candidate predictors of the earliest usable offset (200 ms
window, ±20 counts, all non-stop classes):

| predictor | cv | constant needed | needs speed? | needs route map? |
|---|---|---|---|---|
| (a) fixed time | 0.397 | 355 ms | no | no |
| (b) fraction of prior marker interval | 0.183 | 0.10 × interval | implicitly | no |
| (c) fixed travel distance | 0.177 | 28.7 mm | yes | yes |
| (d) **the magnet's own ≥70 span** | **0.147** | **0.95 × span** | **no** | **no** |

**(d) wins on every axis.** It is the tightest predictor, and it is the only one
that needs neither a speed estimate nor the route map. The reason is simple:
the magnet's ≥70 span *is* a speed measurement — a faster locomotive produces a
shorter span — and it is measured on the very magnet in question rather than
inferred from the previous one.

Distance (c) and fraction (b) are near-identical predictors because spacing is
nearly constant; (c) buys nothing for the route-map dependency it costs.

---

## 5. Proposed rule

Anchored on what the ESP observes directly, with no speed estimate and no route
map.

```
STATE:  baseline           int16, primed at boot from a 2 s median
        state              IDLE | OPEN | GUARD | COLLECT
        buf[200]           uint16 ring for one collection

EVERY 1 ms SAMPLE:
  dev = raw - baseline

  IDLE / GUARD / COLLECT:
      if |dev| >= 70 for 5 consecutive samples:
          -> OPEN   (this is the departure; abandon any collection in progress
                     and KEEP the existing baseline)

  OPEN:
      track peak and polarity
      when |dev| < 70 has held for 30 ms:
          the span is CLOSED at the last over-threshold sample
          -> GUARD until close + 40 ms

  GUARD:
      at close + 40 ms -> COLLECT, buf empty

  COLLECT:
      append raw
      at 200 samples:
          spread = max(buf) - min(buf)
          if spread <= 40:  baseline = median(buf)     <-- baseline valid here
          else:             reject, keep the old baseline
          -> IDLE
```

### The constants, and what each is bought with

| constant | value | justified by |
|---|---|---|
| departure threshold | 70 counts | weakest magnet peaks at 125; line noise ±17 |
| sustain | 5 samples | single-sample bad conversions are a known fault |
| span closed after | 30 ms below 70 | p99 of within-magnet dips |
| **guard after close** | **40 ms** | ordinary settles by 30 ms p50 / 69 ms p99; 40 ms is the knee of the bias curve (§3) |
| **samples collected** | **200** | see below — this is the load-bearing constant |
| spread reject | 40 counts | rejects the creep-to-stop windows; see §7 |

**200 samples is the constant that matters.** Worst-case baseline error against
collection length, everything else held:

| N | locks | rejected | p99 \|err\| | **worst \|err\|** | locks > 20 counts |
|---|---|---|---|---|---|
| 20 | 1554 | 25 | 36.2 | **86.0** | 63 |
| 40 | 1549 | 12 | 26.0 | 67.0 | 27 |
| 100 | 1491 | 59 | 14.0 | 51.6 | 3 |
| **200** | **1455** | **95** | **11.5** | **12.0** | **0** |
| 250 | 1449 | 101 | 10.0 | 11.0 | 0 |

A 200 ms collection is not merely a better estimator than a 100 ms one — it is a
better *detector of its own contamination*. The windows that produced 50-count
errors at N=100 span more than 40 counts when extended to 200 ms, so the spread
test rejects them instead of accepting them. N=250 adds nothing for the extra
time.

### Answering the question as posed

Measured from the **start of the previous magnet's signal**, this rule:

| | p50 | p95 | max |
|---|---|---|---|
| begins collecting at | 150 ms | 192 ms | 351 ms |
| declares the baseline valid at | 350 ms | 392 ms | 551 ms |

Those are *observed outputs*, not constants. The constant is `span + 40 ms`.

---

## 6. Causal replay over the whole session

The replay sees one sample at a time — no future sample, no reference line,
none of X18's `opened_ms`, `closed_ms`, `baseline_`, `nav_mm` or `st_phase`.

```
events 1550   baselines locked 1455   collections rejected 95
baseline error vs the offline line:
    mean -1.72   sd 5.29   p1 -11.5   p50 -1.0   p99 +8.0   worst 12.0 counts
    locks worse than 15 counts:  0
    locks worse than 20 counts:  0
```

**Premature locks: none.** No collection completed while the signal was still
elevated — the worst lock in the hour is 12 counts from the line.

**Missed locks: 95 rejected collections**, all on the spread test, all safe by
construction (the previous baseline is kept). 70 were at a station phase, 3
while stopped, 22 elsewhere. Between locks the baseline goes stale for a median
of 1188 ms and at most 4893 ms while running — and since the line moved by less
than 6 counts across 36 minutes, staleness cost nothing here.

**Collisions with the next magnet: zero.** No collection was still running when
the next departure arrived. Minimum headroom from baseline-valid to the next
departure was **366 ms** (p5 652 ms, p50 840 ms).

**Effect on the next 70-count detection — the point of the exercise:**

- 1,547 offline events; **1,546 matched**, 1 unmatched
- the 1 unmatched (t = 2424.6 s) is the *same physical event* as a replay
  detection 100 ms later — a boundary shift at the stall onset, not a miss
- **the minimum peak of `|raw − causal baseline|` across all 1,550 detections was
  124 counts**, against a threshold of 70

Not one magnet came close to being missed. With worst-case baseline error of 12
counts, the effective threshold ranges over 58–82 counts against a weakest
magnet of 125.

**4 replay-only detections**, at t = 1134.5, 1541.6, 1755.8, 2424.7 s. All four
are at `ZERO_RAMP` (the locomotive creeping to a station stop) at MM62
(Grillers) and MM159 (Bamboo), where a slow crawl over a magnet re-triggers the
sustain test. These are stop-region behaviour and want their own validation.

---

## 7. Stops, direction changes, and the one level change

These are kept out of the ordinary statistics, as they must be.

**Stops.** 30 intervals exceed 3 s. Their "tails" reach 1081 ms from the magnet
start, because the locomotive is stationary *on or beside* a magnet and there is
no tail to decay — the magnet simply does not leave. The rule handles this
correctly by construction (the 70-count departure never closes, so no collection
starts), but the proposition "the baseline may be sampled after a stop" is
**not** established by this recording and needs its own field test.

**The CCW stall.** From t = 2426 s the locomotive sat for 74 s with raw at 1665
counts — 237 counts below the line — parked hard on a magnet. This is also the
one interval in the hour with no safe sampling window.

**The one real level change was not drift.** At t ≈ 2273 s the line stepped from
1936 to 1904 counts, a −32-count move. It happened in the middle of a **79 s
stationary period** (t = 2243.8 to 2322.5 s), after which the locomotive ran
CCW. That is a handling event — the locomotive was stopped, moved or turned —
not thermal drift and not a direction effect. Measured by the rule's own locks:

| block | span | n locks | median line | locks min–max |
|---|---|---|---|---|
| CW, manual, PWM 90 | 20–1080 s | 838 | 1931.5 | 1922–1945 |
| CW, auto, stations | 1100–2160 s | 388 | 1931.5 | 1923–1944 |
| CCW | 2320–2420 s | 32 | 1904 | 1893–1909 |
| CCW, late | 3150–3300 s | 69 | 1904 | 1890–1912 |

The median line is identical to half a count across 36 minutes of CW running in
two different control modes, then steps once, at a stop. (The min–max spread of
individual locks is measurement scatter — ±12 counts of it, per §6 — not drift.) A rule that re-baselines
every marker absorbs a step like this within one interval; a primed-and-held
baseline would have carried a 32-count error for the rest of the evening.

**Direction.** CW 1,355 intervals, CCW 191. The CCW sample is **seven times
smaller** and is concentrated in a shorter, more interrupted stretch of the
evening. CCW numbers above are consistent with CW throughout (earliest safe
start max 149 ms vs CW's 166 ms), but they are not independently strong.

---

## 8. What this recording does and does not support

**Supported:**

- the timing constants of §5, for evening running at 174–439 mm/s, CW strongly
  and CCW indicatively
- the claim that the tail is a distance and that the magnet's own ≥70 span is
  its best available proxy
- the claim that a 200-sample median with a spread test locks a baseline within
  12 counts of the line, 1,455 times in an hour, without one premature lock and
  without weakening a single 70-count detection

**Not supported:**

- **drift.** The line moved less than 6 counts across 36 minutes. Nothing here
  says what the rule does when the line walks over a morning or a midday, which
  is precisely the condition the rule exists to track. The 95 rejected
  collections were free in this recording because staleness cost nothing; under
  real drift they would not be.
- **stops and direction changes.** Excluded by design and untested.
- **CCW at the same confidence as CW**, on 191 intervals against 1,355.
- **other locomotives and other mounts.** One locomotive, one evening, one Hall.

The honest summary: this recording establishes the rule's *geometry* — where in
the interval the window belongs and how wide it must be — on very strong
evidence, and says nothing about its *tracking* behaviour, which is the other
half of the job.

---

*Analysis: `tools/xhr_baseline_timing.py`. No firmware was modified.*
