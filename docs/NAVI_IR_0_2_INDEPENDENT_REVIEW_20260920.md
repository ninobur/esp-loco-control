# NAVI_IR 0.2 — independent review of the 0.1 field failure and its fix

Independent review requested by the operator, 2026-09-20, after NAVI_IR 0.1's
first field attempt and same-afternoon 0.2 edit. This is a desk review: no
hardware access, no locomotive commands sent, no firmware flashed.

## Scope

Read: the sketch README, the 0.1 implementation report, decision 0089, the
0.1 first-startup record and its structured audit JSON, and the yesterday/
today sensor-evidence trail (noon run analysis, mixed-light lap, TX 1.5
slow-departure sweep, repeater check, movement-contract status, the 09-19
session record). Diffed the current tree against the preserved pre-fix
source (`field-records/firmware-images/20260920_NAVI_IR_0_1_pre_fix_source.tgz`)
to see the actual 0.1→0.2 code change rather than only its prose description.
Independently rebuilt and ran the sketch's own host verification suite and
both ESP32 target compiles rather than trusting the prior reports' numbers.

Not reviewed line-by-line: `HallObserver.h`, `RecoveryControl.h`,
`RouteMap.h`, `Stations.h`, `Ops.h`, `LocoConfig.h`. The diff confirms the
first two are byte-identical between 0.1 and 0.2; the rest are static
config/route tables outside this change's scope.

## What actually happened to 0.1 in the field

Boot telemetry at 13:31:50 confirmed `NAVI_IR_0_1` running on Toby. Two
segments followed (full account in
[the first-startup record](NAVI_IR_FIRST_STARTUP_20260920.md)):

1. **13:33–13:34, unpaired.** The receiver heard CRC-valid movement frames
   throughout but had no paired source (`paired:0`), so it had zero movement
   evidence by construction. Ended in `RECOVERY_EXHAUSTED` / `NAV LOST`.
2. **13:38–13:39, paired.** Pairing worked (`paired:1`, `fresh:1`, counts
   climbing 699→1820). But the audit
   (`field-records/analysis/20260920_navi_ir_first_paired_audit.json`) shows
   every one of the first 11 Hall observations carried `ir_quality:
   "NO_SOURCE"` or `"OPTICAL_INVALID"` and `ir_hard_verdict: "CANNOT_ASSESS"`
   — IR never once contributed a usable distance to a Hall comparison in this
   segment. Over the full 83 s window, 516 of 535 received movement snapshots
   were `INADEQUATE_CONTRAST`, 17 `TRACKING`. Median optical span was
   ~191–239 ADC counts, against ~1,247–1,263 in the same day's noon run.
   Ten straight `AMBIGUOUS` rulings (all IR-less) exhausted the ten-observation
   recovery budget at 13:38:58 (event 41) and the segment ended `LOST`.

Neither segment is evidence that the optical sensor or the joint-navigation
concept is broken. The first never had a paired source; the second never
received IR distance evidence of a quality the navigator could use, for
reasons discussed below.

## The 0.2 fix, read against the diff

The current tree already reads `NAVI_IR_0_2` / `"0.2: preserve declaration
while evaluating; missing IR is not a route strike."` Diffing it against the
preserved pre-fix source shows the change is confined to three files:
`NAVI_IR.ino`, `Navigator.h`, `HypothesisNavigator.h`. The substantive change
is in `Navigator::judge()`.

**0.1's rule:** any observation where the retained hypotheses didn't have
IR distance agreement — for *any* reason, including simply not having
received a usable IR interval yet — fell into the same branch as a genuine
Hall/IR disagreement, incremented `unresolvedCount`, and after ten such
observations halted with `RECOVERY_EXHAUSTED`. Since the low-contrast
segment produced essentially no `TRACKING` samples, every observation
landed in that branch for the same reason (no evidence, not conflicting
evidence), and the budget emptied in ~16 seconds regardless of whether Hall
itself was ever wrong.

**0.2's rule:** a new `distanceAssessable` flag is computed per observation —
true only if *every* retained hypothesis has an interval with
`MotionIssue::None` (i.e. an actual usable IR measurement, not merely absent,
stale, unaligned, reset, or optically invalid). A new `WaitingDistance` state
is entered when the Hall side alone would say `Tracking` but
`distanceAssessable` is false: the navigator holds its declared position,
publishes `WAITING_DISTANCE`, and — critically — does not touch
`unresolvedCount`. The fault-budget counter (now gated behind a
`recoveryActive_` latch) only advances when a retained hypothesis actually
*had* assessable IR evidence and still disagreed. A code comment states the
intent directly: "Sensor loss pauses (but does not renew) this budget."

This is a materially different and correctly targeted fix, not a threshold
tweak. It generalizes past the one failure mode actually observed today
(`NO_SOURCE`/`OPTICAL_INVALID`) to all eight `MotionIssue` categories
(stale packet, clock-alignment miss, pairing reset, frame/direction change,
counter-order violation, etc.) — broader than what was field-exercised, but
consistent with the same principle. `stationService()` also gained an
explicit `waitingDistance()` check that withdraws AUTO with a controlled
stop before falling into the existing position-known/unresolved logic, so
the new state is wired into the AUTO safety path, not just telemetry.

One interaction worth flagging for awareness, not as a defect: if Hall
itself returns `Ambiguous` (a real polarity/vote conflict) in the same
observation where IR is simultaneously unassessable, that event bypasses
*both* the `WaitingDistance` branch (requires Hall-side `Tracking`) and the
budget-increment (`distanceAssessable` is false), so `unresolvedCount` stays
frozen wherever it was — neither advancing toward `RECOVERY_EXHAUSTED` nor
resolving. In practice this only holds the state in `Unresolved`, and AUTO
still depends on the existing all-branches-must-agree station consensus
(`RecoveryControl.h`, unchanged) rather than on this counter. It matches the
README's own disclosed gap ("not a wall-clock or travel-bounded safety fence
if Hall stops producing observations"); it's now possible to point at
exactly where that gap lives in code.

## Independent verification performed here

Rather than accept the implementation report's numbers, I rebuilt and ran
everything myself from the current tree:

| Check | Result |
|---|---|
| `tests/test_navi_ir.cpp` (ASan/UBSan, `-Wall -Wextra -Werror`) | **PASS — 47 checks** (up from 0.1's 35; new cases cover `WaitingDistance`) |
| `tests/replay_core.cpp` + `tests/replay_noon.py` (historical noon replay) | **PASS** — 513 events, 438 tracking, 75 waiting_distance, 0 ambiguous, 0 lost, 0 committed-MM differences, 30 reestablished, peak branches 1 |
| `tests/console_states.cpp` + `tests/test_console_contract.py` | **PASS — 6 state transitions**, cross-checked against the actual `_apply_nav_state`/`USABLE_NAV` bookkeeping in the live `server/ngr_app_v1_11_2.py` via AST extraction (not a mock) |
| `arduino-cli compile` — default (AUTO gated off) | Clean, 1,008,819 B flash (77%), 75,020 B RAM (22%), no non-library warnings |
| `arduino-cli compile` — `NGR_ENABLE_EXPERIMENTAL_AUTO=1` | Clean, 1,009,315 B flash, same RAM |

The noon replay's reclassification is itself a useful confirmation: the 75
events that 0.1-era reporting called "ambiguous" are, under the corrected
model, `waiting_distance` — and the replay still lands on zero committed-MM
differences from the old sketch, so the relabeling didn't change any
position outcome on the one known-good historical run. The two new test
files (`console_states.cpp`, `test_console_contract.py`) are not yet
mentioned in the sketch README's own Verification section — a small doc gap,
not a code gap.

I did not find a regression: nothing in the diff loosens polarity checking,
the one-fault cap, the 24-hypothesis ceiling, CRC/MAC/boot/sequence
validation, or AUTO gating. The change only reclassifies *why* an
observation lacks confirmation, and only pauses (never resets or grants)
the recovery budget on that basis.

## What 0.2 does not resolve

0.2 fixes a navigation-logic bug. It does not explain or fix the sensor
condition that triggered it:

- **The noon-to-afternoon contrast collapse is still an open question.**
  The 12:11–12:23 noon run was clean in both lighting zones — 2,174/2,174
  and 2,510/2,510 movement snapshots `TRACKING`, median span ~1,250 in sun
  *and* shade (see
  [noon run analysis](IR_NOON_RUN_ANALYSIS_20260920.md)). The 13:38 paired
  segment, on the same car, same day, roughly an hour later, saw median span
  collapse to ~191–239 and 516/535 snapshots `INADEQUATE_CONTRAST`. The
  first-startup record already asked the operator whether the IR car's
  sensor position, wiring, or wheel insert changed between the two runs, or
  only Toby was reflashed — that question is still unanswered as of the
  documents reviewed here. 0.2 will now *wait* gracefully instead of
  declaring LOST if the same condition recurs, but it will not produce a
  working joint-navigation demonstration if the optical channel is still
  mostly `INADEQUATE_CONTRAST` next time.
- **Low-speed/departure blind spot is unchanged.** The TX 1.5 synthetic
  sweep in
  [the slow-departure record](IR_SLOW_DEPARTURE_LIMITATION_20260919.md)
  found zero completed pulses registered at or below ~1 Hz wheel rotation
  (19 expected, 0 observed at 1000 ms and slower, all three duty
  fractions). Crawl and departure will present as `WaitingDistance` rather
  than a false strike now, which is strictly better than 0.1's behavior, but
  the sensor still cannot measure travel at those speeds.
- **No physical distance-error bound exists.** Unchanged from 0.1:
  `distance_bounds` is still `UNVALIDATED` everywhere in telemetry; nominal
  association can suggest alternatives, never hard-exclude one.

## Documentation currency

`firmware/README.md`'s catalog table still lists this sketch as version
"0.1" with 0.1's evidence text, even though the tree has read 0.2 since
13:51 today. Per this repo's own librarian-check standing rule I've updated
that row to reflect 0.2 and today's field result; I have **not** committed
`firmware/README.md`, since it already carries other unrelated pending
catalog edits (IR_SCOPE_ESPNOW_RX, IR_USB_BENCH) from earlier today that
aren't mine to commit.

## Bottom line

The 0.1 field test did not show a working joint Hall/IR navigator — but it
also didn't show a broken one. It showed an unpaired first segment (no
navigation input existed) and a second segment where a real, verifiable
navigation-logic bug (treating "no evidence" as "bad evidence") converted a
sensor problem into a false `RECOVERY_EXHAUSTED`. The 0.2 edit fixes that
specific bug in a way that is well-targeted, generalizes correctly to the
whole class of "distance not assessable" conditions, is now covered by host
tests including a first console cross-validation, and compiles clean at
close to 0.1's flash/RAM footprint. It has not been flashed or field-run.

The open question is not "is the fix right" — the diff, the passing tests,
and the noon-replay reclassification all support that it is — but "will the
optical channel deliver usable evidence on the next attempt," which is a
sensor/lighting/rigging question 0.2 cannot answer by itself. Before the
next flash, resolving what changed physically between the noon run and
13:38 would do more for the next attempt's chances than any further firmware
change.

## References

- [Sketch README](../firmware/test-programs/NAVI_IR/README.md)
- [0.1 implementation report](NAVI_IR_0_1_IMPLEMENTATION_REPORT.md)
- [0.1 first-startup field record](NAVI_IR_FIRST_STARTUP_20260920.md)
- [Decision 0089](decisions/0089-navi-ir-joint-evidence.md)
- [Noon run analysis](IR_NOON_RUN_ANALYSIS_20260920.md)
- [Slow-departure limitation](IR_SLOW_DEPARTURE_LIMITATION_20260919.md)
- `field-records/analysis/20260920_navi_ir_first_paired_audit.json`
- `field-records/firmware-images/20260920_NAVI_IR_0_1_pre_fix_source.tgz`
