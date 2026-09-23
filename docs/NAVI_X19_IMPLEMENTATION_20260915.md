# X19 — no-closure field-test build: implementation, compile, diff, regression

**Date:** 2026-09-15 · **Otto 9950011** · `NAVI_ONE_1_0X19_NO_CLOSURE_FIELDTEST`
**Status:** built, compiled, host-tested. **NOT FLASHED.** Awaiting the
operator's decision. **X18 is untouched.**

---

## 1. What was built

A separate sketch, `firmware/programs/NAVI_ONE/variants/NAVI_ONE_X19/`. X18 keeps its own
directory, its own tests and its own binary; nothing in it was edited.

One experiment: **replace closure-dependent Hall event framing with
local-excursion event detection.** Nothing else was redesigned, and the diff in
§4 is the evidence for that claim — `Navigator.h`, `RouteMap.h`, `Stations.h`,
`Ops.h`, `LocoConfig.h` and `LapBaselineController.h` are byte-for-byte
identical to X18, and `MagnetRecognizer.h` differs only in comments.

### The detector

`ExcursionDetector.h`, new, 492 lines. It replaces `HallCapture.h`, which is
deleted from X19 along with `TwoSided.h` (dead in X18 already).

```
L(t)        the RAW sample within the trailing 300 ms whose DISTANCE FROM THE
            REFERENCE is smallest
departure   raw(t) - L(t), signed
candidate   |departure| >= 70 for 2 consecutive samples
            -> 400 ms fixed window -> peak, polarity, widths
            -> 500 ms refractory FROM DETECTION -> search again
```

Absent by construction, and each one checked by `grep` in §5: exit margin, exit
hold, ±25 closure, any requirement to fall below the departure threshold,
local/waveform-return/peak-relative closure, PWM in any Hall decision,
decimation, and a duration floor.

**Polarity-aware, as instructed.** The minimum is a minimum of *distance from
the reference*, never of signed value. Written the other way it would be blind
to one pole — the single easiest way to get this wrong, and it is called out in
the header for that reason. Gate 3 in §6 checks both poles and, specifically, a
South magnet arriving on a persistent 90-count North shelf.

**The reference cancels.** `departure = raw(t) − raw(L)`, so a constant offset
subtracts out.

> **CORRECTED 2026-09-15, after the operator's challenge.** This paragraph
> originally went on to say the reference "survives only in choosing which
> trailing sample is quietest, and on a displaced but flat line every candidate
> for L is equally displaced." That reasoning was wrong, and the code built on
> it was worse than wrong: on a resting level displaced 112 counts, a magnet
> pulling back *toward* the erroneous reference was detected on its trailing
> edge with **an inverted pole**, across most of the genuine amplitude range.
> The selection has since been re-anchored on a resting level the detector
> measures for itself, and `baseline_` now appears in no reachable detection
> path. See `docs/NAVI_X19_LOCALREF_CORRECTION_20260915.md`; gate 11 makes the
> counterexample permanent. Everything below describes the corrected detector.

Gate 4 checks a 112-count offset — the size of Otto's bad prime on
2026-09-15 — and finds the peak matches the un-offset case within 2 counts;
gate 11 checks the far harder direction across the whole genuine amplitude
range.

### Polarity from the excursion, not the window

Decision 0064 forbids polarity from one sample and takes it from a signed sum.
X18 summed the whole *passage*, which ended when the signal returned. A fixed
window does not end, so it accumulates a long quiet tail and multiplies every
count of reference error by the window length — the replay measured a sign flip
at a **6-count** error at worst.

X19 finds the peak in the window from the median-of-three judgement copy
(decision 0065), takes the contiguous run around it that stays above 34% of
that peak, and sums **only that run**. Measured tolerance rises to 70 counts at
worst, 104 at the median. The pre-roll is excluded from both the peak search
and the sum.

### The 82 ms floor — not transplanted, not replaced

`NAVIFieldConfig.h` no longer defines `NAVI_PASSAGE_FLOOR_MS`. It defines
`NAVI_X18_PASSAGE_FLOOR_MS = 82`, referenced by nothing, kept only so a diff
shows a removal rather than a rename. No X19 code path consults it.

Instead every candidate reports two widths on `diag/excursion` —
`w_caliper_ms` (samples ≥25 counts from L) and `w_frac_ms` (samples ≥34% of
its own peak) — so 0085 can be re-derived from X19's own field evidence.
Measured conversions from the replay, recorded for that re-derivation:
`w_caliper / X18 duration ≈ 0.93`, `w_frac / X18 duration ≈ 0.83`.

`DetectorConfig::widthFloorMs` exists, defaults to **0 = disabled**, and is the
one place a derived number would go.

**This is the largest known risk in the build and it is worth stating in
numbers rather than adjectives.** §7.

---

## 2. Compile

```
arduino-cli compile --fqbn esp32:esp32:esp32 NAVI_ONE_X19

Sketch uses 966935 bytes (73%) of program storage space.  Maximum is 1310720.
Global variables use 67692 bytes (20%) of dynamic memory, leaving 259988 free.
```

No warnings. Against X18 built the same way (970571 / 60044): **3.6 kB less
flash, 7.6 kB more RAM.** The RAM is the 1152-sample raw ring plus a
960-sample record and its judgement copy, minus one slot of waveform window
(depth 6 → 5). 259 kB of DRAM remains.

---

## 3. Instrumentation

**Pre-roll is 512 ms at 1 kHz, retained and dumped around candidates.** A
record is 512 pre-roll + the detection sample + 400 window = **913 samples,
1826 bytes, 3 MQTT chunks**. This is the evidence September 15 did not have:
every dump that day began at the entry crossing with 12 ms of history, which is
why first detection could not be replayed at all.

**`diag/excursion` — one JSON record per candidate, accepted or refused.** The
primary field-test instrument. Carries the detection timestamp, raw value at
detection, the local reference L, the departure that triggered it, the
operative and shadow references (so drift is visible while detection keeps
working), peak signed and unsigned, polarity, the excursion sum and the sample
indices it used, both widths, pre-roll length and total samples, the
recognizer's outcome/refusal reason/ratio/gain/gap, whether the guard was
tested, the running suppressed-candidate count with the time and departure of
the last one, PWM (telemetry), MM, direction and station phase.

**Volume.** Publishing 1.8 kB of waveform for every magnet at cruise is ~2 kB/s
and would be a larger change to the transport than to the navigator, so — as
the brief permits — the ring buffer holds it and it is dumped around candidates
worth having: **every refusal, every width rejection, one accepted candidate
every 15 s so ordinary cruise is represented, the whole trailing window on
`withdraw()`, and on `cmd/dumpwindow`.** The JSON record is published for
**every** candidate and costs ~500 B/s.

**A payload-overflow bug of my own making was found and fixed before this
report.** The first draft of the excursion record had a worst case of 843 bytes
against a 704-byte transport — the 2026-08-29 failure exactly, where one field
too many makes every field disappear because `json.loads()` throws away the
whole line. Four derivable or always-zero fields were removed (worst case now
665 bytes, 39 to spare) **and** a runtime length check added: an oversize record
increments `exc_oversize` on the status line rather than being sent truncated.

**Reference validity is now measured.** `diag/acquisition` no longer carries
floor rejections (there are none) and instead publishes a one-shot **PRIME**
record: the value the prime settled on and the spread of the raw line across
the 2 s window. It is **reported, not gated** — a wrong prime is evidence to
collect, since detection no longer depends on the reference being right. On
2026-09-15 boot 5 primed ~112 counts wrong and X18 never said so; `[CAL] 2 s
baseline — keep clear of magnets` was a serial print, not a measurement.

**Host decoder:** `tools/x19_waveform_decode.py`. Format version 2, 57-byte
header, verified identical between firmware and decoder. It skips X18-format
records and counts them rather than mis-decoding them.

---

## 4. Diff against X18 — every behavioural change

```
IDENTICAL  LL_LocoConfig_9950011.h   LL_LocoConfig_9950012.h
IDENTICAL  LapBaselineController.h   LocoConfig.h
IDENTICAL  Navigator.h  Ops.h  RouteMap.h  Stations.h
CHANGED    MagnetRecognizer.h   comments only — verified by diffing the
                                non-comment text; no executable change
CHANGED    NAVIFieldConfig.h    the duration floor constant retired
CHANGED    WaveformDump.h       wire format v2
CHANGED    WaveformWindow.h     candidate records instead of passages
REMOVED    HallCapture.h   395 lines — closure lived here
REMOVED    TwoSided.h      510 lines — already dead in X18
NEW        ExcursionDetector.h  492 lines
SKETCH     NAVI_ONE.ino -> NAVI_ONE_X19.ino   +270 / -92 of 1359
```

### Behavioural changes, in full

1. **Event framing.** Entry/exit crossings of a global reference → departure
   from a trailing local minimum. No closure of any kind.
2. **Measurement aperture.** Variable-length passage → fixed 400 ms window from
   detection. Peak reproduces X18's exactly on 32 of 33 `dec=1` records; the one
   difference is +9 counts where the window outlived X18's early close.
3. **Measurement zero.** `entryBaseline_`, frozen at open → `L`, the excursion's
   own local reference. Under a DC offset this yields the *true* amplitude
   rather than amplitude ± offset.
4. **Polarity aperture.** Whole passage → the excursion only. §1.
5. **Duration floor.** 82 ms, enforced → none, measured and reported. §7.
6. **The 500 ms guard is now detect-to-detect.** X18's `MagnetRecognizer` guard
   is close-to-open. With no close, the only honest mapping is
   `openedAtMs = closedAtMs = detection instant`, which makes the guard the same
   interval the detector's own refractory already enforces. **It can therefore
   no longer refuse anything the detector admitted.** In X19 the refractory *is*
   the duplicate-count protection. `gapMs` is still reported, so the run
   measures the true detect-to-detect population. Do not read a `TOO_SOON` in
   X19 telemetry as evidence the guard is working.
   Decision 0083's post-stop successor path is inert for the same reason; the
   code is retained unchanged rather than deleted.
7. **Decimation removed.** Always 1 kHz, always 913 samples.
8. **Lap baseline: the estimator runs, the correction does not.** The central
   experiment is whether local-excursion detection makes reference drift
   irrelevant, and that cannot be observed while something is quietly
   correcting the drift. The reference stays where the prime put it;
   `applied` is computed, published and discarded (`applied_to_reference: 0`).
   The per-marker sample fed to the estimator is now the excursion's own `L` —
   a direct observation of the quiet line rather than the rolling median's
   opinion of it. That substitution is telemetry-only and untested.
9. **`openMigrateMs` is gone**, with the open passage it protected. So is the
   `openMigrateMs` expiry that let magnet samples into the rolling median after
   2 s — the defect written up in `field-records/20260915_OTTO_HALL_ZERO_SHIFT.md`
   §4a. It cannot recur: there is no open passage to migrate under.
10. **`adjustBaseline()`'s pre-roll bug is gone**, with the pre-roll it
    corrupted (§4b of the same record). X19's ring is RAW, so a reference change
    touches nothing but the reference.
11. **Frame reset arms a refractory.** X18 dropped an open passage on a
    declaration because a locomotive declared while parked in a field emitted
    that field on drive-off. X19 has no open passage, but a declaration made
    halfway across a magnet would otherwise let the second half re-trigger under
    the new target. `reset()` therefore arms a full refractory from the next
    sample. Gate 10.
12. **Telemetry.** New `diag/excursion`; `diag/acquisition` repurposed from
    floor rejections to the prime record; `diag/departure` reports `local_ref`
    and `local_depart` instead of `entry_baseline`, and `acquiring` instead of
    `open`; `state/nav` reports `win_ms`/`local_ref`/`reference`/`raw_detect`/
    `depart`/`w_caliper_ms`/`exc_n` in place of
    `dur_ms`/`base_open`/`base_close`/`raw_close`; the status line adds
    `exc_oversize` and reports suppressed candidates where it reported floor
    rejections.

---

## 5. Pre-flash checklist

| | check | result |
|---|---|---|
| 1 | compiles | **yes**, no warnings, 73% flash / 20% RAM |
| 2 | diff reviewed, every behavioural change identified | **§4**, twelve items |
| 3 | no Hall closure dependency in event framing | **confirmed.** `grep -niE "exitMargin\|exitHold\|quietSince\|closure\|floorMs"` over the whole sketch returns only the deliberately-disabled `widthFloorMs`, the boot record's `"closure":"none"`, and prose |
| 4 | PWM is telemetry only | **confirmed.** In `ExcursionDetector.h`, `mayAdapt` appears on exactly three lines: the `sample()` signature, the call it forwards to, and `updateBaseline()`. It reaches no departure test, window, peak, polarity, refractory, recognizer or navigator decision |
| 5 | 512 ms pre-roll actually retained | **confirmed.** Gate 7 asserts `preSamples == 512` and `sampleCount == 913` on a live record |
| 6 | polarity uses the excursion, not the whole window | **confirmed.** Gate 6 asserts the aperture is the arc and never reaches into the pre-roll |
| 7 | one persistent level cannot create a second candidate at +500 ms | **confirmed.** Gate 1 holds a 110-count step for **60 seconds** and gets exactly one candidate; the replay finds zero extras on the recorded 1.9 s, 2.8 s, 22 s, 25 s and 111 s fields |
| 8 | September 15 replay against the actual implementation | **PASS**, §6 |

---

## 6. Regression — the C++ detector against September 15

`tests/replay_x19_20260915.cpp` runs the **shipped header**, with the shipped
defaults, over all 52 complete `diag/waveform` records.

```
next-event targets found      : 11 / 11
unmatched, records dec<=4     :  0   (must be 0)
unmatched, records dec>=8     :  4   (reported, not asserted — see below)
persistent-field extra events :  0   (must be 0)
normal passages, exactly one  : 33 / 33
total candidates              : 65
candidates X18's 82 ms floor
  would have refused          :  5   <-- THE NO-FLOOR EXPOSURE

PASS -- implementation matches the tested algorithm
```

The implementation agrees with the Python replay the architecture was argued
from, including the two non-census candidates that replay also found with its
width screen off.

**The four unmatched candidates are all in one record**, the 16-second
hand-push at 11:29:43, decimated 32×. They are an artifact of the corpus, not
the code: a decimated record has to be replayed with a zero-order hold, so the
2-sample persistence test spans 2 ms of *held* value rather than 2 samples of
real signal, which makes the C++ strictly more permissive than the Python was
on that record. The 1 kHz data underneath was discarded on the locomotive in
2026 and cannot be recovered. They are reported and never asserted on.

**What this regression is not.** It is not acquisition coverage, it cannot
re-open the design question, and it cannot exercise first detection at all —
every X18 dump begins at the entry crossing, so the detector is handed a
synthetic quiet prefix and only the candidates *after* the first one are
meaningful. That limitation is the reason the 512 ms pre-roll exists.

`tests/gate_excursion.cpp` covers ten properties the corpus cannot show,
including the two September 15 has no instance of:

```
1  a 110-count step held for 60 s yields exactly one candidate
2  one candidate on the rise, none on a 1.2 s decay
3  N and S both detect; an S magnet on a 90-count N shelf is a second candidate
4  a magnet on a 112-count offset is still detected, peak within 2 counts
5  a second arc at +340 ms is suppressed AND counted; at +640 ms it is a candidate
6  the polarity aperture is the arc, never the window, never the pre-roll
7  preSamples == 512, sampleCount == 913, pre-roll is the quiet line
8  a 30 ms arc IS a candidate, and reports a width below 82 ms
9  five 1 ms spikes of 200 counts produce no candidate
10 a declaration mid-magnet does not re-emit that magnet under the new frame

ALL GATES PASS (0 failures)
```

Run both with `firmware/programs/NAVI_ONE/variants/NAVI_ONE_X19/tests/run_tests.sh`.

---

## 7. The no-floor exposure, in numbers

X19 has no duration floor, so short acquisition events that X18 refused will
reach the recognizer, and — if their amplitude ratio clears 0.34 — will advance
navigation. A spurious advance produces a polarity mismatch at the next pole
change and a one-strike withdrawal, so **this is the failure most likely to end
the run early.**

What 2026-09-15 says about the size of it:

* **24 floor rejections in ~2.5 hours.** Durations 12–81 ms, raw peaks 70–129
  against a gain near 175 — **ratios 0.40–0.74, all above the 0.34 amplitude
  floor.** Amplitude will not refuse them.
* Of those 24, **six occurred while the locomotive was moving** (pwm 49 or 91):
  18, 22, 24, 31, 39 and 76 ms. The rest are hand-push and dwell artifacts
  during periods when navigation was not advancing anyway.
* The 2-sample persistence test refuses only the single-conversion population
  (bench 2026-09-03: 1.1 in 1000 held still; two in a row is 1.2 in a million).
  It will not refuse a 20 ms event.
* The regression run flags **5 of the 65 candidates** with `w_caliper_ms < 82`.
  The sharpest is `12:49:17.921`: peak 91, ratio 0.52, and an excursion of
  **2 samples**. X19 would accept that and advance on two samples of polarity
  evidence.
* Not every floor rejection was noise. `13:20:55.277` — 76 ms, peak 101, pwm 91
  — was a **genuine magnet starved by a reference sitting 18 counts high**, and
  it is what caused the 13:21 strike. Measured from a local `L` its amplitude
  and width both recover, so X19 should count it correctly. That is the upside
  of the same change, and it is **SUPPORTED, not proven** — there is no
  waveform dump for that passage.

The brief said not to invent a replacement before the field test, and none has
been invented. If the operator would rather not risk it, the single change is
`detCfg.widthFloorMs` in `NAVI_ONE_X19.ino` — the replay indicates a value
near 75 keeps Otto's known 83 ms borderline magnet while refusing the 16 ms and
43 ms transients. **I have left it at 0 as instructed.**

Two further known consequences, both from §4: the 500 ms guard now refuses
nothing extra (item 6), and the reference is deliberately left free to drift
(item 8).

---

## 8. Failure attribution during the field test

If X19 misbehaves, `diag/excursion` should say which stage, without inference:

| symptom | look at |
|---|---|
| **initial detection** | the 512 ms pre-roll in the waveform dump — was the line quiet, and was `L` where it should have been? |
| **excursion framing** | `local_ref`, `depart`, `pre`, `n`, and the record itself |
| **refractory** | `suppressed_total`, `last_supp_ms`, `last_supp_depart` — a real magnet hidden by the refractory shows up here and nowhere else |
| **polarity** | `exc_sum`, `exc_n`, `exc_first`, `exc_last`, `peak_signed`. `exc_n` near the full window means the excursion degenerated to the whole record, which happens on a shelf and is where the polarity protection stops protecting |
| **recognizer / sorting** | `outcome`, `ratio`, `gain`, `gap_ms`, `guard_tested` |
| **navigation** | `state/nav` — `obs` vs `expected`, `mm`, `tgt`, `seq_at` |
| **startup / reference validity** | the `diag/acquisition` PRIME record: `prime_value`, `prime_spread`. Then `reference` vs `local_ref` on every candidate |
| **something unrelated** | `exc_oversize`, `pub_drop`, `q_drop` on the status line |

Safety behaviour is unchanged: one strike still withdraws AUTO, a contradiction
still stops the locomotive, and nothing manufactures position. The one safety
mechanism that *was* closure-dependent — the close-to-open rebound guard — is
documented in §4 item 6 as no longer refusing anything, rather than quietly
retained as if it still worked.

---

## 9. Field-test targets

Against the brief's six, and what the corpus already cannot answer:

1. **Normal cruise** — enough ordinary magnets to show one count each, correct
   polarity, navigation maintained.
2. **Actual slow movement**, especially pwm 25–40. **September 15 contains no
   record at all between pwm 24 and 40.** Not to be confused with stopped.
3. **Station stops** — Patio and Bamboo approaches, dwells, departures. Observed,
   not gated; there is no PWM gate in this build.
4. **A deliberate fringe-field stop**, if safely practical. The case motion
   gating was built for, and the case neither corpus contains: on 2026-09-15
   both dwells sat within ±12 counts, so the sensor was never in a field.
5. **Reference-offset conditions** — let natural Fixed/Shadow divergence happen
   and watch `reference − local_ref` while detection carries on. The lap
   correction is deliberately not applied so this can be observed.
6. **Opposite-polarity transition** — a magnet met while the local field has the
   opposite sign. Gate 3 shows the arithmetic handles it; no recorded instance
   exists.

---

## 10. Files

```
firmware/programs/NAVI_ONE/variants/NAVI_ONE_X19/ExcursionDetector.h        new
firmware/programs/NAVI_ONE/variants/NAVI_ONE_X19/NAVI_ONE_X19.ino           from X18
firmware/programs/NAVI_ONE/variants/NAVI_ONE_X19/WaveformDump.h             wire format v2
firmware/programs/NAVI_ONE/variants/NAVI_ONE_X19/WaveformWindow.h           candidate records
firmware/programs/NAVI_ONE/variants/NAVI_ONE_X19/NAVIFieldConfig.h          floor retired
firmware/programs/NAVI_ONE/variants/NAVI_ONE_X19/MagnetRecognizer.h         comments only
firmware/programs/NAVI_ONE/variants/NAVI_ONE_X19/tests/gate_excursion.cpp
firmware/programs/NAVI_ONE/variants/NAVI_ONE_X19/tests/replay_x19_20260915.cpp
firmware/programs/NAVI_ONE/variants/NAVI_ONE_X19/tests/fixtures_otto_20260915.txt
firmware/programs/NAVI_ONE/variants/NAVI_ONE_X19/tests/run_tests.sh
tools/x19_waveform_decode.py                                   host decoder
```

Evidence this build implements: `docs/NAVI_NEXT_EVENT_WITHOUT_CLOSURE_20260915.md`.
X18 remains at `firmware/programs/NAVI_ONE/variants/NAVI_ONE/`, unmodified, and is the build
to fall back to.
