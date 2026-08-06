# QUORUM 1.7 — INA219 restoration: implementation report

Date: 2026-08-06
Author: Claude Code
Decision implemented: `docs/decisions/0012-restore-ina219-telemetry.md`
Plan step: CTO3_SPEC §12 step 2
Commit: `6daaa05`
Status: **built, not flashed.** Awaiting Sam/CODEX review per standing practice.

---

## What changed

`firmware/QUORUM/QUORUM.ino`, `SKETCH_NAME` `QUORUM_1_6` → `QUORUM_1_7`.
Filename unchanged (no version in filenames; SKETCH_NAME + git carry the
version). One commit, revertible on its own.

### 1. The INA219 service (restored from r12)

| Topic | Payload | Retain | Rate |
|---|---|---|---|
| `ngr/loco/<id>/telem/voltage` | `%.2f` bus volts | yes | 5 s |
| `ngr/loco/<id>/telem/current` | `%.2f` amps | yes | 5 s |
| `ngr/loco/<id>/telem/power` | `%.2f` watts | yes | 5 s |
| `ngr/loco/<id>/state/lowvolt` | `0`/`1` | yes | on change + connect |

Names, payload shapes, retain flags, the 5 s interval and the 14.4 V
threshold are r12's, unchanged, so the console's power tile binds with no
dashboard change. `LOW_VOLTAGE_THRESHOLD_V` and `INA219_TELEM_INTERVAL_MS`
live in the sketch as they did in r12 — both locomotives run 4S packs, and
the per-loco Blynk-era voltage constants still sitting in the
`LL_LocoConfig_*` headers remain dead config this sketch does not read.

### 2. `pwm` and `v` on the mm/marker line

The §9 speed model is keyed *segment × direction × PWM → pKPH*, normalized
by voltage. Both keys now ride the marker event that already carries the
interval:

```json
{"mm":107,...,"dt_conserve_ratio":1.02,"pwm":35,"v":15.42}
```

- `pwm` is `pwmActualAtDetect`, sampled at event **open** per R21 §3. The
  sketch already carried it on `MarkerEvent` and was discarding it.
- `v` is the most recent bus-voltage sample, read on the loop thread.

**This amends the R21 §5.1 marker payload contract**, which read "NOTHING
else." Flagged for CODEX. Size budget recomputed: worst case 174 → 198
bytes, buffer `b[320]`, transport `PubMsg` payload 512. The `v` field is
`%.2f` from a float bounded by the INA219's ~26 V range, so it cannot widen
unexpectedly.

**Note on the spec's wording.** CTO3_SPEC §9 asks for `v=<busVoltage>` "on
the mm/marker line alongside `pwm=`/`dist=`". Those `key=value` fields
belong to the SOLONAV-era line that the QUORUM JSON contract replaced;
`pwm=` and `dist=` are **not** present in 1.6. `pwm` is therefore added, not
merely joined. `dist=` was **not** restored — see "Deliberately not done."

### 3. Boot tolerance and threading

- `ina219Setup()` runs before `calibrate()`. A missing or faulted sensor
  leaves `ina219Available` false, prints one line, and the locomotive runs
  exactly as 1.6 did. **Otto's open INA219 fault is this case.**
- I²C is touched only from the loop thread: `setup()`, and
  `serviceInaTelemetry()` in `loop()` after `servicePwmRamp()`.
  `drainMarkers()` reads a cached float and never calls `Wire`. `hallTask`
  (the task that must never be late) and `networkTask` never touch the bus,
  so a slow or faulted I²C transaction cannot delay marker detection or
  MQTT service.
- Pins named explicitly (`I2C_SDA_PIN 21`, `I2C_SCL_PIN 22`) — the ESP32
  defaults r12 relied on implicitly. Named because pin 33 is the Hall and
  pin 34 the IR sensor, and an unnamed bus in that neighbourhood invites
  exactly the confusion CLAUDE.md warns about.

---

## Two defects found and fixed during implementation

Both were in my own first cut, caught before commit. Recording them because
both are the same failure mode — **a measurement that does not exist being
published as a value**.

1. **`v` reported 0.00 V before the first read.** The first
   `serviceInaTelemetry()` pass is up to 5 s after boot, and markers can be
   detected inside that window (a locomotive can be pushed or driven
   straight off the bench). The §9 table would have been seeded with
   intervals normalized against a zero denominator. Fixed with
   `inaHaveReading`: absent sensor *and* not-yet-read sensor both publish
   `"v":null`.

2. **`state/lowvolt 0` was published from a locomotive with no sensor.**
   The connect-time reseed ran unconditionally, so Otto — whose INA219 is
   faulted — would have announced "voltage is fine" on the authority of
   nothing. That is the unknown-reported-as-clear inversion CTO3 §0 and §14
   name as the one durable lesson of the CTO2 failure. Now gated on
   `ina219Available`: with no sensor the topic is left absent or visibly
   stale, which is a reason to look.

---

## Deliberately not done

- **No control path reads a voltage.** No PWM site, no command gate, no
  chamber logic consults `lastLowVolt` or `lastBusVoltageV`. `state/lowvolt`
  is a published fact. CLAUDE.md lists a low-voltage cutoff among the
  authorities that precede automation; 1.7 supplies the *measurement* that
  claim was missing, and stops there. Making it actuate is a separate
  decision that must first answer the bicameral question (0002/0013):
  a voltage-triggered stop reaching a MANUAL locomotive would be an
  automatic authority overriding a sovereign operator, and E-STOP already
  covers the operator's own emergency authority.
- **`dist=` not restored to the marker line.** The spec's phrasing implies
  it exists; it does not, and it is not needed for the §9 keying — the
  segment is `from-marker → to-marker`, which `mm` plus the previous line
  already gives, and `spanMm()` is deterministic from the surveyed
  spacings. Adding a redundant field to a contract this change is already
  amending was not justified. Flagged for review in case David or Sam
  wants it for the Pi-side parser.
- **No calibration-run support beyond the log format.** §12 step 4 is
  explicitly gated behind a proven route map.

---

## Verification

Built with the installed toolchain (`arduino-cli`, FQBN `esp32:esp32:esp32`,
core 3.3.11, libraries staged in the session scratchpad since `arduino-cli`
has lost access to `~/Documents/Arduino/libraries`):

```
Sketch uses 982459 bytes (74%) of program storage space. Maximum is 1310720 bytes.
Global variables use 52444 bytes (16%) of dynamic memory, leaving 275236 bytes.
```

Was ~952 KB (72%) / 15% at 1.6. Growth is the Adafruit INA219 + BusIO +
Wire libraries. `--warnings all` produces no new warnings from `QUORUM.ino`;
the pre-existing `-Wvolatile` warnings on `pubWindowCount++` and `cmdDrops++`
are untouched, per the standing instruction not to fix them inside a safety
change.

**Not field-verified.** Nothing is flashed. What a field test must confirm:

1. `telem/voltage` tracks a real pack under load, and the retained ghosts on
   the broker are overwritten within 5 s of connect.
2. `v` appears on marker lines, is `null` for the first few seconds, then
   real — and is `null` throughout on a locomotive with a faulted sensor.
3. `pwm` on the marker line matches the commanded PWM at that point in the
   lap (it is the value at event open, so it will lag a ramp — expected).
4. `loop_max_gap_ms` in `state/loopstat` is unchanged from 1.6. This is the
   number that would expose an I²C stall on the loop thread.
5. On Otto specifically: boot prints `[INA] unavailable`, no telem flows, no
   `state/lowvolt` is published, and the run is otherwise identical to 1.6.

---

## Open items this leaves

- **Otto's INA219 hardware fault** (0012 requires it resolved as part of
  this work). Firmware now tolerates it; the hardware repair is David's.
  Until then Otto cannot contribute to the §9 calibration table, and any
  calibration campaign is Toby-first.
- **R21 §5.1 amendment** — the payload contract change needs to land in
  `docs/QUORUM_v3_0_implementation_spec.md` once CODEX has reviewed it.
- **Low-voltage response semantics** — the flag now exists with nothing
  reading it. Same shape as the brake question in 0011.
