# 0067 — The curve into Patio runs at 105 CCW, and ends where the station approach begins

Status: Proposed  (2026-09-01)

## Decision

A second section, counter-clockwise only:

| where (CCW, descending) | throttle |
|---|---|
| MM33 → MM26 | **105**, constant |
| MM25 onward | base cruise, and later the Patio approach |

**No back-off ramp**, deliberately. The section simply ends at MM26.

`landmarkAt()` moves Arches from MM107 to MM108. The station centres are
confirmed as Patio 15, Grillers 63, Arches 108, Bamboo 157.

Ships as NAVI_ONE 0.7.

## Context

Operator, 2026-09-01: *"This one is different because it is a curve before a
station ... It is okay if the approach is a bit slower but the train should
travel smoothly through the curve."*

Measured from the 2026-06-30 calibration (Otto, two coaches, ~19 samples per
marker), CCW speed against what the throttle predicts:

| MM33 | MM32 | MM31 | MM30 | **MM29** | MM28 | MM27 | MM26 | MM25 | MM15 |
|---|---|---|---|---|---|---|---|---|---|
| 0.95 | 0.87 | 0.77 | 0.74 | **0.72** | 0.77 | 0.76 | 0.82 | 0.87 | 0.90 |

Two facts decided the shape of this.

**It is about twice the grade of Grillers**, whose worst marker is 0.87 against
this one's 0.72. So it needs help, and 90 is not enough with a load.

**The entire run-in is uphill.** Nothing recovers to 1.00 anywhere between MM33
and the platform — it is still 0.90 at MM15. The grade is the brake, so no
back-off ramp is needed or wanted.

Patio is at MM15 and a station approach begins ten markers out, at MM25. Ending
the section at MM26 means the locomotive never accelerates in the marker before
it must begin decelerating, which is precisely what the operator asked to avoid.

## Alternatives considered

- **Run the section to MM23**, as first sketched from the raw grade profile.
  Rejected: it overlaps the Patio approach window by three markers, so the
  section would push while the approach pulls.
- **110, to match Grillers.** Rejected. This grade is twice as steep, so the same
  number does not mean the same thing, and the operator accepted a slower
  approach in exchange for a steady one. 105 holds about 89% of normal cruise
  speed through the worst of the curve.
- **113**, which would hold full cruise speed through MM29. Rejected for the same
  reason: fast is not the goal on a curve into a platform.
- **A back-off ramp like the Grillers one.** Unnecessary — the grade does it.

## Consequences

- One steady throttle across the whole curve, so nothing shifts under the train
  mid-bend.
- **Until the station machine exists**, the handover at MM25 is a paced
  105 → 90 at `GRADE_STEP_MS` — 15 counts over about four seconds, one count at
  a time. Smooth, but it is not the intended end state.
- **When the station machine lands, the Patio approach must ramp down FROM 105,
  not from base cruise 90.** Otherwise the step this decision exists to remove
  reappears at MM25. This is the single most likely way to get the station work
  wrong.
- Arches moving to MM108 changes only what telemetry calls that marker today;
  it becomes load-bearing when the station machine reads centres from here.

## When speed control arrives, this goes

Recorded at the operator's request, 2026-09-01, and repeated in `RouteMap.h` at
the code itself.

This section and 0066's are **open-loop grade compensation**. They exist only
because the locomotive cannot measure its own speed, so a human measured where
it struggles and hand-wrote a throttle for those places.

A speed-based controller measures that directly. It sees the train slow on a
grade and adds power without being told the grade is there. At that point
`cruisePwmAt()` becomes a second, uncoordinated compensator fighting the loop —
the controller raising power because the train is slow, the table raising power
because of where the train is, neither aware of the other. The symptom would be
surging on the climbs as the two take turns.

So it does not get ported forward. It gets **retired**:

- `cruisePwmAt()` and its call site in the advance handler both go.
- The section **boundaries** are worth keeping — MM65–80 CW and MM33–26 CCW are
  real features of the railway, and a speed controller may want a target *speed*
  per section. Keep the geography, drop the PWM.
- The durable part is the measured grade index, which lives in the calibration
  data and stays true regardless of control strategy.

What must **survive**: station centres, stop offsets and dwell. Those are
geometry and timetable, not compensation for the locomotive's ignorance.

## Verification

All nine gates pass; compiles clean against core 3.3.11 — 966,931 bytes flash
(73%), 59,188 globals (18%).

Gate 9 grew to **495 checks**, including: 105 across MM33–26 and base cruise at
every other CCW marker; the section is live at MM26 and finished by MM25; the
whole MM25→MM15 run-in undisturbed; and neither section leaks into the other's
direction.

Not field-tested. Not ratified.

## References

- `docs/decisions/0066-the-grillers-climb-runs-at-110-and-backs-off-one-count-at-a-time.md`
- `firmware/test-programs/NAVI_ONE/RouteMap.h` — `cruisePwmAt()`, retirement note
- `firmware/test-programs/NAVI_ONE/tests/gate_section_cruise.cpp`
