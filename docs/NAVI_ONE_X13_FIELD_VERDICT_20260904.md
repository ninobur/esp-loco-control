# NAVI_ONE X13 rule, first railway run — field verdict

**Date:** 2026-09-04, 18:03 to 20:11
**Locomotive:** Toby (9950012)
**Build flown:** `NAVI_ONE_STATION_CURVES_0_3` "Station curve recorder". The
operator flashed the recorder image, not `NAVI_ONE_1_0X13_FIELDTEST`. The two
carry the same recognizer, capture and navigation (0074 + 0075); the recorder
additionally publishes every passage with its `wave_meta` line, which is why
the figures below are complete rather than sampled.
**Decisions under test:** 0074 (shape is diagnostic) and 0075 (a rising
reading is not a false start), both PROPOSED. This run is their field
evidence; it does not ratify them.
**Rollback:** X11 as flown (b7a1ff8). Not needed.

---

## Verdict in four lines

1. **Correct position maintained** for two hours in both directions: 3,284
   magnets accepted, zero DISAGREE, zero CONTRADICTED, zero warnings.
2. **Station stops and departures completed**: 76 of 76, 32 CW and 44 CCW,
   eight or more at each of Grillers, Arches, Bamboo and Patio in each
   direction. No unplanned stop.
3. **The failure this build addressed did not recur.** Sixteen Arches CW
   departure passages, every one accepted, MM111 residuals 0.072 to 0.095.
   Yesterday the same passages read 0.101 to 0.137 and stopped the train
   four times.
4. **Transport loss measured and absent**: 3,303 passages published,
   sequence 1 to 3,303 with no gap, `pub_drop` 0, `cmd_drop` 0.

## The acceptance test, item by item

| stated in the build record | result |
|---|---|
| every station stop, dwell and departure completes | 76 of 76 |
| no shutdown | none |
| AGREE on MM111 after each Arches CW departure | 8 of 8 |
| DISAGREE and CONTRADICTED stay at zero | 0 and 0 |
| `shape_refuse:1` events examined, not acted on | **none occurred** |
| ending condition: a strike on a passage with `shape_refuse:1` | not reached |

## What the archive says about the rule

**Passages the old rule would have refused: 0 of 3,284.** Every accepted
magnet also passed the shape test that no longer has authority. So this run
does not yet measure what 0074 costs; it measures what 0075 bought. With
whole flanks, the shape test had nothing to object to. The count that 0076
waits for, false advances admitted by the physical tests, stands at zero.

**Refusals: 19, all TOO_SOON**, the spacing guard refusing re-reads of a
magnet just passed, ratio 0.12 to 0.25. No TOO_WEAK, no other refusal.

**Amplitude:** lowest accepted ratio 0.618, first percentile 0.726. The
weakest magnet is still MM108 in the Arches zone.

**Residuals by station phase, accepted passages** (the diagnostic the
recognizer now only records):

| phase | n | median | max |
|---|---:|---:|---:|
| cruise / IDLE | 2,310 | 0.0675 | 0.1248 |
| approach | 380 | 0.0712 | 0.0880 |
| zone | 366 | 0.0720 | 0.0898 |
| zero ramp | 136 | 0.0728 | 0.0868 |
| departure | 92 | 0.0682 | 0.1154 |

Yesterday's departure maximum was 0.1372. Today's is 0.1154, and the phases
are indistinguishable from cruise at the median.

## What 0075 did, seen in the records

All sixteen Arches CW departure records were pulled from the mirror and
their pre-roll to first-sample step measured, as yesterday's six were.

| yesterday (recorder 0.1) | today (recorder 0.3) |
|---|---|
| steps of 35, 62, 74, 66, 46, 103 counts | steps of −2 to 6 counts on all sixteen |
| residuals 0.101 to 0.137 | residuals 0.072 to 0.115 |
| one refused, train stopped | all accepted |

The `discards` counter rose from 0 to 167 over the run, about two per
station stop. The branch is still discarding what it was built for, false
starts while stationary, and none of them touched a departure flank.

## One record to look at later, not to act on

Passage 1276, the last Arches CW departure, 20:0x, has a 53-count step a
few samples into its flank, past the pre-roll join, residual 0.0878, ratio
0.853, accepted. Fifteen of sixteen departures have no step above 9 counts.
It is one record and it did no harm. It is in the archive with its
`wave_meta` line and can be examined against the discard counter's timing
when there is a reason to.

## What this run does not settle

- The cost of 0074. No passage the old rule would have refused occurred, so
  nothing was admitted on the strength of the new rule. Whether an F09A- or
  0.341-class artifact will appear under the corrected acquisition is what
  the archive is now counting, run by run.
- 0075 under a departure from rest inside a magnet's field (the Arches CCW
  14:14:40 class). No such rest occurred today; the CCW Arches stops landed
  clear of MM106.
- Anything about the recovery matcher of 0076. That is a separate build
  after a count, and the count is presently zero.

## Files

- Log: `~/ngr-telemetry/pi/NGR/telemetry/all_20260904.log`, 18:03 to 20:11,
  boot at 18:03:38.
- Analysis was done on the mirror only; the serial port was not opened.
