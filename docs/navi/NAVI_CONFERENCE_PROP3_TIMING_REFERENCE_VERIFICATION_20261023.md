# NAVI conference, Proposition 3 — verifying the historical timing reference

**Status: focused investigation only, in support of the architectural
conference. No redesign, no firmware changes, Proposition 4 not opened.**
Answers: what event do the historical timestamps actually represent, how
far is that from the magnetic peak, and does it change how the 69ms
figure should be read.

## 1. The timestamp event — verified from governing code, not assumed

**Otto's calibration corpus (`field-records/cal/*.txt`, June 29–30):**
produced by the SOLONAV lineage (`archive/SOLONAV_1_3_MEDIAN_GUARD.ino`
and siblings `SOLONAV_1_0`/`SOLONAV_1_2` — the log line format at line
2182 is an exact match for the cal file format). Its Hall state machine
(`serviceHall()` → `onMagnetEvent()`, lines ~2200–2290) is explicitly
closure-gated: a candidate only finalizes once `raw` re-enters the
baseline band and **stays there for `HALL_EVENT_EXIT_HOLD_MS` (20ms)**;
`dur = now - hallEventStartedAtMs` is the full open-to-close span, and
`now` at that moment becomes `lastMagnetTime`, which is what the cal
file's `dt` field is computed from. **This is RTB (return-to-baseline),
confirmed, not threshold-crossing and not peak — exactly as David
recalled, verified against the code, not just memory.**

**Toby's automatic-running logs (Sept 23/26,
`NAVI_COHERENCE_0_5_AUTO_ENABLED`, commit `2deadba7`):** a different
lineage entirely. Its Hall detection comes from
`firmware/test-programs/NAVI_ONE_X22/ExcursionDetector.h`, reused via
`HallObserver.h`. Its own comment is explicit: *"THE DETECTION INSTANT.
Polarity, the guard and the navigation event all happen here, exactly as
in X21."* — this fires the moment `persistSamples` (default **2**)
consecutive same-sign over-threshold samples accumulate against a
**locked** reference, i.e. essentially the onset side of the waveform,
the same shape as the familiar 70-count×2 criterion. `det_.detectedAtMs`
becomes `o.openedAtMs` in `Navigator.h::judge()`, which is what the
`ADVANCED` ruling and its timestamp are built from. **This is onset, not
RTB — the opposite reference point from the Otto corpus.**

**This is exactly the risk the assignment named: different NAVI versions
use different rules, and here they're not just different, they're on
opposite sides of the peak.** Otto's calibration `dt` is anchored *after*
the peak (overstates time-to-peak); Toby's automatic `dt` is anchored
*before* it (understates time-to-peak). They cannot be corrected with the
same sign or the same reasoning, and must not be blended.

## 2. Measuring the offset — what's available, and what isn't

**Toby:** `NAVI_COHERENCE_0_5_AUTO_ENABLED.ino` has no native-Hall
recorder of any kind — no reference to a sync/recording mechanism
anywhere in the sketch. The Sept 23/26 captures are decision-level MQTT
telemetry only (`state/nav`, `alert`, `state/station`), the same thing
Codex's script already parsed. **There is no continuous Hall waveform for
Toby's automatic runs to measure onset-to-peak against.** This is a real,
load-bearing gap, not a formality — confirmed by absence, not inferred.

**Otto:** real, native 5kHz Hall recordings exist from this project's own
work this week, and give a direct, computed answer — not RTB-to-peak,
which only matters for the Otto-corpus correction, but the full
onset/peak/RTB split, computed precisely (not eyeballed) from two
independent real waveforms already in hand:

| Case | PWM | onset→peak | peak→RTB | split |
|---|---:|---:|---:|---|
| Fresh run, MM45 (CW) | ~90 | 0.080 s | 0.080 s | 50.0% / 50.0% |
| Oct 22 run, MM44 (CCW) | ~37 | 0.520 s | 0.500 s | 51.0% / 49.0% |

**Both real waveforms split almost exactly in half around the peak.**
This is only two data points, both Otto, both the SOLONAV/line-vs-curve
shape family — not proof the same symmetry holds for Toby or for
NAVI_COHERENCE's own detector — but it's a real, computed, non-hypothetical
starting estimate, and the two independent cases (very different PWM, very
different absolute duration) agree closely.

## 3. Applying this to each dataset

**Otto's calibration ratios (the PWM 60/90 reproducibility work,
Proposition 1/2 evidence):** `dt` is RTB-anchored, so it overstates
time-to-peak by roughly **half the waveform duration** — at PWM 90,
roughly 0.03–0.16s (half of the 0.06–0.32s range already measured in
Experiment 3); at PWM 35–60, roughly 0.19–0.38s (half of 0.38–0.76s).
This affects *absolute* predicted arrival time. It does **not** obviously
affect the *ratios* this work actually reproduced (MM060-079/MM035-055):
if the RTB-vs-peak offset is roughly proportional to local waveform
duration, and waveform duration doesn't change drastically between two
nearby reference sections at the same PWM, the offset partly cancels in a
ratio of two similarly-biased quantities. That's a plausible argument,
not a verified one — I have not computed it directly per-section.

**Toby's automatic prediction (the 69ms MAE / 77.4% improvement
finding):** `dt` is onset-anchored, so it **understates** time-to-peak by
roughly half the local waveform duration. Using the Otto-based estimate
as the only available proxy (not Toby's own data — that's the stated
gap), this correction could plausibly be on the same order as the
reported 69ms MAE itself at Toby's automatic PWM, not clearly smaller.
**I cannot quantify this precisely without Toby's own waveform data.**

## 4. Direct answers to the five questions asked

1. **Timestamp event, identified from code:** Otto/SOLONAV = RTB.
   Toby/NAVI_COHERENCE(X22) = onset. Different, and in opposite
   directions relative to the peak.
2. **Peak-to-RTB (and onset-to-peak) measured:** done for Otto, two real
   waveforms, both splitting almost exactly 50/50 around the peak. Not
   done for Toby — no recording exists to measure it from.
3. **Relationship between historical timestamps and peak arrival:**
   established in direction and rough magnitude for Otto (RTB overstates
   by ~half the local waveform duration); established in direction only,
   not magnitude, for Toby (onset understates by an amount of plausibly
   comparable size to the reported accuracy, not yet measured).
4. **Does 69ms apply to the peak:** no, not as recorded — it's an
   accuracy figure for predicting Toby's onset-detection instant, not the
   magnetic peak. Whether the corrected figure is meaningfully different
   can't be answered without Toby's own waveform-duration data, which
   doesn't exist in the available records.
5. **Preserving the original finding:** the reproducibility results
   (1.86% CV, 69ms MAE on 248 held-out intervals, Otto's PWM 90 CCW
   ratios) remain valid and real *for the events each system actually
   recorded* — a confirmed, repeatable onset-to-onset (Toby) or
   RTB-to-RTB (Otto) timing pattern is genuine evidence of a repeatable
   physical process either way. What changes is only the *interpretation*
   of what point on the waveform that evidence predicts, not whether the
   underlying reproducibility is real.

## 5. What would close the gap

A continuous native Hall recording for Toby during ordinary automatic
operation, at the PWM settings used in the Sept 23/26 runs — the same
kind of recording already used for Otto throughout this investigation.
Nothing else in the existing record substitutes for it. Until it exists,
the Toby correction stays an estimate borrowed from a different
locomotive and a different detection architecture, stated as such.
