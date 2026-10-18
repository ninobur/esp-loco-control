# NAVI_RVM_MANUAL_1_0 — stopped implementation draft

2026-10-18. **Incomplete. Not a buildable ESP32 sketch and not a field candidate.**
The attached assignment is preserved verbatim in [ASSIGNMENT.md](ASSIGNMENT.md).
No locomotive was flashed, no operating firmware was changed, and no live Pi
service or MQTT configuration was changed.

## Result and reason for stopping

A first focused replay reproduced a geographic discrepancy before hardware and
transport integration. Root AGENTS.md §1 requires stopping and reporting a failed
implementation rather than substituting another algorithm without authorization.
The draft is preserved so this result can be independently reviewed, including
the mechanism that produced it. It is not presented as completion of the assignment.

Starting at MM146 CW, the observer reports 147, 148, 149, 150, **153, 157, 158, 160**.
The recording's legacy comparison sequence is 147 through 156. The first four
identities agree; subsequent identities do not. In particular, the observer reports
153 at approximately 51.300 seconds, while the legacy event-open labels place
151 at 50.031 seconds and 153 at 56.524 seconds. Recognition completion and event
opening are different times, but that does not explain the subsequent index drift.
Legacy labels are fallible comparison evidence, not independent geographic truth.
This is sufficient contradictory evidence to withhold a recognition-success claim.

See [actual replay](../../tests/navi_rvm_manual/replay_actual.csv) and
[machine-readable results](../../tests/navi_rvm_manual/results.json). The recorded
fixture and its provenance are already committed under `tests/navi_rvm/fixtures/`.
The fixture was previously inspected; its historical name `heldout_cw` does not
make it a fresh holdout for this work. IR columns exist in that inherited CSV but
are not read by this observer. No thresholds were changed following this result.

## Draft architecture and its present limitation

`ManualControl.h` accepts only operator commands, electrical readings, and time.
It has no observer dependency or navigation-result input. Host tests cover its
PWM ramp, direction-at-zero rule, STOP, E-STOP, neutral interlock, low-voltage
latch and recovery. These are software checks; the hardware output adapter has
not been implemented or exercised.

`WaveformObserver.h` fits continuous Hall samples to an immutable spatial
waveform, using a soft applied-PWM movement prediction. A monotone dynamic
program retains relative accumulated path costs in a 384-cell band. The band
moves with numerical phase independently of successful MM recognition, allowing
unobserved reference portions without renumbering the map. Signed Hall residuals
include polarity directly; there is no independent lobe detector.

A full seam-to-seam waveform contribution is compared with a baseline explanation.
The draft freezes an accepted observation-time coordinate interval by discarding
continuations inconsistent with that registration. Later states inherit these
constraints and accumulated costs. This is more than a last-trusted-MM variable,
but **retaining history is insufficient when a mistaken registration is accepted**.
The replay demonstrates that the present continuation/acceptance combination can
allow excessive forward movement and accept later mapped waveforms. Once frozen,
that error constrains subsequent matching as well. The exact cause requires
review of the competing paths, not adjustment to force the legacy sequence.

The acceptance constants remain experimental: contrast 0.616107 (inherited
appearance hypothesis), mean capped residual <=2, and a <=100 mm interval of
states within one cost unit of the winner. These have NOT been validated as a
joint geographic confidence measure. They are recorded explicitly so they cannot
be mistaken for architect-approved or field-established rules. The one-time
baseline-offset initialization also assumes ordinary track at declaration.
Neither these constants nor bounded-path pruning proves the puzzle invariant.

The smallest next investigation would inspect how alternatives between MM150 and
MM153 lose support, and whether prior waveform registration actually bounds their
elapsed physical progression. It should preserve these observations and compare
whole-pattern explanations. Adding a detector gate, strike counter, motor
intervention, redeclaration, or recovery subsystem would not answer that question.
No such additions were made. No replacement recognition design has been implemented.

## Hardware and source reconciliation

Base: `cbeeedfc49627edb695bba482874d30018e8fc41`, the supplied operator-metaphor
branch, which contains the earlier candidate and audit context. This is a
provenance/rollback point, NOT a known-good field build of this new experiment.
The existing candidate and operational source files remain untouched.

MANUAL_1_1 supplied the structural no-navigation-motor-authority reference.
The newer archived Otto profile establishes PWM GPIO4, direction GPIO16 (not
older GPIO2), Hall GPIO33, 20 kHz/8-bit PWM, INA219 SDA21/SCL22. The newer candidate
also records 150 ms/count manual acceleration, brake-controlled 400–15 ms/count
deceleration, 31 ms/count STOP and low-voltage deceleration, and five 5-second
readings below 13.25 V (excluding <12.5 V disconnected readings), recovering at
14 V. ManualControl expresses these rules; their hardware implementation is
still outstanding. No claim is made to have verified the currently flashed binary.

`FixedReference.h` preserves the earlier frozen CW/CCW numerical arrays unchanged;
only include and namespace names changed. It includes the fixed map-linked peak
and seam coordinates and PWM speed estimates. Existing provenance:
[reference_provenance.json](../../tests/navi_rvm/reference_provenance.json) and
[prepare_reference.py](../../tools/navi_rvm/prepare_reference.py). The original
reference used offline IR-derived coordinates; there is no runtime IR input here.
This limited reference is not a validated multi-lap model and is not learned online.

## Verification and outstanding deliverables

Host C++17 compilation with warnings as errors passed. The focused manual checks
and constant-Hall/PWM-only no-geographic-advancement check passed. The recorded
replay discrepancy is preserved; the replay executable deliberately reports
observations rather than asserting that fallible historical labels are truth.
`results.json` explicitly records sequence inequality.

Not completed: ESP32 .ino integration and compilation; acquisition task; electrical
hardware adapter; MQTT commands and reporting; dashboard delivery test; Pi capture;
imperfect-ensemble/gap/reversal recognition verification; field-test procedure.
There is consequently no exact `.ino` sketch path or ESP32 binary to deliver yet.
Do not flash these draft components or treat host checks as track acceptance.

The inspected existing dashboard consumes `ngr/loco/9950011/state/nav` JSON;
`mm/marker` alone only refreshes marker freshness. The existing Pi run logger
subscribes to `ngr/#`. These are integration findings, not claims that draft data
has reached the Pi. No synthetic geographic messages were published to live topics.

## Reproduce without hardware

From the repository root:

```sh
c++ -std=c++17 -O2 -Wall -Wextra -Werror \
  -Ifirmware/test-programs/NAVI_RVM_MANUAL_1_0 \
  tests/navi_rvm_manual/check.cpp -o /tmp/navi-rvm-manual-check
/tmp/navi-rvm-manual-check
/tmp/navi-rvm-manual-check tests/navi_rvm/fixtures/heldout_cw.csv 146 1
```

Output columns: recording time in microseconds, reported MM, unwrapped reference
phase in millimetres, residual quality, waveform-vs-baseline evidence, and current
coordinate uncertainty in millimetres. The acceptance and path-pruning code is
in the same header used by this executable; there is no replay-only recognizer.
