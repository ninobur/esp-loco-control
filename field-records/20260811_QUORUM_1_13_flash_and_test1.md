# QUORUM_1_13 — flash record and Test 1 result

**Date:** 2026-08-11
**Locomotive:** Otto, 9950011
**Flashed commit:** `411d846` (working tree clean of firmware at flash time)
**Build:** `QUORUM_1_13`, 983 171 B flash / 52 468 B RAM, 8 pre-existing
warnings, 0 errors
**Protocol:** `field-records/QUORUM_HARD_BOUND_ADVISORY_BETA_PROTOCOL.md`

---

## Pre-flight

| step | result |
|---|---|
| Locomotive identified | `/dev/cu.usbserial-0001` → `[BOOT] QUORUM_1_12C — 9950011` (Otto, pre-advisory build) |
| `LocoConfig.h` target | `LL_LocoConfig_9950011.h` ✓ |
| Image preserved | `field-records/firmware-images/otto_9950011_pre_QUORUM_1_13.bin` |
| | 4 194 304 B, sha256 `934097a5aa697540cddc80231c599d2819344b5ec2f695c43c441042f7633b83` |
| | `strings` confirms it contains `QUORUM_1_12C` — the rollback image is the right one |

A read at 460 800 baud failed with `Invalid head of packet`; 115 200 succeeded
(383 s). Use 115 200 for this cable.

**Uncommitted config found and resolved.** `LL_LocoConfig_9950011.h` carried an
uncommitted removal of `MISSION_ONLY_STATION "Arches"` dated 2026-08-08. The
2026-08-10 capture shows Otto arming all four stations (Patio 183, Grillers 178,
Bamboo 177, Arches 157), so it was already running — not a new behaviour.
Committed as `411d846` before flashing, so the beta capture maps to a real
commit. Flashing without it would have restored Arches-only and looked like the
advisory broke station arming.

---

## Flash

`arduino-cli upload --fqbn esp32:esp32:esp32:UploadSpeed=115200`
— 642 557 B compressed written in 57.4 s, **hash of data verified**.

Post-flash serial:

```
[BOOT] QUORUM_1_13 — 9950011
[BOOT] ready. Set session_direction, then start_mm, then auto, then GO.
```

Retained on the broker:

```
ngr/loco/9950011/state/bootid  {"sketch":"QUORUM_1_13","loco":"9950011",...}
```

Build attribution now works: one retained topic identifies the capture.

---

## Test 1 — null case on a non-HARD_BOUND reason: **PASS**

The Aug 10 ghost was still retained before the test, and is itself clean
provenance evidence — **no `adv` field**, because no build before this one
emitted one:

```json
{"e":"NO_QUORUM","mm":23,...,"mg":0,"ev":12,"ring":[["S",12],...]}
```

`mosquitto_pub -t ngr/loco/9950011/cmd/force_lost -m NOQUORUM` →

```json
{"e":"NO_QUORUM","mm":0,"lm":"","since":0,"dir":"UNSET","sc":[0,0,0,0,0,0],
 "ex":[0,0,0,0,0,0],"ld":null,"ru":null,"mg":0,"ev":0,
 "adv":null,"advw":12,"advr":5,"advn":0,"ring":[]}
```
```json
state/nav {"event":"NO_QUORUM","state":"NO_QUORUM",...,"reason":"FORCED_BY_FIXTURE"}
```

- `adv` is **null** on `FORCED_BY_FIXTURE` ✓
- `advw` 12 / `advr` 5 match the firmware constants ✓
- the advisory fields are present, so the payload attributes to this build ✓
- the Aug 10 ghost is replaced ✓

### Honest limit of this result

**`advn` is 0.** The locomotive had never been driven since boot, so the
evidence ring was empty. This null is over-determined: it would have been null
from the empty ring alone, with or without the reason gate. Test 1 therefore
confirms the payload contract and the build identity, but does **not** yet
isolate the HARD_BOUND-only scope on hardware.

That distinction is exactly why `advn` is published. Isolating the gate on
hardware needs a full ring — drive ≥ 12 markers, then force NOQUORUM again and
look for `advn:12` with `adv:null`. That is worth doing at the start of the
first driving session; it is the hardware counterpart of the bench case that
showed `adv:21` under HARD_BOUND and `adv:null` under `FORCED_BY_FIXTURE` from
the same ring.

---

## Test 2 — normal running unchanged: NOT YET RUN

Requires driving. Nothing to observe from a stationary bench. What to watch:
station stops, dwell, ramps and cruise indistinguishable from before;
AGREE/DISAGREE and the QUORUM_OPEN/TIED/ADOPTED/CLOSED sequence as before;
`mm/no_quorum` published only at a genuine terminal event.

---

## Current state of the locomotive

Otto is in **NAV_NO_QUORUM**, MANUAL, stationary, `auto 0` — left there
deliberately. It is the correct fail-safe state for a locomotive that has just
been reflashed and has not been positioned, and it blocks auto operation until
a declaration. Declare session direction and position at the start of the next
session as normal; that clears it.

Nothing was driven. The motor was never commanded.
