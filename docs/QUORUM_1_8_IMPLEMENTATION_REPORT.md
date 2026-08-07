# QUORUM 1.8 — baseline motion gate: implementation report

Date: 2026-08-07
Author: Claude Code
Decision implemented: 0017 (Proposed — ratification awaits the pre-fix test)
Spec: `docs/QUORUM_BASELINE_MOTION_GATE_SPEC.md` (Sam + CODEX reviewed,
`docs/QUORUM_1_8_REVIEW_FINDINGS.md`)
Commit: `daf468d`
Status: **built, not flashed.** See the sequencing warning below — it is
the one thing that can go wrong operationally.

---

## ⚠ Sequencing: pre-fix test FIRST, on the 1.7 aboard Otto now

The falsifier all parties agreed to — park on a magnet, require the
baseline to migrate — is only demonstrable on firmware **without** the
gate. Otto currently runs 1.7, which is exactly the test vehicle needed.
**Flash 1.8 only after the pre-fix capture shows the migration** (or, if
it does not, 1.8 is not flashed at all and 0017 reopens). Capture must run
continuously from before the stop to ≥60 s after departure; the corruption
self-erases.

## What changed

One functional line, three comment blocks, version bump. Diff: +61/−2,
entirely in `firmware/QUORUM/QUORUM.ino`.

1. **The gate** (in `updateBaseline()`):

```c
if(actualPwm <= MOTOR_DEAD_ZONE_PWM) return;
```

placed *after* the `medPrimed` fallback (per CODEX — a prime taken from
the first moving sample could land mid-magnet) and before the 500 ms rate
check. `actualPwm` is `volatile` and already read on the hall task for
§3's PWM-at-detect; no new threading pattern.

2. **Priming-invariant comment** at the fallback, stating: the fallback is
unreachable in a correct boot (`calibrate()` primes before `hallTask`
exists — verified in `setup()` ordering); it is retained because a
single-sample prime beats a zero baseline (which pins thresholds at ±38
around 0 and holds an event open forever); and the gate must never move
above it.

3. **Sensor-layer header note**: a dwell on a magnet now legitimately
holds one event open for the entire stop, closing at departure as a single
arrival-stamped marker with `ms` saturating at 65535 — an *expected*
signature, not a stuck detector. A stuck event **while moving** still
self-heals, because pushes continue in motion and migration can close it;
that is why the gate is on motion rather than `!evActive` (spec §7 R6,
endorsed by Sam).

4. **Version history** entry and `SKETCH_NAME` → `QUORUM_1_8`.

Not changed: detector thresholds, navigator, topics, payloads, timing
gates, station logic, INA219 service. Nothing touches the bicameral
boundary — the gate reads `actualPwm`, writes nothing.

## Build

Core 3.3.11, FQBN `esp32:esp32:esp32`, libraries from the session
scratchpad (PubSubClient, Adafruit_INA219, Adafruit_BusIO):

```
Sketch uses 982475 bytes (74%) of program storage space.
Global variables use 52444 bytes (16%) of dynamic memory.
```

+16 bytes over 1.7. `--warnings all` shows only the pre-existing set
(seven `-Wvolatile` on counter increments, one `-Wextra` enumerated
conditional at a line untouched by this change) — left alone per the
standing rule against fixing warnings inside safety changes.

## Flash-verification discriminators (the bootid lesson)

`state/bootid` saying `QUORUM_1_8` is necessary but not sufficient — the
sketch-name string lands in partial builds. Genuine-1.8 discriminators:

- Serial boot order: `[BOOT] QUORUM_1_8 — <loco>` then `[INA]` then
  `[CAL]` (the 1.7 fingerprints must persist: `[INA]` line, `pwm`/`v` on
  `mm/marker`).
- The behavioural one: parked at PWM 0 over a magnet, `state/loopstat`
  shows `delta` large and **steady** while `baseline` does not move. On
  1.7 the baseline migrates; on 1.8 it must not. This is also acceptance
  row 3/4, so verification and acceptance are the same run.

## Field programme (spec §5, reviewed)

1. **Pre-fix on 1.7 (now):** magnet park >70 s → require migration;
   control park clear of magnets → require stability.
2. **Flash 1.8**, verify discriminators above.
3. **Acceptance matrix:** clear / fringe (<±38) / N magnet / S magnet /
   stalled-above-dead-zone rows, each >70 s, expectations per spec §5 —
   including the long-dwell saturation checks (`ms == 65535`, one event,
   one advance, `drift` ≈ 0, successor marker accepted, stale arrival
   timestamp benign).
4. **Regression lap:** `miss_streak` 0; `loop_max_gap_ms` /
   `hall_task_max_gap_ms` at the 2026-08-06 baseline (33–34 / 2–3 ms);
   a low-PWM crawl segment still navigates (LOW_PWM gate unaffected).

Capture command for every stage (also in the spec):

```
mosquitto_sub -h 192.168.68.142 -t 'ngr/loco/9950011/state/loopstat' \
  -t 'ngr/loco/9950011/mm/marker' -t 'ngr/loco/9950011/state/nav' \
  -t 'ngr/loco/9950011/alert' -v -W 600 > field-records/logs/<date>_<stage>.log
```

## On ratification

If the pre-fix test reproduces the migration and the acceptance matrix
passes: promote 0017 from Proposed to Accepted (status edit + date, per
the decisions README), and record the field verdict in docs/. If the
pre-fix test fails to reproduce: 1.8 is not flashed, 0017 stays Proposed,
and the investigation reopens from the captured data.
