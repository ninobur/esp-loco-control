# Response to the NAVI_ONE 0.1 review

**Date:** 2026-08-30
**Subject:** `docs/reviews/NAVI_ONE_0_1_REVIEW_20260829.md`
**Result:** `NAVI_ONE_0_2`. Two findings ruled on and implemented; one of the two
turned out to rest on a false premise, and the measurement is now a gate.

## Verification of the review

The review was checked against the source rather than accepted. Every claim
examined was true. The ones re-derived line by line:

| Finding | Confirmed at |
|---|---|
| 1 — the strike does not latch | `oneStrike()` cleared `autoRunning` only; `judge()` left `state = Declared`; `cmd/go` passed every check afterwards |
| 1b — judging continues after the strike | nothing gated `judge()`; a matching polarity during coast-down **advanced navMm on the discredited position** |
| 3 — recognizer reset from the wrong thread | `Navigator::declare()`/`setDirection()` call `rec_.reset()` on the loop thread; `recognizerResetRequest` then resets it again on the Hall task |
| 5 — `clipped_` contaminated between passages | set by `updateBaseline()` at any time, cleared only in `close()` |
| 6 — evidence destroyed when the broker is down | `xQueueReceive` then `if (mqtt.connected())`; no drop counter; no `setConnectionTimeout` anywhere |
| 7 — `cmd/estop` with a non-numeric or empty payload *clears* e-stop | `estopped = atoi(payload) != 0` |
| 7b — garbage on `cmd/direction` selects REVERSE | `motorDirection = (n == 2)` behind an `n != 1` gate |
| 9 — the selector file | header says TARGET **Otto**, active include is **Toby's**; Toby's line appears twice; Hans points at a file that is not there; there is no Otto line at all |
| 10 — voltage protection fails silent, no hysteresis | `inaReady = ina219.begin()` with no warning path; trip and recover both at 14.4 V |
| 13.1 — the entry sample is stored twice | it is written to the pre-roll, replayed, then pushed again |

Two corrections to the review, both small: `replay_lap` does print
`detections in the record: 177`, so the five extra records are unasserted
rather than invisible; and the strike-then-advance case (1b above) deserves to
be its own finding — it is the most dangerous behaviour in the sketch, because
a strike could be *followed* by a silent wrong advance on the very position it
had just discredited.

## Ruling 1 — the strike latches. Implemented.

Recorded as **decision 0058**. `NavState::Struck`; `positionKnown()` false;
every later passage rules `NoPosition`; `nav_ready` 0 and the console's position
fields `UNSET`; enrolment withdrawn and `state/auto 0` published so `cmd/go` is
refused; only `declare()` clears it. Contract gate **T4** pins all of it.

## Ruling 2 — the witness stops and names. Implemented, and it will never fire.

The operator ruled that `Trust::Contradicted` should stop AUTO and report the
true marker. It now does. Arming it changed nothing measurable, and the gate
written to watch it work is what showed why:

> `verifySequence()` compares each stored reading against the polarity of the
> marker it was stored at — and a reading is only ever stored *after* it matched
> that marker's polarity. The word always fits. `Contradicted` is unreachable
> from `judge()`.

**The review's Finding 2 rests on a false premise.** It states that after a
missed magnet the witness *"catches this almost immediately"* and `seqAt`
*"names the true position — verified in the route data."* Across all 171 start
positions it catches it **zero** times. The window-10 uniqueness fact is true;
the code does not use it to locate anything, it re-checks a test that already
passed.

What the gate measured instead, one magnet missed, from every start position:

| | |
|---|---:|
| caught immediately by polarity | 90 / 171 |
| advanced silently at the moment of the miss | **81 / 171** |
| caught by the ten-magnet witness | **0** |
| worst case markers hidden before the chain refused it | **6** (~1.8 m) |

The witness is kept armed as an **assertion**: it should never fire, and if it
ever does an invariant has broken and stopping is right. No claim is made for
it anywhere. Recorded as **decision 0059**, which also states the honest bound:
*no identification error survives more than six markers; roughly half are caught
on the next magnet; nothing on board detects the other half sooner.*

## Gates

All three green on `NAVI_ONE_0_2`:

- survey replay — 195/195 primaries accepted, 156/156 non-primaries refused, 0 misclassified
- contract — **176 checks, 0 failures** (was 87; T4 extended, T6 kept adversarial across the new latch, T9 added)
- real-lap replay — 172 advances, 0 refusals, circuit closed at MM040, PROVEN

Compiles at 974,707 bytes (74%), 54,324 bytes globals. Banner reads
`NAVI_ONE_0_2 — 9950012`. **Not flashed.**

## Not done — awaiting the operator

The review's remaining findings are real and untouched. In the order I would
take them:

1. **7 — command validation** (`estop`/`direction` payloads, declarations
   accepted while running). Safety-shaped, small, no ruling needed.
2. **3 — the reset-thread race.** `declare()`/`setDirection()` must stop calling
   `rec_.reset()` directly; the host gates rely on it doing so, so the fix
   touches the tests too.
3. **6 — transport.** Drop counters, `setConnectionTimeout`, and a decision
   about what to do with evidence generated while the broker is down.
4. **10 — voltage.** Warn when the INA is absent; carry QUORUM's 13.25/14.0
   band; decide whether low voltage should keep bypassing the ramp in MANUAL.
5. **9 — the selector file**, which has re-grown every trap its own comments
   document.
6. **5 — the shape test's abstentions**, and **8 — the Hall-side ADC settle
   read**.
7. **11 — a fourth gate over `handleCommand()`**, which is the layer where both
   of 0.1's field bugs lived and the only layer with no coverage.
8. **13 — the small items**, including the `TooSoon` comment that still says
   500 ms when the guard is 200.

## References

- decisions 0058, 0059
- `firmware/test-programs/NAVI_ONE/` at `NAVI_ONE_0_2`
- `docs/NAVI_ONE_NEXT.md`
