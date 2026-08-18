# QUORUM IR speed and Hall-agreement integration specification

**Status:** Draft 1 — proposed for review, not implementation authority  
**Date:** 2026-08-15  
**Baseline:** the field-accepted descendant of QUORUM 1.16R  
**Initial locomotive:** Toby, 9950012  
**Initial operating condition:** one locomotive on the railway, supervised, with the operator holding E-STOP authority

## 0. Purpose and sequencing

This work is separate from QUORUM 1.16R. QUORUM 1.16R must first be field-tested
with Toby and Otto in the CTO Bubble. IR integration must not be added to that
test version or used to reinterpret its result.

After that test, this specification adds a continuous physical speed source and
an independent distance witness to Toby. The first sensor is the already-proven
IR wheel installation on the sensor car coupled into Toby's consist. A dedicated
ESP32 samples the wheel and sends compact snapshots directly to Toby by
ESP-NOW. The sensor ESP has no MQTT telemetry role. Toby consumes the snapshots
locally and publishes the operational record.

The work advances through distinct authority stages:

1. **Transport verification:** Toby receives and republishes IR facts, but IR
   changes neither navigation nor motor output.
2. **Agreement observation:** Toby continuously compares IR distance and speed
   with Hall/mile-marker evidence, but the comparison remains advisory.
3. **Solo speed authority:** after the first two stages pass their evidence
   gates, fresh valid IR speed governs Toby's speed throughout supervised solo
   AUTO operation. Loss of IR returns control to existing PWM presets.
4. **Navigation assistance:** Hall/IR agreement may affect marker disposition
   only after logged shadow results meet the navigation gate in this document
   and the operator explicitly authorizes that change in a later revision.

Stage 3 is an intended deliverable, not an indefinite experiment. Stage 4 is
deliberately evidence-gated because IR is being added to cure known uncertainty,
not to introduce a second untested navigator.

### 0.1 Terminology

- `MMnnn` means a numbered physical mile marker on the route.
- `mm` in a distance or speed unit means millimetres.
- `marker speed` means route distance from `spacingMm[]` divided by the time
  between accepted physical Hall anchors.
- `IR speed` means wheel distance divided by the locally measured pulse
  interval.
- IR is a distance odometer, not an absolute-position sensor. It can support
  how far the consist travelled; it cannot identify a route location by itself.

## 1. Governing principles

### 1.1 Evidence and authority

- PWM is actuator effort. It is neither speed nor proof of motion.
- A `MOVING` state inferred from PWM is not proof of motion.
- Valid IR wheel motion is the continuous speed measurement.
- Accepted physical mile-marker progression is the sparse position and
  route-distance measurement.
- Hall polarity remains marker-identity evidence.
- IR distance and Hall evidence must be compared; neither source silently
  rewrites the other.
- A fresh explicit IR `STOPPED` state is numeric zero. Missing radio or
  unusable moving measurement remains unknown/null and is never silently
  converted to zero.

### 1.2 Bicameral control remains constitutional

In MANUAL, the operator is sovereign. IR and navigation remain informative and
continue publishing, but neither may write the motor. In AUTO, the speed
controller may act through the existing motor-authority path. E-STOP and NEUTRAL
remain effective in every state and outrank IR, navigation, station, and CTO
requests.

### 1.3 Reliability before degraded machinery

No new stop, lockout, or loss-of-AUTO response is created merely because IR is
missing, stale, invalid, delayed, or contradictory. Those cases are recorded
and the locomotive falls back to the PWM preset already selected for its
current operational intent. A stronger protective response requires a concrete
field failure and a separate ruling.

## 2. Scope

### 2.1 In scope

- An ESP-NOW IR-speed sender based on the proven 10-spoke sensor-car setup.
- A source-tagged IR receiver in Toby's QUORUM firmware.
- Continuous Toby-originated IR and marker-speed telemetry.
- A Hall/IR agreement observer based on physical distance along `spacingMm[]`.
- A measured-speed AUTO governor using valid IR speed.
- Clean, bounded fallback from IR speed control to the existing PWM presets.
- Replay and supervised field gates for each increase in authority.

### 2.2 Out of scope for the first implementation

- Any change to QUORUM 1.16R before its two-locomotive Bubble test.
- Direct MQTT, Wi-Fi telemetry, or broker credentials on the sensor ESP.
- IR equipment on Otto.
- Two-locomotive testing while IR has motor authority.
- CE, unpaired CTO mode, or changes to Bubble choreography.
- IR-based station position, stopping-distance control, train separation, or
  consist-length calculation.
- Changing the frozen `CtoPeerPacket` version or layout.
- Sending IR samples through MQTT and reading them back for control.
- Treating packet arrival count as wheel-pulse count.
- Treating missing IR as zero speed.

Once IR speed is field-accepted, populating the already-reserved
`CtoPeerPacket.speedValid` and `speedX10` fields may be specified separately.
It is not part of the first solo campaign.

## 3. Existing decisions and required new record

This specification preserves:

- decision 0014: speed is the controlled variable and PWM is the actuator;
- decision 0015: speed enters through a source-tagged, freshness-bearing seam;
- decision 0022: the target wheel is the measured 96.52 mm circumference,
  10-spoke plastic wheel, hence 9.652 mm per completed pulse;
- decision 0005 and the IR quality gate: invalid or stale speed is null, not
  zero, as narrowed by decision 0036 for an explicitly stationary wheel;
- decision 0036: a stationary IR wheel reports numeric zero, but zero never
  grants upward motor authority;
- decision 0020: compensation requires observed evidence.

It supersedes one part of decision 0021. Speed is still computed beside the IR
sensor and MQTT remains outside acquisition and control, but the sampler and
governor are now on separate ESP32s joined by local ESP-NOW. If this draft is
accepted, that architectural change must receive a new numbered decision record
rather than silently editing decision 0021.

## 4. Architecture and contracts

```text
10-spoke wheel
    |
    v
IR sensor ESP  -- ESP-NOW snapshots -->  Toby QUORUM
 acquisition       cumulative facts       |  IR speed governor (AUTO only)
 quality state                              |  Hall/IR agreement observer
 local speed                                |  MQTT summaries and evidence
 no MQTT                                    v
                                          existing PWM authority path
```

There are four contracts:

1. **Sensor contract:** convert optical pulses into a locally qualified speed
   snapshot and a monotonic physical pulse count.
2. **Radio contract:** transport the latest cumulative snapshot; packet loss
   must not become distance loss.
3. **Agreement contract:** compare IR distance with route distance and Hall
   identity without inventing evidence.
4. **Control contract:** in AUTO, use fresh valid IR as feedback; otherwise use
   the existing PWM preset for the same motion intent.

## 5. Sensor ESP

### 5.1 Acquisition and quality

The sender shall reuse the proven `IR_SPEED_LOCAL` acquisition and quality
model rather than recreate it from memory:

- nominal 1 kHz physical sampling;
- rolling optical envelope and hysteretic edge detection;
- five completed intervals in the speed median;
- the existing `VALID`, `REACQUIRING`, `MARGINAL`, `INVALID_CONTRAST`, and
  `STALE` meanings;
- 96.52 mm wheel circumference and 10 spokes;
- measured moving speed consumable only in `VALID`;
- the sender's wire layer derives `STOPPED` after 2.5 seconds without cumulative
  pulse progress and carries numeric zero;
- marginal, reacquiring, invalid and unavailable states carry no numeric speed.

No received radio state may change edge detection, pulse count, envelope, or
speed calculation. ESP-NOW failure therefore cannot make the sensor forget
physical wheel motion.

### 5.2 Cumulative evidence is mandatory

Every snapshot shall carry a monotonic `completedPulses` count. Toby derives
distance from pulse-count differences, not from the number of packets it
received and not by numerically integrating a lossy stream of speed reports.

This is essential. If three radio packets are lost and the fourth reports a
four-pulse increase, Toby still learns the entire 38.608 mm travelled. A packet
containing speed alone cannot provide that guarantee.

The sender also carries a boot/session identifier. A changed identifier tells
Toby that the cumulative count restarted; the receiver must not calculate a
cross-reboot pulse delta.

### 5.3 Radio role

The sender shall:

- use ESP-NOW unicast to Toby's configured MAC address;
- include Toby's locomotive ID as the intended recipient;
- run on the same Wi-Fi channel as Toby;
- have no broker address, MQTT client, MQTT queue, or MQTT credentials;
- retain only bounded local debug output behind a commissioning build switch.

For the initial campaign, the radio channel may be a documented fixed
configuration. A dynamic channel-discovery mechanism is not required unless
field evidence shows that the fixed configuration is operationally inadequate.

## 6. ESP-NOW wire contract

### 6.1 New packet type

IR shall use a new packed packet with its own magic and version. It shall not
reuse or extend the frozen CTO packet structures. The implementation shall use
fixed-width integer fields and an exact-length check. A representative logical
schema is:

```text
magic, version
sensor_id, target_loco_id
boot_id, tx_sequence
captured_ms
completed_pulses
last_interval_ms
speed_mmps_x100       // measured value, 0 for STOPPED, sentinel otherwise
speed_valid
validity_state
optical_span
sample_missed
open_aborts
```

The implementation report shall give the final byte layout, `sizeof` result,
units, endianness assumption, and maximum numeric ranges. It shall not put a
floating-point value on the wire.

The wire validity enum includes `STOPPED` in addition to the five proven local
acquisition states. `STOPPED` canonically carries numeric zero. Local
`IR_SPEED_LOCAL` acquisition remains unchanged; packet production derives the
state from 2.5 seconds without completed-pulse progress.

`speed_valid == 1` means that the packet carries a current numeric speed. It is
set for both `VALID` and `STOPPED`; `validity_state` distinguishes measured
motion from the explicit stationary value.

`completed_pulses` and health counters are source counters. `tx_sequence` is a
radio-report counter. They must never be conflated.

### 6.2 Receiver validation and threading

The ESP-NOW receive callback shall do only bounded validation, copy, and
enqueue. It shall not call MQTT, Serial, navigation, station logic, the speed
governor, or a motor function.

The packet router must preserve the existing CTO and role-echo packet paths.
For an IR packet it shall verify:

- exact packet length;
- magic and version;
- configured sensor ID and source MAC;
- `target_loco_id == LOCO_ID`;
- a recognized validity enumeration.

Loop-owned code shall consume the queue, reject duplicate/out-of-order
sequences within a boot ID, handle a changed boot ID explicitly, and publish
literal counters for bad length, bad identity, bad version, queue drops,
sequence gaps, duplicate/out-of-order packets, and sensor reboot.

### 6.3 Freshness

Transport cadence and the `IR_RX_STALE_MS` threshold shall be fixed only after
the telemetry-only run measures actual inter-arrival gaps. The chosen values
must support continuous control without making a short ordinary radio gap look
like stopped motion.

Freshness is receiver age since the last accepted packet. `captured_ms` is a
sensor-clock fact and must not be directly subtracted from Toby's `millis()`.
If clock alignment is later required for Hall-time interpolation, it must be an
explicitly tested estimator, not an assumption that the two boot clocks match.

## 7. Toby's local speed-source seam

Toby shall maintain one loop-owned source-tagged snapshot:

```text
source: IR_VALID | PWM_PRESET
speed_valid
speed_mmps
speed_pkph
source_age_ms
sensor_boot_id
sensor_pulse_count
```

The source decision is mechanical:

| Condition | Control source | Meaning |
|---|---|---|
| fresh packet and sensor state `VALID` | `IR_VALID` | numeric moving IR speed is consumable |
| fresh packet and wire state `STOPPED`, active intent is zero | `IR_STOPPED` | numeric zero confirms the requested stop |
| fresh packet and wire state `STOPPED`, active intent requests motion | `PWM_PRESET` | publish sensor zero and `MOTION_UNCONFIRMED`; zero has no upward authority |
| missing link, link-stale, marginal, reacquiring, invalid, rebooted, or rejected packet | `PWM_PRESET` | IR speed is null; use the existing preset |

There is no rule that turns missing radio into zero. `STOPPED` is an explicit
fresh sensor state. Because pulse silence can also be caused by blindness, a
positive motion intent never increases PWM from that zero; it uses the preset.

## 8. Continuous telemetry published by Toby

The sensor ESP publishes nothing to MQTT. Toby is the sole telemetry producer
for this integration.

### 8.1 IR speed

Toby shall publish `ngr/loco/<id>/telem/ir_speed` once per second, whether IR is
valid or not. The message is non-retained and carries schema
`quorum-ir-speed/1`.

Required fields are report sequence/time, sensor identity and boot ID, source
pulse count, accepted radio sequence, packet age, validity state, optical span,
and both `speed_mmps` and `pkph`. The two speeds are measured numbers in
`VALID`, exactly zero in `STOPPED`, and JSON `null` otherwise.

### 8.2 Mile-marker speed

Toby shall restore/use `ngr/loco/<id>/mm/speed` for marker-derived physical
speed. A normal production record is created when a Hall event becomes an
accepted physical marker anchor. During the observation-only Test A build, the
topic may carry one record for every received Hall event so IR distance is not
hidden when Q1.16R holds or rejects an event; such a record must carry
`nav_accepted`, use null marker speed when unaccepted, and publish a revision if
a held event is later committed or discarded. Speed for an accepted anchor is:

```text
accepted route distance in millimetres / accepted event-time interval
```

The route distance comes from `spacingMm[]` in the actual direction, including
MM170/MM000 wrap. A merely detected, pending, quarantined, or discarded event
does not create an accepted marker-speed sample. If a pending event is later
vouched and committed, its original detector timestamp is used.

The marker payload carries schema `quorum-mm-speed/1`, from/to MM, direction,
route distance, event-time interval, speed in mm/s and pKPH, and the navigation
disposition that made the anchor acceptable.

### 8.3 Ongoing combined view

Because marker speed naturally changes only at markers, Toby shall also publish
`ngr/loco/<id>/telem/speed` once per second. It repeats the latest IR and latest
marker-derived values with their separate ages and validity flags, plus:

- current control source;
- requested target speed, if AUTO speed control is active;
- fallback PWM preset;
- commanded and actual PWM as actuator facts, not motion facts;
- current Hall/IR agreement state.

This topic makes both speed values continuously visible without fabricating new
marker samples. No consumer may interpret an aging marker value as a fresh
measurement.

### 8.4 Transport discipline

All three streams use the existing bounded publication queues. They must not
publish from the ESP-NOW callback or Hall task, and must not displace
event-bearing marker/navigation records. One-Hz summaries are coalescible;
physical marker evidence is not.

## 9. Hall/IR agreement model

### 9.1 What is compared

At every accepted or held Hall detector event, Toby compares:

- **IR distance:** pulse-count change since the last accepted physical Hall
  anchor, multiplied by 9.652 mm/pulse, adjusted to the Hall event time within
  a measured radio/quantization error budget;
- **route hypotheses:** the sum of `spacingMm[]` for zero, one, two, or more
  physical marker intervals in `navDir`;
- **Hall identity:** observed polarity against `NGR_DNA1` at each candidate
  destination.

The locomotive's direction supplies the sign; the wheel sensor supplies
distance magnitude. On a direction change, sensor reboot, or pulse-count
discontinuity, the distance anchor is invalidated and must be re-established.

A fixed sensor-car-to-locomotive offset cancels between successive anchors.
Coupler slack does not necessarily cancel during acceleration or braking and
must therefore be measured as part of the agreement error budget.

Toby shall retain a short bounded history of accepted IR snapshots, each with a
Toby-side receive timestamp. At a Hall event's `detectedAtMs`, the observer may
interpolate between bracketing cumulative-count snapshots, or project a nearby
snapshot using fresh valid IR speed. The method and its maximum permitted age
must be published with the result. If the event cannot be placed within the
measured timing-error budget, the result is `IR_UNAVAILABLE`; the observer does
not force a fit.

The unpowered sensor-car wheel avoids traction-wheel slip, but wheel lift,
skidding, coupler motion, and optical error remain possible. IR is therefore an
independent witness, not an infallible witness.

### 9.2 Distance hypotheses

For a Hall event, the observer evaluates route distance for `k` physical marker
intervals from the last accepted anchor:

- `k = 0`: too little travel for another physical marker; supports a duplicate
  or phantom hypothesis;
- `k = 1`: ordinary next marker;
- `k > 1`: supports one or more missed physical Hall events;
- no hypothesis within tolerance: IR/Hall disagreement or unavailable timing.

The tested hypothesis range and tolerance shall be named constants. The
tolerance must be derived from wheel quantization, measured ESP-NOW report age
and jitter, interpolation error, route-spacing accuracy, and observed coupler
slack. It must not be selected merely to make a replay pass.

### 9.3 Required agreement result

Every comparison produces one of:

- `IR_UNAVAILABLE`
- `AGREE_NEXT`
- `SUPPORTS_DUPLICATE`
- `SUPPORTS_SKIPPED_<n>`
- `DISTANCE_FITS_IDENTITY_DISAGREES`
- `NO_DISTANCE_FIT`
- `AMBIGUOUS_DISTANCE_FIT`

The evidence record includes Hall event ID/time/polarity/peak/duration/baseline
drift, pulse delta, IR distance, each considered route span, selected `k`,
residual and tolerance, expected polarity, source age, and final Hall
disposition.

### 9.4 Anchor discipline

The IR distance anchor moves only when a Hall event is accepted as a physical
anchor. It does not move merely because the detector produced an event. A held
event therefore remains reversible: if its successor vouches for it, the
original event can be inserted with its original timestamp and the IR distance
can be split around it. If it is discarded, accumulated IR distance continues
from the preceding accepted anchor.

### 9.5 Authority stages

During stages 1 and 2, agreement is telemetry only. QUORUM 1.16R's quarantine,
scoring, suffix rescue, and self-resolution outcomes remain byte-for-byte
independent of IR.

Navigation authority requires a later explicit enable and operator approval.
Before that enable, replay and field evidence must demonstrate all of the
following:

1. real `k=1` markers are not rejected by the distance test;
2. known phantom insertions are classified `SUPPORTS_DUPLICATE` before they can
   poison the evidence ring;
3. deliberately removed input events are classified as skipped-marker
   hypotheses without inventing polarity;
4. invalid or stale IR produces `IR_UNAVAILABLE` and the unmodified 1.16R result;
5. direction reversal, wrap, sensor reboot, packet gaps, and vouched pending
   events have pinned tests;
6. every changed navigational outcome in the replay corpus is enumerated and
   explained.

Passing these gates authorizes a separate revision to say exactly which
agreement outcomes may commit, discard, or reposition. This draft does not
silently grant that authority.

## 10. AUTO speed control

### 10.1 Motion intent

AUTO shall express one motion intent containing both:

- a physical target speed; and
- the existing PWM preset that applies if measured speed is unavailable.

Cruise, station approach, final approach, departure, traffic limitation, and
zero requests remain the owners of intent. The IR governor is not a second
mission or traffic layer; it only chooses actuator effort to meet the active
intent.

Physical target speeds shall be commissioned from Gate 2 observations of the
existing presets and then selected by the operator. They must not be guessed
from PWM or copied from a different locomotive/consist. The implementation may
store per-profile cruise, station-zone, final-approach and departure targets,
while the fallback member of each intent remains today's proven PWM value.

All motor changes must still pass through QUORUM's existing PWM request/ramp
authority and CTO limiting choke point. The IR service must not become a direct
writer to `commandedPwm`, `actualPwm`, or the hardware PWM channel.

### 10.2 Governor behavior

When AUTO is active and IR is fresh and valid, the governor controls speed in
general—not only at crawl—using IR as feedback. The earlier measured-speed
governor in `archive/NGR_LL_DNA_CTO2_r12_CONTINUITY_FIRST.ino` is the starting
control model, but only its bounded correction behavior is inherited. Its
Hall-sparsity assumptions and its no-magnet launch stop are not.

The port shall preserve these useful characteristics:

- a small hold band around target;
- bounded one-step corrections;
- correction pacing that cannot fight the existing PWM ramp;
- a configured maximum PWM;
- no integral wind-up or maximum-throttle response to absent measurement.

The exact gain, hold band, service period, and acceleration limit must be tuned
from supervised IR data. They shall not be copied blindly from CTO2, whose
feedback arrived only once per marker and whose field behavior included
overspeed and stall.

### 10.3 Fallback and return

If IR becomes non-valid, link-stale, missing, rejected, or rebooted:

1. the IR numeric speed becomes null immediately;
2. the governor stops making measured-speed corrections;
3. AUTO requests the current intent's existing PWM preset through the normal
   ramp path;
4. AUTO does not stop, disenroll, enter NO_QUORUM, or raise PWM because of the
   missing measurement.

When valid IR returns, the governor resumes from the current actual/commanded
PWM without a step. Any controller memory is reset or seeded so an old error
cannot be applied after the gap.

A zero-speed intent remains zero regardless of IR. CTO limits, station stop,
E-STOP, NEUTRAL, and manual authority may always reduce or remove motor
authority; the governor may never override them.

A fresh `STOPPED` report is treated asymmetrically. With a zero-speed intent it
confirms zero. With a positive-speed intent it publishes zero plus
`MOTION_UNCONFIRMED`, makes no upward correction, and uses the current intent's
PWM preset. This is the control consequence of decision 0036.

### 10.4 Manual behavior

In MANUAL, IR and marker speed continue to publish and agreement continues to
run, but the governor performs no motor request. Manual throttle, direction,
brake, and E-STOP behavior are unchanged.

## 11. Verification campaign

Only one substantive behavior change is introduced between stages. Each stage
produces a durable Pi capture and an implementation report tied to the exact
firmware identity.

### Gate 0 — QUORUM baseline

- Field-test QUORUM 1.16R with Toby and Otto in CTO Bubble operation.
- Record the accepted commit/tag and results.
- Branch IR development only from that accepted baseline.

### Gate 1 — sender and transport, no authority

Configuration: sensor car coupled to Toby; Toby alone; motor may be manual or
AUTO presets, but IR cannot affect it.

Pass requires:

- Toby continuously publishes the sensor's boot ID, cumulative pulse count,
  validity, speed, span, packet age, and receiver counters;
- lost ESP-NOW reports do not lose cumulative distance once a later packet
  arrives;
- duplicate, out-of-order, wrong-source, wrong-target, wrong-version, and
  reboot cases are visibly and correctly handled;
- a stationary wheel publishes fresh `STOPPED` with numeric zero; marginal,
  reacquiring, invalid, or link-stale conditions publish null;
- no MQTT or Wi-Fi telemetry originates from the sensor ESP;
- no CTO packet or role-echo regression occurs.

### Gate 2 — speed and agreement observation

Configuration: Toby alone, full supervised laps including shade, sun, curves,
grades, station starts, and direction reversal.

Pass requires:

- IR speed agrees with clean marker-derived speed within the existing 10%
  calibration expectation, with outliers enumerated rather than averaged away;
- the marker-speed calculation is correct in CW and CCW and across MM170/MM000;
- the measured ESP-NOW gap/jitter distribution supports named cadence and
  freshness constants;
- the Hall/IR observer classifies every event and publishes enough evidence to
  reproduce its choice offline;
- ordinary events, Q1.16R quarantines, known phantom sites, and any known
  baseline-unreliable intervals are separately enumerated;
- IR remains advisory and replay confirms unchanged navigation and PWM output.

### Gate 3 — supervised solo speed authority

Configuration: Toby is the only locomotive on the railway; the sensor car is
in Toby's consist; AUTO is active; the operator remains within immediate
E-STOP reach.

Tests include level running, Patio Corner, grades, starts, station approach and
departure, load changes, and a deliberate temporary loss of IR radio/signal.

Pass requires:

- target speed is held without the CTO2 pattern of repeated overspeed and stall;
- PWM varies as effort while IR speed remains the controlled variable;
- deliberate IR radio loss changes the source to `PWM_PRESET` without a stop,
  surge, AUTO dropout, or false zero-speed report;
- a deliberate physical wheel stop publishes `STOPPED`/zero but cannot produce
  an upward PWM correction;
- valid IR return is bumpless;
- station zero requests, E-STOP, NEUTRAL, and MANUAL all retain priority;
- logs identify every source transition and every governor correction.

Failure of a gate returns the work to the preceding authority stage. It does
not justify adding a stop or lockout unless the log demonstrates the specific
hazard that such machinery would prevent.

### Gate 4 — navigation authority review

Gate 4 is a review and spec-revision gate, not part of the first speed-control
field run. It requires the six items in §9.5, a complete changed-outcome
enumeration, and an operator ruling on the exact dispositions IR may influence.

## 12. Required tests before field use

The implementation shall include deterministic tests for:

- packet size, packing, magic/version and all validation failures;
- sequence wrap, pulse-count wrap, packet loss, reordering, duplication and
  sensor reboot;
- freshness transition and null-speed publication;
- cumulative distance across packet loss;
- CW/CCW `spacingMm[]` walking and MM170/MM000 wrap;
- `k=0`, `k=1`, multiple-skipped, ambiguous and no-fit agreement cases;
- Hall event held, later vouched, and later discarded;
- direction change and IR anchor reset;
- MANUAL non-authority;
- AUTO IR-to-preset fallback and bumpless return;
- E-STOP, NEUTRAL, station zero and CTO cap dominance;
- publication-queue pressure with continuous one-Hz summaries;
- no calls to MQTT, Serial, navigation or motor code from the ESP-NOW callback.

Both Toby and Otto QUORUM profiles must still compile after the receiver seam is
added, even though only Toby is configured with an IR sensor. With IR disabled
in a profile, compiled behavior shall be the existing PWM-preset behavior and
no IR packet may acquire motor or navigation authority.

## 13. Values that must be measured, not guessed

The following are deliberately unresolved in Draft 1:

- ESP-NOW transmit cadence and receiver stale threshold;
- the Hall-time interpolation age/jitter budget;
- the Hall/IR route-distance tolerance and maximum `k` evaluated;
- per-profile target speeds;
- governor hold band, gains, service period, acceleration limit and maximum
  PWM;
- observed coupler-slack contribution to distance error.

Gate 1 or Gate 2 supplies each value. The implementation may expose provisional
named constants to gather evidence, but Stage 3 motor authority may not open
until the constants and their measurements are recorded in an accepted spec
revision or implementation report.

## 14. Implementation deliverables

1. A separate sender sketch, initially under
   `firmware/test-programs/IR_ESPNOW_SENDER/`, carrying no secrets and no MQTT,
   implemented against
   `docs/IR_TEST_CAR_ESPNOW_FIRMWARE_SPEC.md`.
2. Per-locomotive sensor identity/MAC/channel configuration without changing
   frozen route-common CTO wire definitions.
3. A reviewed receiver queue and source snapshot in QUORUM, first delivered as
   the observation-only `QUORUM_1_16R_IR_TEST_A` build specified by
   `docs/QUORUM_1_16R_IR_TEST_A_FIRMWARE_SPEC.md`.
4. The three Toby-originated telemetry streams in §8.
5. Offline capture/replay support for IR packets, Hall events, agreement results,
   source changes and governor corrections.
6. An implementation report with a call-site audit proving MANUAL non-authority
   and single motor-write authority.
7. A new decision record superseding the same-ESP32 clause of decision 0021.
8. A field report for each gate, including firmware identities and the
   operator's direct observations as ground truth.

## 15. References

- `docs/decisions/0005-timeout-means-blind-not-stopped.md`
- `docs/decisions/0036-a-stationary-ir-wheel-reports-zero-without-upward-authority.md`
- `docs/decisions/0014-speed-hold-not-throttle-hold.md`
- `docs/decisions/0015-ir-does-not-block-cto3.md`
- `docs/decisions/0020-compensation-requires-observed-evidence.md`
- `docs/decisions/0021-ir-speed-is-local-and-telemetry-is-summary.md`
- `docs/decisions/0022-plastic-ten-spoke-wheel-is-the-ir-target.md`
- `docs/IR_DEV_REQ/QUALITY_GATES_SPEED_OUTPUT.md`
- `docs/IR_DEV_REC/2026-08-09_SYNCHRONIZED_HALL_IR_LAP.md`
- `firmware/test-programs/IR_SPEED_LOCAL/README.md`
- `firmware/test-programs/IR_SPEED_LOCAL/IR_SPEED_LOCAL.ino`
- `archive/NGR_LL_DNA_CTO2_r12_CONTINUITY_FIRST.ino`

## 16. Acceptance summary

The integration is successful when Toby can do all of the following:

- receive accurate continuous IR speed and cumulative distance from the proven
  sensor car without depending on MQTT;
- publish ongoing IR speed and mile-marker-derived speed, including honest
  validity and age;
- show, event by event, whether IR distance agrees with Hall progression;
- use valid IR as the general AUTO speed feedback during supervised solo
  running;
- report a stationary wheel as `STOPPED`/zero, while keeping missing or invalid
  evidence null;
- fall back to the existing PWM presets when IR is unavailable—or when
  `STOPPED` conflicts with a positive motion intent—without stopping, surging,
  or increasing power because of zero;
- leave MANUAL, E-STOP, QUORUM 1.16R navigation, and CTO Bubble behavior
  unchanged until their respective later authority gates are explicitly opened.
