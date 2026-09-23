# Independent IR movement: current evidence and implementation audit

## Stationary TX 1.4 failure

After the operator confirmed the car was stationary, a fresh Pi capture was
retrieved as `/private/tmp/ir_current_20260919.log`. Latest boot was 0x536abd10.
The inspected final 59.999 seconds contained 611 packets and 2072 counted
pulses (about 34.5/s). No sample misses or queue drops were reported.
Raw ADC percentiles: p1=2559, p5=2630, median=2646, p95=2665, p99=2686;
extremes 2139 and 3087. Reported envelope span min/median/max: 31/47/79.
The 32-count contrast gate therefore admitted stationary noise. Among 2014
fully observed rise/fall pairs, median width was 8 ms; 371 were <=2 ms and
756 were <=5 ms. Simple very-short-pulse rejection alone is insufficient.

TX 1.4 FAILED stationary acceptance. Earlier replay against quieter recordings
does not validate it. Do not use its cumulative count as physical distance.
The evidence identifies a noise-admission mechanism, not the electrical cause
of that noise. USB power, lighting, and sensor coupling have not been isolated.

## Existing capability

- Scope TX: boot/session id, cumulative detected rises, sample sequence, raw
  waveform, envelope thresholds, and health counters. No calibrated distance,
  defensible distance range, or explicit overall measurement validity.
- Scope latch timeout clears the high state and can count again without a
  physical low transition. Recovery must require a genuine low crossing.
- `IR_SPEED_LOCAL` already documents 96.52 mm circumference and ten spokes
  (9.652 mm per completed pulse), from three measured ten-revolution runs.
  Confirm this is still the installed wheel before using the calibration.
- That prototype already has completed-pulse counting, 120-count minimum
  span, marginal span below 300, explicit validity, and low-crossing rearm after
  an open-pulse timeout. These existing decisions must inform the next change.
- `firmware/common/IRSpeedWire.h` carries boot id, capture timestamp,
  completed count and health, but no bounded distance contract. Its wire-only
  STOPPED state is derived from silence and cannot serve as proof of stopping.
- NGR-Files NAVI_FRESH odometer exposes count and a simple contrast validity
  flag only. That is not enough to establish distance-error bounds.

## Required implementation and evidence

1. Reuse a single testable detector for firmware and replay, including identical
   percentile ranks, priming, latch behavior, and timing. Model packet gaps and
   boots explicitly; do not compare different boots as contiguous samples.
2. Preserve detected edges and completed/qualified pulses separately. Test
   contrast thresholds against both noisy stationary and weak moving signals.
   A stricter gate alone is not proof of an adequate operating range.
3. Add timestamped boot-scoped count and calibrated nominal distance snapshots.
   Include explicit invalidity reasons and separate inferred correction counts.
   Default corrections to zero; never silently alter raw counts.
4. Distance ranges require demonstrated missed/doubled pulse budgets and endpoint
   phase uncertainty. Without those, report unknown/unbounded distance with a
   reason. Silence, stale data, saturation, and inadequate contrast cannot
   establish stopping. A healthy endpoint cannot erase an intervening failure.
5. Obtain independent counted turns or measured travel at crawl/slow/medium/fast,
   departure/stop, shade/sun, and lighting transitions. Record known-stationary
   intervals, including sensor obscuration. Preserve timing and calibration.
6. Compare those measured ranges with actual route-map marker spacing and Hall
   sensing extent. Demonstrate same/next/later marker feasibility without using
   IR to assign identity. Earlier Hall-aligned laps alone do not prove this.

No navigation integration, error bounds, or field acceptance is claimed yet.
The goal remains active. `tools/ir_stationary_audit.py` reproduces the newest-boot
noise summary; its stationary premise must come from the operator.
