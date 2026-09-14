# NAVI_ONE X17 — pre-flash audit of the fixed-baseline field test

**Date:** 2026-09-14
**Build:** `NAVI_ONE_1_0X17_FIXED_BASELINE_FIELDTEST`
**Locomotive:** Otto, 9950011
**Verdict:** **CONDITIONALLY PASS.** The build is exactly what it says it is.
Nothing in it is unsafe. But the decisive question it was built to ask already
has a measured answer in this repository, and the mechanism means the predicted
field outcome is a latched passage — the same symptom the test is investigating.
Flash it knowing that, and knowing which number tells you it happened.

Audit only. No file was modified by this audit.

## 1. Every claim checked

| Claim | Result |
|---|---|
| Identity is X17 | **Verified.** `NAVI_ONE.ino:78` |
| Three boot lines print as quoted | **Verified.** `.ino:1166–1168`, plus the morphology line and `[CAL] 2 s baseline — keep clear of magnets` |
| Otto's profile is active | **Verified.** `LL_LocoConfig_9950011.h`, entry 70, exit 25 |
| 82 ms floor preserved | **Verified.** `floorMs = NAVI_PASSAGE_FLOOR_MS` (82), shared constant |
| Startup baseline authoritative | **Verified.** `fixedAfterPrime = true`, `.ino:163` |
| Rolling median is shadow only | **Verified.** `baseline_` has exactly one write site in the whole header (`HallCapture.h:361`) and exactly one arithmetic reader (`:132`) |
| `shadow_baseline` / `shadow_delta` published | **Verified.** 1 Hz in `state/status`; 10 Hz in `diag/departure` during DEPART |
| All 12 gates pass | **Verified independently.** `run_tests.sh` exit 0, every gate green |
| Compiles for Otto | **Verified.** 969,587 bytes flash (73%), 59,444 bytes RAM (18%) |

The `CaptureConfig` initializer at `.ino:159–164` is positional. I checked it
field by field against the struct: eight initializers, eight fields, same order.
No silent shift.

## 2. Blocking-class finding — the suite does not test the change

**B1. Exactly one capture in the entire gate suite runs the flashed baseline
policy.** `tests/gate_baseline_latch.cpp:210` sets `fixedAfterPrime = true`.
Every other `CaptureConfig` in every gate — gate 8's marker-visibility cases,
gate 11 ("the baseline through a station stop"), gate_ops' capture rig,
gate_polarity, gate_station_baseline, gate_grillers, gate_interrupted and both
survey replays — runs the **adaptive** baseline the flashed image does not have.

And the one case that does use the new policy applies a sustained sub-entry
shift of +30 counts that never opens a passage. It asserts that the shadow
follows and the authoritative baseline does not. Both are true. Neither is the
question.

This is structurally identical to blocker B2 of the X16 audit, where the suite
exercised a 40 ms floor while the image flashed 90 ms. The value going
unexercised this time is the entire change. **"All 12 gates passed" is not
evidence about X17.** It is evidence about X16 plus one benign new assertion.

## 3. What the flashed policy actually does — measured

The exit test is `|raw − baseline_| < exitMargin` for 8 ms (`HallCapture.h:132`,
`:400`). With `baseline_` frozen at startup, that reference cannot follow the
quiet line. If the quiet line moves as far as the exit margin, a passage that
opens can never close.

Simulated at Otto's real thresholds (entry 70, exit 25), startup baseline 1918,
forty 220-count magnets crossed after the line has moved:

```
 shift | X16 adaptive baseline          | X17 fixed baseline
counts | closed   base  shadow  open?   | closed   base  shadow  open?
     0 |     40   1918    1918     no   |     40   1918    1918     no
    10 |     40   1928    1928     no   |     40   1918    1928     no
    20 |     40   1938    1938     no   |     40   1918    1938     no
    24 |     40   1942    1942     no   |     40   1918    1942     no
    25 |     40   1943    1943     no   |      0   1918    1943    YES
    29 |     40   1947    1947     no   |      0   1918    1947    YES
    56 |     40   1974    1974     no   |      0   1918    1974    YES
```

**At 24 counts every magnet closes. At 25 none of them do**, and the passage is
still open at the end of the run. The cliff is the exit margin exactly.

The repository's own measurements of that quantity:

- the 2026-09-11 evening run changed regime by **29 counts** (1918–1920 → 1947–1949);
- the 2026-09-13 bench anchor drifted **+56 counts in 38 minutes**.

Both are above 25. On this evidence the field test's decisive question —
*does the raw resting level move far enough that a startup-fixed baseline
becomes unsafe?* — is already answered yes by data collected before the build
existed. What X17 adds is the shadow telemetry that will let us watch it happen
in real time, which is genuinely new and worth having.

The failure, when it comes, is the Northpoint symptom: one passage open across
several magnets, merged or swallowed markers, navigation falling behind, a
correct one-strike stop. It is safe. It is not subtle. But it will look exactly
like the fault under investigation unless `shadow_delta` is read alongside it.

## 4. Non-blocking findings

**F2. A soft reset does not recover it.** `HallCapture::reset()` clears `open_`
but leaves `baseline_` untouched. The latched passage closes; the next magnet
re-latches immediately, because the quiet line is still more than 25 counts from
the frozen reference. Recovery needs a power cycle — hands on the flip switch,
wherever the locomotive stopped. Under X16 a reset bought a genuine reprieve;
under X17 it buys one magnet.

**F3. A bad prime is permanent for the run.** If the 2 s calibration lands on a
magnet (+167 counts), simulated over 20 crossings:

```
X16 adaptive   primed=2085   closed 18/20, baseline recovered to 1918
X17 fixed      primed=2085   closed  0/20, baseline still 2085
```

Zero markers from boot — loud, immediate, and exactly the rapid feedback that
was asked for. The `[CAL]` serial warning and operator placement are the only
guards; nothing checks the primed value for plausibility.

**F4. The shadow is blind at rest.** The shadow median is still gated by
`mayAdapt` (PWM > 24) and, for the first 2 s of an open passage, by
`openMigrateMs`. So `shadow_delta` freezes through every station dwell and
whenever Otto is stopped — the same gate that forced the throttle-pulse
procedure on the bench. It will answer "does the level move while running", not
"does it move while parked". It does keep updating during a latch once
`openMigrateMs` has elapsed, which is what makes the latch diagnosable.

**F5. The header's rationale now contradicts the compiled build.** The comment
above `sample()` still says `baseline_` "is allowed to move under an open
passage so that a stale offset cannot hold one open for ever", and the long
block above `updateBaseline()` explains Finding 09 as the reason. With
`fixedAfterPrime = true` that escape is gone. A future reader will find a
documented rationale the flashed image does not implement.

**F6.** With a shifted line the frozen `entryBaseline_` also carries the offset
into the recording, so peak and signed sum are measured with a DC error. At
+56 counts a weak South passage could sum positive. In practice the latch
happens first, so this is a second-order concern.

## 5. Is it safe to flash?

Yes, with the expectation set. Nothing here can raise a throttle, hold a stop
open, or advance position on a fragment. The 82 ms floor, the 500 ms guard,
the recognizer, the navigator, the stations and decision 0080 are untouched —
I diffed them. The worst case is that Otto stops, which is the designed
response.

What it should not be called is a clean test of a hypothesis. It is an
instrumented reproduction of a failure whose trigger has already been measured
twice. That is still useful — the 1 Hz `shadow_delta` trace is exactly the
evidence needed to design a deliberate re-baselining rule — but the run should
be planned as a measurement, not as a candidate for adoption.

## 6. Minimum telemetry to watch

1. **`shadow_delta` in `state/status`, 1 Hz.** This is the whole experiment.
   `|shadow_delta| ≥ 25` is the tripwire: from that moment the next passage to
   open cannot close.
2. **`base_open` and `base_close` on each marker.** Under X17 they must be
   identical and equal to the startup baseline for the whole run. Any
   difference means the fix did not hold.
3. **`dur_ms` on markers.** The first duration in the hundreds of milliseconds
   is the latch beginning.
4. **`floor_rej` and `diag/acquisition`.** Should behave as they did on X16;
   a change means the floor interacted with the new policy.
5. **The boot record's `baseline_mode`** — must read `startup_fixed`.
6. **The primed baseline itself**, from the first status line. If it is not
   within a few counts of the run's usual quiet level, power-cycle and re-place
   before running: under X17 that number is fixed for the session.

## 7. References

- `firmware/test-programs/NAVI_ONE/HallCapture.h` — `fixedAfterPrime`, `:361`
- `firmware/test-programs/NAVI_ONE/tests/gate_baseline_latch.cpp:198–226`
- `docs/NAVI_OTTO_20260913_INVESTIGATION_SUMMARY.md`
- `docs/NAVI_PEAK_CLOSE_REPLAY_20260913.md`
- `field-records/20260913_NORTHPOINT_ACQUISITION_LATCH.md`
- Decisions 0080, 0081, 0085
