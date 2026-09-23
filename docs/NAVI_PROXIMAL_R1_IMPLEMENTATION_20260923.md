# NAVI COHERENCE PROXIMAL_R1 implementation

2026-09-23. Author: Codex. Built and host-tested; independent review pending.
Not flashed, not field accepted. David authorized implementing his pasted
twelve-point recovery instructions after the confirmed global-correction failure.
See decision0098 and NAVI_AUTO_CORRECTION_FAILURE_20260923.md.

## Artifact and Authority

Existing path and familiar Arduino shortcut retained:
firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/.
Boot name NAVI_COHERENCE_0_6_PROXIMAL_R1; health_revision PROXIMAL_R1.
This is now a behavior-changing control experiment, not an observation-only
health build. Prior flashed CAL0_FIX1 is recoverable at89dc369. No TX/RX/Pi or
motor-control interface was changed, and no upload was performed.

## Recovery Rules Implemented

1. Incumbent retained on isolated disagreement, no better candidate, tie, or
   lack of continuous travel evidence after reference establishment.
2. Physical candidates determined before polarity scoring. Absolute +/-10-MM
   boundary, never the old171-position scan.
3. Ten map-position slots approximate3m physical history. Multi-marker advances
   insert UNKNOWN slots; observed wrong poles remain the poles actually read.
4. Scores use actual observations, not an assumed ten successes. A unique
   strictly better candidate wins without an extra score margin. Two or more
   actual contradictions trigger investigation, enforcing the explicit rule
   that one wrong pole alone does not reposition NAVI.
5. Corrections reinterpret MM assignments while preserving poles, UNKNOWNs,
   measurement points and ordering. The physical anchor cannot shift with them.
6. Reversal retains observations, invalidates the unsigned-distance reference
   and requires ten new-direction positions before another recovery decision.
7. A distant/out-of-window Hall event is refused while retaining the incumbent;
   it no longer makes NAVI globally unresolved merely by exceeding the existing
   ten-marker IR admission horizon.

## Engineering Choices Requiring Review

Startup declaration is locally provisional. Same-epoch measured segment lengths
screen translated startup candidates against their map spacing. If IR is absent
at startup, the available constraints are the declared local region and directed
history, not an invented measured bound. All21 offsets may remain possible when
the evidence cannot distinguish them; pattern scoring still cannot leave +/-10.

Once mature history fully agrees with the incumbent, or a correction is accepted,
NAVI establishes a reference at the current MM and aligned measurement. Following
contradictions freeze that reference; only later assignments may be reinterpreted.
Candidate travel from the fixed reference must match measured travel. If reference
or current data is absent/cross-epoch, keep the incumbent. Later coherent history
can establish a fresh reference without resurrecting an old IR epoch.

These reference-establishment details are Codex's conservative implementation,
not verbatim operator rulings. Please review them specifically. A ten-position
window is not a fixed exactly3000mm distance; surveyed spacing varies. No speed
bound is inferred from PWM because no applicable calibrated bound was supplied.
The existing +/-15% nominal tolerance is used as an operational screen, not
claimed as certified measurement accuracy. Calibration0 remains UNVALIDATED.

## Cyclometer Boundary and Integration

IrHealthMonitor.at() returns the nearest original-Hall-time measurement within
150ms only when it belongs to the current active epoch. Nearest unhealthy or
prior-epoch points cannot be replaced by older good points. The returned point
has owner/epoch/boot/time/count/calibration/pitch but no route identity.
NAVI owns the route reference and all filtering/scoring. IR cannot propose MMIDs.

Existing ordinary advancement still uses the previous MovementSource adapter.
That adapter's timing fallback and stricter unreliableSamples/TRACKING semantics
are not silently rewritten here. The recovery path uses the newer health/epoch
semantics. This split is explicit, not a claim that all navigation now uses the
epoch layer. UNKNOWN gaps are materialized at a later supported Hall event;
background missed-window accounting remains separate work.

## Transparency

New nonretained diag/recovery is emitted after accepted Hall advances. It includes
event_serial, policy, reason, positions, observations, incumbent_score, best_score,
ties, offset, startup, reference_mm, travel_mm and candidate masks. Mask bit0=-10,
bit10=0, bit20=+10. Travel/ref unavailable uses null/-1. Unexamined masks stay0.
The emitted example below is synthetic, NOT event430's actual field result:
referenceMM39, travel1200mm, incumbent42, only+1 feasible; observed scores7->9.
The old nav correction event/schema is retained; operator warning now prints the
actual denominator. Health shadow becomes0 and boot identifies recovery authority.

## Verification

- ESP32 core3.3.11, esp32:esp32:esp32, warnings all: PASS. Flash1,016,043 bytes,
  static RAM73,012 bytes. Three existing Adafruit_INA219 enum warnings only.
- C++17 Wall/Wextra/Werror ASan+UBSan navigator suite: PASS,2052 local offset
  corrections and6840 single-misread holds; existing movement-window cases pass.
- Proximal policy suite: PASS. Actual Pi events408..430 opening word reproduces
  old whole-route uniqueMM166 match. New established recovery holds without
  travel; provisional recovery cannot nominate-70. This is a recorded-word
  regression, NOT a full raw-radio replay of the entire AUTO session.
-421 tie cases hold;65 unique one-vote, non-perfect improvements accepted.
  UNKNOWN denominator/warmup/retained-history tests pass; both directions admit
  measured+1-in-travel-direction while excluding+4/+7; reversal, epoch-change,
  wrap and excessive-distance incumbent-retention checks pass.
- Existing station suite: PASS,32 armed approaches. Station machine unchanged.
- Health epoch/alignment tests: PASS, including original110-byte calibration0
  packet fixture, nearest unhealthy/prior-epoch refusal. JSON13 payloads parse;
  maximum638 bytes against960-byte buffer.

## Before Field Use

Ask Claude/SAM to review physical-reference establishment, startup without IR,
retained-history interpretation, and the unchanged ordinary-Hall fallback.
No claim of full Twenty Questions completion:70-count Hall threshold,650ms guard,
independent missed-window progression and TX stationary contrast remain outside
this change. No IR distance-accuracy claim is made by these synthetic tests.

After review: flash Toby only at115200, verify PROXIMAL_R1, stationary30-60s,
then one manual run with known starting interval/direction and logged handling.
Check diag/recovery before authorizing AUTO. Stop on unexpected behavior.
