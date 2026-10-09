# NGR Architectural Note

## NAVI_EWO_0_1 — Architectural Inheritance and Simplification

**Date:** October 9, 2026  
**Status:** Governing design proposal for reconciliation before implementation; documentation only.  
**Scope:** NAVI navigation, geographic operating instructions, universal stopping, physical control, and future CTO operations.

This note applies the enduring decision-system principles by retaining NAVI's navigation judgment, keeping observation distinct from actuator action, and requiring evidence before additional control complexity. It does not authorize firmware changes, merging, flashing, or two-train activation.

### 1. Purpose

`NAVI_EWO_0_1` deliberately returns to demonstrated physical-control methods while retaining NAVI's architectural advances. The October 2, 2026 field operation demonstrated eight complete automatic station cycles using geographic PWM control without continuous closed-loop speed regulation. Document C subsequently established that geographic instructions, rather than procedural history, determine what NAVI must do.

**The objective is to combine proven physical behavior with geographic authority, without restoring obsolete station machinery or introducing unnecessary complexity.**

### 2. Fundamental separation of responsibilities

NAVI has three distinct responsibilities:

- **Navigation:** determine geographic position and direction from Hall landmarks, IR distance, and NAVI's existing judgment.
- **Operating judgment:** determine the instruction applicable at the current geographic position, including base tiles and any authorized overlay.
- **Physical execution:** translate that instruction into motor operation with the established PWM actuator ramp.

The stopping mechanism does not independently navigate, interpret Hall observations, or determine dispatcher intent.

**NAVI determines where it is and what the railroad requires. A reusable physical maneuver executes that requirement. The sensors observe the result.**

### 3. Geographic authority replaces procedural admission

A maneuver must be executable from the locomotive's current geographic position and physical state. It must not require passage through an earlier admission point, an `ARMED` state, or completion of prior procedural phases.

The same location and physical circumstances should produce the same operating requirement regardless of how the locomotive arrived there, except for the minimal execution progress needed to distinguish an unfinished stop from an authorized departure. This applies to cold entry, STOP/GO, missed magnets, reversals, and interruptions.

**A stopping maneuver is a sequence of geographic objectives, not a procedure that must be entered at its beginning.**

### 4. One universal stopping mechanism

The standardized stopping maneuver is:

**DECELERATE → STOP → WAIT → RESTART → ACCELERATE**

A stopping instruction supplies a geographic destination, applicable physical stopping profile, waiting condition, and the operating requirement after release. The destination defines the approach geography backward toward the locomotive.

The mechanism is independent of station identity. Four-Station Local is its first application; future uses include dispatcher holds, temporary geographic restrictions, and stopping behind another consist. The entire sequence—not only a braking calculation—is transferable.

### 5. Geographic stopping accuracy and completion

For Four-Station Local, the initial requirements are:

| Property | Initial requirement |
|---|---|
| Intended stopping point | Station +1.5 MM |
| Preferred stopping range | Station +1.0 to +1.7 MM |
| Acceptable completed station stop | Station 0 through +3 MM |
| Base station waiting period | Five seconds |
| Waiting-clock initiation | Station +1 MM |
| Departure authorization | Waiting requirement satisfied **and** physical stop completed |

The intended stopping point is deliberately between magnets. A physical stop within the accepted region completes the station-stopping requirement even when it differs from the target. NAVI must not restart solely to reach a mathematical coordinate or another Hall marker.

The preferred range is a performance objective, not an admission requirement. The station clock may begin while the final approach is still completing; physical stopping and elapsed waiting are independent requirements for departure. For a temporary traffic stop, release of the applicable geographic restriction is the waiting condition rather than a fixed station dwell.

### 6. Preserve demonstrated PWM behavior

The October 2 `NAVI_EYES_WIDE_OPEN_INTEGRATED_R2_FT2` field run is the physical-control reference for `NAVI_EWO_0_1`. The initial implementation should preserve or adapt its demonstrated geographic PWM approach and station-speed behavior, monotonic PWM braking, PWM-zero physical stopping, five-second station service, automatic departure, and known geographic grade compensation.

The historical station state machine is not reusable operating authority. These values are initial reference parameters, not proof that the same stopping distances apply at arbitrary speeds, locations, grades, or consist configurations.

**PWM executes the maneuver. IR measures its physical progress.**

Continuous IR speed homeostasis, adaptive glide-path correction, and ticker-tape playback are not prerequisites for `NAVI_EWO_0_1`.

### 7. NAVI owns continuous geographic position

A confirmed Hall landmark establishes a geographic reference. IR measures progress from that reference. Subsequent confirmed landmarks reconcile position against the surveyed map. A manual declaration must not manufacture an exact physical Hall anchor.

The stopping mechanism consumes NAVI's existing geographic judgment through a read-only interface. It must not create a second position estimator or permit Hall-only navigation advancement contrary to Decision 0116. NAVI's genuine Missed Magnet behavior, direction awareness, and Decision 0120 PWM-zero movement rules remain binding.

### 8. Current tiles and temporary overlays

The base geographic layer defines ordinary railroad operation. An overlay changes the applicable instruction over a limited region without erasing the base layer. When removed, NAVI reevaluates its current position and resumes the underlying instruction.

A temporary traffic stop must not become a station arrival merely because it is near a station. If a higher-priority stop occurs on the base station stopping tile, the base station waiting requirement begins when the higher-priority mandate is lifted. Departure accelerates toward the underlying geographic requirement; it does not universally target cruise speed.

### 9. Two-train operations remain the next objective

The universal stopping mechanism is the essential physical capability for future CTO operations. NAVI must eventually determine the geographic boundaries of the entire consist, not merely its Hall-sensor position.

**Required clear separation = 600 mm + the distance required for the standard stop at the follower's current speed.**

The stopping destination is projected backward from the leading consist's rear boundary with the required final clearance. Following may require reduced speed without stopping. When a stop is necessary, the follower uses the same standardized maneuver as station service. When sufficient separation returns, the temporary restriction relaxes and the follower resumes its underlying geographic instruction.

These are architectural requirements for later CTO implementation, not authorization to activate two-train control in `NAVI_EWO_0_1`.

### 10. Preserve existing control authority

The new implementation must preserve manual throttle and direction; STOP/GO and AUTO admission; MQTT control and command responsiveness; E-stop and low-voltage protection; existing NAVI Hall/IR judgment; genuine Missed Magnet handling; Decisions 0116 and 0120; and the existing `requestPwm()` and `serviceRamp()` actuator path.

No new controller may compete for PWM authority.

### 11. Operational transparency

NAVI should report the current geographic instruction, stopping destination, requested and applied PWM, stop completion, waiting-clock initiation, release, and departure. Telemetry must explain behavior without unnecessary processing or communications load. Observations, judgments, and actuator actions must remain distinguishable.

### 12. Necessity and Simplicity Test

Before introducing a mechanism, algorithm, state variable, restriction, or dependency, ask:

**Question 1:** What does NAVI actually need to decide, and what can the existing physical system already accomplish without another decision?

**Question 2:** What is the simplest way to accomplish the required task?

**Screening principle:** Added complexity must justify its inclusion and must not increase system fragility.

Preserve demonstrated functionality. Add machinery only when it addresses an identified operational need and a simpler approach cannot adequately provide the benefit.

### 13. Implementation acceptance criteria

Before `NAVI_EWO_0_1` is ready for field testing, its implementation must demonstrate:

1. A universal stopping executor independent of station identity.
2. Geographic instruction evaluation without approach-entry latches.
3. Correct behavior when entering or restarting midway through a maneuver.
4. Stopping between magnets without requiring a terminal Hall event.
5. Station-clock initiation at +1 MM and departure only after both waiting and physical-stop requirements are complete.
6. Recovery of demonstrated PWM braking, dwell, and departure behavior without restoring `StationMachine`.
7. Preservation of navigation, manual control, MQTT, and actuator authority.
8. Host-test and compilation evidence, followed by independent review.

Field testing remains necessary to validate physical stopping accuracy and arbitrary-location stopping performance.

### 14. Historical references and reconciliation flags

**Demonstrated physical-control reference:**  
`ab0938b0f01531a38ce4d8a1d9d932ffad4f0334` — October 2 `R2_FT2` firmware; strongest supported physical-control attribution.

**Stationless implementation baseline:**  
`c7c21163b016e443524eb9fa483f8b706ce76e22` — removes legacy station authority; it is not an implemented `NAVI_EWO_0_1` candidate.

**Governing architecture consulted:**  
Document C; Decisions 0116, 0120, 0121, and 0123; and the October 8 architectural decisions.

**Discarded experimental implementation:**  
`22b05a22d30d0ab76f5feb654a3e6f57a005545c` — closed-loop Pass 2 glide-control candidate, retained as historical development evidence and not the implementation foundation.

The following conflicts are intentional and unresolved; this note does not silently change the governing record:

| Governing record | Conflict requiring explicit reconciliation |
|---|---|
| Document C §§6–21 and Decision 0123 | They require IR-measured physical-speed control, steady-speed homeostasis, adaptive glide-path adjustment, and ticker-tape fallback. This note proposes geographic PWM physical control as the initial `NAVI_EWO_0_1` precedent and says those mechanisms are not prerequisites. A later operator decision must decide whether this note supersedes, narrows, or is rejected in favor of those provisions. |
| Document C §§22–23 / October 8 Decision 1 | They allow minimal execution progress for a station stop/dwell/departure but leave its representation and reset conditions to reconciliation. This note relies on that same minimal progress for “completed stop” departure; it does not define the state representation. |

**Central principle**

> **NAVI determines where it is and what the railroad requires. The standardized stopping mechanism executes the instruction from the locomotive's current geographic and physical state. The existing sensors observe the result.**
>
> **Preserve what works. Add only what is necessary. Complexity must earn its place.**
