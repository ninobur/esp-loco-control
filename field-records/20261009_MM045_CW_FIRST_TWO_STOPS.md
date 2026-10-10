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

## Later CW trial — reported 20-marker loss

After reporting a CW start at040–041, landings in044–045 and a second measured
landing at044.5 with dashboard044, David later reported: “OTTO lost20 MM this
round.” Preserve this as an operator-reported20-marker location discrepancy;
no paired physical/dashboard coordinates or exact timestamp accompanied this
latest statement. Do not infer its exact lap boundary or assign it to the new
50→35 profile. That profile was committed as0fdc6c9 for a subsequent bench
reflash; the running image is not independently identified by a Git revision.
The report shows the accumulating location discrepancy remains unresolved.

### Paired location supplied for the later stop

David clarified: “stopped at 059-060. said he was at 044”. The physical
Hall-sensor stop is between MM059 and MM060, fractional position unspecified;
the reported dashboard marker is MM044. This pair indicates approximately
15–16 marker indices of discrepancy at this stop. Preserve the preceding
“lost20 MM this round” as his earlier assessment; do not silently replace it
or manufacture an exact20-marker difference from the supplied pair. The
reported dashboard marker alone supplies no fine physical offset. This is
further evidence of location disagreement, not a measurement of final braking
error relative to the target. No code or calibration change follows from it.

## Telemetry analysis of the evening runs — Claude, with David and Sam

Source: the unfiltered snapshot `logs/20261009_MM045_EVENING/all_20261009.log.gz`
(original SHA-256 `56292fad…1978`), Otto `9950011` topics only. Read-only
analysis; no firmware, threshold or navigation change follows from it. Times
are the log's own labels.

### Sessions identified in the log

| Run | Boot | Declared / GO | Ended | Hall accepted / missed |
|---|---|---|---|---|
| A | `FD2F3506…` | 055–056 / 18:56:49 | 18:58:56 at MM149: obstruction stop and power cycle (David confirmed) | 94 / 0 |
| 1 | `E71CF024…` | 025–026 / 19:03:34 | dispatcher release 19:13:51 | 142 / 218 |
| 2 | `E71CF024…` (same boot as run 1) | 040–041 / 19:31:33 | dispatcher release 19:40:25 | 384 / 0 |
| 3 | `2A0C8220…` | 040–041 / 19:49:11 | E-stop 19:54:39 | 71 / 104 |

The IR car was dislodged and reseated between runs A and 1 (log: IR pulses stop
at 19:02:10 while PWM is 70; signal returns by 19:03:00). Between runs 2 and 3,
David reports the car was moved to the table and Otto was **reflashed with edited
ramp values**. Run 3's braking telemetry (PWM falling about one count per 300 ms)
matches the constants committed in `93d5226` (300 ms final step, 150 ms AUTO
acceleration); the boot record does not carry a Git revision, so this is a match,
not an identification. Run 3 therefore ran a different binary from runs 1 and 2.
The navigation core (`NaviIntegratedCore.h`) is unchanged between `6bf81a1` and
the branch head.

### Stopping behaviour (all runs)

Every completed stop began braking on entry to the final MM, 296 mm before the
MM045 + 150 mm target, and ended 267–296 mm short of it (about MM044.6, matching
the physical 044.5 reports). With the 31 ms/count ramp Otto coasted 10–20 mm;
with the 300 ms/count ramp (run 3) it stopped moving at about PWM 26–27 and
travelled 29 mm and 0 mm. **Stop position was set by where braking started, not
by ramp rate.** Below roughly PWM 25–30 Otto did not move at that location.

Run 1's third approach stalled at applied PWM 31 in the penultimate section at
19:12:15 (IR speed 0, position frozen 308 mm before the final section) and stayed
there with AUTO running until release. The overlay's PWM there is a function of
position only, so a stall cannot change it. This is the automatic-restart failure
David reported: Otto never reached the dwell.

### IR acquisition irregularities

- **Run 1:** at 19:05:22, near MM095, IR signal span spiked to 1327 and then
  settled at about 255–303 (previously 383–431). From then to the end, every
  sample reported `InadequateContrast` (fault 6), readiness `Unavailable`, and
  aborts increased one-for-one with completed pulses. Counting continued. The
  per-pulse event stream reported almost nothing in this period, so the
  periodic-interval test below cannot be applied to it.
- **Run 3:** contrast was reported healthy and aborts were 0 throughout motion.
  In steady running, unusually long pulse intervals (more than 2× the local median,
  typically 2.5–3.2×) recurred most often every **8** counted pulses (also 9, 17,
  18).

| Run | Long intervals per steady pulse | Uncounted time, in pulse periods | Observed drift |
|---|---|---|---|
| A (before foot incident) | 0 | 0% | none |
| 2 | 1.6% | ≈2.4% | none |
| 3 | 5.6% | ≈8.6% | ≈15.5 of ≈166 markers ≈9.3% |

Run 3's uncounted fraction matches its drift. The excess time is distance the IR
count did not register, whether the cause is missing spokes, wheel binding or
slipping, or optical counting; the data do not decide between these. Eight counted
pulses cannot be assumed to be one revolution. The pattern was absent before the
foot incident. In run 2 it was small enough (≈7 mm per magnet interval) to stay
inside NAVI's ±45 mm acceptance window, so every magnet was accepted and the count
was corrected at each one. **Working hypothesis:** the IR car or its measuring
wheel has been defective since the incident and worsened after later handling.
This is not confirmed. The proposed bench check is to mark the wheel and compare
pulse counts and long intervals with its angular position over several
revolutions.

### How the IR error became location loss

The Hall sensor kept detecting magnets throughout: before run 1's fault, 69
detections and 69 acceptances; afterwards, 235 detections and 73 acceptances. NAVI
accepts a magnet only when IR distance since the last accepted magnet matches the
map within ±15% of one interval (about ±45 mm), a tolerance that does not widen
across consecutive misses. With IR reading short, real magnets arrived "early",
failed that check (run 1: 159 of 181 rejections were 5–10% short against cumulative
mapped distance) and were recorded as missed magnets. NAVI then advanced by IR
distance alone and fell behind.

Later groups of accepted magnets did not correct this. In run 3, the lag
consistent with each group's magnet polarities grew 3 → 6–8 → 9 → 9 → 11 → 14 →
15. The final 15 matches the physical 059–060 against dashboard 044 pair, so NAVI
accepted real magnets under the wrong numbers, which locked the offset in.
`position_reliable` stayed 1 throughout.

**Central finding (David, Sam and Claude):** NAVI detected each Hall/IR
disagreement but interpreted every one as a missed magnet, never as IR error, and
had no response to persistent disagreement. Optical faults and aborts were
diagnostic only and did not withdraw IR distance authority, and the periodic
pulse irregularities were not assessed as counting faults. A possible IR
measurement defect therefore exposed a separate navigation weakness: NAVI did
not recognize or reconcile the growing disagreement between its location and
the railway. Whether NAVI should keep IR distance authority while the IR
instrument reports a fault, and how persistent Hall/IR disagreement should be
handled under Decision 0116, are architectural questions for David. **No
navigation change was made.** The only firmware change during the evening was
the ramp reflash before run 3.
