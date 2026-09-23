# NAVI — IR Health, Odometry, and MM Reference
Date: 2026-09-22
Status: Design clarification for repository preservation
Scope: IR measurement semantics and its interface to NAVI. This does not redesign position recovery, station logic, AUTO, Hall baseline, or the MM-map correction algorithm.

## 1. Governing distinction

IR is an instrument, not a navigation agent.

When functioning, the IR measuring wheel answers:

    WE HAVE MOVED THIS MUCH.

NAVI, using a known MM landmark and the map, answers:

    THIS IS WHERE WE ARE.

IR inherently measures wheel movement and distance. Synchronization with an MM does not enable distance measurement. It provides the map reference that allows NAVI to interpret IR odometry as distance from a known MM.

## 2. IR is always used when functional

There is no "earn trust" probation.

IR is used whenever its continuous health checks show that the measuring system is functioning. A special ten-magnet IR trust period is not required.

The separate rolling ~10-MM / ~3000-mm NAVI position-history mechanism must not be confused with IR qualification.

## 3. IR availability

IR availability answers only:

    Is the IR measuring system presently capable of supplying meaningful wheel-motion measurements?

It does not answer:
- whether the locomotive should be moving;
- whether the locomotive is progressing on the railway;
- whether a Hall observation is an MM;
- which MM was observed;
- whether AUTO should continue;
- where NAVI is.

### Startup checks

Startup establishes that a usable IR instrument is present:
- recognized/paired source;
- fresh packets arriving;
- valid packet size, magic/version/type and CRC;
- valid boot ID;
- valid calibration/pitch;
- adequate optical contrast;
- no saturation;
- sampling functioning correctly.

Once these checks pass, IR is AVAILABLE. Locomotive movement is not required to establish availability.

### Ongoing checks

While operating, continue checking:
- packets continue arriving;
- packet sequence and timestamps remain ordered;
- pulse/rise counters do not move backward;
- calibration/pitch does not unexpectedly change;
- sampling continuity is maintained;
- optical contrast remains adequate;
- input is not saturated;
- pulse acquisition does not show a recognized malformed/aborted condition.

A recognized measurement-system failure makes IR UNAVAILABLE with an explicit reason. When the health condition clears, IR becomes AVAILABLE again.

## 4. Zero movement is valid data

Absence of new wheel pulses is not stale data and is not, by itself, an IR failure.

If fresh valid IR packets continue to arrive and the cumulative wheel count is unchanged, IR reports zero incremental movement.

Examples include:
- intentional stop;
- station dwell;
- very slow movement before another pulse completes;
- wheel positioned between optical transitions;
- a physical condition in which the wheel is not turning.

IR does not diagnose why the count is unchanged.

The existing detector term SIGNAL_STALE must have no NAVI health or navigation authority merely because no pulse has completed for a period of time. Packet/data staleness is a separate communications condition: packets themselves have stopped arriving.

A NAVI-facing movement state may be described as STILL or simply as movement = 0.

## 5. NAVI interprets IR in locomotive context

IR reports measurement. NAVI interprets that measurement using locomotive state and other evidence.

Examples:
- intended station dwell + IR movement 0 -> coherent;
- decreasing PWM + decreasing IR movement -> coherent;
- movement commanded + IR movement 0 + no Hall progression -> locomotive is not progressing; apply the established failure/shutdown rule;
- IR movement + Hall progression -> coherent;
- IR movement 0 + successive Hall progression -> presently unexplained contradiction; record/notify and continue according to established NAVI rules;
- IR unavailable -> NAVI operates using its own non-IR resources.

An unexpected IR measurement is not automatically an IR malfunction.

## 6. IR odometry versus MM-referenced distance

IR odometry exists independently of the map.

If the IR counter advances, IR can report the measured distance traveled even before NAVI has encountered an MM.

Before a known MM reference:
    IR: "we have moved X mm."
    NAVI cannot yet infer "we are X mm from MMnnn" from IR alone.

When NAVI establishes a valid MM at Hall detection time T, it associates:
- MM identity;
- Hall detection timestamp T;
- corresponding IR cumulative count C.

This establishes the MM distance reference.

Thereafter:

    distance_from_MM = (current_IR_count - C) * distance_per_count

This permits IR/map distance comparison, including the current +/-15% MM landmark window.

## 7. Normal stops preserve the MM distance reference

A normal deceleration, zero movement, station dwell, and restart do not break IR odometry or the MM distance reference.

During a dwell the pulse count simply does not increase. On departure, counting continues from the retained value.

Therefore:

    movement -> stop -> dwell -> restart -> movement

remains one continuous IR distance interval when the IR measuring system remained functional throughout.

## 8. Measurement failure and recovery

A genuine IR measurement failure can destroy distance continuity across the failure because NAVI cannot know how much wheel travel occurred while the instrument was unable to measure reliably.

After such a failure:

### While failed
- IR = UNAVAILABLE.
- MM-referenced IR distance continuity is invalid.
- NAVI uses its non-IR navigation resources.

### When IR health returns
- IR = AVAILABLE immediately.
- IR can immediately measure new movement, distance, and speed from the recovered measurement stream.
- The lost distance across the failure is not reconstructed.
- Therefore the previous MM distance reference remains invalid.

### Re-establishing the MM reference
A new MM reference requires a valid MM established by NAVI without relying on the broken IR distance chain.

At that MM detection timestamp, NAVI associates the MM identity with the corresponding IR cumulative count.

This is the new synchronization/reference point. MM-referenced IR distance is then valid again.

In short:

    IR recovery restores "WE HAVE MOVED THIS MUCH."
    A new MM reference restores "THIS IS WHERE WE ARE."

## 9. Interval integrity

A normal stop does not invalidate an IR interval.

A genuine measurement-integrity failure occurring between two endpoints does invalidate MM-referenced distance across that interval if pulse counts may have been lost or corrupted.

Existing cumulative diagnostics can help identify such failures, including:
- sampleGaps;
- saturatedSamples;
- openAborts;
- inferredAdded/inferredRemoved where applicable;
- reset/order/calibration discontinuities.

Care is required with the existing `unreliableSamples` counter. Current code increments it whenever detector reason is not TRACKING, which includes states arising merely from lack of recent movement. It therefore must not be used unchanged as proof that a stop-spanning distance interval is invalid.

## 10. Communications freshness is separate

IR packet freshness is a communications/availability question.

- Fresh valid packets + unchanged wheel count = valid zero movement.
- No fresh packets = IR communications unavailable.
- Fresh packets reporting a recognized optical/sampling failure = IR measurement unavailable.

Do not infer zero movement from loss of communications.

## 11. Implementation boundary

The IR health layer should expose, at minimum, concepts equivalent to:

    IR_AVAILABLE / IR_UNAVAILABLE(reason)

and NAVI should separately retain whether it has a valid MM distance reference.

Do not make TRACKING synonymous with IR validity.

Do not make absence of wheel pulses synonymous with stale data.

Do not grant IR authority to determine map position.

Do not require a ten-magnet trust ceremony before using a functioning IR instrument.

This clarification intentionally leaves the larger NAVI position-recovery algorithm unchanged.
