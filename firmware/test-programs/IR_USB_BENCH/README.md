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
continue. Closing the window stops recording. A disconnect is shown as an error;
restart the viewer after reconnecting. Opening the port may reset some ESP boards.

## Instrument contract

- GPIO34, 12-bit ADC, explicit 11 dB attenuation, nominal 1 kHz sampling.
- Actual microsecond timestamps; missed scheduler slots are counted, not filled.
- 512-record queue; sampling never waits for serial. Overflow increments dropped.
- USB UART 460800 baud, 32-byte records (32 kB/s at nominal sample rate).
- Little-endian layout: magic `BIR1` (4), boot ID (4), sequence (4), actual
  timestamp us (8), raw ADC (2), cumulative missed slots (4), cumulative queue
  drops (4), CRC16-CCITT (2; initial 0xffff, polynomial 0x1021, first 30 bytes).
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

Status: built and bench exercised; not field accepted. Initial USB capture showed
occasional wire loss followed by NO DATA; sustained acquisition is not yet accepted.
See `docs/IR_USB_BENCH_BUILD_20260919.md`. No production promotion.
