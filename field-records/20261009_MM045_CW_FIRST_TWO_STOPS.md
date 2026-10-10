# MM045 STOP — first two reported CW landings

**October 9, 2026. Source: David's direct field reports in this conversation.**
Preserve the operator observations independently of the programmed location and
any explanation, following the NGR decision-system principles. No telemetry or
raw log was supplied with these reports. The reported running build/device has
not been independently verified; the local candidate before correction was
`557de9ab181459693791de6d7f498ef4826d82c8`.

## Operator observations

| Trial | Direction | Physical Hall-sensor landing | Other reported behavior |
|---|---|---|---|
| First stop | CW, explicitly confirmed | Midway between MM044 and MM045 | Deceleration smooth; automatic restart about three seconds after actual physical stop |
| Second CW round | CW | Between MM054 and MM055; fractional position not specified | No additional timing or deceleration observation supplied |

David's first description was “Hall sensor stopped midway between 044 and 045.”
His second was “Second CW round, stopped between 054 and 055.” These are physical
observations, not substituted NAVI readings. NAVI/dashboard MM, initial declared
MM, entry within/outside the sequence, boot identity and event timestamps remain
unavailable. A “yes” reply to the question asking for the dashboard MM did not
provide a numeric value and is not recorded as one.

## Programmed expectation and limits

There is one programmed target in both directions and on every circuit:
**150 mm CW from MM045, midway to MM046**. The first reported CW landing is
approximately one MM interval before that target. The second is in the interval
approximately nine MM intervals beyond it; its exact stop coordinate is unknown.
There is no programmed second stop at MM054/MM055. The observations do not yet
establish a location offset, a braking calibration error or a repeat defect.

The configured five-second dwell begins at applied PWM zero. The operator's
roughly three-second observation is measured from physical rest. Those are
different timing references; coasting and the delay before rising PWM causes
movement can affect their relationship. No exact coasting duration is inferred.
Telemetry is needed to compare PWM-zero, rest, release and actual departure.

## Requested correction

David said “I asked for 150” after the agent reported the existing 62 ms/count
restart. The STOP restart is corrected to **150 ms per PWM count**, retaining
ordinary cruise 90 and grades, stopping geometry, decrement and applied-zero
dwell. See the [implementation record](../docs/NAVI_MM045_STOP_IMPLEMENTATION_20261009.md#first-cw-trials-and-restart-rate-correction--october-9).
No landing-position adjustment or navigation-algorithm change follows from
these observations. This is not field acceptance.
