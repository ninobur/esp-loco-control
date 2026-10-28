# NAVI minimal waveform recognition architecture — engineering design

**Date:** 2026-10-27 · **Author:** Claude · **For:** David and Sam's review before any coding
**Branch:** `docs/navi-waveform-recognizer-design-20261027`, based on `research/navi-matched-filter-20261026` (research commits `a52eb6a` and `f961ad6`, which are not changed here)
**Status:** design proposal only. No firmware, DNA map (`RouteMap.h`) or governing specification is changed. Nothing in this document has decision force until David approves it.

**Evidence labels used throughout.** Every figure carries one:

| Label | Meaning |
|---|---|
| **[M]** | Measured on the locomotives: firmware source or recorded telemetry |
| **[R]** | Offline replay of recorded native Hall data |
| **[S]** | Synthetic, built from recorded waveforms |
| **[E]** | Engineering estimate, not measured on hardware |
| **[P]** | Proposal |

**Sources reviewed:**

- **Research:**
  - `research/navi_matched_filter/` (REPORT, harness, ledger, manifest, residual and overlap figures, stress and latency results);
  - the 945-curve study (`research/navi_wolf_pack/.../curve_shapes/`);
  - Muse's three Oct 26 reports.
- **Fielded firmware:** `NAVI_POWER_OF_NORMAL_MANUAL_ONLY` on `experiment/navi-manual-frozen-baseline-20261026` (`621a029`). Both locos ran it on Oct 25–26 [M: `state/bootid`].
  - *Correction, 2026-10-27:* this is wrong. `621a029` was committed Oct 26 20:30, and no log shows it running on a loco: its `state/nav` fields `timing_lower_ms` and `timing_calibration_ready` appear in no Oct 24–27 log except the NAVI_WAVEFORM_0_1 boots. The Oct 25–26 runs, and the Oct 27 morning Toby run, used earlier commits of the same sketch. `state/bootid` names only the sketch. See `NAVI_70X2_DIAGNOSTIC_REPLAY_20261027.md` §8.1.
- **Telemetry:** the Oct 25–26 `state/loopstat` logs in `~/Pi_Log_Downloads`.
- **Earlier IR ruling:** `docs/NAVI_NON_IR_ACQUISITION_PWM90_20261021.md` (commit `348514e`).
- **New analyses for this document:** `research/navi_waveform_design_20261027/design_analysis.py` with its output.

---

## 0. Summary

1. **Recognizer.** Use the simplest configuration the evidence supports. The pipeline is:
   - per-tick median of the 5 Hall reads;
   - 2 ms averaging;
   - **one template scale (240 ms) with a 0.5·D level-only window**: 241 samples, 0.96 s;
   - normalized correlation **|r| ≥ 0.7** and fitted amplitude **|A| ≥ 70**;
   - one event per local maximum, with a 120 ms hold.

   On held-out recordings (Otto Oct 26 + Toby) this recognizes **5,999 / 6,027 (99.54 %), 8 false, 1 duplicate**, in causal emulation [R]. The six-scale research bank gives 6,002 / 6,027 with 13 false [R]. A scale bank buys nothing measurable on this railway.
2. **Cost.** The recognizer needs about **3 KB of RAM** and **about 120 k multiply-adds per second** [E]. The measured worst heap low-water on the current build is **36.8 KB** [M]. It fits on Otto and on Toby. The full bank (~33 KB) would not fit safely.
3. **Timing.** The event's *passage timestamp* (template-peak sample time) agrees with the waveform extremum to a median of 0 ms and a 95th percentile of 6 ms [R]. That is about 1.5 mm at PWM 90.

   The *decision*, however, is available a fixed **359 ms after passage** [R]. That is a median of **84 mm** of travel at PWM 85–95, a 95th percentile of 103 mm, and up to 233 mm above PWM 95 [R]. Geographic arithmetic should use the passage timestamp. Anything that acts on "where am I now" has to accept that the observation is ~0.36 s old.
4. **Output contract.** Each event carries a sequence number, passage time, decision time, polarity, r, A and a gap flag. The recognizer never assigns a mile marker and never consults the map.
5. **Matcher.** The matcher is the existing single-expected-target idea, with one change: **every recognized event becomes an explicit observation with an explicit geographic outcome**, including "location expected, polarity disagrees with map".
6. **The central open decision is the matcher's response to a miss or to a polarity disagreement.** Replay shows the following [R]:
   - **Next-only rules** of any kind (Proposition 8 included) stay displaced after one miss or one map disagreement. In the Oct 26 validation set: 829 correct advancements, 603 incorrect.
   - **Timing-based alternatives** suppress thousands of genuine encounters as soon as speed changes.

   I do not propose a rule. I propose a bounded track test and present the measured options (§4.4, §9).

   **Added 2026-10-27, experimental:** a minimal map-aware matcher (§4.5). It decides on current evidence, retains recent observations, and may revise. On held-out replay it made 5,834 correct and 132 wrong assignments, against 3,365 / 1,332 for P8 [R]. Its penalties are not validated, and it is not an approved navigation rule. The October 21 hold rule stays established. The map-aware matcher is to be tried on the track beside it.
7. **IR is not in this design.** David's ruling of 2026-10-21 excludes IR from the current stage, and the fielded firmware contains no IR path [M]. The assignment asks how IR *can* support matching. §5 answers that only so David can decide whether to re-admit it.

---

## 1. Architecture overview

```
 hallTask (core 0, prio 3, 1 ms tick)       rvmTask (core 0, prio 1)                loop/net (core 1)
 ┌────────────────────────────────────┐     ┌──────────────────────────────────┐    ┌───────────────┐
 │ 5 × analogRead per tick  [M]       │     │ WaveformRecognizer               │    │ publish obs.  │
 │ median-of-5  → 1 kHz               │ ──▶ │  ring of 500 Hz samples          │──▶ │ + nav state   │
 │ pair-average → 500 Hz sample       │ SPSC│  r, A at each new sample         │obs │ (MQTT)        │
 │ (+ NSR recorder, unchanged)        │ ring│  peak + hold → MagneticEvent     │ Q  └───────────────┘
 └────────────────────────────────────┘     │                                  │
                                            │ GeographicMatcher                │
                                            │  last confirmed MM, next MM, dir │
                                            │  event → Observation + outcome   │
                                            └──────────────────────────────────┘
```

| Component | Responsibility | Evidence that justifies it | Why it is necessary |
|---|---|---|---|
| Tick median + 2 ms average | Remove single-sample spikes; produce a uniform series | Spikes of 69–97 counts at ~1 per 35 s (Muse); median-of-5 makes the recognizer immune to spikes of up to 3 adjacent samples [S] | Without it a spike *is* a short bump |
| WaveformRecognizer | Decide *that* a magnetic encounter happened, its polarity and its passage time | 99.5 % recall, 8 false on 6,027 held-out magnets [R] | It replaces the 70×2 sentinel, the baseline tracker and the return test, all of which need a frozen level |
| GeographicMatcher | Decide *which* mapped magnet the encounter corresponds to and whether to advance | Single expected target already in `NaviTarget.h` [M] | Geography is the only place map knowledge belongs |
| Observation record | Keep every event and its geographic outcome visible | MM066/067/069: correct observations, wrong map [R, M] | Without it a map disagreement is indistinguishable from a recognizer fault |

**Removed, relative to the fielded build** [M → P]:

- `NaviCandidate` (70×2 onset against a live baseline);
- `NaviBaseline` (frozen median at the gate);
- `NaviEncounter` (observed return to baseline);
- the gate-driven reset of the candidate detector (`sharedIdentificationGate`).

Nothing in the recognizer depends on a baseline that exists before the waveform.

**Kept:**

- `NaviTarget`'s single expected target;
- `RouteMap.h`;
- declarations and suspension;
- the NSR recorder (it must keep recording native samples for replay);
- E-stop and battery protection.

Whether `NaviTimingPredictor` stays is part of the open decision in §4.4.

---

## 2. Signal-processing pipeline

### 2.1 Input sampling (A)

**Fielded acquisition** [M]:

- `hallTask` runs every 1 ms tick (`vTaskDelayUntil(&wake,1)`) on core 0 at priority 3.
- Each tick it takes 5 `analogRead`s about 46 µs apart. That is 5 kHz "native", but in bursts.
- Each sample is timestamped with `esp_timer_get_time()`.
- Recognition currently receives only 20 ms block means (`Tracing`, `BLOCK_US = 20000`) through `blockQ`.

**Proposed** [P]:

- **Per tick, `m = median(5 reads)`.** The median is stateless and per tick, so the 5-read burst is exactly the median window. A 1 ms bin can never be represented by a single sample — the failure Muse found in binned medians.
- **Every second tick, `y = (m_prev + m)/2`, timestamped at the midpoint.** This gives the 500 Hz recognizer input.
- **The NSR recorder and the 20 ms tracing stay unchanged**, for replay and diagnostics.

**Parity check.** This per-tick conditioning was replayed on all held-out data. It gives the same result as the research's sliding median: **5,999 / 6,027, 8 false, 1 duplicate** [R, `design_analysis.py` §2].

**Why 500 Hz.**

- The fastest magnet observed has a FWHM of about 80–90 ms (PWM 90; Muse) [R]. The 240 ms template core is 120 samples at 500 Hz. 250 Hz would still give 60 samples. Lower rates were **not** replayed.
- 500 Hz is the evaluated rate. Halving it would halve CPU but needs its own replay before adoption.

**Effect of filtering on shape and latency.**

- The tick median adds at most 1 ms of delay; the pair average adds 1 ms.
- The magnet waveform is at least 40× wider than either, so the shape is unaffected [R: identical recognition].

**Missing or irregular samples** [P]:

- Each 500 Hz sample carries its tick timestamp.
- If the hallTask tick that should produce the next sample is late by more than 2 ms, the missing samples are filled by linear interpolation, up to 20 ms (10 samples).
- A longer gap marks every window touching it invalid: no score, no event. The gap is reported in the observation stream.

The only gaps in the recordings are NSR transport losses: `nsr_native_drop` was 100–245 per session [M], and 0.3 % of recorded ticks were spaced more than 2 ms apart [R]. Acquisition-side gaps on the loco are **not measured**. The recorded data cannot separate them from transport losses, so a tick-lateness counter is part of validation (§8).

Of Toby's 12 replay misses, 9 are windows invalidated by recording gaps [R]. On the loco those samples would exist.

### 2.2 Normalization (B)

Each window is normalized only by its own mean: x′ = x − mean(x over the 241-sample window). Nothing is carried from one magnet to the next. There is no frozen level, no return test and no gate-time freeze.

| Condition | Behaviour | Evidence |
|---|---|---|
| Constant ADC offset | Exactly invariant (cancels in x − x̄) | ±100 counts: identical results [R] |
| Ordinary slow drift | Negligible inside a 0.96 s window | 100-count sine, 10–60 s period: −0 to −2 of 2,008 [R]; ramp 5 counts/s: −7 [R] |
| Faster drift | Degrades | ramp 20 counts/s: −35; 100 counts/s: −185 and +45 false; 3 s sine: hundreds false [R] |
| Signal-amplitude change | r ignores amplitude; A measures it | recognized |A|: Otto p1 117 / median 169; Toby p1 142 / median 212 [R] |
| Abrupt background step | A step *alone* scores low; a step up and back down within ~0.3–1 s looks like a bump | ±100 steps: −19 and +31 false; ±200: −89 and +40 false [R]. Field false events are mostly such steps and bursts [R] |

**Rejected: level + slope removal.** It looks more drift-tolerant on paper. In practice it creates opposite-polarity sidelobe events about ±175 ms from every magnet [R]: 23 false vs 8 on validation at the best setting, and about 2,400 at loose settings. It is not proposed.

### 2.3 Template matching (C)

**Template representation** [P]:

- The mean of the 945 oriented, peak-normalized Oct 15 curves (128 points, departure → return) [R].
- Resampled once at boot to the 240 ms core: 120 samples at 500 Hz, padded with 60 zeros each side, L = 241.
- Then made zero-mean and unit-norm: T̂.
- Stored as `float[241]` (964 B) or generated at boot from the 128-point shape (512 B constant).
- The template is the same for Otto and Toby. Toby was evaluated with Otto's template: 99.70 %, 0 false [R].

**Polarity.**

- One positive-oriented template is used. Polarity = sign(⟨x′, T̂⟩) = sign(A).
- On both locos, ADC above level = North [M: `expectedHallSign`].
- Polarity is **never** an input to recognition.

**Correlation, at each new 500 Hz sample** with the window centred on sample c:

```
S1 = Σ x        S2 = Σ x²        (running sums, O(1) per sample)
D  = Σ x·T̂     (241 MACs per sample)
var = S2 − S1²/L
r  = D / sqrt(var)                       // Pearson; T̂ already zero-mean, unit-norm
A  = D / ‖T − T̄‖                        // least-squares template amplitude, counts
```

Running sums must be computed in double precision, or as int32 on integer ADC values re-based every window, to avoid cancellation. This is an implementation note, not a design choice.

**Amplitude.** A is the fitted peak excursion in counts that best explains the window. It needs no prior baseline, because the level is fitted in the same window.

- **0.7 and 70 are research-derived defaults**, selected on Oct 25 Otto data only [R]. They are not universal constants.
- Dev sweep, at the research window: recall is flat for amplitude floors of 50–110 and loses 31 real Otto magnets at 130 [R]. Parked false events fall from 41 to 7 when the floor is applied [R]. Raising r to 0.8 removes about 2 false events and costs about 3 magnets per 1,700 [R].

**Candidate → event (consolidation)** [P]:

- A sample c is a **peak** if:
  - |r(c)| ≥ 0.7;
  - |A(c)| ≥ 70;
  - the window is valid;
  - |r(c)| ≥ |r| of every sample within ±60 ms (0.25 s·D; 30 samples each side).
- It becomes a recognized event once the 30 following samples are known and none is higher. That is the "hold".
- Repeated correlation maxima from one magnet are consolidated by that ±60 ms rule. The one magnet whose score has two local maxima within 120 ms keeps only the higher.
- Research NMS was ±0.5·D = ±120 ms. The design table below uses that value. **±60 ms is not yet replayed**; the latency it would save is noted in §6.

**Measured duplicates:** 1 in 6,027 held-out magnets with the evaluated rule [R]. The polarity-flip sidelobes of the level+slope window do not occur with level-only normalization [R].

**No timing gate is used anywhere in recognition.**

### 2.4 Scaling (D)

Held-out = Otto Oct 26 + Toby, 6,027 reference magnets [R, `design_analysis.py` §1]. "Template words" is RAM for float templates [E]:

| Configuration | Offline recognized | Causal recognized | Causal false / dup | Decision latency after passage | Template words |
|---|---|---|---|---|---|
| **1 scale 240 ms, pad 0.5** | 5,999 (99.54 %) | 5,999 (99.54 %) | **8 / 1** | **359 ms** | **241** |
| 1 scale 240 ms, pad 1.0 | 5,996 | 5,996 | 7 / 0 | 479 ms | 361 |
| 1 scale 300 ms, pad 0.5 | 5,997 | 5,997 | 7 / 1 | 447 ms | 301 |
| 2 scales (0.24, 0.6), pad 1.0 | 6,003 | 6,001 | 11 / 0 | 479 ms | 1,261 |
| 3 scales (0.24, 0.6, 1.5), pad 1.0 | 6,005 | 6,001 | 11 / 0 | 479 ms | 3,511 |
| 6 scales (research bank), pad 1.0 | 6,007 | 6,002 | 13 / 1 | 298 ms (narrowest scale decides first) | 5,731 |

Misses by speed band show no band where the bank helps. For 240 ms pad 0.5 vs the 6-scale bank, misses / references [R]:

| PWM band | 240 ms, pad 0.5 | 6-scale bank |
|---|---:|---:|
| < 60 | 2 / 30 | 2 / 30 |
| 60–84 | 8 / 237 | 6 / 237 |
| 85–95 | 18 / 5,598 | 17 / 5,598 |
| > 95 | 0 / 162 | 0 / 162 |

The data contain only 30 held-out magnets below PWM 60, so slow passes are thinly represented. In the dev data the slow-crawl session `278F…` has flat-topped ~400-count excursions that no tested scale handles well [R].

**Recommendation [P]: a single fixed 240 ms scale, pad 0.5.**

- **Not** a bank: it costs 5–24× the memory and CPU for at most +3 magnets in 6,027.
- **Not** adaptive speed scaling: the research's interval-derived scaling recognized only 12.8 % of validation magnets, because one miss corrupts the scale [R].

---

## 3. Event data contract

`MagneticEvent`, emitted by the recognizer, one per recognized encounter [P]. The first eight fields are essential; the rest are diagnostic.

| Field | Type | Meaning |
|---|---|---|
| `seq` | u32 | Monotonic per boot; gaps reveal queue drops |
| `t_pass_us` | u64 | **Passage timestamp**: hallTask time of the 500 Hz sample at the template-peak position of the winning window |
| `t_decide_us` | u64 | When the event became final (peak + hold + half window) |
| `polarity` | i8 | +1 N (ADC rises), −1 S |
| `r` | f32 | Correlation at the peak (sign = polarity) |
| `amplitude` | f32 | Fitted A, counts (sign = polarity) |
| `gap_near` | bool | A filled gap ≤ 20 ms lies inside the window |
| `scale_ms` | u16 | Always 240 in the proposed build; kept so the contract survives a later scale change |
| `level` | f32 | Diagnostic: window mean (the local level), counts |
| `pwm`, `dir` | u8, i8 | Diagnostic: applied PWM and declared direction at t_pass (context, not evidence) |
| `r_runner_up` | f32 | Diagnostic: largest |r| suppressed by this peak's hold (helps audit consolidation) |

**Exact conditions for a recognized encounter** [P]: all of the following hold at sample c.

- The 241-sample window centred on c contains no unfilled gap.
- |r(c)| ≥ R_MIN (0.7).
- |A(c)| ≥ A_MIN (70 counts).
- |r(c)| is the maximum of |r| over [c − H, c + H], with H = 60 samples (±120 ms, the evaluated rule).
- No event has already been emitted inside [c − H, c].

**What the recognizer must not do.**

- It does not read `RouteMap.h`.
- It does not know the next MM.
- It does not consult PWM, direction or time-since-last-event when deciding.
- It never drops an event because of what the matcher would think of it.
- It has no "gate". It runs from boot to power-off whether the loco moves or not. Parked false-event rate: 7 events in 4,274 parked seconds across 34 sessions, all during start-up disturbances in two Otto sessions [R].

---

## 4. Geographic matching interface

### 4.1 Matcher state and inputs

**State:**

- `last` (confirmed MM, or UNSET);
- `next` = `routeMod(last + dir)`;
- `dir`;
- `t_last_pass_us` (passage time of the last confirmation);
- a counter of consecutive non-advancing observations.

**Inputs:** `MagneticEvent`s, declarations, suspensions.

**Route data used:**

- `polarityAt(next)`;
- `spacingMm(next, dir)`, for context only, unless §4.4 decides otherwise.

### 4.2 Observation record — the one structural change

Every event produces exactly one `Observation`:

```
Observation {
  event        : MagneticEvent           // as recognized, unmodified
  expected_mm  : u8                       // what the matcher was waiting for (or NA)
  expected_pol : i8
  outcome      : CONFIRMED | POLARITY_DISAGREES | NOT_MATCHED | NO_POSITION
  advanced_to  : u8 | NA
  dt_since_last_ms : u32                  // passage-to-passage, context
}
```

It is published, and recorded in NSR as a NAVI record. This is how NAVI says "I encountered the expected magnet location, but its observed polarity disagrees with the map", without any diagnostic subsystem.

### 4.3 Minimal decision rules for cases A–D

| Case | Observation outcome | Position | Evidence and comment |
|---|---|---|---|
| **A. Expected encounter**: polarity agrees with `next` | CONFIRMED | advance to `next`; `t_last_pass` = `t_pass` | Dev, map correct: next-only polarity matching made 1,586 correct and 68 incorrect advancements, 54 held [R] |
| **B. Credible, polarity disagrees** | POLARITY_DISAGREES | **open decision §4.4**: hold or advance | MM066/067/069 since Oct 26 [R, M] |
| **C. Earlier or later than movement predicts** | Reported normally. `dt_since_last_ms` is in the record; timing has **no veto** in this design | as A or B | Timing used as a veto suppressed 196–3,048 genuine encounters per partition [R] |
| **D. Several events in a short interval** | Each gets its own observation | at most one advance per event; the first matching event advances, later ones are compared with the *new* `next` | Recognizer duplicates: 1 in 6,027 [R]. False events lie 0.16–11 s from the nearest magnet [R] |

**Physical reachability.** A second encounter can be physically impossible as a *new magnet*:

- The minimum mapped spacing is **280 mm** [M: `RouteMap.h`]. The magnet's IR-measured width is about **55 mm** (47–66 mm) [R, 945 curves].
- So two distinct magnets cannot produce waveforms whose centres are closer than about 280 / 55 ≈ 5 waveform widths. That is a speed-free physical bound.
- Applying it needs a per-event width estimate. The fixed-scale recognizer does not measure width.
- The measured duplicates (1) and close false events (3 within 0.17 s of a magnet) do not justify adding it now. It is **not proposed**, only recorded as the defensible form if it is ever needed.

**Distinguishing what the recognizer and the matcher can each tell apart:**

- **Repeated correlation peaks from one magnet:** the recognizer's hold (§2.3).
- **A new magnet waveform:** the recognizer.
- **A non-magnetic disturbance that resembles a waveform:** the recognizer **cannot** reliably separate this. Field false events have |A| 102–390 and r 0.70–1.00 [R], the same as magnets. It shows up in the matcher as a polarity disagreement or an extra event.
- **A credible but geographically inconsistent encounter:** the matcher.

### 4.4 The open matcher decision: what happens after a miss or a disagreement

The recognizer misses about 0.5 % of magnets [R]. At 171 magnets per lap that is roughly one per one to two laps. Any matcher that can only ever advance to `next` has no way back after a miss: the next real magnet is compared with the wrong expectation.

Replay of the proposed recognizer's events, each span starting from a known anchor [R, `design_analysis.py` §4]. Counts are correct / incorrect advancements, with other outcomes in brackets:

| Matcher rule | Dev, Oct 25 Otto (map correct) | Validation, Oct 26 Otto | Transfer, Toby |
|---|---|---|---|
| **P8: next only, polarity required, hold otherwise** (current proposition) | 1,586 / 68 (54 held) | 829 / 603 (561 held) | 2,525 / 742 (717 held) |
| Advance on every credible event, polarity as a flag | 844 / 864 | 126 / 1,867 | 1,443 / 2,541 |
| next or next+1 by Hall-measured timing, polarity required (0.6–1.6× window) | 1,094 / 10 (585 genuine rejected on timing) | 180 / 11 (1,785 rejected) | 885 / 24 (3,048 rejected) |
| same, advance on location even if polarity disagrees | 1,094 / 193 (414 rejected) | 1,778 / 13 (196 rejected) | 1,307 / 187 (2,485 rejected) |

What this shows:

- **P8 works where the map is right and the recognizer does not miss.** It is wrong from the first miss or map disagreement until polarity coincidence happens to re-align it. On Oct 26 that occurred every lap at MM066.
- **Advancing on every event is worse.** One miss is never repaired.
- **Timing used as an identifier works only while speed is steady.** After stops or speed changes it rejects thousands of genuine encounters. This is the failure mode the assignment warns against, now measured.
- **None of the variants is acceptable on replay evidence alone.** The replay spans also include stops and direction changes that a fielded AUTO run would handle with declarations. These figures are therefore an upper bound on harm, not a forecast.

What I propose [P]: **keep P8 behaviour (hold, do not search) for the first implementation, and add the Observation record.** Every disagreement and every hold is then visible and countable on the track, and nothing new is built. In parallel, run the bounded test in §8 (T5), so David can decide the rule from track evidence. The remaining choices are listed in §9 as decisions, not built.

**The MM066/067/069 map question is independent of this.** Once David has inspected the magnets and either turned them back or changed the map, case B on those magnets disappears. (David corrected them on Oct 27. In Toby's 08:33 run all three read as mapped: `docs/navi/NAVI_TOBY_FIELD_RUN_20261027.md`.)

**Correction to the table above (added 2026-10-27).** `design_analysis.py` §4 included each span's starting-anchor event as the first encounter after the anchor, an off-by-one. With that event excluded, as in `matcher_replay.py`, the P8 figures become:

- dev: 1,594 / 60, with 48 held;
- validation: 830 / 600, with 561 held;
- transfer: 2,535 / 732, with 708 held.

The conclusion is unchanged. The other rows were not re-run.

### 4.5 EXPERIMENTAL PROPOSAL — minimal map-aware matcher (not an approved navigation rule)

**Status.** This is an experimental proposal, added 2026-10-27 at David's request after a discussion between David and Sam. The **established rule remains the October 21 hold rule** (P8): advance to `next` only when polarity agrees, otherwise hold. Nothing in this section changes that rule. The penalties below are hand-picked and **not validated**. The proposal exists so that its effect on actual navigation can be measured on Otto and Toby.

**Principle (David and Sam).**

- NAVI pursues one target, but interprets each encounter in the context of the surrounding map.
- A contradiction should challenge its interpretation, not erase its awareness.
- Recognition, interpretation and trust are separate steps.
- The simplest model that works wins.

**The rule, in its simplest form (David's choice: decide on current evidence, retain recent observations, permit revision).**

1. NAVI keeps its last trusted position (the anchor) and the polarity of every recognized encounter since then. This is the rearview mirror.
2. Each new encounter can be explained in only four ways:
   - **NEXT:** the next mapped magnet;
   - **AFTER_ONE_MISS:** the magnet after it, one having been missed;
   - **AFTER_TWO_MISSES:** two having been missed;
   - **NOT_A_MAGNET:** a false event; position unchanged.
3. Each explanation carries a fixed penalty:
   - polarity disagrees with the map: C_MIS = 1;
   - each assumed miss: C_SKIP = 1.5;
   - not a magnet: C_SP = 1.5.
4. NAVI reports the position at the end of the **cheapest explanation of the whole history since the anchor**.
   - Only explanations within 4 of the best are kept, so the set stays small: a handful of positions just ahead of the anchor.
5. When a later encounter makes a different history cheapest, the reported position changes. That is a **revision**, and it is reported as one.
6. A polarity disagreement on the cheapest explanation is reported as an infrastructure observation: "MMxxx observed S, map N". The DNA map is never changed.

**What it does not use:** speed, PWM, time since the last encounter, IR, or a search of all 171 magnets. It needs about a few hundred bytes of state [E] and a few dozen lines of code.

**Replay** [R]:

- **Script and output:** `research/navi_waveform_design_20261027/matcher_replay.py`; output in `matcher_replay_output.txt`.
- **Input:** the proposed recognizer's events. Each span starts from its first anchor.
- **Truth used for scoring:** the research reference, which is route-anchored oracle evidence, not physical truth.
- **Counting:** an event is correct when its assigned identity matches the reference. A false event is correct only if labelled NOT_A_MAGNET.
- **Episodes:** a "wrong episode" is a run of consecutive wrong assignments.

| Matcher | Dev, Oct 25 Otto | Validation, Oct 26 Otto | Transfer, Toby Oct 25–26 | Oct 27 Toby (magnets corrected) |
|---|---|---|---|---|
| **P8, October 21 hold rule** (established) | 1,594 / 60, 48 held; 6 episodes, longest 23 | 830 / 600, 561 held; 10 episodes, longest 180 | 2,535 / 732, 708 held; 7 episodes, longest 184 | 273 / 0 |
| **Map-aware, decide now, may revise** (experimental, preferred form) | 1,677 / 26; 9 episodes, longest 8; 9 revisions | 1,934 / 57; 17 episodes, longest 4; 25 revisions | 3,900 / 75; 17 episodes, longest 13; 45 revisions | 273 / 0; 0 revisions |
| Map-aware, decide one encounter later | 1,671 / 19, 13 held | 1,928 / 53, 10 held | 3,872 / 70, 33 held | 272 / 0, 1 held |
| Map-aware, decide two encounters later | 1,662 / 15, 26 held | 1,930 / 41, 20 held | 3,846 / 63, 66 held | 271 / 0, 2 held |

Correct / wrong assignments; "held" means no identity was assigned. The figures in my chat reply of 2026-10-27 were summed wrongly; this table supersedes them.

On held-out Otto Oct 26 + Toby Oct 25–26:

- **P8:** 3,365 correct, 1,332 wrong, 1,269 held. When it is lost, it stays lost for up to 184 encounters.
- **Map-aware, decide now:** 5,834 correct, 132 wrong, 0 held. The longest wrong episode is 13 encounters.

Waiting before deciding buys little, and adds one magnet interval of delay per encounter waited. That is why "decide now, allow revision" is the preferred form.

**Assumptions.**

- At most two consecutive magnets are missed.
- A false event and a miss are each rarer than a single map disagreement. That is what the 1.5-versus-1 penalties encode.
- The anchor is correct. The matcher never questions the anchor itself.

**Limitations, all measured on replay** [R]:

- **It never identified a false event as a false event:** 0 of 13. It always explains them some other way, which costs one or more wrong assignments each time.
- **Its wrong episodes last up to 13 encounters** (Toby). During an episode NAVI reports a position that is wrong but self-consistent.
- **The penalties were set once, by hand, and not tuned** (David: no penalty optimization now). Different penalties would give different numbers.
- **It is scored against the research reference, not physical truth.** The replay spans also include stops and direction changes that operational running handles with declarations.
- **A revision moves the reported position**, possibly by up to three magnets, possibly backwards relative to the last report. Anything that acts on position, such as station stops in a later AUTO build, would need a rule for revisions. This build is manual only, so nothing acts on it.

**What the track test must answer** (T5, run once with each matcher): does map-aware judgement improve actual navigation on Otto and Toby? It is measured as:

- the fraction of encounters with the correct identity, against a lap-by-lap operator record or a clean replay;
- the number and length of wrong episodes;
- the number of revisions;
- the number of map-disagreement observations.

---

## 5. IR and movement evidence

**Established requirements** [M]:

- On 2026-10-21 David excluded IR from the current development stage. The historical ±15 % distance tolerance is vestigial, and no automatic distance/time LOST bound applies (`348514e`).
- The fielded build has no IR code path. Only the IR test car's MAC is declared in `LL_LocoConfig_9950011.h`.
- Earlier IR work found undercounts of 1–39 % from merged spokes (`c4fa2854`).

**Therefore** [P]: the recognizer and matcher in this design do not use IR.

If David re-admits IR, the smallest useful role is:

- **Distance since last confirmation**, compared against `spacingMm(next)` and reported in the Observation as context. This is the same role `dt_since_last_ms` has.
- **Time-stamp compensation is not needed.** Passage time is already within a few ms of the extremum (§6).
- **Early or late interpretation, case C.** Distance would be better evidence than time, because it is independent of speed. It would only become a rule by David's decision, after IR validity is re-established.

**When IR is unavailable or invalid:** nothing changes. The design does not depend on it.

---

## 6. Timing analysis

The three times, all [R] on causal emulation of the proposed configuration:

| Time | Definition | Accuracy or delay |
|---|---|---|
| **Physical passage** `t_pass` | 500 Hz sample time of the template-peak position in the best-matching window | vs the waveform extremum: median 0 to −2 ms, p95 6 ms, worst 36 ms (Toby), 218 ms (one Otto validation outlier). At PWM 90 (~250 mm/s), 6 ms ≈ 1.5 mm |
| **Recognition** `t_decide` | Peak known plus hold complete | t_pass + 359 ms, fixed: window half 240 ms + hold 120 ms − 1 ms |
| **Reporting** | Event in the matcher (rvmTask) | + ≤ 20 ms if rvmTask processes every block [E]; + queue time to MQTT (not measured) |

**Is the template peak a reliable physical reference?**

- It is reproducible to the same degree as the extremum and the threshold onset. Lap-to-lap, same magnet pair, PWM 85–95: interval SD 92.8 ms for the template peak vs 92.2 ms for the extremum and 93.0 ms for the onset, over 656 groups [R]. Speed variation, not the timestamp, dominates.
- Whether the field extremum sits exactly over the magnet's physical centre is **not measured**. A direction-dependent offset would cancel in same-direction intervals and is invisible to Hall-only data. It needs a standstill test (§8 T3).

**Timestamp NAVI should use:** `t_pass_us`, for intervals, speed estimates and any distance arithmetic. Never `t_decide_us`, which adds a constant 359 ms.

**Effect of the delay**, at a fixed 359 ms [R]:

| Affected quantity | Effect |
|---|---|
| Magnet-to-magnet intervals, speed estimates | **None** if computed from `t_pass` (both ends delayed equally) |
| IR distance (if re-admitted) | Must be sampled at `t_pass`, not at reception, or it over-counts by the travel during the delay |
| Position reporting | "Last passed MMxxx at t_pass" is accurate. "Now at MMxxx" is 0.36 s stale |
| Travel during the delay | PWM 85–95: median **84 mm**, p95 103 mm. Above 95: median 111, max 233 mm. Below 60: median 46 mm |
| Control decisions (stopping at a station, holding point) | A stop triggered *on* recognition of MMxxx begins about 84 mm past it at PWM 90. A fixed offset like that can be absorbed in where the stop is triggered. It cannot be removed by timestamp correction. The ramp-down distance (David: no coast, only a ramp to PWM 30–15) is added after it |

**Reducing latency** — options, none evaluated as final:

- Halving the hold to ±60 ms saves 60 ms, but is not replayed.
- A narrower pad saves more, at the cost of noise robustness; pad 0.25 is not replayed.
- Early "prefix" recognition at the peak (120 ms) produced about 2,000 duplicates and about 800 false events on validation [R] and is **not viable** as tested.

---

## 7. ESP32 feasibility

**Fielded firmware facts** [M]:

| Item | Value |
|---|---|
| `hallTask` | 4 KB stack, priority 3, core 0, 1 ms tick |
| `rvmTask` | 8 KB stack, priority 1, core 0; high-water free 7,380–7,460 B over Oct 25–26 |
| `networkTask` | 8 KB, core 1; Arduino loop also on core 1 |
| WiFi | Platform default places the WiFi task on core 0, so hallTask shares core 0 with it. This is not measured on these locos |
| `heap_free` | 40.7–74.4 KB over Oct 25–26 sessions |
| `heap_min` | 36.8–62.3 KB (worst 36,800 B, Toby, Oct 25 19:14) |
| `blockQ` | 128 × `Block` (~16 B) = 2.5 s |

**Proposed recognizer resources** [E, not measured on hardware]:

| Item | Size or cost |
|---|---|
| Template T̂ | 241 float = 964 B (or 512 B constant shape + boot resampling) |
| Sample ring | 512 float = 2 KB (window 241 + hold 60 + margin) |
| Filtering state | 5 ints per tick (median), 1 pending tick value: < 40 B |
| Running sums, peak tracker | < 64 B |
| Event queue | 8 × ~48 B ≈ 400 B |
| **Total RAM** | **≈ 3.5 KB**, about 10 % of the worst measured heap low-water |
| CPU per 500 Hz sample | 241 MAC + 2 running sums + sqrt + divide. ≈ 1.5–2.5 k cycles at 240 MHz ≈ 6–10 µs |
| CPU total | ≈ 3–5 ms of every second on core 0 (0.3–0.5 %) |
| Per-tick work added to hallTask | median of 5 ints + one add every 2 ticks: < 1 µs |

**Where it runs** [P]:

- hallTask only conditions and pushes a 500 Hz value into a single-producer/single-consumer ring. That is constant, tiny work at priority 3.
- rvmTask (priority 1) runs the correlation. It wakes on each 20 ms block, as now, and processes the 10 new samples: about 60–100 µs per wake [E].
- This keeps the correlation out of the 1 ms acquisition loop and cannot delay analogRead scheduling.

**WiFi and control.** Added load on core 0 is below 1 % [E]. The MQTT path on core 1 is untouched. Control (PWM ramp, E-stop) runs on core 1 and is not affected.

**The full research bank would need** about 23 KB of templates plus a 9 KB ring [E]. That is close to the 36.8 KB worst low-water, and it is not recommended.

**Lowest-cost configuration meeting the demonstrated requirements:** the single 240 ms scale above.

---

## 8. Validation plan

**Offline (replay), before any flashing:**

| # | Test | Acceptance (from evidence, not a guarantee) |
|---|---|---|
| R1 | C++ recognizer fed the recorded native samples through the *firmware* tick-median path, compared with the Python reference on all 34 captures | Identical event list: same seq order, t_pass within ±2 ms, r within ±0.001, A within ±0.5 count. Any difference must be explained before going further |
| R2 | Held-out score with the frozen parameters | ≥ 5,999 / 6,027 recognized and ≤ 8 false (this is the replay figure: equal, not "better") |
| R3 | Baseline stress set (offset, drift, steps, noise, spikes) through the C++ path | Matches `stress_validation.json` within ±2 per condition |
| R4 | Observation outcomes on Oct 26 replay at MM066/067/069 | Every pass appears as POLARITY_DISAGREES (or CONFIRMED after the map/magnets are corrected); none is dropped |

**On the locomotive** (requires David's separate authorization to flash):

| # | Test | Measured quantities and acceptance |
|---|---|---|
| T1 | Resource check, parked and at PWM 90, both locos | heap_min, rvm stack high-water, recognizer µs per wake, hallTask tick lateness, MQTT drop counters. Acceptance: heap_min ≥ the current build's measured range minus ~4 KB; no new drops; tick lateness unchanged |
| T2 | Ordinary laps, PWM 90, both directions, Otto then Toby, with NSR recording | On-loco event list = replay of the same NSR capture (R1 criteria). Recognition rate reported against the route; no fixed pass figure claimed beyond "comparable to replay" |
| T3 | Standstill reference: stop with each of 3 magnets under the sensor (S and N), record the stationary level; then pass the same magnets slowly | Measures the offset of t_pass from the physical magnet position, which is currently unmeasured |
| T4 | Speeds: crawl, PWM 50–60, PWM 90, the highest the operator permits | Recognition per band. Slow crawl is the least covered condition (30 held-out magnets below PWM 60) |
| T5 | Matcher evidence run: manual laps with known declarations, Observation stream recorded, MM066/067/069 left as found until inspected | Counts of CONFIRMED / POLARITY_DISAGREES / NOT_MATCHED, and every hold, for David's decision §9.1 |
| T6 | Baseline disturbances: the start-up and motion steps observed in 7463C, 8139C, F420, D1EC | Instrumented: what are they physically? Recognizer behaviour around them is recorded, not tuned |

**Only hardware can establish:** CPU and heap cost; acquisition jitter; the physical offset of t_pass (T3); behaviour at speeds outside the recorded range; and the nature of the background steps.

---

## 9. Open decisions (David)

1. **The matcher after a miss or a polarity disagreement** (§4.4). The options are:
   - (a) keep P8 hold-and-wait, which is proposed for the first build;
   - (b) advance on a location-consistent encounter regardless of polarity, with the disagreement recorded;
   - (c) allow `next+1` when a miss is evident, which contradicts Proposition 8's no-search rule.

   **Implication to weigh.** (a) never advances on a contradiction, but stays one magnet displaced after any miss until polarity coincidence re-aligns it. (b) and (c) can recover from a miss, but act on evidence the current proposition rules out, and (c) depends on timing that has failed whenever speed changed. This needs track evidence (T5) before a choice.

   **(d), added 2026-10-27 and experimental:** the minimal map-aware matcher (§4.5). It recovers from misses and from map disagreements without timing, and allows revision.

   - **Implication:** NAVI may report a position on evidence that disagrees with the map, and may later move that report by up to three magnets.
   - **What it means for later decisions:** in operational running, a revision has to be handled by anything that acts on position.
   - David has directed that (d) be prepared for an experimental build, to be compared with (a) on the track. That direction is not approval of (d) as a navigation rule.
2. **MM066/067/069.** Turn the magnets back to the map, or change the map, after inspection.
3. **Thresholds.** Accept R_MIN = 0.7 and A_MIN = 70 as defaults to be confirmed on track, or require a separate tuning run.
4. **IR.** Remains excluded (current ruling), or re-admitted as context only (§5).
5. **Stopping-point offset.** Whether station and holding stops should be triggered from `t_pass` plus a distance (and therefore need speed), or simply accept the about-84 mm decision delay at PWM 90 as part of where the stop begins.

---

## 10. Implementation recommendation

For review; not authorization to code.

1. **Replace** the 70×2 candidate, frozen baseline and return-to-baseline path with one `WaveformRecognizer`:
   - per-tick median-of-5, 2 ms average;
   - 240 ms template, 241-sample window, level-only normalization;
   - |r| ≥ 0.7, |A| ≥ 70;
   - peak plus 120 ms hold.

   It emits `MagneticEvent` (§3) from boot, unconditionally.
2. **Keep** `NaviTarget`'s single expected target with P8 behaviour. Add the `Observation` record (§4.2) and publish and record it. Do not add timing vetoes, reachability rules, searches or recovery.

   *Amended 2026-10-27 by David's direction:* the experimental build also carries the map-aware matcher (§4.5) as a second matcher, for comparison on the track. Recognition events and geographic judgements are published separately.
3. **Run correlation in rvmTask.** hallTask only conditions.
4. **Gate the build on replay parity (R1) before any flash.** Then run T1–T6 with NSR recording on. Flashing and any field run need David's separate authorization.
5. **Name the build for what it enables.** Example: `NAVI_WAVEFORM_RECOGNIZER_MANUAL_OBSERVE` — manual operation only, AUTO locked, as in the current build.

Every element above answers: what it does, which recorded evidence supports it, and why it cannot be left out. Everything that failed that test — scale bank, adaptive scaling, slope removal, matching pursuit, joint fitting, timing vetoes, IR — is left out, with the evidence in the cited results.
