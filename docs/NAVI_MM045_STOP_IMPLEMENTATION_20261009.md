# NAVI MM045 repeating STOP implementation

**October 9, 2026 — review candidate, not flashed, merged, or field accepted.**
Runtime identity: `NAVI_EWO_0_1_MM045_STOP`, in the existing integrated sketch.

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

Release uses the existing **62 ms per PWM count** acceleration ramp. Actual
stop position and available position evidence do not gate dwell completion.
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

Independent review and David's authorized track testing remain necessary to
calibrate the final ramp and measure stop error in each direction. Test normal
entry, entry within the 50/penultimate/final portions, STOP/GO during the sequence,
and subsequent-circuit repeat. Distinguish an established run from a new start
or physical reposition. No Four-Station Local service, collision overlay, or
CTO/CE coordination is implemented by this single-target trial.
