# EWO 0 5 operating decisions — October 14, 2026

These are David's subsequent decisions in the implementation conversation.
They supplement the verbatim [IR criteria](ir-credibility/APPROVED_RULES.txt),
whose original 0_4 title predates the version-number clarification. The tested
first DART increment is 0_4; this continuation is 0_5.

NAVI retains one geographic authority. Hall and IR measurements remain separate
from their interpretation. Throttle execution cannot create geographic identity.
No change is made to maps, calibration, opening coordinates, STOP geometry,
70×2 detection, or the observation-only role of morphology and Type-6.

## Previous lap

Historical navigation uses the immediately preceding completed lap's recorded
MM-to-MM arrival time for the same interval and direction. No averaging,
minimum sample count, statistical predictor, or older-lap substitution.
Station timing retains the actual braking, dwell and departure sequence,
including time from actual throttle application to subsequent landmarks.
The current incomplete lap must not overwrite the preceding complete lap.

## No Hall by MM+2

If no qualifying opening is available by the expected MM+2 position or its
applicable previous-lap arrival time, stop and report a likely infrastructure
or hardware problem. Do not invent the missing MM or advance without a Hall
selection. This complements the already approved stop when an observation is
more consistent with MM+2 than MM+1.

## Throttle fallback without authoritative IR

Retain the geographic TILE/STOP OVERLAY instructions. Each newly accepted
70×2 opening establishes the tile, immediately applies its designated entry
PWM, and starts a time-based ramp toward the next tile's entry PWM. Use the
previous lap's corresponding tile traversal duration. At the next selected
opening, reset to that tile's entry instruction and ramp. Time expiry alone
never establishes the next tile. Preserve final braking, dwell and departure.

## Mid-tile IR failure

Hold the current commanded PWM until the next accepted Hall opening. Do not
start a new ramp, restart the tile clock, or reset to entry PWM on IR loss.
At the next accepted opening, establish its tile and begin the prescribed
time-based fallback. Continue Hall-synchronized fallback until reliable IR
operation is restored.

If final braking has already started, let the existing braking, dwell and
departure sequence complete. STOP, E-stop, electrical protection and NAVI's
navigation-failure intervention retain their authority.

## Manual STOP → GO — settled by David

Pause and resume both timing clocks across manual STOP → GO, preserving the
current geographic identity. The clocks pause during manual STOP and resume
on GO. A new position declaration is not required merely because the train
stopped and restarted. If the locomotive was physically moved while stopped,
the existing position-declaration safeguards still apply.

Implementation keeps raw observation timestamps intact and removes manual-stop
duration only from the arrival-comparison and tile-ramp clocks. Scheduled
station braking, dwell and departure retain their existing execution.

## Deployment authorization

David subsequently authorized flashing Otto, connected by USB, when the build
passes review and tests. That authority supersedes the earlier no-flash limit.
It does not itself authorize locomotive movement or changes to Pi services.
