# NGR Architectural Note

## NAVI_EWO_0_1 — Architectural Inheritance and Simplification

**Date:** October 9, 2026  
**Status:** Mandatory primary architectural guide for all future NAVI firmware revisions, established by David's October 9, 2026 documentation-reconciliation instruction; documentation only.
**Scope:** NAVI navigation, geographic operating instructions, universal stopping, physical control, and future CTO operations.

This note applies the enduring decision-system principles by retaining NAVI's navigation judgment, keeping observation distinct from actuator action, and requiring evidence before additional control complexity. It does not authorize firmware changes, merging, flashing, or two-train activation.

### Governing authority and document precedence

Every NAVI firmware revision must begin by reading this guide and reconciling
the proposed work against its applicable requirements. This requirement applies
to CODEX, Claude, and every other developer, beyond `NAVI_EWO_0_1`.

This guide supersedes earlier architectural documents, implementation
instructions, and decisions wherever their provisions conflict with its
principles. Compatible earlier decisions remain binding, particularly Decisions
0116 and 0120. Report conflicts explicitly; do not restore superseded mechanisms
by relying on an older record. Any proposed departure requires David's explicit
approval before implementation. Every implementation review must assess
compliance with this guide and its Necessity and Simplicity Test.

Architectural precedence is:

`This guide → compatible existing NAVI decisions → implementation specifications → historical documentation`

The standing agent policy in `AGENTS.md` governs work authorization and
deployment. Mandatory architectural reference does not itself authorize
implementation, merging, flashing, or two-train activation.

This reconciliation considers both source histories:
`db329782d422559dcb886f93db8c1f3e3922a7ab` (the 14-section note) and
`7a31d913ef2218687b488936a998f82655f5d3c6` (simplified IR-loss fallback).
The fourteen numbered sections below remain the architectural foundation.

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

Continuous IR speed homeostasis, adaptive glide-path correction, and PWM
ticker-tape recording/playback are superseded requirements. They must not be
carried into future NAVI revisions through older documentation. Geographic PWM
execution uses the demonstrated actuator behavior; IR remains physical evidence,
including navigation distance, rather than a mandatory continuous speed regulator.

**Simplified IR-loss fallback, retained from `7a31d913`:**

- At steady speed, hold the existing PWM while valid IR feedback is unavailable.
- During an established deceleration, continue the already-established monotonic
  PWM reduction, without introducing a new feedback correction.
- A currently applicable geographic instruction retains authority. The fallback
  supplies actuator continuity; it does not establish position, stop completion,
  or permission to depart.
- On valid IR recovery, applicable IR evidence resumes under the existing
  validity and continuity rules. Recovery does not activate the superseded
  homeostasis or adaptive glide controller.
- No ticker tape, historical PWM memory, playback alignment, missed-pulse
  prediction subsystem, blending controller, or new mode-history subsystem is
  required or authorized by this fallback.

Decision 0116 still prohibits Hall-only navigation advancement, manufactured
geographic progress, and Hall-only re-anchoring. Temporary feedback loss must not
be confused with a genuine IR frame/source/scale/order break or Decision 0120's
PWM-zero movement hold. Hall-checkpoint scheduling during IR loss remains
unresolved; no exception to either decision is implied.

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

**October 9 reconciliation and explicit supersession**

| Earlier provision | Disposition under this mandatory guide |
|---|---|
| Document C §§6–21 and §§35.2–35.4; Decision 0123: continuous speed homeostasis, adaptive glide correction, and fixed-geographic-PWM rejection | Superseded as required physical-control architecture. Use demonstrated geographic PWM behavior and the existing actuator ramps under §6. Physical operating objectives and smooth execution remain compatible. |
| Original Document C §35.6 and Decision 0123 at `b646cab5995124ff84ae2173c0d9912c84357815`: ticker-tape recording/playback | Superseded by `7a31d913`; simplified steady PWM hold and continued established monotonic deceleration survive. Any recovery wording that restarts homeostasis or adaptive glide correction is superseded by this guide. |
| Document C §§22, 34.1 and 35.5; Decision 0123: dwell begins only after a physical stop | Superseded for ordinary station service: the clock begins at +1 MM; departure requires both the waiting requirement and completed physical stop. Section 8's higher-priority-stop exception remains: base station dwell begins when the mandate is lifted on the stopping tile. |
| Document C §22: geographic stop-acceptance region not yet defined | Section 5 supplies the +1.5 target, +1.0 to +1.7 preferred range, and 0 through +3 acceptance region. Do not restart solely to reach the exact target or a Hall marker. |
| Document C §§34.1–34.2 and §35.1: departure always targets cruise | Departure targets the underlying current geographic operating requirement under §§8–9. Cruise is appropriate only when that requirement calls for it. |
| Document C §§22–23 and October 8 Decision 1: minimal tile-local execution progress | Compatible in principle. Representation, lifetime, and reset conditions remain unresolved; no procedural entry latch or remembered visit may become independent operating authority. |
| Earlier adaptive recording/backup proposals, including the October 7 proposal at `ec1d62c0398af26018c8d03d3949894f5e38458c` | Historical design evidence; their recording/playback and continuous adaptive-control requirements do not override this guide. |

Historical source text and commits remain evidence of prior reasoning and
observations. Supersession does not rewrite physical evidence or establish that
a proposed implementation has been field accepted.

**Remaining architectural questions — do not invent implementation defaults**

1. Which, if any, Hall checkpoint may schedule an already-established monotonic
   PWM ramp during IR loss without becoming navigation progress or an IR
   coordinate substitute (the open boundary retained from `7a31d913`).
2. The representation, lifetime, and reset of minimal execution progress for the
   currently applicable stop/wait/departure instruction.
3. How to initiate the +1-MM waiting rule for cold entry beyond +1 or for an
   accepted early stop between Station 0 and +1. The geographic acceptance range
   does not itself answer timer initialization in those cases.
4. How physical stop completion is established while usable IR evidence is
   unavailable. The earlier PWM-zero plus 50-ms-without-a-pulse criterion is
   retained as a prior candidate criterion, not proof of a physical stop during
   information loss; no new loss-specific criterion is decided here.
5. The physical stopping profile and current-speed-to-stopping-distance mapping
   for arbitrary destinations, grades, speeds, and consists. Field validation
   and the existing future-CTO evidence requirements remain necessary.

No new IR validity threshold, timing/alignment model, interpolation formula,
fallback navigation authority, or stop-detection algorithm is decided here.

**Central principle**

> **NAVI determines where it is and what the railroad requires. The standardized stopping mechanism executes the instruction from the locomotive's current geographic and physical state. The existing sensors observe the result.**
>
> **Preserve what works. Add only what is necessary. Complexity must earn its place.**
