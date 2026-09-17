# R5 across stops: how far the line may move before carrying the baseline fails

**Question.** R5 freezes its baseline across a stop. How far can the line move
while Otto stands still before that becomes a failure, and what is the simplest
recovery that does not sample a stationary magnet?

**Answers.**
- Failure begins at **60 counts** of total staleness; nothing below 60 fails,
  above 75 the rule never re-acquires at all.
- Recorded staleness across the 20 stops is within ±12 counts except one at
  **+37** (the handling event). Nothing recorded comes near the cliff.
- The simplest safe recovery is a **stale-baseline timer armed by fresh
  excursions**: it recovers **17 of 17** injected lockouts and re-primes onto a
  stationary magnet **zero** times.

**Source.** `xhr_20260916_191904.xhr`, session **`C3B93D0B`**, md5
`6f78f5de8cb9116a26e62362baf1e09e`. Held reference line from
[NAVI_BASELINE_DRIFT](NAVI_BASELINE_DRIFT_20260916_C3B93D0B.md).

**Reproduce.**

```bash
python3 tools/xhr_baseline_stop_recovery.py xhr_20260916_191904.xhr --session C3B93D0B --all
```

**Injection.** For one stop and one shift, the offset ramps linearly from 0 at
the stop's start to δ at its end, then holds. A ramp rather than a step: a step
inside the stop would itself be an edge the detector could see, confounding "the
line moved while you stood still" with "something passed". δ ∈ {0, ±20, ±32,
±50, ±60, ±70}, 20 stops, 220 replays per rule.

---

## 1. Recorded stop behaviour — nothing injected

| # | ends | dur | line | resting offset | note |
|---|---|---|---|---|---|
| 0 | 30.9 s | 30.9 s | 1934 | −1 | |
| 1 | 1107.6 s | 26.8 s | 1934 | +0 | |
| **2** | 1173.9 s | 37.9 s | 1934 | **+203** | **resting ON a magnet** (Grillers) |
| 3 | 1277.1 s | 37.9 s | 1932 | +2 | |
| 4 | 1384.8 s | 37.9 s | 1935 | −9 | |
| 5 | 1476.2 s | 37.9 s | 1937 | −1 | |
| **6** | 1581.0 s | 37.9 s | 1935 | **+199** | **resting ON a magnet** (Grillers) |
| 7 | 1684.9 s | 37.9 s | 1930 | +4 | |
| **8** | 1793.3 s | 37.9 s | 1936 | **+160** | **resting ON a magnet** (Bamboo) |
| 9 | 1885.7 s | 37.9 s | 1939 | −3 | |
| **10** | 1990.7 s | 37.9 s | 1934 | **+130** | **resting ON a magnet** (Grillers) |
| 11 | 2095.3 s | 37.9 s | 1931 | +2 | |
| 12 | 2188.4 s | 21.5 s | 1935 | −8 | |
| **13** | 2324.4 s | **82.2 s** | 1936 | −32 | **the handling event** |
| 14 | 2408.9 s | 37.9 s | 1901 | +0 | |
| **15** | 2560.3 s | **135.3 s** | 1904 | **−230** | **the stall, resting ON a magnet** |
| 16 | 2635.4 s | 37.9 s | 1904 | −3 | |
| 17 | 2750.2 s | 37.9 s | 1904 | −2 | |
| 18 | 3164.3 s | **405.8 s** | 1899 | +4 | |
| 19 | 3259.1 s | 37.9 s | 1899 | −1 | |

**Baseline staleness actually carried across each stop** is within ±12 counts
everywhere except **stop 13 at +37**, where Otto was physically moved during an
82-second stop and the line genuinely changed by about −32.

**At δ = 0, R5 detects both magnets after every one of the 20 stops**, with **0
false detections and 0 incorrect locks**. Five stops park Otto on a magnet at
+130 to +230 counts; at those the ≥70 span never closes, so no collection can
start. The protection is structural, not luck.

---

## 2. Injected shifts: where carrying fails

Binned by |total staleness| = |recorded shift − δ|:

| \|staleness\| | cases | a magnet missed | never re-acquires | worst false detections |
|---|---|---|---|---|
| 0–45 | 102 | **0** | **0** | 1 |
| 45–55 | 29 | **0** | **0** | 1 |
| 55–65 | 37 | 1 | 0 | 14 |
| 65–75 | 31 | **12** | **7** | 459 |
| 75+ | 10 | **8** | **10** | 337 |

- **Smallest shift at which carrying fails: 60 counts** (stop 17, δ = −60).
- Largest that still works: 81 counts (stop 1, δ = +70).
- Nothing below 60 fails anywhere.

Between 60 and 81 the outcome depends on the **polarity of the next magnet**: a
magnet whose polarity opposes the shift loses its margin first. That is why the
boundary is a band rather than a line.

### Why 70 counts is a cliff and not a slope

With a frozen baseline, clean track reads `|dev| = staleness`.

| staleness | clean line reads | against the 70-count threshold |
|---|---|---|
| 40 | 40 | recoverable |
| 60 | 60 | recoverable, margin thin |
| **70** | **70** | **the line itself is a departure** |
| 89 | 89 | detector permanently open |

Past the threshold the span never closes on clean track, no collection can
start, and the error can only grow. Worse, just *above* 70 the detector
**chatters** — noise of ±17 counts takes it back under threshold and up again —
so each false close is offered to the cadence gate as a magnet. That is where
the 150 to 459 false detections come from.

### The first magnet is lost before the second

At δ = ±70 the pattern across stops is mostly `1-` or `2-`, not `X`: the shift
takes out whichever of the first two magnets opposes it. Only stop 13 loses both
(from δ = −50, because its recorded +37 already puts the total at 87).

---

## 3. A recovery that cannot sample a stationary magnet

Three designs were built and tested. The first two failed, and how they failed
is the useful part.

### Attempt 1 — span timeout + trailing peak-to-peak

*If a departure has been open for 4 s and the trailing 3 s peak-to-peak of raw
is ≥ 100 counts (motion, measured without any baseline), re-prime from a quiet
200 ms window.*

The motion signal separates beautifully in isolation — running p1 = 160 counts,
parked p95 = 26 — and on the recorded waveform this fires **zero** times, so it
is free. But it **recovered only 6 of 17** lockouts and produced **2 incorrect
re-primes** (+130 and −239 counts).

The flaw: a *trailing* window cannot tell "moving" from "just stopped". With an
already-stale baseline the span is open long before the locomotive arrives at
the next stop, so the 4 s span condition is already satisfied on arrival while
the 3 s window still contains the approach magnets. It re-primed onto the
magnet Otto had just parked on.

It also missed most lockouts because, just above 70 counts, the detector
chatters instead of staying open, so a *span* timeout never expires.

### Attempt 2 — stale-baseline timer

*If no baseline has been accepted for 10 s and the motion tests pass, re-prime.*

This recovered **17 of 17** — and was **disqualified on the recorded waveform**,
where it re-primed onto parked magnets three times: **+203, +199 and +130
counts**. The timer expires shortly *after* Otto stops, exactly when the recent
past still looks like motion.

### Attempt 3 — stale timer armed by *fresh* excursions

```
STALE_MS   = 10000     no baseline accepted for this long -> ARM
EXCURSIONS = 2         fresh band-breaking excursions required AFTER arming
BAND       = 40        the quiet-run band
QUIET_MAX  = 2500 ms   running never stays inside the band longer than this
PP_MIN     = 100       trailing 3 s peak-to-peak, baseline-free motion
```

An *excursion* is the quiet run restarting — the signal breaking out of a
40-count window — which happens only when field structure passes the sensor.
The discrimination is decisive:

| | quiet-run length |
|---|---|
| while running | p50 477 ms, p95 1271, p99 1858, **max 2337** |
| while parked | **p5 3832 ms**, p50 28517 |

**A locomotive that has just come to rest produces no further excursions,
however recently it was moving.** That is the property the first two attempts
lacked, and it is what makes this one safe.

---

## 4. Results of the recovery

### On the recorded waveform

| | R5 | R5 + recovery |
|---|---|---|
| locks | 1510 | 1494 |
| re-primes | — | 23 |
| max \|baseline error\| | 13.0 | 13.0 |
| re-primes onto a magnet | — | **0** |

23 re-primes fire, all at stops, all within 9 counts of the line — except one
flagged at −41 counts, **t = 2271.3 s, stop 13**. That is the handling event:
the held reference holds the pre-stop value of 1936 until motion resumes, while
the true line after the stop is 1901–1904. Measured against the line that
actually applied, the re-prime to 1895 was **−6 counts**, not −41. The yardstick
scored a correct re-prime as an error.

**None of the five park-on-a-magnet stops produced a re-prime.**

### On the injected lockouts

All 17 cases where R5 never re-acquired a baseline:

| \|staleness\| | stop | δ | re-prime | error | first lock | false detections |
|---|---|---|---|---|---|---|
| 65 | 14 | +70 | +2.9 s | −4.0 | 5.5 s | 75 → **2** |
| 71 | 8 | −70 | +1.1 s | −7.0 | 4.9 s | 235 → **1** |
| 71 | 3 | +70 | +2.6 s | −6.0 | 5.8 s | 249 → 26 |
| 72 | 9 | +70 | +1.9 s | +4.0 | 4.9 s | 83 → 36 |
| 73 | 12 | −70 | +4.6 s | +3.0 | 8.6 s | 150 → 18 |
| 73 | 19 | −70 | +1.7 s | −1.0 | 5.7 s | 268 → 20 |
| 73 | 4 | −70 | +3.3 s | +3.0 | 6.2 s | 258 → 18 |
| 75 | 14 | −70 | +2.9 s | −4.0 | 5.5 s | 34 → 7 |
| 77 | 16 | +70 | +2.3 s | +3.0 | 5.1 s | 78 → 29 |
| 77 | 10 | +70 | +1.7 s | −7.0 | 5.8 s | 40 → **0** |
| 78 | 2 | +70 | +1.6 s | −8.0 | 5.7 s | 34 → **0** |
| 78 | 6 | +70 | +1.8 s | −3.0 | 5.8 s | 42 → **0** |
| 81 | 1 | +70 | +2.3 s | +1.0 | 5.1 s | 38 → 24 |
| 82 | 18 | +70 | −24.4 s | +8.0 | 5.9 s | 337 → 314 |
| 87 | 13 | −50 | −53.1 s | −41.0 † | 5.7 s | 67 → **0** |
| 97 | 13 | −60 | −53.1 s | −41.0 † | 5.7 s | 67 → **0** |
| 107 | 13 | −70 | −53.1 s | −41.0 † | 5.7 s | 26 → **0** |

**17 of 17 recovered.** Re-prime typically 1.1–4.6 s after the stop ends, first
valid baseline 4.9–8.6 s after it, error within ±8 counts. False detections fall
by one to two orders of magnitude.

† The three stop-13 entries are the same handling-event artefact as above: −41
against the held pre-stop line, −6 against the line that actually applied. A
negative re-prime time means it fired during the stop itself, on the excursions
produced by Otto being physically moved — which is correct behaviour, because
the line really had changed.

**Incorrect locks: 2 at δ = 0 across all 20 stops, 4 across the 17 lockout
cases** — and all of them are the stop-13 artefact.

---

## 5. Recommendation

Add the recovery to R5, or to the cadence-gate rule, as a background condition:

```
STATE: lastOkMs        when a baseline was last accepted
       armed, exc      recovery state
       band lo/hi, runStart    the quiet-run tracker
       pp              trailing 3 s peak-to-peak of raw

EVERY SAMPLE:
  update the quiet run (raw inside a 40-count band) and pp

  if (now - lastOkMs) >= 10000:
      if not armed:            armed = true ;  exc = 0
      if the quiet run just restarted:   exc++          <-- fresh motion
      if exc >= 2 and pp >= 100 and quietRun <= 2500
                 and spread(last 200) <= 32:
          baseline = median(last 200) ;  lastOkMs = now
          armed = false ;  drop any open departure and the cadence clock
```

It costs one timer, one counter and a running band tracker. It is **inert on
recorded data at every stop where Otto rests on a magnet**, and it converts
every injected lockout into a recovery inside 9 seconds.

**It does not restore the first magnet after a big shift.** Recovery needs two
fresh excursions, which means two magnets must pass first. Below 60 counts none
of this matters; above 60 the first magnet or two are lost and the recovery
limits the damage to those rather than to the rest of the run.

---

## 6. Measured versus injected

**Measured, on the unmodified waveform:**

- staleness carried across all 20 stops is within ±12 counts, except +37 at the
  handling event
- R5 detects both magnets after every stop, with no false detections and no
  incorrect locks
- five stops rest on a magnet at +130 to +230 counts, and the ≥70 span
  protection holds at every one
- the quiet-run separation: running max 2337 ms, parked p5 3832 ms
- the recovery fires 23 times, never onto a magnet, and leaves the worst
  baseline error unchanged at 13 counts

**Injected, and therefore not evidence about the field:**

- the 60-count failure threshold and the 70-count cliff
- every lockout, and the recovery's 17-of-17 result
- the false-detection counts of 150 to 459

**Still not established:** whether a real stop ever accumulates 60 counts. At
the measured maximum drift of 1.19 counts/min that needs **50 minutes standing
still**; the longest stop here was 6.8 minutes and the largest real shift across
any stop was 37 counts, from handling rather than drift.

---

*Analysis: `tools/xhr_baseline_stop_recovery.py`. No firmware was modified.
X18's navigation rulings were not examined.*
