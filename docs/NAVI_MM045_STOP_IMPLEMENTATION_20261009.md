# NAVI MM045 repeating STOP implementation

**October 9, 2026 — review candidate, not flashed, merged, or field accepted.**
Folder, primary sketch and runtime identity: **`NAVI_EWO_0_1_MM045_STOP`**.
Open [NAVI_EWO_0_1_MM045_STOP.ino](../firmware/programs/NAVI_EWO_0_1_MM045_STOP/NAVI_EWO_0_1_MM045_STOP.ino).

## Sketch naming and location — October 9 cleanup

David approved matching sketch names and a clear repository index after the
former generic filename made this candidate difficult to locate. The package
was renamed from `NAVI_EYES_WIDE_OPEN_INTEGRATED` to
`NAVI_EWO_0_1_MM045_STOP`; its firmware source is preserved byte for byte from
reviewed commit `f40e388b5fcfa5d6be8cba8aab6dc02d07a21312`. Test consumers use
the new path. The old directory contains only a signpost.

The short [repository opening page](../README.md) and [firmware index](../firmware/README.md)
separate this current candidate from diagnostics and historical evidence. The
complete previous [catalog](../firmware/HISTORY.md) and
[sketch README](../firmware/programs/NAVI_EWO_0_1_MM045_STOP/HISTORY.md) are preserved.
No control, navigation, PWM, grade, radio, server or configuration behavior is
changed by the naming cleanup. No historical Git structure is rewritten.

David's visible working location is
`/Users/davidbrown/esp-loco-control/CURRENT/firmware/programs/NAVI_EWO_0_1_MM045_STOP/`.
The full review checkout keeps sibling dependencies together. The enclosing
checkout's branch, uncommitted changes and other working copies are preserved.
The code rollback point for the rename is `f40e388b`; prior behavior rollback
points below remain unchanged.

### Naming cleanup validation

- All **25** moved source/test files compare byte for byte with their pre-rename
  counterparts at `f40e388b`; all three executable test consumers change paths
  only. Folder, primary filename and runtime identity match.
- Both historical documents retain the complete original text verbatim after
  an explanatory banner. Current index/readme/report relative links resolve.
- **PASS:** all 12 host executables with ASan/UBSan from the visible `CURRENT`
  checkout. The 12 MM045 integration/format tests pass again, as does the pulse
  JSON serialization test.
- **PASS:** ESP32 / Otto 9950011 compilation from the renamed visible path,
  core 3.3.12, placeholder credentials and `-Werror=format`. Flash remains
  **1,018,883 / 1,310,720 bytes**, static RAM **58,124 / 327,680 bytes**.
  Only the same three Adafruit INA219 enum-deprecation warnings occurred.
- **Two additional pre-existing test-fixture failures remain.** The broader
  tool run passes 13 of 14 tests; `test_actual_callback_and_queues_cannot_feed_pulse_loss_to_navi`
  in `tools/tests/test_navi_pulse.py` fails to compile its test scaffold because
  it does not declare `pulseTransportQ`, `pulseTransportObservation` and
  `NAVI_PKPH_MM_PER_SEC`. Separately,
  `server/tests/test_ir_speed_dashboard.py::SpeedTest::test_actual_ewo_firmware_payloads`
  expects four emitted rows and receives more. Both identical failures were
  reproduced against an untouched `git archive` of `f40e388b`, using the old
  paths. No test assertion, firmware behavior or server implementation was
  changed to hide these failures. Fixing those older fixtures is separate work.
- The enclosing `/Users/davidbrown/esp-loco-control` remains on its original
  branch/commit with all six pending-work paths and their contents unchanged.
  Its local Git exclusion list ignores only `/CURRENT/`, so the separate review
  checkout cannot accidentally enter that checkout's commits. The existing
  review worktree was moved into `CURRENT`; no duplicate editable MM045 copy
  was made. No hardware action was performed.

## Authority and provenance

David explicitly commissioned the code and settled the profile, repeated
operation, five-second dwell, continuous final ramp, and startup distinction.
The [mandatory guide](https://github.com/ninobur/esp-loco-control/blob/c5fcef1c741dbb217699f7eda4f9a99bf3123053/docs/NAVI_EWO_0_1_ARCHITECTURAL_NOTE_20261009.md)
and [complete MM045 decision record](https://github.com/ninobur/esp-loco-control/blob/c5fcef1c741dbb217699f7eda4f9a99bf3123053/docs/NAVI_STOP_MM045_BIDIRECTIONAL_TEST_DEFINITION_20261009.md)
include his final clarification: IR supplies movement, velocity **and location**
within an established run. Only a new start or reposition uses the startup rule.
Those documents govern over conflicting older material on this firmware branch.

Implementation branch: `codex/navi-cruise-stop-20261009`, based on the stationless
`c7c21163b016e443524eb9fa483f8b706ce76e22`. The immediate code rollback point is
`00d5dc590cbaf7fd65326fb15471c71d18581566` (cruise 90 with grades retained).
The prior recorded R2_FT2 field reference remains
`ab0938b0f01531a38ce4d8a1d9d932ffad4f0334`; no hardware rollback was performed.

This follows the enduring decision-system principles: NAVI owns location,
physical evidence remains independent of expected results, and the established
actuator executes the selected instruction. No adaptive speed controller,
additional safety cap, station admission machine, or CTO protocol is introduced.

## Resulting operation

The trial applies under the existing AUTO/GO controls and repeats each circuit.
Ordinary cruise is **90 PWM**, with the existing location/direction-dependent
grade settings and transitions unchanged. One STOP overlay selects the command
before the existing single `requestPwm()` / `serviceRamp()` actuator path.

The same target is used in both directions: **150 mm CW from MM045**, the
midpoint of MM045–MM046. The eleven-MM footprint is projected backward from
that point. A fractional MM uses the surveyed interval length, not an assumed
universal millimetre spacing. The lookup can accept other route targets.

| Distance remaining to target | Instruction |
|---|---|
| Eleven through seven MM sections | 90→80→70→60→50, ten PWM steps over each MM |
| Seven through two MM sections | Five full MM sections at 50 |
| Two through one MM section | 50→30 evenly over the penultimate section |
| Final MM | Continuous time-based ramp to zero; nominal 30→20 in its first half, 20→0 in its second half are calibration aims |
| Applied PWM zero | Five-second dwell; completion requires PWM still zero |
| Release | Ramp toward current cruise, ordinarily 90; do not reissue the completed stop in its footprint |

The final ramp uses the existing **31 ms per PWM count** decrement. From an
applied PWM of 30, that is nominally 930 ms of actuator steps, subject to loop
scheduling. This is **not a prediction of distance or physical stopping time**.
David expressly allowed PWM 20 at the halfway point to be an aim rather than
a position gate. The ramp continues even with no further IR arrivals. No speed
feedback attempts to force agreement with the geographic aims.

Release uses **150 ms per PWM count** for STOP restart, as David corrected
on October 9 after the first trials. The previous implementation incorrectly
used ordinary AUTO's 62 ms/count. The existing actuator can execute its first
count immediately after dwell; subsequent counts are 150 ms apart, nominally
about 13.5 seconds from zero toward PWM 90. Ordinary AUTO acceleration remains
62 ms/count and the final decrement remains 31 ms/count. Actual stop position
and available position evidence do not gate dwell completion.
An early stop is not corrected by additional movement. Existing Decision 0120
still withdraws AUTO on actually observed displacement at PWM zero and requires
operator verification/redeclaration.

## Entry, interruption, and position authority

- During an established run, entry within any profile section and STOP/GO retain
  NAVI's existing IR location. There is no start-marker or prior-phase gate.
- For a new start/reposition, operator declaration is location truth. Its MM
  instruction applies immediately. IR supplies movement and velocity until the
  first MM contact establishes the fine reference; declaration-time travel is
  not misrepresented as an exact marker offset.
- The new read-only offset accessor exposes NAVI's existing physical relationship,
  including reversals and genuine missed magnets. It creates no second position
  estimator and advances no location. A provenance bit distinguishes the startup
  interval from an actual Hall anchor; entry/resume does not reset it.
- Unavailable established fine-position evidence preserves the current command.
  It does not revert to a fabricated marker position. An established final ramp
  and its PWM-zero dwell can finish without further IR.
- Manual, STOP, AUTO off, E-stop and low-voltage authority remain. Disabled AUTO
  cannot depart. An unfinished final stop can resume in its footprint; manual
  relocation is evaluated at the current instruction on GO. Nonzero applied
  PWM invalidates previously accrued continuous zero-PWM dwell.
- A completed visit remains consumed during departure, until leaving its
  footprint, including departure under Manual. The next visit can repeat.
  Direction change evaluates the new direction. Explicit declaration resets
  this operation's progress; it is the operator's new location instruction.
- Entry already at PWM zero in the final section can begin dwell immediately;
  the implementation does not accelerate merely to recreate earlier phases.

## Observation and source changes

`NaviStopOverlay.h` holds the portable geographic lookup and minimal execution
progress. The integrated sketch selects its command and adds `telem/stop` JSON
on relevant changes and once per second while an overlay is active. Existing
MQTT topics and payloads, native recorder wire formats, and server code remain
unchanged. Native requested/applied PWM records remain available.

STOP telemetry includes timestamp, current MM/direction, target, valid physical
position and signed stop error (positive beyond the target in travel direction),
IR speed when available, geographic PWM aim, requested/applied PWM, section,
execution state, AUTO state, and timed release. `OPERATOR_MM`, `HALL_IR`, and
`UNAVAILABLE` distinguish evidence sources. Unavailable fine coordinates and
errors are JSON null, never invented measurements. Dwell entry and release are
explicit execution transitions. The first active record identifies the entry MM.

The map/grade table, selected locomotive profile, `requestPwm()`, `serviceRamp()`,
command handler, PWM-zero movement hold, battery logic, IR receiver, reverse
handler, and existing operation-admission rules were checked against c7c21163.
They are unchanged. Declaration additionally resets only the new STOP progress.
Legacy station machinery remains retired.

## Validation and remaining field work

- **PASS:** 12 integrated host executables, including both map polarities and
  underlying EWO regression. New ASan/UBSan cases cover all profile sections,
  both directions, all surveyed interval midpoints, route wrap, cold entry,
  established IR entry/resume, reversal, IR loss, dwell timing/counter wrap,
  Manual interruption, release suppression and repeat.
- **PASS:** 12 integration/format tests. The actual sketch's cruise, STOP decision,
  telemetry formatter and actuator functions are compiled and exercised with
  hardware-output stubs. Cases include both grades, a startup inside the 50
  section, a pulse-free final ramp, exact five-second dwell and ramped departure.
  Emitted STOP JSON is parsed and its source/unknown-coordinate/release fields
  checked. The final modified cases were rerun after review corrections.
- **PASS:** Otto 9950011 ESP32 build, core 3.3.12, format warnings treated as errors.
  Verification uses placeholder credentials and is not a deployment image.
  Only the pre-existing Adafruit INA219 enum-deprecation warnings appeared in
  the full build. Final flash: **1,018,883 / 1,310,720 bytes**; static RAM:
  **58,124 / 327,680 bytes**.
- **PASS:** whitespace and source-boundary review. These are implementation
  checks, not independent review or physical evidence.

An independent Codex review of `f40e388b` against `c7c21163`, using the governing
documents at `c5fcef1c`, completed October 9 with **no actionable findings**.
The reviewer independently passed all 12 host executables and all 12
integration/format tests, checked geometry, IR continuity/startup, interruption,
dwell/repeat and grade/control authority, and did not rerun the ESP32 build.

David's authorized track testing remains necessary to calibrate the final ramp
and measure stop error in each direction. Test normal
entry, entry within the 50/penultimate/final portions, STOP/GO during the sequence,
and subsequent-circuit repeat. Distinguish an established run from a new start
or physical reposition. No Four-Station Local service, collision overlay, or
CTO/CE coordination is implemented by this single-target trial.

## First CW trials and restart-rate correction — October 9

David reported smooth deceleration on the first CW trial, with the Hall sensor
physically midway between MM044 and MM045. Automatic restart occurred about
three seconds after physical rest. On the second CW round, he reported a
landing between MM054 and MM055. Both rounds have the same programmed target,
midway between MM045 and MM046; there is no second programmed stop at MM054.
The [operator field record](../field-records/20261009_MM045_CW_FIRST_TWO_STOPS.md)
preserves both observations and the unavailable provenance/telemetry.

David then corrected the restart ramp to 150. The STOP restart request now
uses 150 ms/count; PWM 90, grade settings, stopping geometry, deceleration,
five-second applied-zero dwell, and the actuator itself are unchanged. This
is a requested parameter correction, not a diagnosis or retuning of the
landing discrepancy. The pre-correction source is `557de9a`.

The actual-sketch/actuator integration check passes at 150 ms/count, including
no second count at 149 ms and a count at 150 ms, departure suppression, retained
ordinary AUTO ramps, grades, Manual/E-stop/low-voltage behavior and dwell.
An initial timing assertion incorrectly assumed the existing actuator delayed
its first count after dwell. Source inspection showed that its clock predates
dwell; the test now checks that immediate first count and the requested 150 ms
spacing thereafter. No actuator implementation was changed to satisfy the test.
Otto/core 3.3.12 compilation with local credentials passes: flash 1,018,871 bytes,
static RAM 58,124 bytes. The earlier independent review covered `f40e388b`, not
this later correction. The running locomotive is not updated by editing this
file; an operator upload is separate. No upload was performed by the agent.
