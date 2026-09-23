# 2026-09-10 — is the IR test car working? Determination, and two blockers found

**Asked:** determine whether the IR test car is producing useful data.
**Answer: it cannot be heard, and neither reason is "the car is broken."**
Read-only throughout — MQTT subscribe, ARP, and source inspection. Nothing was
flashed, published, or changed on the Pi or any locomotive.

---

## Blocker 1 — the car is unicast-addressed to a locomotive that no longer exists

`firmware/programs/IR_ESPNOW_SENDER/ir_espnow_config.h:23`

```c
static const uint8_t TOBY_STA_MAC[6] = { 0xB0,0xCB,0xD8,0xD0,0xFF,0x4C };  // Otto, bench-paired 2026-08-18
static constexpr uint32_t IR_TARGET_LOCO_ID = 9950011UL;   // Otto
static constexpr uint8_t  IR_ESPNOW_CHANNEL = 11;
static constexpr uint32_t IR_TX_INTERVAL_MS = 50;          // 20 reports/s
```

**Otto's board has been replaced since that pairing.** Commit `4f6e0b0`
*"Otto: new-board pin fix"* changes `MOTOR_DIR_PIN` from **2 to 16** in both of
Otto's QUORUM profiles. That is a physically different ESP32 module, and the STA
MAC is factory-programmed per module — a new board has a new MAC.

`B0:CB:D8:D0:FF:4C` does not appear anywhere on the LAN today.

**ESP-NOW unicast to a stale MAC is delivered nowhere and reports success at the
sender.** The car has no way to detect this. It is the exact failure mode the
1.16Ra instrumentation exists to expose — *"`esp_now_send()` returning `ESP_OK`
never means a peer received the packet"* — and here it applies to the car.

## Blocker 2 — nothing on the track is flashed to receive it

From the broker, `ngr/loco/+/state/bootid`:

| loco | sketch | online | can receive the car? |
|---|---|---|---|
| Otto 9950011 | `QUORUM_1_13X` | **1** | **no** — Hall-only survey build, zero IR, zero ESP-NOW |
| Toby 9950012 | `QUORUM_1_16R_IR_TEST_A` | **0** | has the code, but is off |

The one locomotive on the track is running the sketch that was chosen precisely
*because* it has no radio in it. Even with a correct MAC, nothing would hear the car.

## Is the car itself alive? Undetermined, and stated as undetermined

Every IR path on the broker is retained-offline, and there is no `telem/ir_speed`
or `telem/ir_status` topic present at all:

```
ngr/spoke/IR_SPEED_SENSOR/status/online   0
ngr/diag/irscope01/status                 {"online":0}
```

The bench repeater `ESPNOW_REP_2` is up on channel 11 and its foreign-frame
counter climbs steadily — 1,258,563 → 1,258,747 over 20 s, about **9.2 frames/s**.
**That is not the car.** Two reasons: the car sends at 20/s, not 9.2/s; and it
unicasts, so the repeater's receive callback never fires for it
(`ESPNOW_REPEATER.ino:238` — the callback only sees frames addressed to this
device or broadcast). The foreign traffic is background.

So the repeater cannot be used as a witness for a unicast car. **No listener on
the property can currently observe it.**

## What settles it — one value

**Otto's current STA MAC.** It is printed at his boot banner over serial
(`QUORUM.ino:5731`) and appears in the Deco client list. Neither the locomotive
nor the repeater publishes its MAC or IP to MQTT, so it cannot be read remotely.

A LAN sweep found exactly one Espressif-OUI host, `192.168.68.59 /
e0:5a:1b:2b:de:40`. **That is a candidate, not an identification** — the bench
repeater is also an ESP32 on the same network, and nothing distinguishes them
from off-board. A wrong MAC here produces silence indistinguishable from the
failure already being chased, so it is not worth guessing to save one look at a
boot line.

Once the MAC is known: one line in `ir_espnow_config.h`, re-flash the car.

## A finding that reshapes the next task

The task after this is *"find the NAVI variant Toby used to integrate the IR test
car."* Checking all five NAVI trees for the car's packet type:

| tree | `esp_now` | `IR_TEST_A` | **`IrSpeedPacket`** | `IR_FITTED` |
|---|---:|---:|---:|---:|
| `NAVI` | 9 | 7 | **5** | 0 |
| `NAVI_2` | 4 | 5 | **3** | 0 |
| `NAVI_CL2` | 1 | 4 | **1** | 0 |
| `NAVI_ONE` | 1 | 3 | **0** | 2 |
| `NAVI_ONE_STATION_CURVES` | 1 | 3 | **0** | 2 |

**NAVI_ONE cannot receive the towed car.** Its `IR_FITTED` symbol governs the
locomotive's *own* pin-34 wheel sensor, which is a different instrument. The car
integration lives only in the three older NAVI trees — and NAVI_ONE is the one
with the proven navigator (3,284 magnets, 76 stops, zero false positions).

That tension is the substance of the next task and is not resolved here.

## Evidence

- Car config: `IR_ESPNOW_SENDER/ir_espnow_config.h:14,16,17,23`; cadence `IR_ESPNOW_SENDER.ino:600`.
- Board swap: `git show 4f6e0b0` — `MOTOR_DIR_PIN 2 -> 16`, both Otto profiles.
- Broker state: `mosquitto_sub -h 192.168.68.142`, read-only, 2026-09-10 ~06:31Z.
- Repeater foreign counter: `ngr/survey/ESPNOW_REP_2/health`, 5 samples over 20 s.
- Repeater receive path: `ESPNOW_REPEATER.ino:238,430,504`.
- LAN: gateway 192.168.68.1 reachable as positive control; full /24 sweep, one Espressif host.

---

## Addendum, same night, later: the car is transmitting, and it is not the sketch
## this record assumed

The determination above was made without a listener. A listener then turned up:
the CP2102 on the Mac's USB was `ESPNOW_REP_2`, the bench repeater, site
`MAC_BENCH`, `192.168.68.81`, channel 11 — running, and printing its health line
to the console.

### What it counted

`ESPNOW_REPEATER.ino` increments `rxForeign` for every ESP-NOW frame it receives
whose length exceeds `sizeof(CtoPeerPacket)` (45 bytes). The console showed:

    HLT,5000,repeat,ch=11,src=0,ap=-36,foreign=34,drop=0,tx=0/0
    HLT,10000,repeat,ch=11,src=0,ap=-57,foreign=86,drop=0,tx=0/0

(86 − 34) / 5 s = **10.4 oversized ESP-NOW frames per second on channel 11**,
with `src=0` — no CTO-shaped source at all. Something is on the air that is not
a locomotive.

### What that rate identifies

`IR_SCOPE_ESPNOW_TX.ino` samples GPIO34 on a fixed 1000 µs schedule
(`sampleSeq*1000` µs, line 143) and ships `BATCH_N = 96` samples per packet.
That is 1000 / 96 = **10.417 packets per second**. Its `Packet` is exactly 250
bytes — far above the repeater's 45-byte foreign threshold, so every one is
counted and none is relayed.

Measured 10.4/s against predicted 10.417/s is a match to 0.2%. Both the frame
size class and the cadence agree. The IR test car is powered, sampling, and
broadcasting.

### This overturns Blocker 1, and it should be said plainly

The body of this record concluded the car was aimed at a locomotive that no
longer exists. That was true of `IR_ESPNOW_SENDER`, whose `ir_espnow_config.h`
still unicasts to `B0:CB:D8:D0:FF:4C`. It is not true of what is actually
running. `IR_SCOPE_ESPNOW_TX` sends to `BROADCAST` (line 19, used at line 159)
and adds the broadcast address as its only peer (line 193). A broadcast address
cannot go stale, so the board swap does not reach it.

`IR_SCOPE_ESPNOW_TX.ino` was last modified 2026-09-09 20:05 — the evening the
operator said he set the car up. The unicast sender was last touched 2026-08-28.
The mtimes agree with the radio evidence.

Blocker 2 stands unchanged: nothing on the track is flashed to receive this. For
a bench test that does not matter, because the raw recorder can.

### What is still not established

- **That the optics are producing useful counts.** A transmitting car and a
  seeing car are different claims. The batch cadence is driven by the sample
  timer and is identical whether the IR sensor is reading a wheel or reading
  nothing. Only the payload settles it — `pulses`, `runMin/runMax`,
  `thrHigh/thrLow`, and the 96-sample waveform inside each packet.
- **The 2026-09-10 ~06:31Z figure of 9.2/s** from the MQTT `foreign` counter is
  ~12% below tonight's bench figure. Consistent with frames lost at range, but
  it is not proof of anything and is not used here.

### The instrument for the next step

`firmware/programs/IR_SCOPE_ESPNOW/variants/IR_SCOPE_ESPNOW_RX` is the raw channel-11 recorder: it
prints every frame as `RX <millis> <rssi> <len> <crc16> <hex>` at 921600 baud,
and ACKs type-3 fusion reports. Compiled clean this session against
esp32:esp32:esp32 — 884,380 bytes flash (67%), 45,472 bytes RAM (13%). The
operator's own decoders `tools/ir_scope_espnow_to_csv.py` and
`tools/ir_scope_espnow_analyze.py` consume that format.

It has not been flashed. Flashing it to `ESPNOW_REP_2` would overwrite the
repeater firmware, which is a separate instrument the operator may want back; a
spare ESP32 avoids that trade.

### Interruption

Mid-measurement the CP2102 disappeared from USB entirely (no `/dev/cu.usbserial-0001`,
no CP2102 in `ioreg`), and `ngr/survey/ESPNOW_REP_2/online` went to `0`.
Unplugged or unpowered at the bench; nothing in this session can cause that.
Both repeaters now read offline.

### Method, addendum

Read-only throughout. Serial console read at 115200 with DTR/RTS deasserted;
baud identified by sweep. No MQTT publish, no command sent to either repeater,
no locomotive touched, nothing flashed.
