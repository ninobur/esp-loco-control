# NAVI_SIMPLIFIED — code review

**Reviewer:** Claude (Opus 5) · **Date:** 2026-09-18 · **Branch:** `agent/toby-1-13-flash`
**Subject:** `firmware/test-programs/NAVI_SIMPLIFIED/` @ `SKETCH_NAME = NAVI_SIMPLIFIED_DEV_20260918`
**Status of the subject:** untracked in git. Nothing in this directory is under version control at the time of review.

Nothing in this document is a ruling. It records what the code does against what the code says it does.

## What was actually run

| Check | Result |
|---|---|
| `g++ -std=c++17 -Wall -Wextra tests/test_navigation.cpp` | builds clean, **passes** ("NAVI_SIMPLIFIED navigation checks passed") |
| `arduino-cli compile --fqbn esp32:esp32:esp32 --clean` (core 3.3.11, Otto profile) | **builds**. 966,439 B flash (73%), 54,708 B static RAM (16%) |
| Sketch-local warnings | 8, all `-Wvolatile` (`++` on a volatile). No others. |
| Ten-marker word uniqueness over the real `ROUTE_POLARITY`, both directions | verified by the test, 171×171×10, every word unique |
| Station machine walked host-side through a Grillers CW stop | see finding 1 |

The recognizer, navigator and reachability headers are clean under `-Wall -Wextra` and are genuinely host-testable, which is the part of this design that is working. Everything below is in the `.ino` and in the gap between the prose and the constants.

---

## 1. The station departure ramp runs at 62 ms/count, not the 200 the operator asked for  — behavioural

`Stations.h` sets `STATION_DEPART_STEP_MS = 200` and quotes the ruling directly: *"Restart from the station should have a slow ramp. 200."* The machine duly returns `stepMs = 200` on the `DEPART` order.

The single call site drops it:

```c
// NAVI_SIMPLIFIED.ino:1036
if (o.setThrottle) requestPwm((int)o.pwm, AUTO_STEP_UP_MS, o.stepMs);
```

`requestPwm(target, up, down)` — `o.stepMs` lands in the **down** slot. A departure is an **up** move, so `serviceRamp()` paces it with `stepUpMs`, which is `AUTO_STEP_UP_MS = 62`.

Walked host-side, Grillers CW (`departCW = 110`):

```
mm= 64 off= +1 phase=DEPART  event=DEPART  pwm=110 stepMs=200
        -> requestPwm(target=110, up=62, down=200); walk 0->110 takes 6.8 s
```

Intended 22.0 s. Actual 6.8 s — **3.2× faster than specified**. Every other order the machine issues is a down move (`ARMED`, `APPROACH`, `ZONE`, `ZERO_RAMP`), so this is the one path where the slot is wrong, and it is the path the Grillers traction work is about: *"traction not adequate to launch 3 coaches."* The stopping point was moved to −1 to give Toby level track to launch from; the launch itself is then done at a third of the intended gentleness.

The same slot error silently affects the `MISSED` and `PHASE_TIMEOUT` hand-backs, which also hand up to `cruisePwm`.

## 2. `NAVI_BASELINE_ADAPT_PWM` is demanded at compile time and then not used — silent drift

The sketch refuses to build without it, in the strongest language in the file:

```c
#ifndef NAVI_BASELINE_ADAPT_PWM
#error "This locomotive's profile has no NAVI_BASELINE_ADAPT_PWM. Measure the tractive
        floor from its own PWM/speed fit; do not copy another locomotive's."
#endif
```

Toby's profile carries 25, Otto's 24. The macro appears nowhere else in the build. `SimpleHall.h` hardcodes the number in both places that need it:

```c
if (pwm <= 30) { ... BaselineReject::LowPwm ... }   // SimpleHall.h:114
if (pwm <= 30) lowPwm_ = true;                      // SimpleHall.h:129
```

So the guard that exists to stop one locomotive flying another's measured value guarantees only that a measured value is *present*, not that it is *used* — and the value actually in force (30) is neither locomotive's.

## 3. The build identity claims a 500 ms rebound guard that is not in the code

```c
// Corrective field-test build. Decisions 0080/0081: morphology is diagnostic
// only, passages are never paused or stitched, and the rebound guard is 500 ms.
#define SKETCH_NAME "NAVI_SIMPLIFIED_DEV_20260918"
```

`NAVI_GUARD_MS 500U` is defined in both profiles and referenced by neither the sketch nor any header. `SimpleHall` has no rebound guard at all: after a close (`underCount_ >= 30`), `state_` becomes `Guard` or `Idle`, and the entry test at the top of `sample()` runs in both — so a second `Open` can be declared roughly **32 ms** after the previous one closed.

This matters because `state/bootid` is, by this file's own comment, *"the ONLY thing that tells telemetry which build is running."* A build that announces a 500 ms rebound guard it does not have will be read back from the logs as one that had it.

The 80 ms figure in the file banner is a different thing — the baseline settling guard — and that one is real (`SimpleHall.h:113`).

## 4. A queue overflow recorded while position was unknown fires on the *next* declaration

```c
// NAVI_SIMPLIFIED.ino:1168
static uint32_t handledEventDrops=0;
if (hallEventDrops!=handledEventDrops && navigator.positionKnown()) {
  navigator.haltForLoss();
  withdraw("HALL EVENT QUEUE OVERFLOW. Position is not known. Declare it.");
  handledEventDrops = hallEventDrops;
}
```

`handledEventDrops` is only advanced inside the branch. If a drop happens while position is unknown — which is exactly when the operator is about to declare — the counter stays unacknowledged. The operator then declares position, `positionKnown()` goes true, and on the very next loop pass the locomotive withdraws the declaration it was just given and tells him to declare it again. One spurious withdrawal per stale drop, landing at the least helpful possible moment.

Acknowledging the drop unconditionally, and acting on it only when position is known, separates the two.

## 5. `StopCause` / `StopArmingPolicy` is wired to nothing

`Ops.h` defines `StopArmingPolicy` with a documented purpose: latch the reason for a stop across the whole ramp *"so a recovering battery cannot turn the last 1 -> 0 step into a controlled stop."* The class is never instantiated. `requestPwm()` accepts the argument and discards it — it is not read, not stored, not even `(void)`-cast:

```c
static void requestPwm(int target, uint16_t up, uint16_t down,
                       StopCause stopCause = StopCause::Controlled){   // unused
```

Three call sites pass `StopCause::Safety` (`withdraw`, e-stop, low voltage) in the belief that it means something. Either wire it or delete it; as it stands the safety/controlled distinction reads as implemented at every call site and is implemented nowhere.

## 6. Two comments describe a step-back that `setDirection()` does not do

`applyTravelDirection()`:

```c
navigator.setDirection(d);        // steps navMm back along the OLD heading
```

and `serviceStatus()` at length: *"after a direction change, which steps navMm back one so the next advance lands on the marker about to be met again."*

`SimpleNavigator::setDirection()` does not touch `navMm`:

```c
void setDirection(int8_t dir) {
  if (!dir || dir==s_.navDir) return;
  s_.navDir=dir; s_.target=nextMarker(s_.navMm,dir); beginUnresolved(); resetPending_=true;
}
```

`target` is therefore set one marker beyond the one the locomotive is about to re-meet. The error is **masked**, not harmless: `beginUnresolved()` drops the state to `Unresolved`, `positionKnown()` goes false, and `judge()` never takes the advance path — so the wrong target is never consulted. The behaviour on the railway is that a manual reversal costs the position outright and recovers only after a full ten-marker word. That may well be what is wanted, but it is not what either comment says, and if the step-back were ever added the target would then be wrong by one.

## 7. `state/bootid` publishes `seq_n: 0` and `baseline_adapt_pwm: 30`

Lining the format up against the arguments (`NAVI_SIMPLIFIED.ino:998`):

| field | published | actual |
|---|---|---|
| `seq_n` | `0` | 10 — the navigator's word length, the thing the whole recognizer rests on |
| `baseline_adapt_pwm` | `30` | profile says 25 (Toby) / 24 (Otto); see finding 2 |
| `guard_ms` | `80` | correct (baseline guard) |
| `amp_floor` | `0.00` | honest — morphology is diagnostic only in this build |

`seq_n: 0` is the one that will mislead a log reader: it describes a navigator with no sequence matcher.

## 8. `STATION_DWELL_MS` is 5 s; the header says 30 s three times

```c
static const uint32_t STATION_DWELL_MS = 5000UL;   // Stations.h:151
```

Against the prose above it: *"Dwell 30 s, deliberately longer than NAVI_2's 15. The operator wants the dwell to exercise the baseline latch of finding 08"*, *"which for a 30 s dwell with the baseline frozen is the safer place to be sitting"*, and a reference to a 37.7 s dwell passage at Grillers.

The test pins the current value (`assert(STATION_DWELL_MS==5000UL)`), so it is deliberate in the test at least. Whichever is right, the stated reason for the long dwell — exercising the baseline latch — is not being exercised at 5 s.

---

## Lower-priority

9. **`hallTask` spins without yielding on serial exhaustion.** `if (nextEventSerial == UINT32_MAX) { estopAsserted = true; continue; }` — `continue` skips `vTaskDelayUntil()`, and the condition never clears, so the task busy-loops at priority 3 on core 0 forever. Unreachable in practice (2^32 openings), but the intended behaviour is surely stop-and-idle, not starve the scheduler.

10. **The IR boot probe breaks the settle rule the same function documents.** `irService()` explains at length why the first read after a channel switch must be discarded, and the steady-state path does it twice. The probe path reads `IR_PIN` once and uses that first, contaminated reading as the probe value. The probe's whole job is to report the pin's span honestly.

11. **Reachability suppression is off during `Unresolved`.** `physicallyUnreachable()` is gated on `navigator.positionKnown()`, which is false throughout the ten-marker recovery — so rebounds (see finding 3) are pushed into the polarity word exactly when the word is being assembled. A corrupted word will fail to match rather than match wrongly, so the failure mode is `UNRESOLVED_LIMIT_STOP`, not a false position. Worth knowing when reading a limit-stop.

12. **The first `Ambiguous` passage re-accelerates using the position it just discredited:** `requestPwm(cruisePwmAt(before.navMm, before.navDir, ...))`. Bounded — it only picks between 90, 105 and 110 — but `before.navMm` is by definition the value the contradiction was about.

13. **`OVERSHOOT_ABANDON` re-accelerates a missed station to cruise** while still in `Ramp`. Field landings (1–3 markers of coast) keep this well clear in practice; the point is that the recovery from "I did not stop where I meant to" is "go back to cruise."

14. **Hardcoded `120` in a per-locomotive message:** `warn("THROTTLE CAPPED at PWM 120 for NAVI reachability")` while the cap is `NAVI_MAX_OPERATING_PWM`. Both profiles are 120 today, so the message is true today.

15. **`BaselineReject::Interrupted` and `SimpleHall::firstOverMs_` are dead.** An interrupted collection increments `rejected_` and reports nothing; the enumerator exists for it.

16. **Shadowed `dt`** in the `Ruling::Advanced` block shadows the outer `const uint32_t dt`. Compiles clean; reads badly in a block that is about timing.

17. **`-Wvolatile`:** eight `++` on volatiles. They are cross-task counters and the increments are not atomic regardless; the warning is the compiler pointing at that.

---

## What is right, and worth saying

- **The layering holds.** `Ops.h`, `Stations.h`, `RouteMap.h`, `SimpleNavigator.h`, `Reachability.h` are pure, Arduino-free and driven by a host test that builds warning-free under `-Wall -Wextra`. The stated reason — *"this layer had none, so it is the layer that failed on the railway"* — is borne out by where the findings above landed: every behavioural one is in the `.ino`.
- **The build names itself.** `BUILD_CLASS = DEVELOPMENT_NOT_FOR_FIELD`, `FIELD_ACCEPTED 0`, and the risk printed at the top of the file and at boot.
- **Truncation is guarded everywhere it matters.** Every `snprintf` into a published buffer checks its return; `serviceStatus()` publishes an `ALERT_OVERSIZE` rather than a truncated alert. `pub()` refuses an oversize payload and counts it.
- **Nothing is lost in silence.** The publish queue holds while the broker is away instead of dequeuing into nothing, and every genuine loss is counted and published.
- **No `atoi()`.** Every payload parser refuses what it cannot read, and the emergency topic asserts on ambiguity.
- **The ten-marker word is verified against the real route**, both directions, rather than asserted.

## Suggested order of work

1. Finding 1 — one-line fix, and it is the one the locomotive can feel.
2. Findings 2, 3, 7 — the build says things about itself that are not true. These are cheap and they are what the logs will be read against.
3. Finding 4 — one spurious withdrawal, at the worst moment.
4. Finding 5 — decide: wire it or delete it.
5. Findings 6, 8 — reconcile prose and constants; say which is authoritative.
6. **Put the directory under version control.** None of the above is reviewable over time while the subject is untracked.
