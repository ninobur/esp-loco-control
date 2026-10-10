# MM045 STOP — reported CW landings and location discrepancy

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

## Subsequent operator report — provisional accumulating offset

David later reported that Otto appeared to be off by “20 mm now” and was
“loosing 10 mile markers each lap.” The surrounding statement explicitly uses
mile markers, so the apparent offset is recorded as approximately 20 marker
indices, not a measured 20-millimetre distance. This is the operator's provisional
assessment, not a verified NAVI/physical coordinate comparison. He said he could
provide the dashboard MM for the third stop; at that point no third physical interval or numeric dashboard reading had
yet been supplied. The subsequent comparison is recorded below.

The reported lap-to-lap growth warrants comparing physical Hall-sensor location
and dashboard MM before any reposition/redeclaration. The agent recommended
pausing AUTO and did not shift the STOP target or alter the navigation algorithm
in response. The separately requested 150 ms restart correction was committed
as `6bf81a1`; it does not diagnose or resolve this reported location discrepancy.

## Third reported stop — physical location versus dashboard

David reported: “The Dashboard is correct for the maneuver, wrong for the
location. Now he is stopping at Grillers, around 062.5. thinks he is past 44.
Did not bother to restart.”

Physical location is approximately MM062.5 at Grillers; NAVI/dashboard location
is reported as past MM044, with no exact fraction supplied. This establishes a
substantial discrepancy, roughly 18 marker indices, rather than merely a
physical landing difference from the programmed target. The approximate pair
does not establish an exact per-lap loss or its cause. This follows the two CW
reports; a separate direction confirmation for this stop was not supplied.

David subsequently clarified: “He did not restart automaticaly.” This confirms
a reported automatic restart failure at the third stop, rather than an operator
decision to leave Otto stopped. It is recorded separately from the location
discrepancy; no cause or relationship between the two failures is established.
Raw telemetry and the running build identity remain unavailable. AUTO was
recommended paused.

Bounded source inspection found that the STOP overlay does not directly change
NAVI's marker count. Declaration, Hall confirmation and IR-distance-based
missed-marker advancement are the sources of marker changes. Hall and IR inputs
continue to be serviced in the main loop. These facts do not identify which
input or calculation caused the observed discrepancy. No navigation change or
stop-target compensation was made.

## Operator clarification — obstruction, IR-car disturbance and power cycle

David reported that he stopped Otto for a track obstruction and dislodged the
IR car with his foot. He subsequently replaced the IR car while the locomotive
was stopped and reset the location near MM024–025, before the first reported
STOP trial. Preserve that approximate physical location independently of the
log's declared interval 025–026; they are not silently reconciled.

David relayed Claude's observation that the preceding session went silent at
18:58:56 at MM149 while running and rebooted at 19:00:54. Asked whether this was
the obstruction stop and whether he power-cycled Otto, David confirmed:
“Yes. Around MM149. I power cycled Otto.” The obstruction location and deliberate
power cycle are therefore operator-confirmed. Claude's exact silence time is
attributed to the relayed analysis, not independently reverified here.

The earlier session break is consistent with the reported intervention and is
not treated as evidence of an unexplained spontaneous reboot. The later trials
follow the power cycle, IR-car replacement and location redeclaration. That
redeclaration supplies a new location starting point; it does not independently
validate optical acquisition or IR distance after replacement. Possible changes
to IR-car placement/alignment remain hypotheses, not a confirmed cause of drift.
