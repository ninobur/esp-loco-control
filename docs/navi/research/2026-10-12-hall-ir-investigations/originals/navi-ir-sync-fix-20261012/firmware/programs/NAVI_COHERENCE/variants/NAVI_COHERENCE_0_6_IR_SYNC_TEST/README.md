# NAVI_COHERENCE_0_6_IR_SYNC_TEST_R1

Experimental Type-5 / Type-6 receiver-arrival comparison on the historical
`NAVI_COHERENCE_0_6_POSITION_STATIONS_R1_20Q3_SYNC_R1` control baseline.
This is AUTO-enabled development firmware, **not field accepted**. It is not
NAVI_EWO. The selected profile is **Otto / 9950011**; the inherited historical
header comments mentioning Toby do not select the profile.

David authorized the five review repairs and flashing after successful
compilation on October 12, 2026. He subsequently required passing regressions
and confirmation that the original navigation behavior was retained before
flashing. This README and the tests record that scope; no transmitter change
or navigation algorithm change is part of this experiment.

The design follows the NGR principles of retaining physical observations and
their provenance, keeping evidence separate from its interpretation, and
leaving navigation judgment with NAVI.

## Repairs

- The original Type-5 `IrRx`, 24-entry receive queue, admission, overflow
  responses, movement history, health, NSR1 recording, and navigation calls
  are retained. Accepted Type-5 points are additionally copied to a diagnostic
  history. None of the new diagnostic values are navigation inputs.
- Type-6 has a separate **statically allocated 32-entry queue** and its own
  drop counter. Its failures cannot call the Type-5 transport-gap or health
  reset paths or consume the Type-5 queue's capacity.
- Both channels retain 64 accepted diagnostic points. Comparison selects the
  newest point whose receiver timestamp is at or before the Hall timestamp.
  A 16-entry diagnostic Hall queue defers the comparison until a subsequent
  Type-5 drain. Pending pre-Hall packets defer only the diagnostic output.
  Hall acquisition and NAVI's immediate judgment remain unchanged.
- Type-6 requires the same paired MAC as Type-5. Pair changes clear diagnostic
  histories and pending Hall comparisons; packets received before pairing are
  excluded. Foreign packets cannot overwrite the selected source's history.
- Valid Type-6 observations after sequence gaps retain their count, timestamp,
  boot and sequence. The gap changes the continuity/eligibility flags; it does
  not replace the measurement with zero. Unsigned sequence wrap is supported.
- The new helper is a normal header, avoiding the submitted sketch's unknown
  `SyncPoint` and Arduino automatic-prototype ordering errors.

Type-6 service drains at most the 32 packets waiting when its service starts,
then prints at most one comparison after the original control services.
Diagnostic Hall overflow and insufficient retained history are reported and
cannot call navigation loss handlers. The new state is confined to
`IrSyncDiagnostics.h` and the explicit diagnostic taps in the `.ino`.

## Serial interpretation

`[IR_SYNC]` records contain Hall serial/time, paired `src`, both raw cumulative
counts and receive ages, validity, boot IDs, sequence IDs, `t6_cont`,
`same_boot`, `compare_ok`, and `t6_minus_t5`.

- `t5_valid` / `t6_valid` mean an accepted observation was found in retained
  history at or before Hall; a valid zero Type-5 count is not missing data.
- `t6_cont` describes received sequence/count adjacency, not proof that every
  physical optical pulse was detected. The first event in a boot and the first
  event after an observed gap are not continuous.
- `compare_ok` requires both observations, equal boot IDs and Type-6 continuity.
  `t6_minus_t5` is still shown across a sequence gap if the boots match; it is
  a raw cumulative difference and is not evidence of uninterrupted delivery.
- With missing observations or different boots, the difference placeholder is
  zero and `compare_ok=0`; the raw observations and separate validity/boot
  fields remain visible.
- `gaps` counts discontinuity events, not missing packets. `bad`, `old`,
  `foreign`, `rx5drop`, `rx6drop`, and `halldrop` are cumulative diagnostic
  counters at reporting time, not snapshots of counters at the Hall instant.
- `hist5lost` / `hist6lost` distinguish an unavailable historical point after
  ring overwrite. A buffer limit is not repaired by inventing a position.

This is receiver-arrival age, **not optical-capture age**. The two ESP32 clocks
are not synchronized. Printing is deferred until after navigation services,
but Serial/radio/CPU overhead still needs physical measurement. Host tests
prove deterministic state isolation; they do not prove unchanged real-time
scheduling or field reliability. Type-6 observations remain Serial-only;
the existing NSR1 UDP wire format is unchanged.

## Build and regression validation

Core: **esp32:esp32 3.3.12**, FQBN `esp32:esp32:esp32`.
Libraries: PubSubClient 2.8, Adafruit INA219 1.2.3, Adafruit BusIO 1.17.4,
plus the bundled ESP32 libraries. The historical QUORUM credentials and shared
headers are resolved from this checkout; do not substitute newer branch APIs.

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all \
  firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_SYNC_TEST
python3 firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_SYNC_TEST/tests/run_all.py
```

The regression runner retains all ten original C++ suites and the health JSON
checks. It also compares the actual original/corrected ingress and processing
functions under a mocked queue/clock, using the real movement, health and NAVI
classes. Across 240 navigation judgments and ten Type-6 delivery/failure
patterns it checks Type-5 state, IR/MM references, recorder packets, rulings
and full public navigation status for equality. Both builds use the identical
Type-5/Hall inputs and local times. AddressSanitizer and UndefinedBehaviorSanitizer
are enabled. Source assertions verify that all 13 inherited headers, Hall
acquisition, Type-5 processing and control-loop order remain unchanged apart
from the documented diagnostic taps. Tests also cover causal selection,
late processing, gaps, foreign sources, re-pairing, sequence wrap/order,
bounded history and diagnostic queue loss.

Implementation/build/flash evidence is recorded under the local
`NGR-Files/navi-ir-sync-fix-20261012/` directory. The rollback image is
`rollback/otto-9950011-pre-ir-sync-0x000000-0x27ffff.bin`; it includes private device data and
must not be published. Its hash and target identity are recorded in the
implementation manifest. The original uploaded source/README are preserved
there as well. The image covers every flash sector overwritten by this upload
(up to `0x113fff`), plus additional lower-flash state. Full-chip reads failed
with serial corruption; high flash above `0x27ffff` is neither backed up nor
written by this upload. The neighboring `IR_HEALTH` baseline was not modified.

## Deployment and field protocol

Deployment status and exact build sizes are recorded in `VALIDATION.md`.
Boot identity: `NAVI_COHERENCE_0_6_IR_SYNC_TEST_R1`.
The original flat import filenames are links to this canonical Arduino folder.

The IR car must transmit the compatible 61-byte Type-6 packet contract.
The September 28 transmitter does not. This task does not change the IR car.
For a controlled run, record the Serial stream, confirm the paired MAC and
matching boots, and compare raw counts/ages alongside continuity and drop
counters. Preserve discontinuities instead of excluding them from the evidence.
Observe control-loop/Serial timing under the actual traffic load. A successful
build or boot is not field acceptance or proof of optical measurement age.
