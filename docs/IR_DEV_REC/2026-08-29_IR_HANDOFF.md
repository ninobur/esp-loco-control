# IR development handoff — 2026-08-29

Written to start the separate IR thread with what was measured, not with what
has to be rediscovered. **The plan is the operator's: prove the new unit on the
bench with the known-good test-car sketch, then mount it.**

## Two silent failure modes are now measured, in opposite directions

| | when | magnitude | what the health fields said |
|---|---|---|---|
| **undercount** — spoke merging | 2026-08-26, four laps | 1% to **39%** short of surveyed truth | 96–97% contrast-valid, 1–2 latch discards. Normal. |
| **overcount** — threshold chatter | 2026-08-29, Toby's power car | rise intervals p05 = **2 ms**, bursts to **239 pulses/s** (2.3 m/s on a 0.33 m/s railway) | `open_aborts` 0, `sat` 17. Normal. |

Neither raised a flag. That is the whole argument for the observer staying an
observer (decisions 0055, 0057).

The overcount was on a wrong-side jerry-rigged mount and is **not** evidence
about a proper installation — only that the failure exists and is silent.

## Acceptance test for the new unit

Wheel geometry is known: LGB 10-spoke plastic, 96.52 mm measured rolling
circumference, **9.652 mm per pulse** (decision 0022).

> Turn the wheel by hand through exactly one revolution. It must read **10**.

That single check catches both modes:

- **fewer than 10** → merging. Cross-check in-pulse duty cycle: ~29% healthy,
  55% failing (the 2026-08-26 laps rank-ordered exactly by this).
- **more than 10** → chatter. Look at rise-to-rise intervals; under 15 ms at
  hand speed is spurious.

Then: prove zero stationary pulses in shade **and** direct sun, and monotonic
accumulation while rolling.

## Traps found on 2026-08-29

**Flash RX 1.1 on the Pi receiver.** The 08-27 capture ran RX **1.0**, which
sends no type-4 acknowledgment, so TX 1.2 retried every retained fusion report
until eviction — **7,687 lines carrying 168 unique reports, 45.8x**. That storm
competes with the raw waveform stream. `firmware/test-programs/IR_SCOPE_ESPNOW_RX/`
is 1.1 and fixes it.

**`docs/IR_PACKET_FORMAT.md` §3 is wrong.** It documents `MIN_SPAN = 120` for
the TX sketch; the firmware says **32**. Do not calibrate against the document.

**Presence must be DECLARED, not probed.** NAVI_ONE briefly inferred whether IR
was fitted from signal variance. That reasoning is backwards: a floating
GPIO34 has *high* variance (it picks up charge from the previously sampled
channel), while a fitted sensor watching a stationary wheel is driven to a
steady level and has *low* variance. A properly mounted sensor on a stopped
locomotive would be declared NOT FITTED. Replace the probe with an operator
declaration — a profile line or `cmd/ir 1`. This is decision 0056 applied to
the instrument instead of the map.

**Sampling pin 34 is not free.** Pins 33 and 34 share ADC1. Reading an
unconnected pin 34 every 1 ms alongside the Hall pin produced ~1.1 spurious
Hall crossings per second — invariant to the motor (1.03/s running, 1.12/s
stopped) and to the bench surface (metal vs stone). Stopping it took the rate
to zero. When IR is fitted, sample at **100 Hz not 1 kHz** (spokes arrive ~30 ms
apart at cruise) and **discard the first read after each channel switch**.

## The good configuration, for when it is mounted

Trailing unpowered wheel on the power car, same rigid frame as the Hall sensor,
read locally on GPIO 34. No tow offset — the distance measured is the distance
the Hall sensor travelled. No radio in the measurement path (decision 0021).
And an unpowered wheel cannot spin against debris, which is the one thing PWM
can never detect.

## What IR is for, and what it is not

It witnesses. It does not vote. It has no path to `navMm`, no verdict, and no
arm in the identity conjunction. Its value is as the **independent witness**:
on 2026-08-27 it recorded 29 mm of wheel travel while the navigator advanced
four markers, and across that run 254 markers of believed advance against
47.0 m of travel — roughly half a lap never driven. That is the finding that
closed the case on silent magnets.

## References

- decisions 0021, 0022, 0043, 0055, 0056, 0057
- `docs/IR_DEV_REC/2026-08-26_IR_FOUR_LAP_DISTANCE_TRUTH.md`
- `docs/reviews/NAVI_FRESH_0_2_REVIEW_20260829.md`
- `tools/ir_scope_espnow_analyze.py` parses the captures natively
