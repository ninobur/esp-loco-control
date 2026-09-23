# TX 1.5 live bench observation

The user's Arduino IDE upload was observed progressing from compilation through
flashing. Type-5 movement packets then appeared in the existing Pi raw capture
and passed the Python decoder's wire-format and CRC checks. RX reflash was not
required. Non-UTF-8 serial noise in the real log exposed and prompted a decoder
text-reading fix; packet CRC checks remain enforced.

An initial boot recorded 14 completed pulses. The operator explicitly confirmed
the wheel moved during handling, so those counts are not stationary false-pulse
evidence. A subsequent boot, 0x4d1e4080e443f121, was monitored separately.

For the subsequent bench window:
- 1393 decoded movement snapshots.
- Sensor timestamps 421971 through 140758178 us: 140.336 seconds observed.
- Completed pulses: 0 -> 0. Observed rises: 0.
- Saturation counter: 0. Sample-gap counter: 1 (not a zero-gap test).
- Reasons: 4 PRIMING, 1389 INADEQUATE_CONTRAST.
- No valid-distance claim; calibration remains unconfirmed.

This supports more than two minutes of zero spurious pulse growth in the
current bench setup after handling. It does not test sunlight, battery power,
known travel, slow departures, or physical distance accuracy. The setup was
being monitored after the instruction to leave it stationary; no additional
motion was reported during this window. Lighting was not independently measured.

Source: Pi `/home/david/NGR/ir_espnow/ir_espnow_raw_20260919.log`.
Local captured copy archived as
`field-records/logs/20260919_ir_tx15_bench_capture.log`.

Next physical test: independently counted full wheel turns, at an ordinary
rolling rate, bookended by stationary dwells; confirm the unchanged ten-spoke
96.52 mm circumference wheel first. Then stationary battery/sun and complete
speed/lighting matrix. TX 1.5's synthetic slow-cycle limitation remains open.
