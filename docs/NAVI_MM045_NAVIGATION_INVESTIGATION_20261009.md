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

## Subsequent operator context

David confirmed an obstruction near MM149 and deliberate power cycle, resolving
the earlier session break as an operator intervention. He also reported an
accidental IR-car disturbance, replacement while stopped and location reset
near MM024–025 before the first STOP trial. See the field record for the full
account and its distinction from the logged declaration 025–026. These facts
make the IR-car setup after replacement relevant to investigation; they do not
prove that the disturbance caused subsequent distance or Hall-matching errors.

## Independent second-run analysis — continuation

Source: complete snapshot committed asbf95cc4; log interval19:48:05–19:54:40,
locomotive boot2A0C8220A9F02278. Declaration event at19:49:01.127 is MM040 CW.
Event totals are71 TARGET_CONFIRMED and104 MISSED_MAGNET, i.e.175 advances from
040 to044 over one NAVI circuit plus four markers. This is not an independently
measured physical lap. The first23 confirmations assign041 through063.
First missed event:19:50:25.292, assignedMM064.

At19:50:24.372, target064 Hall support is present. Cumulative IR travel since
saved063 anchor is250.952 mm, compared with mapped325 mm; shortfall74.048 mm.
The normal gate tolerance is48.75 mm. A subsequent support event at19:50:24.559
remains64.396 mm short. The field is gone by the later missed-marker event.
Subsequent support events against targets065,066,067,068 are respectively
128.444,153.536,202.584,227.676 mm short of the current mapped expectations.
Those target identities are NAVI assignments, not independently identified
physical magnets. Abort counter remains9265 throughout this onset, optical
reason6 (TRACKING), with no new gap/saturation. Contrast failure is not needed
for this observed onset. Counter growth in the earlier run is not its diagnosis.

The mismatch predates the first rejection. Saved anchors assigned045 and063
are277,051,008 and281,877,008 µm:4,826 mm travel for5,465 mapped mm,639 mm short.
Saved anchors assigned044 before and after the circuit are276,761,448 and
327,540,620 µm:50,779.172 mm travel against52,264 mapped mm. These calculations
use NAVI-assigned identities and must not be called independent wheel accuracy
measurements or physical circumference measurements.

### Confirmation gate versus saved anchor

The source tests latest IR travel against the distance window, but saves the
earlier matching Hall point as the origin. The log independently shows18
confirmations after the initial one whose saved anchor lies outside that
window relative to the preceding saved anchor and intervening mapped spans.
Examples:

- MM056: saved-anchor travel231.648 mm versus300 mapped;68.352 mm short,
  beyond45 mm tolerance. Latest IR is28.956 mm beyond the saved anchor, bringing
  the tested travel inside the window.
- MM063: saved-anchor travel250.952 mm versus315 mapped;64.048 mm short,
  beyond47.25 mm tolerance. Latest IR is19.304 mm beyond the saved anchor.

Total signed saved-anchor/map residual over70 successive confirmations is
−1,487.188 mm, including−651.804 mm through063. This describes repeated
coordinate anchoring, not a count of physical markers lost. It cannot alone
explain the independently reported15–16-marker discrepancy. No correction to
pitch or position is inferred from it. The gate/anchor inconsistency is concrete;
whether changing it is architecturally appropriate remains David's decision.

Later confirmed streaks do not prove recovery of physical identity: source
logic checks only current target polarity and distance; it does not identify
a unique multi-marker physical sequence. Target-only HALL_SUPPORT logs omit
opposite-polarity observations and the full waveform. Therefore this snapshot
cannot prove which later confirmations were mislabeled. Similarly, nearest
one-second speeds (approximately150/182/225 mm/s in sample problematic events)
are themselves IR-derived; no independent speed is available to establish a
speed-dependent distance bias. Do not present such a correlation as established.

Next bounded evidence needed: retain native raw Hall and accepted cumulative IR
records during an independently identified physical marker interval, with the
installed IR build identified. Inspect existing raw optical capture at the same
time if available. This distinguishes distance scale/count problems from marker
assignment and exposes both judgment-time and saved-anchor travel. No firmware,
recording service or hardware configuration was changed in this investigation.

## Completed pulse timestamp check

Used EVENT records in `telem/ir_pulse` from the fullbf95cc4 snapshot, deduplicated
by sequence within each same-boot window and ordered by TX `completed_us`.
Log wall-clock timestamps are delivery labels, not precise physical event times.
No interpolation of unrecorded events was used. Type6 is observational; it
reports the completed-pulse measurement counter also underlying Type5 distance.

### First run, first error onset:19:05:15–19:05:30

275 retained EVENT records, completed counts5691–6095.130 intermediate events
are absent from this log. Diagnostic log_drop grows819→930, while seq_breaks,
pulse_breaks,time_breaks,interval_breaks andrx_drop remainzero. Log omissions
must not be called missing measured distance: cumulative counts retain them.
Among usable recorded intervals, median38 ms. Intervals593 ms at19:05:22.069
(count5925) and549.001 ms at19:05:22.440(count5926) appear near onset.
The latter has consecutive completed counts and exact matching TX timestamp
spacing: it is an actual long counted-measurement interval, not a missing
intermediate log entry. It is followed by increasing optical discontinuities;
this window alone cannot separate wheel slowdown from uncounted optical cycles.

### First run, following error circuit:19:08:17–19:08:25

116 events cover every completed count9700–9815, no missing intermediate events.
All interval fields arezero and event_valid false, with optical reason1.
Source completion timestamps remain present; their largest consecutive spacing
is338.094 ms during departure. Other large spacings include130.907 and130 ms.
Acceleration after dwell prevents interpreting these alone as missed spokes.
No sequence/pulse/time/interval breaks or RX queue drops are recorded. Existing
optical discontinuities increase3701→3816. The reported scalar interval is
unavailable, not a measuredzero-duration pulse. Timing cannot cleanly diagnose
this second onset independently of departure and ongoing acquisition problems.

### Later drifting run:19:50:15–19:50:35

333 retained events, counts29062–29434;40 intermediate events absent from log.
All333 are event-valid. Median reported interval50 ms,max185 ms. No recorded
sequence/pulse/time/interval breaks or RX queue drops. Discontinuities remain61.
Near the first missed marker, these pairs are consecutive in counted distance:

| Log timestamp | Completed counts | TX timestamp difference / reported interval |
|---|---|---|
|19:50:23.261|29214→29215|158.999 ms|
|19:50:23.837|29222→29223|180 ms|
|19:50:24.411|29230→29231|162 ms|
|19:50:24.965|29238→29239|171 ms|
|19:50:25.553|29246→29247|173 ms|
|19:50:26.095|29254→29255|159 ms|
|19:50:26.739|29262→29263|185 ms|

Nearby ordinary intervals are approximately55–60 ms. Each long interval ends
only one counted completion after its predecessor, and repeats eight completed
counts after the preceding long interval. These are genuine irregularities in
the counted-measurement timestamps despite healthy contrast. They are not
explained by MQTT/event-log delivery omissions. No assumption about spokes per
wheel revolution is needed to establish this eight-count recurrence.

The evidence favors a periodic measurement/wheel issue as a possible trigger,
rather than transport loss alone. It does not distinguish a wheel slowing,
slipping or binding from optical cycles failing to count. Independent physical
motion or raw optical edges are required to prove skipped spokes specifically.
Healthy contrast does not prove complete pulse counting. This corrects the
prior incomplete review that had not examined pulse completion timestamps.
The separate navigation disagreement/identity problem remains; these pulse
irregularities do not resolve its design implications. No firmware changed.
