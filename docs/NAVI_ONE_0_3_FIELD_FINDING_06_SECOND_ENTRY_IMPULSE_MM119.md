# NAVI_ONE 0.3 — Field finding 06

## The entry-impulse polarity latch recurred at MM119, sign-mirrored

**Date:** 2026-08-31, 11:01:15
**Locomotive:** Toby (9950012), NAVI_ONE 0.3, CCW, AUTO
**Sketch:** `agent/toby-1-13-flash` @ `84b1730`
**Status:** Observed. Not a decision. Nothing here has been ratified.
**Relation:** second occurrence of the defect in finding 05, ~15 minutes later.

---

## Sequence

| time | event | physical | reading |
|------|-------|----------|---------|
| 11:01:11.919 | AGREE, ADVANCED → mm 120 | MM120 (N) | obs N |
| 11:01:13.059 | **NOT_A_MAGNET / TOO_WEAK** | **MM119 (N)** | **peak 43, ratio 0.2216, obs S** |
| 11:01:14.149 | AGREE, ADVANCED → mm 119 | MM118 (N) | accepted under the lagged label |
| 11:01:15.191 | DISAGREE / WRONG_MAGNET / POLARITY_MISMATCH | MM117 (S) | struck against expected MM118 (N) |

Two passages of lag this time, against three at MM70. `MM117 S, MM118 N,
MM119 N, MM120 N` per `ROUTE_POLARITY`, so MM118's N matched the lagged
expectation and only MM117's S exposed it.

---

## The same defect, with the sign reversed

MM119 is surveyed **N**. The recorded passage's raw signed excursion is
**min −43, max +223** — a full-amplitude North field, in line with its
neighbours (204–252 in this dump).

A single **−43** sample at the entry crossing latched `pol_` to 0 (South).
Every subsequent sample was then negated on storage, so the real +223 North bell
was stored as −223 and never raised `peak_`, which stayed at the impulse's own
43. Ratio 0.2216, below the ~0.34 floor, `TOO_WEAK`, `NotAMagnet`, no advance,
lag, strike.

Finding 05's MM70 was the mirror image: a **+41** sample, true field South at
−183, latched North.

| | MM70 (10:46) | MM119 (11:01) |
|---|---|---|
| surveyed pole | S | N |
| entry impulse | **+41** | **−43** |
| true field peak | −183 | +223 |
| latched pole | N (wrong) | S (wrong) |
| reported peak / ratio | 41 / 0.1925 | 43 / 0.2216 |
| ruling | TOO_WEAK | TOO_WEAK |
| lag before strike | 3 passages | 2 passages |

Both impulses are 41–43 counts — three to five counts above the `entryMargin`
of 38 — and both are opposite in sign to the field that was arriving.

---

## The impulse is an artifact, not field physics

Both sit at **sample index 12**, which is the entry-crossing sample by
construction: `PRE = 12`, so the replayed pre-roll occupies indices 0–11 and the
sample that opened the passage is pushed at 12 (`HallCapture.h:81-88`).

At full resolution, with their neighbours:

```
MM70    -3  -6  -6  -6  -7  -7  | +41 |  -10  -4  -11  -13  -16  -23  -23
MM119   +3  +5  +4  +12 +14 +17 | -43 |  +20  +20  +19  +22  +23  +26  +28
```

A magnetic field is continuous; it cannot go −7 → +41 → −10 in two
milliseconds. These are single-sample electrical or ADC artifacts.

Two consequences follow:

1. **The field was already arriving, in the opposite direction.** MM70's trend
   through the impulse is −7, −10, −11, −13, −16, −23…; MM119's is +17, +20,
   +20, +19, +22, +23, +26… Both would have crossed the 38-count threshold
   within roughly ten milliseconds and opened the passage with the **correct**
   pole. The artifact did not create a passage that would not otherwise have
   existed — it opened the same passage a few milliseconds early and with the
   pole inverted.
2. **Artifacts of this size are routine on this channel.** Across the 22
   distinct passages now captured — 3,691 samples, 3.7 s of passage time —
   there are 15 single-sample deviations of 30 counts or more, the largest
   being 62, 60, 56, 53, 50 and 46. They are harmless inside a passage, where
   they perturb the shape slightly and nothing else. They are harmful only in
   the approach window, where the field is still below the entry threshold and
   a single sample therefore decides the pole.

**Not claimed:** any estimate of how often this occurs. A dump is produced only
when a strike happens, so entry-window artifacts that did not flip a pole are
invisible to us. Two occurrences in one session is a floor, not a rate. The
physical origin of the artifacts remains unknown, exactly as in finding 05.

---

## Effect on the candidate rules

Both candidates from finding 05 recover both events. Re-run over all 22
distinct passages captured today, taking the two known mis-latches at their true
poles:

| rule | correct | worst-case margin |
|---|---|---|
| extrema, `maxPos >= maxNeg` | **22 / 22** | 4.5 : 1 (MM70) |
| accumulated signed area | **22 / 22** | 310 : 1 (MM70) |

MM119's own margins are 5.2:1 on extrema and **449:1** on area. The second event
does not separate the two rules further, but it does not weaken the case for
area either: area remains the rule no single sample can defeat.

---

## What this changes

Finding 05 documented a single occurrence and could reasonably be read as a rare
coincidence. It is not. **This is the second stop from the same cause in about
fifteen minutes of running**, and in both cases the locomotive was halted, and
position surrendered, by an instrument defect rather than by anything wrong on
the track. Both markers involved — MM70 and MM119 — are healthy, full-strength
magnets whose published readings said otherwise.

The survey replay Codex asked for before adopting any remedy is now the item on
the critical path.

## No change proposed

Nothing here is a proposal and nothing has been decided. No firmware, threshold
or control was changed. Recognition thresholds remain untouched.

## References

- `docs/NAVI_ONE_0_3_FIELD_FINDING_05_IMPULSE_FLIPPED_POLARITY_AT_MM70.md`
- `firmware/test-programs/NAVI_ONE/HallCapture.h:33,60-90` — `entryMargin`, the
  pole latch, the pre-roll replay and the oriented-peak update
- `firmware/test-programs/NAVI_ONE/RouteMap.h` — MM117 S, MM118 N, MM119 N, MM120 N
- `~/ngr-telemetry/waveforms/waveform_20260831T110115_224_slot*.csv`
