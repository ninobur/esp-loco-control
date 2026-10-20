# NAVI_RVM_CONTINUITY_MANUAL_TOBY: the Toby (9950012) build of NAVI_RVM_CONTINUITY_MANUAL

**Status:** field-test firmware, made at David's request on 2026-10-19 to test
the shape navigator on Toby. **Not field accepted.** Navigation is observation
only, with no motor authority.

**What differs from Otto's build:** only `LocoConfig.h` and the hardware profile
`LL_LocoConfig_9950012.h` (copied unchanged from `firmware/config`).

| Setting | Toby | Otto |
|---|---|---|
| Loco name and id | 9950012 | 9950011 |
| Motor PWM pin | 4 | 4 |
| Motor direction pin | **2** | 16 |
| PWM frequency / resolution | 20 kHz / 8 bit | 20 kHz / 8 bit |
| Safe direction-change PWM | 15 | 15 |
| Battery thresholds (V): disconnected / shutdown / recovery / counter | 12.5 / 13.25 / 14.0 / 5 | as Otto's profile |
| Hall GPIO | 33 | 33 |
| INA219 I2C | 21 / 22 | 21 / 22 |

The Hall GPIO and I2C pins are as in Toby's NAVI_COHERENCE 0.6 firmware.

Every other header is byte-identical to Otto's build; `tests/navi_rvm_continuity/run_checks.sh`
checks this.

**Untested on Toby:**
- The shape model (`RvmShapeModel.h`) and the continuity constants (50 / 24 / 16)
  were built on **Otto's** Hall data.
- **Polarity:** N = ADC above the baseline. This is the convention of Toby's own
  NAVI_COHERENCE 0.6 detector (departure = raw − baseline; departure ≥ 0 → N),
  which matched the map 926/926 in his 20Q3 run 5.
- If the sign were wrong, the first recognized magnet would be a CONTRADICTION,
  with no claim made.

**Build:**

```sh
tools/navi_rvm_continuity/build.sh --toby --credentials-dir DIR --build-path BUILD
```

**Flashing:** the upload command is in
`docs/NAVI_RVM_CONTINUITY_MANUAL_DELIVERY_20261019.md`, with this sketch's
path.
