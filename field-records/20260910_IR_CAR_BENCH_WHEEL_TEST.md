# The IR test car counts room light — bench trace, 2026-09-10

Branch `agent/toby-1-13-flash`. Car ESP32 on `/dev/cu.wchusbserial10`, flashed
`IR_SCOPE_ESPNOW_FUSION_TX_1_2` (operator-confirmed). Read-only: serial console
only, DTR/RTS deasserted, nothing flashed, nothing published.

## The verdict in one line

The car's radio, sampler and queues are faultless. Its optical channel is not
measuring the wheel. It counted 4,582 pulses in 177 seconds while the car sat
still on the bench.

## What is healthy, and it is worth stating

    STAT samples=274635 rawsent=2860 ... senderr=0 missed=0 late=12 pulses=2935 sat=0 q=0/0/0/0

- `samples` +5001 per 5.001 s — the 1 kHz schedule is exact.
- `rawsent` +52 per 5 s = **10.4/s**, identical to the foreign-frame rate
  `ESPNOW_REP_2` counted earlier tonight. Same box. The identification made from
  the repeater's counter is now confirmed at the source, not inferred.
- `missed=0`, `senderr=0`, `qdrop=0`, `sat=0`, `late=12` and static, all four
  queues empty. No transport fault of any kind.

## The trace

Operator sequence: hands off ~30 s, one slow revolution by hand, hands off
~30 s, then hand cupped over the sensor. Full record in
`field-records/logs/20260910_ir_car_bench_pulse_trace.log`. 38 five-second
windows:

- **minimum 1.40 pulses/s, maximum 53.99, mean 26.16**
- **zero windows at 0/s.** One window below 2/s.
- 54.0/s is **81% of the 66.7/s ceiling** imposed by `DEBOUNCE_US = 15000`.

## Why the acceptance test could not be run

The 2026-08-29 handoff sets the test: one revolution by hand must read 10. The
noise floor in a five-second window is ~130 counts. Ten counts cannot be seen
inside that. **The instrument is not merely wrong, it is not resolving.**

Nor is there a stationary baseline to measure against. A car that is not moving
must produce a flat counter. This one never stopped in 177 seconds.

## The mechanism, from the source

`IR_SCOPE_ESPNOW_TX.ino` builds its thresholds adaptively (line 148): a 256-bin
histogram over `ENV_N = 2048` samples — a **2.048-second** window at 1 kHz —
refreshed every 250 ms, taking the 5th and 95th percentiles as `runMin`/`runMax`,
then `thrHigh = runMin + 2/3 span`, `thrLow = runMin + 1/3 span`.

The only thing standing between that and free-running counting is

    static const int MIN_SPAN = 32;

**32 counts out of a 12-bit 4095 full scale is 0.78%.** Any modulation above
eight tenths of one percent of range clears the gate, and the percentile
thresholds then re-centre themselves on it. With no wheel turning, the recogniser
does not fall silent — it rescales onto whatever is left in the room and
fires. Room-light flicker is far above 0.78%.

The dips in the trace corroborate it directly: the rate collapses toward zero
(1.4, 2.6, 4.8, 6.4/s) in exactly the windows where a hand was near the sensor,
and recovers to ~30/s when the hand is withdrawn. **The detector is
demonstrably tracking ambient light.**

## What this would have done in service

Silently. The packets arrive, the CRCs pass, `missed` and `senderr` stay zero,
and `pulses` climbs at a plausible-looking rate. Every counter a consumer would
check reads healthy while the number it is there to produce is fabricated. This
is the IR analogue of the rule already in force for the Hall channel: a
detection that cannot be attributed must be refused, not counted.

## What is NOT established here

- **That the car fails with the wheel actually turning.** On the track the wheel
  modulation may well dominate the room. This trace says the detector cannot
  tell the two apart and has no floor that would make it try — it does not say
  the wheel signal is absent.
- **The correct value for `MIN_SPAN`.** Setting it needs the measured optical
  span of a turning wheel against the measured span of the room, and the console
  does not print `runMin`/`runMax`. Those live only in the ESP-NOW payload
  (`p.runMin`, `p.runMax`, `p.thrHigh`, `p.thrLow`, set at line 149). Tuning the
  constant without that measurement would be threshold-fitting, which is barred.

## The instrument for the next step

`firmware/test-programs/IR_SCOPE_ESPNOW_RX` prints every channel-11 frame as
`RX <millis> <rssi> <len> <crc16> <hex>` at 921600 baud. Compiled clean this
session: 884,380 bytes flash (67%), 45,472 bytes RAM (13%). It decodes through
the operator's own `tools/ir_scope_espnow_to_csv.py` and
`ir_scope_espnow_analyze.py`.

It needs a second ESP32 on USB — the `ESPNOW_REP_2` board would do, at the cost
of overwriting the repeater firmware. **Not flashed. Awaiting authorization.**

## Bearing on Stage 1

Stage 1 is Otto alone on NAVI_ONE with no CTO and no source change, and it does
not depend on the car. This does not block it. It does block using the car as a
speed reference for anything — including the coast measurement the spacing
ladder still wants.

## Method

Serial read at 115200 with DTR/RTS deasserted, pyserial. Rates computed from the
sketch's own `STAT` counters, differenced across its 5 s print interval and
normalised by the `samples` delta rather than by wall clock, so host scheduling
cannot bias them. Nothing flashed, no MQTT publish, no locomotive touched.
