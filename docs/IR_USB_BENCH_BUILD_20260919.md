# USB IR bench tester - 2026-09-19

## Decision and scope

David requested a USB bench tester after the earlier MQTT scope showed large
missing-data regions. This is a raw measuring instrument for the non-powered
wheel on the IR car, not a replacement navigation system or detector fix.
No QUORUM, locomotive profile, motor control, or production protocol was changed.

Artifacts: `firmware/programs/IR_USB_BENCH/` and `tools/ir_usb_bench.py`.
The README beside the sketch specifies the wire format and test procedure.

## Prior observations preserved

- The housing is bogie-mounted with a 3 mm aperture shroud, close wheel clearance,
  and an axle spacer. Trials were in the normal mounting, not hand-held alignment.
- Tape backing produced 99 completed pulses over approximately ten hand-turned
  revolutions. David reasonably accepted this as a successful practical bench test.
- Later insert trials on TX_1_5 produced 330 pulses for nine or ten turns, then
  465 for ten, then 670 from the last stable baseline (664 if six preliminary
  counts are excluded). Those totals did not establish a stable conversion.
- The counts stopped during several observed stationary holds. Earlier claims
  of stationary false counts were not adequately synchronized to David's motion
  and should not be treated as established ground truth.
- Printed backing changes both light entry and reflected emitter light. Neither
  sunlight nor the insert itself has been established as the cause.
- The old MQTT scope used different detector timing. Its count totals are not
  directly interchangeable with TX_1_5. The USB firmware has no detector at all;
  its viewer's fixed hysteresis is explicitly diagnostic.
- Historical firmware contains seven-spoke metadata. Earlier discussion assumed
  ten pulses/revolution. Confirm the actual installed wheel independently before
  making a numerical pulse-accuracy claim.

## Verification

- Built with esp32:esp32 3.3.11, `esp32:esp32:esp32`: 279280 bytes program,
  22204 bytes global RAM. Uploaded successfully to the IR car at
  `/dev/cu.wchusbserial110`, with flash hashes verified.
- Four automated tests passed: fragmented records, corrupted-stream recovery,
  unsigned sequence endpoints, and diagnostic hysteresis/discontinuity handling.
- Synthetic GUI rendered a waveform; Freeze display left acquisition/counting
  active and showed an explicit frozen indicator. Synthetic data is not evidence
  of hardware quality.
- Actual USB GUI rendered the GPIO34 waveform at approximately 1000 samples/s.
  Boot a7ebf9ab; CSV `ir_usb_20260919_235131_703561.csv` in the user's local
  `~/NGR/ir_usb_bench_logs` folder. Data logging stays independent of GUI redraw.
- The observed capture contained 29177 records, and the viewer ultimately showed
  13 missing wire records, no firmware missed slots, no firmware queue drops,
  zero CRC failures, and four rail-valued ADC samples. Byte discard/resynchronization
  also occurred; zero CRC failures must not be described as lossless transmission.
- Last captured source time was 46.146046 seconds. The display then showed
  NO DATA. Cause is not established; operator was asked whether power/USB changed.

## Status and next gate

Built and bench exercised, NOT field accepted and NOT yet a validated lossless
instrument. Preserve the raw samples and discontinuities. Confirm sustained USB
acquisition, then record one slow revolution with known physical motion before
attributing extra edges to material, optical geometry, electrical noise, or logic.
Do not change the wheel or production firmware merely to fit an incomplete trace.

The fixed 0-4095 scale prevents small stationary noise being visually enlarged
by autoscale. It does not filter or hide the underlying samples. All threshold
changes, test IDs and diagnostic counts are saved alongside raw readings.
