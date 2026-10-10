# MM045 navigation investigation — October 9, 2026

Read-only investigation of David's three reported CW stops. No firmware,
threshold, target, safety rule or running service was changed.

## Evidence and provenance

Existing Pi file `NGR/telemetry/all_20261009.log` copied at approximately
19:19 local time. Sharing copy: `/Users/davidbrown/Downloads/OTTO_MM045_STOP_20261009.log`.
Log SHA-256: `0946fc37d8bd4b053e4c34e47f565483d3abf6c972b8ee24d255fea1449b2f79`.
Source inspected at branch `codex/navi-cruise-stop-20261009`, commit `0b2ee2b`.
Runtime sketch name is present, but the log does not identify the exact source
commit. Times below are the log's wall-clock labels, without timezone suffix.
Relevant locomotive boot ID: `E71CF0243E5B71B6` at 19:00:54.785.
Declared interval 025–026 CW at 19:03:25.967; AUTO GO at 19:03:34.797.
Physical observations remain independently preserved in the
[field record](../field-records/20261009_MM045_CW_FIRST_TWO_STOPS.md).

## What the log establishes

| Event | Log evidence |
|---|---|
| First final braking | 19:04:12.292, NAVI MM044, applied PWM31, BRAKING |
| First zero and dwell | 19:04:13.118, applied PWM0, DWELL |
| First release | 19:04:18.126, RELEASED, commanded PWM90; five seconds after dwell |
| First recorded missed marker in this enrollment | 19:05:23.160, NAVI MM096 |
| Second final braking | 19:08:10.113, NAVI MM044, BRAKING |
| Second dwell/release | 19:08:10.970 DWELL; 19:08:15.963 RELEASED |
| Third maneuver | PENULTIMATE, not BRAKING/DWELL; aim/applied PWM31 before dispatcher stop |
| Third stalled coordinate | 13,830,628 µm against target 14,139,000 µm: 308.372 mm short in NAVI's coordinate |
| Operator stop command | 19:13:51.495, after the stalled maneuver |

The third automatic restart did not occur because the logged routine never
entered final braking, zero-PWM dwell or release. NAVI placed Otto 8.372 mm
short of the final section boundary, while PWM31 produced no further measured
progress. This explains the missing state transition; it does not establish
whether the lack of progress arose solely from physical stall or also from
unobserved movement. David independently reported physical rest near MM062.5.
The operator stop command came later and must not be blamed for the preceding
failure to complete the maneuver.

Before the first dwell there were 18 Hall confirmations and no missed-marker
events. Between first and second dwells there were 71 confirmations and 100
missed-marker advances, totaling 171. Between second dwell and third stall
there were 53 confirmations and 118 missed-marker advances, again totaling171.
These are NAVI circuits, not independent measurements of physical circuits.

IR telemetry at the three stops reports completed pulses 4,315 / 9,699 / 14,995
and cumulative abort counts 55 / 3,743 / 9,033. Sample-gap count stays1;
saturation count is0 in these snapshots. Aborts therefore increase by3,688
and5,290. Abort counts are diagnostic evidence, not a count of lost spokes:
no distance correction can be calculated by multiplying them by pulse pitch.

Despite the physical discrepancy, status at 19:12:20.271 reports NORMAL,
MM044, position_reliable=1 and HALL_IR_READY. Fresh, mathematically consistent
cumulative packets do not establish physical distance accuracy.

## Source trace and limits

`NaviIntegratedCore.h::observeHall` accepts target-polarity Hall support within
its mapped distance gate. `confirm` then resets the physical origin to that
Hall event's IR coordinate. `evaluateMissing` advances mapped markers using
cumulative IR distance when a Hall target was not confirmed. `observeIr`
explicitly retains distance applicability across diagnostic counter changes.
The STOP overlay reads this coordinate; it does not directly advance/reset MM.

Consequently, IR undercounting and incorrect Hall-to-target association remain
candidate explanations. The sharp increase in aborts and lost Hall agreement
makes IR acquisition/distance the first diagnostic focus, but the summary log
cannot prove missing pulses or identify physical magnets behind each event.
A numeric fit to David's approximate ten-marker-per-lap observation would not
establish causation and is not used to recalibrate the system.

The Pi's `NGR/navi_sync` directory was inspected read-only. Its newest recording
is dated September28; there is no October9 native recording there. Existing
one-second status/event records lack the raw Hall waveform and raw IR optical
samples needed to distinguish the candidate causes.

## Recommended next investigation

Preserve this log and compare installed IR build identity and existing optical
recording facilities before another bounded track capture. Capture native Hall
and accepted IR evidence plus raw optical data where existing equipment permits,
with an independently identified physical marker before/after the problem area.
Do not change pitch, target location, distance gates or Hall thresholds from
this summary evidence. Any code modification requires a concrete proposal and
David's approval under AGENTS.md; recording/deployment changes also require
separate authorization. No proposed fix is represented as already approved.
