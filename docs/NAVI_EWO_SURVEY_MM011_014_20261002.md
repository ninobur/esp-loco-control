# Lowline MM011–014 surveyed geometry correction — 2026-10-02

Authority: David physically remeasured these intervals and supplied the exact
values below. They are measurements, not inferred, smoothed or normalized data.

| CW interval | Index | Previous mm | Corrected mm | Change mm |
|---|---:|---:|---:|---:|
| MM010–MM011 | 10 | 300 | 300 | 0 |
| MM011–MM012 | 11 | 290 | 335 | +45 |
| MM012–MM013 | 12 | 300 | 324 | +24 |
| MM013–MM014 | 13 | 315 | 360 | +45 |

All 171 interval entries previously summed to **52,150 mm**; the corrected
sum is **52,264 mm**, an increase of **114 mm**. No other interval or polarity
entry changes. The maximum interval becomes 360 mm; minimum remains 280 mm.

## Active source and convention

Integrated EWO's `NaviMapAdapter.h` includes
`firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/RouteMap.h`.
That active map defines `ROUTE_SPACING_MM[n]` as MM n→n+1 CW. `spanMm(n,+1)`
reads entry n; `spanMm(n+1,-1)` reads the same entry. Thus these physical
corrections apply in both directions without changing navigation algorithms.
Historical duplicate route tables are not edited.

## Reviewed build and controlled flash scope

Base: `ad89e67090d648054ede719a387403f3bd8937b4`, branch
`codex/ewo-pwm-zero-localization`. Sketch:
`firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/NAVI_EYES_WIDE_OPEN_INTEGRATED.ino`.
Its active X22 profile selects **Otto / 9950011**. Runtime sketch identification
remains `NAVI_EYES_WIDE_OPEN_INTEGRATED_R2_FT2`.

The reviewed FT2 startup and 0120 PWM-zero declaration-required behavior are
unchanged, including stationary dwell preservation, AUTO withdrawal, no automatic
Hall/IR recovery, and no 650-ms fallback. Station logic/locations, numbering,
polarity, tolerance, Missed Magnet, IR processing, Hall reference and reversal
are unchanged. The only source edit is the three route values and descriptive
map comments.

David authorizes an Otto-only stationary flash after the full sanitized EWO
suite, Python interoperability tests, actual Otto ESP32 compile, exact diff
verification, commit and push all succeed. Stop on failure or unexpected source
differences. No Toby/IR Test Car flash, AUTO run, Pi/dashboard/service change or
merge is authorized. This remains **NOT field accepted**; David performs the
physical field test. Rollback: reviewed pre-survey `ad89e670`; previously flashed
FT2 `ab0938b` also remains available.

## Pre-flash verification

- Full sanitized EWO host/regression suite: PASS, including unchanged FT2,
  PWM-zero declaration hold, operations/stations, IR authority and recorder tests.
- Python interoperability: 10 tests PASS, including actual AUTO withdrawal/
  admission functions and dashboard rendering.
- Array comparison against `ad89e670`: exactly indices 11, 12 and 13 changed;
  index 10 and all 171 polarities unchanged. Integrated sketch/core, server and
  tools are byte-for-byte unchanged from the reviewed base.
- ESP32 compile PASS (`esp32:esp32:esp32`, ESP32 core 3.3.12). Flash
  1,012,043 / 1,310,720 bytes (77%); static RAM 57,644 / 327,680 bytes (17%),
  leaving 270,036 bytes before runtime allocations.
- Compiler dependencies identify Otto's `LL_LocoConfig_9950011.h` and the active
  corrected RouteMap. The exact corrected 171-entry spacing table was also
  verified inside the compiled ELF, not merely in an unused source copy.
- USB-connected ESP32 MAC read as `68:09:47:ac:4e:b4`, matching Otto's previously
  verified device identity. Port for this session: `/dev/cu.wchusbserial110`.
- Compiled application binary SHA-256:
  `28c5d908be93ceabe9455c964ebc433b335ab8ea1f01c5cf45f1aa9e80795d34`.

At this record's commit, flashing has not yet occurred. The authorized upload
must use this committed source's verified build, followed by stationary boot,
Wi-Fi/MQTT, EWO telemetry and Type-5 reception checks. This document does not
claim those post-flash checks or field acceptance in advance.

## Stationary post-flash result (documentation-only follow-up)

Flashed firmware commit: **`17e8ed116e8efd90d5396f450885a0073916bf7f`**, pushed
before upload. Clean source and the application hash above were verified before
`arduino-cli upload --input-dir /private/tmp/ewo-survey-otto-esp32-20261002`.
Upload identified Otto's MAC and verified hashes of every written region,
including the application. No firmware source changed after that build/flash.

- MQTT captured the new boot ID **`76906767B3A7C6D7`**, sketch
  `NAVI_EYES_WIDE_OPEN_INTEGRATED_R2_FT2`, class
  `INTEGRATION_CANDIDATE_NOT_FIELD_ACCEPTED`, `field_accepted=0`.
- Serial confirmed Wi-Fi at `192.168.68.73`, MQTT CONNECTED/state 0; live
  connectivity reports identify broker `192.168.68.142` and IR radio ready.
- After David powered the Test Car, valid Type-5 sequence advanced 1483→1532
  while consumed IR observations advanced 404→451; `ir_applicable=1` and
  measured pulses remained zero. Later dashboard IR sequence was 1832, with
  measured speed 0 / STOPPED. Source MAC `38:18:2B:30:8C:2C` matched the stored
  display-only pairing. No IR Test Car change or command was made.
- PWM=0, AUTO=0, running=0, confirmations=0 and misses=0 throughout the checked
  samples. Navigation is undeclared after boot: dashboard UNSET/MM --/ready 0.
  No movement, declaration or AUTO-run command was sent.
- Dashboard receives fresh EWO nav/IR/readiness telemetry. Its deployed HTML
  supports readiness and the firmware warning line but lacks the newer dedicated
  `ir_distance_state` status rendering. Compatibility is partial at that display
  layer; no dashboard or Pi update was performed. Its old station/warning fields
  retain historical values and are not evidence of a new departure.

Unexpected observations, recorded without fixes or inferred causes:

- Rejected IR-wire counter increased (762→772→781 in consecutive diagnostic
  samples), even while valid Type-5 observations continued. IR queue drops=0.
- Post-boot Hall queue drops=50, legacy Hall recorder drops=3 and native Hall
  recorder drops=175 in the later sample; event drops=0 and publish drops=0.
- UDP failure count=91 in two connectivity snapshots. Capture completeness is
  not established; these counters require review before relying on field NSR1.

Otto was left stationary for David's physical run. Toby, the IR Test Car and all
Pi services were untouched. These checks are not field acceptance.
