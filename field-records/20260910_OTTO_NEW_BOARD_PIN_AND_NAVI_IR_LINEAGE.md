# 2026-09-10 — Otto's new board is not in the NAVI profiles, and "NAVI_2" is QUORUM inside

Two findings, both from source and git, both blocking work that was about to be
authorized. Read-only; nothing flashed, nothing changed on the Pi.

---

## 1. BLOCKER — Otto's new-board pin fix is missing from the Stage 1 profile

Commit `4f6e0b0` (2026-09-09) records Otto's board swap:

> *"LL_LocoConfig_9950011.h: MOTOR_DIR_PIN 2->16 for Otto's new ESP32 board"*

The fix reached **two** trees out of eleven that carry an Otto profile:

| tree | `MOTOR_DIR_PIN` | |
|---|---:|---|
| `firmware/programs/QUORUM/` | **16** | fixed |
| `firmware/programs/QUORUM/variants/QUORUM_1_13X/` | **16** | fixed — this is why tonight's survey drove correctly |
| `firmware/programs/NAVI_ONE/variants/NAVI_ONE_STATION_CURVES/` | **2** | **the Stage 1 image** |
| `firmware/programs/NAVI_ONE/variants/NAVI_ONE/` | 2 | |
| `firmware/programs/NAVI_2/`, `NAVI_CL2/`, `NAVI/build/`, `MANUAL/`, `SENSORTEST/`, `QUORUM_1_13D/`, `firmware/config/` | 2 | |

Toby is unaffected — his board did not change and 2 is correct for him
everywhere.

**Stage 1 as specified would compile Otto's direction line onto GPIO 2 while his
board wires it to GPIO 16.** The H-bridge direction input would float and the
commanded direction would be undefined — a locomotive that may run the wrong way
through a station while navigation believes otherwise. On many ESP32 modules
GPIO 2 is also the onboard LED, so the symptom could be a blinking light and a
train that will not reverse.

**Stage 1's file-change list gains a third item:** Otto's NAVI profile takes
`MOTOR_DIR_PIN 16` with a comment pointing at `4f6e0b0`.

**Wider hazard, flagged not fixed:** nine trees still carry the stale pin for
Otto. Any of them flashed to him today is wrong in the same way. The per-tree
profile copies mean a hardware change has to be applied eleven times and was
applied twice. That is a librarian problem, not a one-file problem, and it is
the operator's call whether to sweep the rest.

---

## 2. The IR car integration exists only in QUORUM-navigator builds

**Task asked:** find the NAVI variant Toby used to integrate the IR test car.

**Found: `NAVI_2_0`.** `docs/research/20260829_A_CLEAN_LAP.md` records
*"Locomotive: Toby (9950012), `NAVI_2_0`"*, and
`firmware/programs/NAVI_2/` includes `IRSpeedWire.h`, the frozen 72-byte
`IrSpeedPacketV1` contract shared verbatim with the car's sender. The
recollection is correct.

**But it is not built on NAVI's navigator.** Counting the navigation machinery:

| | lines | `NAV_NO_QUORUM` | `Navigator.h` | `MagnetRecognizer` | `StationMachine` | `IrSpeedPacket` |
|---|---:|---:|---:|---:|---:|---:|
| `NAVI_ONE` | 1,386 | 0 | 2 | 3 | 1 | **0** |
| `NAVI_ONE_STATION_CURVES` | ~1,470 | 0 | ✓ | ✓ | ✓ | **0** |
| `NAVI_2` | 5,912 | **10** | **0** | **0** | **0** | 3 |
| `NAVI_CL2` | ~4,500 | **3** | **0** | **0** | **0** | 8 |
| `QUORUM` (for scale) | 5,783 | 26 | 0 | 0 | 0 | — |

`NAVI_2` and `NAVI_CL2` carry `NAV_NO_QUORUM`, `NAV_EVALUATING`, quarantine and
the evidence ring, and reference **none** of NAVI's components. They are
QUORUM's navigator under a NAVI name, committed 2026-08-29 — the same day
`NAVI_ONE` was created (`03d2945`) and superseded them.

**So the two things do not currently exist in one build:**

- the proven navigator (3,284 magnets, 76 stops, zero false positions) has **no
  radio at all**
- the IR car wire lives only in navigators the CTO brief explicitly excludes

Marrying them means porting `IRSpeedWire.h` and a source-validated ESP-NOW
receive path **into** NAVI_ONE. That is Stage 2 of the NAVI_CTO plan — adding
radio to NAVI as observation only — scoped to the car rather than to a peer
locomotive. It is tractable: the wire is frozen, the receive path validates the
sender MAC, and NAVI_ONE already has `serviceIr()` as an observation-only hook.

**It must not enter Stage 1.** Stage 1 exists to prove Otto's Hall model alone,
and pin 34 sampling was measured producing ~1.1 spurious Hall crossings per
second. Radio and IR arrive after the Hall model is accepted, not with it.

---

## 3. Correction to my own reading of the repeater

I offered the repeater's foreign-frame counter (~9.6/s) as a possible sign of the
car transmitting. **That was wrong**, for the reason the concurrent
determination `4e4e3b3` gives: the car **unicasts**, so the repeater's receive
callback never fires for its frames, and the car sends at 20/s not 9.2/s. The
foreign traffic is background. The repeater cannot witness a unicast car at all.

That determination's two blockers are independently verified here:
the car targets `B0:CB:D8:D0:FF:4C` (`ir_espnow_config.h:23`) which was Otto's
**old** board, and `4f6e0b0` proves the board was replaced. Otto's current STA
MAC is the one value that settles it, and it is printed at his boot banner.

## Evidence

- `git show 4f6e0b0` — `MOTOR_DIR_PIN 2 -> 16`, both Otto QUORUM profiles.
- `find firmware -name "LL_LocoConfig_9950011.h"` — eleven copies, two fixed.
- `docs/research/20260829_A_CLEAN_LAP.md:4` — Toby on `NAVI_2_0`.
- Navigator counts: `grep -c` over each sketch, table above.
- `firmware/programs/NAVI_2/IRSpeedWire.h` — frozen 72-byte contract.
