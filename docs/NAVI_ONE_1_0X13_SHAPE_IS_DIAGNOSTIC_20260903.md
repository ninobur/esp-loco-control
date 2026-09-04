# NAVI_ONE 1.0X13 "Shape is diagnostic" — the field build, stated

**Date:** 2026-09-03, night
**Decision:** 0074 (revised tonight at the operator's direction; proposed, not ratified)
**Builds:** `NAVI_ONE_1_0X13_FIELDTEST` "Shape is diagnostic" and
`NAVI_ONE_STATION_CURVES_0_3`. Both compiled. **Neither flashed.**
**Operational goal, kept in view:** Toby completes normal running, controlled
stops, dwells and departures with correct position and without unnecessary
shutdowns.

---

## The five questions

1. **Observed failure.** Passage 1805, Arches CW departure, MM110 → MM111:
   correct pole, spacing 1.9 s, amplitude ratio 1.005, refused WRONG_SHAPE at
   residual 0.1372, navigation withdrawn, Toby stopped. The same rule stopped
   him at MM111 in the morning (0.1301) and three times on 2026-09-02.
2. **Rule that caused it.** The Gaussian residual ceiling as a refusal
   (`MagnetRecognizer.h`), and the X5 stop-episode widening that turns any
   refusal near a stop into a shutdown (`NAVI_ONE.ino`, the NotAMagnet case).
3. **Smallest change.** The recognizer accepts on time, amplitude and (in the
   Navigator) polarity and sequence; shape and archaeology are computed after
   acceptance and recorded. The stop-episode widening is removed.
4. **Complexity demoted.** WRONG_SHAPE, NO_CURVE and INSUFFICIENT can no
   longer occur as outcomes. The archaeology's rescue role is gone; its
   measurements stay. Nothing is added.
5. **Field proof.** Arches CW departures complete and Toby continues to MM111
   and beyond with position correct, on the same track where he stopped four
   times today.

## The build, stated

| | |
|---|---|
| **Single behavioural difference** | A passage that passes the time guard and the amplitude floor is a magnet. The residual and the two-sided archaeology no longer refuse, and an ordinary refusal near a stop is no longer routed to a shutdown. |
| **Deliberately unchanged** | Entry/exit margins, floor, 200 ms guard, 0.34 amplitude floor, the 0.13 constant (still computed against), polarity and sequence rules, stations, ramps, dwell, the stitch across a stop (0070), the refusal dump, the withdraw window, 0071/0073 acquisition, the 0075 discard condition. No quorum, CTO, IR. |
| **Diagnostics added, inert** | `Verdict.wouldShapeRefuse` and `Verdict.shapeOutcome`; on `mm/marker` the fields `shape_refuse` and `shape_outcome`; on the recorder's `diag/wave_meta` `would_shape_refuse` and `shape_outcome`. X13 also publishes the waveform of any passage the former rule would have refused, so the archive holds the record behind the diagnosis. Nothing reads these. |
| **Observed failure addressed** | Seq 1805 and the three other shape shutdowns of 2026-09-02/03, every one a real magnet. |
| **Operational acceptance test** | Repeated laps including Arches CW stops. Pass: every station stop, dwell and departure completes; no shutdown; `mm/marker` shows AGREE on MM111 after each Arches CW departure; DISAGREE and CONTRADICTED stay at zero; `shape_refuse:1` events, if any, are examined in the archive, not acted on. |
| **Rollback image** | X11 as flown (commit b7a1ff8), which the operator built and flashed himself. X12 (0075 only, commit e505ed0) is the intermediate. |
| **Condition that ends the experiment** | A false advance: a DISAGREE or CONTRADICTED on a passage with `shape_refuse:1`. That is the residual catching something the physical tests missed, and it reopens 0074 with a record to argue from. |

**A conflict with the one-change rule, stated.** Relative to X11, the last
build flown, X13 carries two behavioural changes: the 0075 discard condition
and this. The operator withdrew the requirement to prove 0075 separately on
the railway and asked for this build on top of it. If he would rather keep
one change per flight, X12 is 0075 alone and X13 on X11 is a one-line revert
of the discard condition; either is a minute's work and is his call.

## What replays say

**The 2026-08-28 survey, 2,799 records (gate 1).** No verdict changed. Every
one of the 154 non-primaries is still refused, on amplitude. The residual
never cast a deciding vote there.

**The bespoke station run, all 1,805 passages in field order.** One verdict
changed: seq 1805, WRONG_SHAPE → MAGNET, `shape_refuse` set. The five
TOO_SOON refusals stand. `would_shape_refuse` is set on exactly one passage
of 1,805.

**Passage 1805 itself.** Admitted on ratio 1.005, gap 1.9 s, pole N as
expected. The shape diagnosis still reads WRONG_SHAPE 0.1372 beside it.

**The adversarial fixtures.** Here the operator's premise meets its one
counter-example, and it is stated first:

- **Finding 09 A, 2026-09-01, a real record.** A latched baseline offset
  held Toby's reading about 40 counts up for 12 s while he stood still, and a
  blip at the end gave the passage a judged peak of 73: ratio 0.363–0.384
  against the 0.34 floor, residual 0.27. Shape refused it alone. Under 0074
  it is admitted, navigation advances one marker it should not, and the next
  passage, opposite pole, strikes: AUTO withdrawn, position STRUCK at MM31.
  **This is an observed non-magnet that the residual uniquely excluded.** It
  contradicts the premise that no such case exists. What it does not
  contradict is the diagnosis: the latch is an acquisition fault (finding 08,
  gate 8), the fault was found because the shape refusal exposed it, and the
  polarity chain still stops the train one marker later rather than letting
  it run. Registered in gate 12 as a carried risk, not asserted away. F09B,
  the nine-minute version, is still refused on amplitude.
- **The four synthetic non-magnets of gate 12 E** (DC ramp, electrical step,
  shoulder, double lobe), built to clear amplitude, time and pole by
  construction: the ramp is still refused; the step, shoulder and double
  lobe are now admitted. None has been observed on the railway. Registered
  as risks; that is the theoretical cost the operator asked to have stated.
- **Gate 13's archaeology on split adversarials** is unchanged: the function
  still says INSUFFICIENT or WRONG_SHAPE about them. It is now a measurement.

**Gates.** 1–14 green, 253 + 79 checks in the two changed gates; the only
assertions touched are the ones in gate 12 that encoded shape's authority
(F09A/F09B advance counts, section E "REFUSED"), each replaced by a
registered risk with the numbers. Gate 12's rig mirrors the firmware's
NotAMagnet routing.

**Compile.** X13 983,191 B flash, 64,508 B globals. Recorder 0.3 984,911 B,
64,588 B. Both 75%.

## Files changed for 0074

- `MagnetRecognizer.h` (both copies): shape block runs after acceptance;
  `wouldShapeRefuse`, `shapeOutcome` on the Verdict; header comment.
- `NAVI_ONE.ino`: Judged carries the two fields; `mm/marker` publishes them;
  waveform published when `wouldShapeRefuse`; `if (j.kind == 1)` replaces
  `if (j.kind == 1 || j.stopEpisode)`; kind 3 is no longer produced (the
  loop's kind-3 branch remains, unreachable, untouched).
- `NAVI_ONE_STATION_CURVES.ino`: the same, plus the two `wave_meta` fields.
- `tests/gate_interrupted.cpp`: rig mirrors the routing; the shape-authority
  assertions become risks.
- `tests/replay_station_run.cpp`: new, replays a recorder run in field order
  and prints every changed verdict.
