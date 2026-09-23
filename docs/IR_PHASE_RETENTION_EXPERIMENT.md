# Opt-in phase-retention experiment

Follow-up comparison on the recorded Toby moving window 17:34:17-17:43:12:
both default and retain modes produced 3284 completed pulses, 3562 rises,
753 gaps, and 137573 TRACKING samples out of 301728 received samples.
Thus the opt-in change has no demonstrated count benefit on this field window.
Its improvement remains limited to the synthetic slow-cycle evidence below.
During this follow-up, Arduino IDE was observed compiling the usual TX sketch;
no interference with that user-initiated operation was performed.

`Detector(true)` retains armed phase and last usable thresholds when the fast
envelope becomes too narrow. Current contrast/reliability remains invalid in
that condition. Real low/high crossings are still required; saturation, sample
gaps and open-pulse timeout invalidate/rearm as before. This is implemented in
the shared detector but OFF by default, including in TX 1.5.

Run the host speed sweep or raw replay with an extra argument `retain` to
enable it. Host results across 25/50/75 percent high duty:

- 20-500 ms periods: 19 expected / 19 observed after warmup, both modes.
- 1000 and 2000 ms: 19/19 with retention, versus 19/0 without it.
- 4000 ms at 25 or 50 percent: 19/19 with retention.
- 4000 ms at 75 percent: 19/0; high duration exceeds the 2.5 s timeout.
- Actual failed stationary capture: zero rises/completions/Tracking samples
  over 58656 received samples, with 15 radio gaps.
- Synthetic stopped high plateau: no additional count. Synthetic stationary
  noise: zero count. Host distance-contract regression tests still pass.

No inferred pulses are added. Low-contrast samples remain unreliable even
when the cached thresholds permit a completed observed cycle. Therefore this
improves count retention but does not yet produce trusted slow-distance bounds.
Stale cached thresholds can also be crossed by changing light. An identical
ADC waveform from light modulation and wheel rotation is indistinguishable to
this algorithm; field checks of the existing optical arrangement are required.
Do not promote it solely because synthetic counts improve.

Next: compare real continuous moving spans and stationary lighting transitions,
then choose detector behavior based on observed false/missed counts. The
physical TX 1.5 stationary test and unchanged-wheel calibration confirmation
remain pending. No new firmware version or flash was produced in this step.
