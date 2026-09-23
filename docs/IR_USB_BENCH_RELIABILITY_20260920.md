# IR USB bench reliability repair - 2026-09-20

## Scope

David authorized continuing work on the bench tester. No wheel, sensor board,
navigation firmware, or production detector changes were made. Version 1.1 was
uploaded only to the IR car's ESP32. This supersedes the initial transport status
in `IR_USB_BENCH_BUILD_20260919.md`; it does not invalidate that historical record.

## Evidence and changes

The first viewer's log stopped at ESP uptime 46.146046 seconds. After closing the
viewer, a direct twelve-second USB read received 12031 consecutive records with
zero CRC errors. The boot ID was unchanged and uptime exceeded 508 seconds.
Therefore the ESP had not stopped sampling or reset. The precise desktop stall
trigger was not established; blaming optics or a particular GUI function would
exceed the evidence.

Changes are confined to the diagnostic instrument:

- A separate process owns serial input and CSV recording, independent of GUI
  redraw, pause, and the GUI's Python execution lock.
- Bounded blocking USB reads replace eager byte-at-a-time polling.
- A bounded, nonblocking queue carries display updates. When full, recording
  continues; display skips are counted separately and break the plotted line.
- A three-second silent connection triggers a counted port reopen. The next
  sample is marked discontinuous; no missing readings are fabricated.
- Firmware 1.1 batches sixteen readings in 128 bytes at 115200 baud (8 kB/s).
  The initial instrument sent 32 bytes per sample at 460800 baud (32 kB/s).
  Sampling remains 1 kHz. Actual timestamps, per-sample missed-slot counts, boot
  identity, sequence numbers, queue drops and CRC remain available.
- A CSV audit tool independently checks continuity; visual smoothness is not
  the acceptance criterion.

The earlier independent-process viewer at the higher baud still showed occasional
wire loss. The lower-bandwidth format is thus part of the tested configuration,
not merely a cosmetic change. We have not isolated all causes experimentally.

## Verification

Arduino ESP32 core 3.3.11, ESP32 Dev Module: build passed (279412 bytes program,
22204 bytes global RAM); upload hashes verified. Six automated tests passed,
including fragmented/corrupted BIR1 and BIR2 parsing, actual timestamp recovery,
missed-slot deltas, sequence wrap, diagnostic hysteresis, and continued CSV
recording with the display queue deliberately left full.

The live test uses `ir_usb_20260920_000537_550165.csv` under the local
`~/NGR/ir_usb_bench_logs` directory. It contains actual hardware data, not demo
samples. GUI screenshots confirmed a live trace, functional threshold controls,
and separate integrity counters. Final sustained-capture audit is recorded below.

Checkpoint: 199894 actual records spanning 199.893 seconds (over three minutes),
one boot, zero missing/nonforward sequence numbers, zero missed sampling slots,
zero queue drops, zero CRC errors. The single gap row is the initial capture
boundary, not an internal gap. Four ADC rail-valued readings were retained.
The GUI remained live; the observed display-skip and reconnect counters were zero.
This passes the initial sustained bench-capture gate for the tested configuration.
The viewer was left running, so the local recording extends beyond this checkpoint.

## Limits

This validates a measuring instrument over the observed run, not magnet/IR
navigation reliability or printed-insert pulse accuracy. Rail-valued samples and
spikes remain in the record. Do not remove them just to obtain the desired count.
Diagnostic thresholds are user-adjustable and recorded, not production logic.
Physical revolution count, illumination and sensor position still need operator
ground truth. No further wheel turns were requested during this repair.
