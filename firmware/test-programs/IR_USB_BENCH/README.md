# IR USB Bench

Diagnostic instrument for the non-powered measuring wheel. Not navigation
firmware and not a replacement production detector. No motor outputs or radio.

## Purpose

Record uninterrupted raw GPIO34 waveforms while the wheel is stationary and
turning, to distinguish optical/electrical fluctuations from threshold logic.
The earlier MQTT scope suffered missing batches. A display alone cannot repair
those missing measurements. This instrument uses the existing USB cable.

## Run

1. Compile/upload `IR_USB_BENCH.ino` for ESP32 Dev Module on the IR car only.
2. Close Arduino Serial Monitor so it does not share the port with the viewer.
3. Run `python3 tools/ir_usb_bench.py --port /dev/cu.wchusbserial110` from the repo.

Viewer dependencies: pyserial and matplotlib. macOS uses its native plotting
backend. `--demo` exercises display/logging with synthetic data, never hardware
evidence. `--window` sets a 1-30 second view (default 10). CSV logs default to
`~/NGR/ir_usb_bench_logs`. Freeze affects the display only; recording and counting
continue. Closing the window stops recording. A separate process owns USB and CSV
logging; display stalls cannot block it. A silent connection is reopened after
three seconds, and reconnects are counted. Opening a port may reset some boards.

## Instrument contract

- GPIO34, 12-bit ADC, explicit 11 dB attenuation, nominal 1 kHz sampling.
- Actual microsecond timestamps; missed scheduler slots are counted, not filled.
- 32-batch queue (up to 512 samples); sampling never waits for serial. Overflow
  increments dropped by the number of samples lost.
- Version 1.1 uses USB UART 115200 baud: normally 16 samples per 128-byte batch,
  8 kB/s rather than version 1.0's 32 kB/s at 460800 baud.
- Little-endian BIR2 header: magic (4), boot ID (4), first sequence (4), first
  actual timestamp us (8), cumulative missed slots at first sample (4), dropped
  sample total (4), valid count 1..16 (2). Six bytes per sample: timestamp offset
  us (2), raw ADC (2), missed-slot delta (2). Pad unused samples with zeros.
  Final CRC16-CCITT (2; initial 0xffff, polynomial 0x1021) covers first 126 bytes.
  Flush a short batch before timestamp/delta overflow; never truncate time.
- The parser also decodes BIR1 recordings; live baud is now 115200.
- Sequence increments for every acquired sample, including queue drops. It does
  not increment for missed acquisition slots; the missed counter covers those.
- Parser resynchronizes after boot text/corruption. Sequence gaps, missed slots,
  queue drops and reboot break the plotted trace. CRC failures are displayed.
- Firmware contains no filter or pulse detector. Host low/high sliders implement
  a separate fixed-hysteresis diagnostic requiring low before high. It deliberately
  has no debounce or adaptive baseline: extra crossings must remain visible.
- Changing thresholds or selecting New count starts a new diagnostic count and
  disarms it until low is observed. This can omit an initial partial pulse.
- CSV includes raw samples, firmware counters, threshold settings, diagnostic
  edges, test IDs, CRC errors, and explicit discontinuities. No distance is claimed.
- Firmware boot changes clear the display and diagnostic count. A new CSV should
  be used for each independent physical configuration.

## Bench protocol

Keep the wheel in its normal bogie. Capture 15 seconds still, one slow revolution,
then a known ten-revolution run and another stationary hold. Confirm the physical
number of spokes instead of relying on historical wheel metadata. Record insert
material, paint/finish, lighting and mechanical changes separately. Freeze after
motion to inspect raw peaks and threshold crossings. Full samples remain in CSV.

Acceptance: stable live display, approximately 1000 received samples/s, no growing
queue-drop or CRC counters, and explicitly measured acquisition gaps. Then evaluate
count repeatability and stationary false counts. Compilation or a synthetic demo
does not prove optical performance, field reliability, or NAVI integration.

Audit a capture with `python3 tools/ir_usb_audit.py PATH_TO_CSV`. Counts alone
cannot establish physical revolutions. A display skip is distinct from wire loss:
the recorder continues saving samples when its bounded display queue fills.

Status: built and bench exercised; not field accepted. The initial version had
wire loss and a stopped reader. Version 1.1 isolates recording from the GUI and
reduces transport bandwidth. See the dated bench build/reliability reports in
`docs/`. No production promotion.
