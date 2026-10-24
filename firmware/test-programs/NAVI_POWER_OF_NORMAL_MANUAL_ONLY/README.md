# NAVI_POWER_OF_NORMAL_MANUAL_ONLY

EXPERIMENTAL. Clean-sheet NAVI built from
`docs/navi/NAVI_CONFERENCE_POWER_OF_NORMAL_RECORD_20261024.md`, authorized
2026-10-24. Not field accepted. **AUTO and GO are hard-refused** —
`navi_power::ARRIVAL_BOUNDARY_VALIDATED` is `false` in `NaviArrivalBoundary.h`
and `handleCommand()` checks it directly; it cannot be bypassed from MQTT.
Manual throttle, direction, declare, E-STOP and battery protection work
normally.

## What is new

`NaviCandidate.h`, `NaviEncounter.h`, `NaviTarget.h`, `NaviArrivalBoundary.h`
— built directly from the conference record, not adapted from
`RvmShape.h` / `RvmContinuity.h` / `RvmNavigator.h`, which both independent
audits of r5.1 (Claude and Codex, 2026-10-24) found in active conflict with
the agreed design (Gaussian shape as an admission gate, a terminal
Contradiction state that halts AUTO on a single wrong-polarity encounter,
and a PWM<15 exclusion that discards standing baseline evidence).

## What is reused unchanged, from r5.1 commit `5b7a518b`

`RouteMap.h`, `RvmStations.h`, `RvmOps.h`, `NaviEstop.h`, `NaviWordStorage.h`,
`NaviSyncRecorder.h`, `LocoConfig.h`, both `LL_LocoConfig_*.h`. Taken from the
exact flashed commit, not this worktree's HEAD, which carries an unflashed
r6 change to `RvmStations.h`.

## What is NOT done

- **Minimum physically plausible width** (`NaviEncounter.h`,
  `MIN_WIDTH_BLOCKS`): mechanism present, value intentionally `0`
  (disabled). The conference left this numeric value open; it is not
  invented here.
- **Arrival-boundary table** (`NaviArrivalBoundary.h`, `BOUNDARY_TABLE_MS`):
  mechanism present, every entry intentionally unset. Fitting it against the
  historical per-interval travel-time corpus is unstarted.
- **A real lockup bug, root-caused but not fixed** (see
  `docs/navi/NAVI_POWER_OF_NORMAL_ONSET_SHORTFALL_INVESTIGATION_20261024.md`
  for the full investigation). `NaviCandidate`'s single global "one candidate
  open at a time" gate, combined with `NaviEncounter`'s return-to-baseline
  test comparing against a reference frozen at the instant of onset and
  never updated, cannot track the real ~100+ ADC baseline drift this railway
  shows over several consecutive magnets. Once one candidate fails to
  return, detection is blind to everything else for up to 10 s, repeatedly.
  Measured on the boot-1 corpus: 184 such lockups, 48% of the recording's
  duration, accounting for 97% of missed annotated openings. This is an
  architecture question (how "observed return to baseline" should behave
  when real magnets are closely spaced), not a parameter to retune, and is
  reported for decision rather than fixed unilaterally.
- Separately, `NaviCandidate.h`'s two-consecutive-sample test now runs at
  the established rule's own 1 kHz cadence (not the native 5 kHz), per
  `docs/NAVI_RVM_70_COUNT_GATE_20261020.md`. Measured in isolation this
  performs worse on this corpus, but the measurement is confounded by the
  lockup above and is not meaningful until that is fixed — see the
  investigation doc.
- **Full-sketch arduino-cli compile**: blocked on this machine by a
  pre-existing `Adafruit_BusIO` / esp32 core 3.3.12 incompatibility
  (`BitOrder`) in the INA219 battery-monitor include chain. Confirmed this
  affects the unmodified r5.1 donor sketch identically — not introduced by
  this build. The new navigation headers themselves compile and run clean as
  host-side C++ (see `tests/navi_power_of_normal/`).

## Testing

`tests/navi_power_of_normal/run_checks.sh` — unit tests for all four new
headers (Proposition 8's no-stop-on-nonqualifying behavior, observed-RTB
completion, the onset detector including a regression test for the bug
above, the boundary's fail-open behavior, and the hard AUTO gate) plus a
replay of the real boot-1 native Hall corpus. No hardware, no network.
