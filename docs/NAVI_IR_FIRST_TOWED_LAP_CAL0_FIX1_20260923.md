# First reported completed towed lap with CAL0_FIX1

David reported one completed lap after an initial uncoupling, backing up,
recoupling and restart. Direction was not explicitly confirmed; accepted MM
references progress upward with 170 -> 0 wrap. These are NAVI's identities,
not independently verified physical landmarks.

Read-only summary of 555 health records in the current log tail for loco boot
DB1F62954A510442, through 14:22:47.539 PDT. TX reboot before the run is visible
at 14:15:55; run TX boot FAB64DAE1D8A4397. Exclude initial uncoupling maneuver
from continuous coupled-running evaluation.

## Observations

- Coupled restart READY at 14:17:32.549, epoch 2, 11 pulses.
- Contrast loss at 14:18:13.773, 717 pulses, following accepted MM62.
- READY recovery at 14:18:16.300, epoch 3, 719 pulses; fresh MM63 reference
  established at 14:18:17.318.
- Contrast loss at 14:18:20.307, 764 pulses, following accepted MM64.
  Reacquiring/contrast transitions followed; READY at 14:18:22.611,
  epoch 4, 766 pulses. New MM65 reference at 14:18:27.976.
- No further epoch end/start in the received health records through
  14:22:47.539: over four minutes in active epoch 4. At that point pulses=5642,
  health HEALTHY, readiness READY, reference MM48, accepted=4304, rejected=0,
  queue_gaps=0. Shadow nominal travel in epoch 4: 47063.152 mm.
- Accepted references progressed MM41 through 170, 0 through 41 and onward.
  MM41 was recorded at 14:17:35.028 and again at 14:22:37.946.

## Interpretation and Limits

The zero-calibration packet fix works during towing. Epoch/reference recovery
is visible, and the final continuous epoch is encouraging. Zero packet
rejections/queue gaps do not prove zero missing radio packets or wheel pulses.
There were two early optical-health interruptions, not transport-stale epoch
ends. Whether these coincide with physical pauses or continuous movement
requires David's account. Do not label them motion faults without that context.
No measured distance ground truth or full Hall/raw-waveform analysis was done
in this immediate summary; no accuracy or navigation acceptance is claimed.
Latest telemetry was still TRACKING; completion of a lap does not prove stopped.

Next question: did Toby pause or hesitate near the early MM62-MM65 portion,
approximately 14:18:13-14:18:23? Leave stopped for post-run review.
