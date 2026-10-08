# NGR Architectural Guide
## NAVI Adaptive Control Recording and Backup
**October 7, 2026 — Proposed Design**

**Status:** Architectural proposal for review. No implementation authorized.

### 1. Governing principle

**NAVI learns how to operate the railroad by recording what it actually does during successful speed-controlled operation.**

The recording provides a backup when IR movement feedback becomes temporarily unavailable.

The objective is continuity of automatic operation without requiring perfect IR packet delivery.

This is not a second navigation system. It is an alternative source of motor-control instructions when live speed feedback is unavailable.

---

## 2. The 171-tile control map

The entire Lowline is divided into 171 mapped intervals.

Each tile has two distinct elements:

**A. Speed objective**

The prescribed speed behavior for that interval, including:

- Normal cruising speed.
- Station approach speed.
- Controlled acceleration or deceleration.
- Station stopping ramps.

**B. Recorded control behavior**

What NAVI actually commanded to achieve the speed objective:

- PWM values.
- Timing of PWM changes.
- Elapsed time through the interval.
- IR-measured speed and distance.
- Entry and exit conditions.

**The speed objective is the instruction. The PWM recording is NAVI's learned response.**

The recording must not redefine the speed objective.

---

## 3. LIVE operation — learning

During normal automatic operation:

1. NAVI knows its current location and prescribed speed objective.
2. IR supplies cumulative movement measurements.
3. NAVI adjusts PWM to achieve the prescribed speed.
4. NAVI records the PWM commands and their timing.
5. IR measurements establish the corresponding physical progression.
6. Each completed tile refreshes its portion of the 171-tile recording.

This produces a continuously updated record of successful locomotive operation.

The recording is specific to the locomotive, direction, and route.

A recording should not be refreshed using unreliable feedback or an unsuccessful control sequence.

---

## 4. The ticker-tape representation

The simplest useful recording is a time-indexed sequence of actual PWM commands.

For example:

| Elapsed time | PWM | IR distance |
|---|---:|---:|
| 0 ms | 85 | 0 mm |
| 200 ms | 82 | 45 mm |
| 450 ms | 76 | 108 mm |
| 700 ms | 70 | 165 mm |

*Illustrative values only.*

The essential recording contains:

- Tile identity.
- Direction.
- Elapsed time.
- PWM command.
- Corresponding IR-measured position.

The physical measurements establish the recording's relationship to the railroad.

**During replay, elapsed time replaces live IR distance as the progression reference.**

The recorder should preserve the actual PWM changes rather than generate an elaborate mathematical motor model.

---

## 5. First-lap operation

Before NAVI has learned the entire railroad, it may use an initial reference recording.

Possible sources include:

- A previously validated recording.
- An averaged reference from earlier runs.
- A locomotive-specific startup profile.

As NAVI completes intervals under live control, their recordings replace the initial reference.

The system must distinguish learned recordings from provisional startup recordings.

An unvalidated startup recording must not silently acquire the same standing as a demonstrated successful recording.

---

## 6. Validation before use

**This is the most important development step.**

We can test the recording without allowing it to control the locomotive.

### First lap — RECORD

NAVI operates under live IR speed control.

It records PWM commands, elapsed time, and corresponding physical movement.

### Subsequent lap — SHADOW

NAVI continues using live IR control.

Meanwhile, the recorded program runs virtually, producing its expected progression around the loop.

At each received IR observation, NAVI compares:

- Actual versus recorded elapsed time.
- Actual versus expected cumulative distance.
- Actual versus recorded speed.
- Actual versus recorded PWM.
- Tile entry and exit timing.
- Station approach and stopping performance.

**The shadow recording must never issue motor commands.**

### Evaluation

Determine:

- How repeatable the locomotive's behavior is.
- Whether differences accumulate over successive tiles.
- Whether the recorded station approaches remain accurate.
- Whether battery condition or load changes materially affect repeatability.
- Whether a recording from the previous lap performs better than an older reference.

This establishes whether time-based playback is sufficiently predictable to serve as a useful backup.

One important limitation: shadow comparison tests the repeatability of the observed trajectory, but cannot by itself prove that replaying the recorded PWM would produce the same trajectory when live control would otherwise have commanded different PWM. That requires a separately authorized replay experiment.

---

## 7. IR interruption — backup operation

A missing individual pulse packet is not an interruption requiring backup.

Cumulative IR observations already bridge missing individual transmissions.

Backup becomes relevant when NAVI no longer has sufficiently current, coherent movement feedback to regulate speed.

The transition is:

**LIVE → REPLAY**

NAVI uses the last confirmed physical position and the recording's time-distance reference to select the corresponding point in the ticker tape.

It then reproduces the recorded PWM commands according to elapsed time.

The transition should be smooth rather than an abrupt PWM change.

The existing station speed objectives remain unchanged.

However, during replay, NAVI cannot verify that the locomotive is achieving those objectives.

**Replay is open-loop control, not measured speed regulation.**

---

## 8. Asymmetric transitions

The two transitions serve different purposes.

### LIVE → REPLAY: Fast

When usable IR feedback is lost, NAVI promptly adopts the recorded control program.

It does not wait indefinitely for communications to recover.

The entry threshold must be established from observed IR reception behavior.

### REPLAY → LIVE: Slow

When IR returns, NAVI has two sources of guidance:

1. The currently executing recorded program.
2. Fresh cumulative movement observations.

NAVI evaluates the returning evidence before transferring control back to live speed regulation.

The transition should be gradual.

This avoids rapid switching when reception is intermittent.

**Fast to backup. Slow to restore.**

No specific switching threshold or ramp duration is authorized by this document.

---

## 9. Position awareness during replay

This requires particular care.

While IR is unavailable, NAVI can estimate its progression using the recording's time-distance relationship.

That estimate is not a physical observation.

NAVI must distinguish:

- **Confirmed position:** Established through accepted physical evidence.
- **Estimated progression:** Derived from the recorded control trajectory.
- **Recovered position:** Reconciled when valid cumulative IR observations resume.

A later cumulative IR observation can restore measured wheel-distance progression across the interruption.

Trackside Hall location anchors, when implemented, will provide an additional independent geographic reference.

The recording must never silently promote estimated movement into confirmed location.

---

## 10. Station operation

The recording covers the entire railroad, including station approaches.

The existing five-tile approach remains governed by its prescribed speed objectives:

- The first three tiles maintain their designated speeds through feedback-controlled PWM adjustments.
- The final two tiles contain the established speed-adjusted deceleration ramps.

The recording captures the actual PWM behavior produced by those controls.

During replay, NAVI follows the corresponding recorded command sequence.

Station approaches deserve particular attention during shadow validation because small position errors may produce materially different stopping locations.

The recorder does not introduce a new station controller.

---

## 11. Simplicity requirements

The first implementation should contain only four conceptual components:

| Component | Responsibility |
|---|---|
| Recorder | Capture actual PWM and physical progression |
| 171-tile memory | Store the latest qualified control history |
| Shadow evaluator | Compare recording against subsequent live operation |
| Replay controller | Reproduce recorded commands when authorized |

Avoid introducing:

- A second navigation engine.
- A predictive motor model.
- Synthetic IR measurements.
- Elaborate statistical learning.
- Independent station-control machinery.
- Communications acknowledgments or packet recovery.
- Unapproved operational restrictions.

The recorder must not complicate the IR communications system.

---

## 12. Development sequence

**Phase 1 — Recording**

Implement observation-only recording of live PWM and corresponding IR movement.

Verify correct tile association, timing, and memory requirements.

**Phase 2 — Shadow evaluation**

Compare recordings against subsequent live runs.

Measure repeatability across the complete 171-tile loop, especially station approaches.

**Phase 3 — Review**

Evaluate the evidence.

Determine whether the recording is sufficiently repeatable to justify open-loop backup operation.

Resolve startup-recording behavior, entry thresholds, recovery criteria, and transition ramps.

**Phase 4 — Controlled replay**

Only after explicit authorization, enable playback during selected IR interruptions.

Validate its behavior before extending it to normal AUTO operation.

---

## 13. Architectural boundaries

This design does not authorize changes to:

- IR physical detection.
- ESP-NOW transport.
- Hall navigation authority.
- Station speed objectives.
- Existing station stopping logic.
- Manual control.
- Dispatcher authority.
- PWM limits or braking behavior.

The initial recorder and shadow evaluator are **observation-only**.

No automatic fallback control is authorized until its behavior has been evaluated and approved.

---

## Governing statement

> **NAVI controls the railroad using live physical evidence. It records the commands that achieve the prescribed speed objectives across all 171 tiles. When live feedback is unavailable, a validated recording may provide temporary continuity of control. When feedback returns, NAVI verifies it and gradually resumes live regulation.**

**The recorder remembers what worked. NAVI remains responsible for deciding when that experience is applicable.**
