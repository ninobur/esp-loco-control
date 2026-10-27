# Toby field run, 2026-10-27 08:33: verdict after magnet correction

**Status:** field record. Observation only; it changes no decision, no firmware, and no map.

## Context

- Between Oct 25 and Oct 26, David levelled the ballast between MM055 and MM080, because Toby was slipping. He removed and replaced the magnets and did not check polarity afterwards.
- From Oct 26, MM066, MM067 and MM069 read with polarity opposite to RouteMap.h on both locos (`research/navi_matched_filter/REPORT.md` §2.2).
- On the morning of Oct 27, David corrected ("moved") the magnets, then ran Toby.

## Run

| Item | Value |
|---|---|
| Loco | Toby 9950012 |
| Build | `NAVI_POWER_OF_NORMAL_MANUAL_ONLY` (auto locked, not field accepted), recognizer `NAVI_CANDIDATE_70X2_RAW_PLUS_RTB_NO_SHAPE` |
| Boot ID | `96348B2C028EA090` |
| Operation | Manual. Session direction CCW, start interval 112–113, throttle 90 from 08:35:20, manual throttle-down and brake 08:42:14–08:42:25; the run ended near MM008 |
| MQTT log | `~/Pi_Log_Downloads/9950012_20261027_083340.log` (SHA-256 `62f65026a0e58a37833761448d2d37b4c157248cfc4a9f22124035d336a6334f`) |
| NSR capture | `~/Pi_Log_Downloads/navi_session_9950012_96348B2C028EA090_6479CB4A_20261027_083340.nsr` (SHA-256 `eb54a206f962e752985ca63ae6770612ad0d9b41da0f108a6d1805089c6077d1`): 530.5 s, 2,652,336 native samples, 0 CRC-bad records, 0 serial gaps |

## Observations: firmware, live

| Measure | Result |
|---|---|
| Encounters (`diag/navi_encounter`) | 276, all `CONFIRMED` |
| Nonqualifying / abandoned | 0 / 0 |
| Sequence | Every encounter was exactly the next mapped magnet (no break, MM112 → MM000 → MM170 → MM008); all 171 positions seen, about 1.6 laps |
| Polarity vs RouteMap.h | Agreed on all 276 |
| Peak amplitude | min 139.2, median 218.6 counts |
| Dropped blocks, onsets, nav events | 0 |
| Free heap | lowest 58,172 B |

The re-laid section, on both passes (08:36 and 08:40):

| MM | Map | Observed, pass 1 | Observed, pass 2 |
|---|---|---|---|
| 066 | N | N | N |
| 067 | S | S | S |
| 069 | N | N | N |

MM055–MM080 otherwise also agreed with the map on both passes.

## Observations: proposed waveform recognizer, offline replay

- Method: the recognizer from `NAVI_WAVEFORM_RECOGNIZER_DESIGN_20261027.md`, configured as `single240_p0.5` (level-only, |r| ≥ 0.7, |A| ≥ 70).
- Scoring: the research harness's route-anchored reference, which is not the template. The NSR capture was decoded unchanged, with no repair.

| Mode | Recognized | False | Duplicate | Unscored |
|---|---|---|---|---|
| Offline | 274 / 274 | 0 | 0 | 1 |
| Causal (live-equivalent) | 274 / 274 | 0 | 0 | 1 |

- The reference scores 274 magnets, two fewer than the firmware's 276. Its scored span (113.2–522.2 s) leaves out the edges of the run.
- The one unscored event falls outside that span.

## Interpretation

- The MM066/067/069 polarity reversal seen on Oct 26 is no longer present. This agrees with David's account: those magnets were replaced reversed during the ballast work, and have now been corrected.
- On this run, the current firmware tracked without error. The proposed recognizer, replayed, also found every scored magnet.

## Limits

- This is one run, on one loco (Toby), in one direction (CCW), in manual mode.
- It says nothing yet about Otto or about CW running.
- One clean run does not change the miss rate measured over the Oct 25–26 corpus. In particular, it does not settle the design's open decision 1: what the matcher should do after a miss or disagreement.
