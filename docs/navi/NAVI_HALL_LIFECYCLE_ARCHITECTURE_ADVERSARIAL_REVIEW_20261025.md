# Adversarial review: revised NAVI Hall lifecycle architecture

**Date:** 2026-10-25 · **Reviewer:** Claude · **Status:** review only. No firmware, specification, or governing record was changed. Nothing here authorizes implementation.

**Under review:** `docs/navi/NAVI_HALL_LIFECYCLE_ARCHITECTURE_REVISED_20261025.md` at `f5b8a2b` (cited below as **ARCH §n**).

**Reference sources:**
- **AUDIT:** my compliance audit, `docs/navi/NAVI_DESIGN_TO_IMPLEMENTATION_COMPLIANCE_AUDIT_20261025.md` at `44be78a`.
- **CONF:** the conference record, `9a6bbb7`.
- **AGR20, ODR, GDS, REC:** the recovered agreements.
- **CS, INT:** the clean-sheet and integrated proposals.
- **INV:** the 2026-10-24 onset investigation.
- **Firmware:** `05393117:firmware/test-programs/NAVI_POWER_OF_NORMAL_MANUAL_ONLY/`.

**Labels:** **[E]** established requirement · **[D]** demonstrated implementation defect · **[P]** proposed innovation · **[U]** unresolved decision · **[F]** field or replay fact from this review.

---

## 0. New evidence and one erratum

### 0.1 Erratum to AUDIT §6 and its appendix

The audit's replay did not reproduce the firmware's 5→1 kHz decimation phase.

- **What the firmware does:** it evaluates the first read of each five-read burst. I checked the field opening timestamps (MM157 at 214.962442 s, MM156 at 213.506218 s, Toby's false opening at 192.407435 s). Every one falls on a read with `serial % 5 == 1`.
- **What the audit's replay did:** it started its stride counter at the beginning of the file instead.
- **Effect of the corrected (phase-aligned) replay:**
  - Opening times now match the field to the microsecond.
  - The clean 20:07 run gives 176 closures, matching the field's 176 confirmed. The misaligned replay gave 177.
  - MM157 gives 328 discarded onsets (the audit said 323) and 235 clusters (the audit said 233 after the first two). The match of MM159→MM49 against the map still has **0 mismatches**.
- **Conclusion:** the audit's MM157 findings stand. The counts quoted in AUDIT §A-8 and §6 are approximate to within those differences.

### 0.2 Replays run for this review

All replays are phase-aligned and built from the real `NaviBaseline`/`NaviObservation` headers. The code is in the Appendix.

| ID | Recording | Purpose |
|---|---|---|
| R1 | Otto `7463C946…` (15:14, MM157 lockup) | What early closure would do at MM157 |
| R2 | Otto `C2C3E51F…` (20:07, the clean 176-confirmation lap ending at rest in MM56's field) | Multi-lobe safety of early closure on a clean lap |
| R3 | Toby `2623C224…` (18:06, the false MM109 during a stall) | Stop and stall behavior |

R1 and R2 also replay a **hypothetical early-closure variant**: the encounter also closes when the deviation from its opening level falls below F × its running peak, for one block. This variant is mine, built only to test the proposal. **It is not a proposal, and F is not a recommended value.**

### 0.3 Field case the architecture does not mention

**R3, Toby 18:10:07, false MM109 confirmed:**
- **The stall:** between MM110 (180.81 s) and the next real magnet (199.86 s), Toby sat at **applied PWM 90 for about 19 s**. Hall stayed flat at 1825 ± 2 the whole time. That is a stall: motor on, no displacement.
- **The spikes:** at 192.407 s and 192.408 s, two isolated single reads, +79 and +120, landed on consecutive 1 kHz evaluation slots. They satisfied 70×2.
- **The false confirmation:** the encounter's block-level peak was **3.1 ADC**. It closed by the normal return rule and was confirmed as MM109, because the polarity matched (MQTT `diag/navi_encounter`, `peak_adc:3.1`).
- **The slip:** the real MM109 passage at 199.859 s was then confirmed as **MM108**, and every later position was off by one. Same-polarity neighbours (MM110, 109 and 108 are all N) made the slip invisible to polarity checking.

This is the only demonstrated false advance on Oct 25. It occurred **during a stop-like condition**, which is exactly the regime ARCH §4 calls unprotected.

---

## 1. Does the architecture follow from the October 25 evidence?

**Partly.** The evidence demonstrates three defects:
- the frozen closure reference **[D]** (AUDIT C-2);
- the silent discard of later onsets **[D]** (AUDIT C-3);
- baseline admission of magnetic evidence **[D]** (AUDIT C-1, BASE-03 **[E]**).

ARCH addresses all three. Early recession closure **[P]**, however, is **not demanded** by the evidence:

| Case on Oct 25 | Did full-RTB closure fail? | Would early recession closure help? |
|---|---|---|
| Clean lap R2 (176 encounters) | No. Longest open-to-close was 0.66 s, median 0.34 s. 0 nonqualifying. | No benefit; adds risk (§2) |
| Stopped in MM56's field at the end of R2 | Remained open at rest, which the operator says is correct (ARCH §1) | No: the train stopped while the signal was still rising. A recession rule changes nothing here unless the stop falls on the falling shoulder. |
| MM157 lockup R1 | Yes, but because the **reference** was stale, not because return was slow | **No.** See below. |
| Toby false MM109 R3 | No: it closed and confirmed falsely | No: a 3.1-ADC block peak closes at once under any recession rule |

**R1 result (hypothetical variant, F = 0.3 and F = 0.5):**
- The stuck encounter does close: at 216.22 s for F = 0.3, at 215.68 s for F = 0.5.
- It closes on the **wrong feature**. At 216.22 the signal momentarily passes near the stale 1957 level. That point is the S-going dip in the MM158 interval, which against the new background is itself a magnetic excursion.
- Each later opening then finalizes a few-block baseline collection that started on that dip. The baseline never escapes, and every subsequent onset reads as N against a stale-low level.
- Result: **6 nonqualifying N encounters in 13 s (F = 0.3), 10 (F = 0.5), and the MM158 target never confirmed.** NAVI is still stuck at MM157. The difference is that the failure is now visible as nonqualifying counts.

**Finding:** the MM157 failure is a **reference and baseline failure under a lasting background offset**. Closure timing is secondary. Early closure is a proposal in its own right; ARCH should not present it as the remedy the evidence points to.

The background offset's physical cause is still unresolved **[U]** (AUDIT F-3.1). The operator's wiring adjustment preceded the clean R2 lap, whose background held 1935–1938. AGENTS.md §8 and CONF Prop 9 both say not to design lifecycle machinery around a condition until its physical plausibility is established. **ARCH lists "handling persistent raw background shifts" as unresolved (§5) but stages three behavior changes (Steps 2, 3 and 5) that depend on it.**

---

## 2. Can early closure safely coexist with trailing lobes?

**Not demonstrated, and in R2 it depends on an uninvented constant.** Hypothetical variant on the clean lap, at speeds of about 93–273 mm/s:

| Variant | Confirmed | Nonqualifying | False advances |
|---|---|---|---|
| Firmware rule (no early closure) | 176 | 0 | 0 |
| F = 0.3 | 176 | 0 | 0 |
| F = 0.5 | 176 | 3 | **1, a one-position slip** |

The F = 0.5 slip:
- **What happened:** MM54 opened at 342.140 s and closed early. A same-field re-departure at **342.424 s (0.28 s later, about 33 mm at the measured 116 mm/s during the stop ramp)** was confirmed as MM55, because MM54 and MM55 are both S.
- **Consequence:** the real MM55 at 345.372 s was then judged against MM56 (N) and rejected.
- **The other two nonqualifying events:** both were same-polarity re-departures, 1 ms and 170 ms after an early close (141.041 s and 269.060 s).
- **What it means:** re-departures of 70 or more on the shoulder of a principal excursion are real. Without early closure they are harmless, because the discard rule swallows them. In R2, 98 such re-departures occurred inside 72 encounters, all within 124 ms of the peak block.

**What would falsify early-closure safety (the traces to examine):**
1. Any re-departure of 70 or more after the recession point, in labeled traces across speeds, directions and both locomotives.
2. Same-polarity adjacent-magnet pairs in the map. Polarity cannot catch a slip there. The map has many: 163–166 SSSS, 7–10 SSSS, 11–14 NNNN, 28–32 NNNNN, 36–41 SSSSSS.
3. Slow passages and stops on the falling shoulder.
4. Excursions measured against a biased reference. A lobe below 70 becomes 70 or more if the reference is off by its margin, which is why §4 matters.

**Contradiction with CONF Prop 7 [E]:** the record says rebounds are excluded because "rebound peaks fall below the 70-count sentinel threshold". That holds only when the reference is accurate *and* the encounter is open through the shoulder. Early closure removes the second condition. ARCH does not acknowledge that it weakens the discriminator Prop 7 relies on.

---

## 3. Is independent MM-to-MM timing protection sufficient without IR?

**No, and the limitation can be stated exactly.** ARCH §4 already concedes the principle. Here are the numbers.

Let *s_min* be the minimum mapped spacing (280 mm in `RouteMap.h`) and *v_max* the conservative maximum speed. The highest apparent speed in R2 was 273 mm/s from a single interval, which is a map-dependent figure. The earliest-arrival guard is then T_min = s_min / v_max ≈ **1.0 s**.

A same-field lobe at distance *d* behind the opening arrives at *d / v*. The guard blocks it only if *d / v* < T_min, that is, only if **v > d · v_max / s_min**.

- For the observed d ≈ 33 mm, that threshold is **about 32 mm/s**. Below it, a crawling locomotive's own lobe passes the guard.
- **After any stop longer than T_min the guard is open,** whatever has or has not moved.
- **R3 is a counter-example in the field:** the false MM109 came **11.6 s** after the last confirmation, during a stall at PWM 90. Any time-based guard passes it.
- **Applied PWM is not displacement evidence either.** In R3, PWM was 90 and nothing moved.
- **A flat Hall trace is not evidence of standing still,** because ordinary track is flat.

So with IR unreliable, NAVI has **no displacement evidence** in exactly the regime where the guard is needed. The guard protects only moving-at-speed cases. On R2's evidence, those cases are safe anyway unless early closure is introduced.

**Prop 9 [E] tension:** the guard (Step 4) is needed because of early closure (Step 3). Under the current full-RTB rule, R2 shows no lobe false-positive at all. ARCH proposes one new mechanism to contain the side effects of another. That is the pattern CONF Prop 9 warns against: "no additional failure-specific mechanism … unless field evidence demonstrates a problem."

**Consistency question [U]:** the guard is a per-interval *earliest* time limit. Prop 6B, a per-interval *latest* time limit, was withdrawn as "planning to fail". ARCH should state why the reasoning that removed 6B does not apply to Step 4. This review does not answer that question.

---

## 4. Should the baseline correction precede encounter-closure changes?

**Yes. ARCH's order (Step 2 before Step 3) is right, but Step 2 as written cannot be completed in isolation.**

1. Every proposed closure rule measures recession or return against some reference. If the baseline is biased, any closure experiment in Step 3 measures the bias rather than the rule (R1).
2. **Hidden coupling:** BASE-03 **[E]** requires excluding magnetic tails. The natural rule, "exclude blocks far from the committed level", also excludes a **lasting background step** forever:
   - the baseline would never adapt;
   - the 70×2 detector stays latched after one onset, because `excursionActive_` holds until the deviation drops below 70 (`NaviCandidate.h:56–66`);
   - NAVI goes blind to every same-polarity magnet.

   On MM157 that could be *worse* than today. Step 2 therefore **requires the "persistent background shift" decision [U] first.** ARCH defers that decision to "unresolved" (§5).
3. The current start-of-collection rule (one block within 20 of the committed level, `NaviBaseline.h:83–84`) departs from the primary fragment "Magnet Close to start" (AUDIT C-6, **[U]** approval). Step 2 must decide that rule as well, not only the admission rule.

**Recommended order:** shift question (experimental) → Step 2 → Step 3.

---

## 5. Does the architecture change an established agreement?

| ARCH element | Agreement | Finding |
|---|---|---|
| §1 and §5 acknowledge that early closure and earlier confirmation need a new decision | CONF §1.3, Prop 6A: qualification "retrospectively, after the Hall signal itself establishes completion (observed return to baseline)" **[E]** | Correctly labelled **[P]**. **But §5's "Established" list omits retrospective qualification after RTB.** It is the agreement most at stake; it should be listed. |
| Step 3: early closure "*without changing target confirmation*" | Firmware judges the encounter **at closure**: `judgeEncounter()` is called from the `push()` completion (`.ino:406–409`) | **Contradiction.** In the current structure, closing early *is* qualifying early, so Step 3 changes the qualification cycle. Step 3 must either decouple "release the detector" from "qualify the encounter", which is a new state **[P]**, or be merged with Step 6 under the CONF §1.3 decision. |
| §3.2: "timestamp on second sample" | Firmware and INT: the *first* crossing sample is the opening instant (`NaviCandidate.h:9–10, 35–36`). CONF Prop 3 and ODR D-07 require that timing provenance be preserved. | **Contradiction, unflagged.** Changing the timing anchor changes every interval measurement and the comparability of the PWM-90 timing table (`05393117`). Either correct ARCH or raise it as a decision. |
| §3.5: match on "polarity, time, and optionally width" | Timing as a qualification veto is **[U]**, cited only in code comments (AUDIT row 21). CONF Prop 4: movement predicts; Hall confirms. | ARCH treats "time" as a match criterion without marking it unresolved. Mark it **[U]**. |
| §3.4 and Step 4: earliest-arrival eligibility | No source (AUDIT row 24). CONF Prop 2 (no recovery) is not violated. | **[P]**, correctly labelled. Prop 9 justification is missing (§3). |
| §3.1 and Step 1: do not discard evidence | GDS VAL-04 **[E]**; AGR20 Principle 9 **[E]** | Consistent |
| No universal timeout | REC-14 **[E]** | Consistent |
| Step 3: "+80–160 ADC raw shift" | AUDIT measured +116 settled at 2–4 s, +81 at 5 min, about +170 transient | Range stated without a source for 160; give a reference. |

---

## 6. Are the staged instructions specific, isolated, reversible and testable?

| Step | Issue | Amendment |
|---|---|---|
| 0 | Largely done by AUDIT, including "no MM timer closes encounters" (AUDIT §D). | Limit it to the gaps: MM56/MM23 raw analysis, the stride-phase erratum (§0.1), Toby R3. |
| 1 | "Unchanged navigation counts" is too weak: counts can match while sequences differ. Telemetry channel unspecified. Changing the NSR schema would break the Pi decoder. | Acceptance should be an **identical replayed event sequence** (open/close/judgement times and MMs) on R1, R2 and R3. **Additive MQTT `diag/` topics only** (docs/CLAUDE.md hard constraint), and no NSR schema change. Add a **stride-phase-aligned replay** as the reference harness. |
| 2 | Blocked by the shift decision (§4). Its "switch" is undefined: build flag or runtime? | Make it compile-time, defaulting off, with an identical-sequence check when off. Decide the shift policy first. |
| 3 | Not isolated from qualification (§5). The constants are flagged correctly ("do not invent"), but R2 shows the outcome flips between F = 0.3 and 0.5. | Merge with Step 6, or explicitly design a "release ≠ qualify" split, before coding. Acceptance must include same-polarity neighbour runs and slow/ramp passages. |
| 4 | Cannot protect stops or creep (§3). It depends on Step 3 existing. | Defer until Step 3 shows a lobe problem that needs it (Prop 9). If it proceeds, require explicit operator acceptance of the stated v < d·v_max/s_min limitation. |
| 5 | Interacts with direction handling. On a manual reverse the firmware suspends NAVI (`.ino:676–681, 491–495`) but **does not reset the open encounter**: `applyControl()` touches only `target`. After redeclaration, a pre-reversal encounter can close and be judged against the *new* target. This is a latent mechanism in today's firmware too **[D, not field-observed]**. | Add it to Step 5's test matrix and to the open-decision list. |
| 6 | It is the step that changes CONF §1.3. | Needs the explicit amendment decision in the conference record (AGENTS.md §7) *before* Step 3 can be evaluated honestly. |
| 7 | Lacks Toby stall cases and same-polarity neighbour runs. | Add R3 and runs through SSSS/NNNN map sections. |

Every step needs: a named known-good starting commit; one scoped commit; a compile check for both locomotive profiles; and rollback to a single named commit. ARCH says this generally. It should be repeated per step with the actual commit IDs.

---

## 7. New failure mechanisms and complexity

1. **Same-polarity slips** caused by early closure (R2, F = 0.5). Polarity checks cannot detect them; the map has long same-polarity runs.
2. **Wrong-feature closure** under a background offset (R1). It converts a visible lockup into a stream of nonqualifying encounters, or into a slip if a later same-polarity feature lines up.
3. **Baseline lock-out** if BASE-03 is fixed without a shift policy (§4).
4. **Guard false confidence** after stops (§3). Elapsed time is presented as protection where none exists.
5. **New state:** recession tracker, eligibility state, rearm state, and possibly a release-vs-qualify split. That is three to four interacting states in place of today's single open/closed flag, each with new constants. AUDIT showed the current single flag is defective; ARCH has not shown that this much new state is the minimum fix.
6. **Not addressed:** spike-driven false onsets during stalls (R3). This is the only false advance demonstrated on Oct 25. The median-of-five detector input was proposed earlier and **held by the operator ("Not yet")**. This review only notes that ARCH does not cover the case.

---

## 8. Recommended amendments (for David's decision)

1. **Re-frame §2:** the evidence shows a **reference and baseline failure under a lasting offset** plus **silent discard**. It does not show a closure-timing failure. Present early closure as an option, not as the conclusion.
2. **Add to §5 "Established":** retrospective qualification after observed RTB (CONF §1.3). Add to §5 "Unresolved": timing as a match criterion; the opening-timestamp anchor (first vs second sample); stale encounters across reverse/redeclare; spike-driven onsets during stalls.
3. **Correct §3.2:** use the first crossing sample, or raise the change as a decision with its Prop 3 consequences.
4. **Reorder the rollout:**
   - 0′ read-only gaps (§6);
   - 1 observability, with identical-sequence regression;
   - **1a: a bench and track test of the background-offset cause (AGENTS.md §8)**;
   - 2 baseline, after the shift-policy decision;
   - **then re-evaluate**, because a correct baseline and live closure reference may make Steps 3–5 unnecessary;
   - only on field evidence of need, 3 + 6 together under a recorded CONF §1.3 amendment;
   - 4 and 5 only if Step 3's replays show lobes that need them.
5. **Record Prop 6B's status** in the conference record before Step 4 is designed (AUDIT C-5).
6. **Adopt a phase-aligned replay harness** (Appendix) as the deterministic test for every step.

---

## 9. Summary ledger

| Item | Class |
|---|---|
| Continuous Hall; 70×2 sentinel; one target; no search/recovery; shape diagnostic; nonqualifying waits; BASE-03; no timer completion; **retrospective qualification after RTB** | **[E]** |
| Frozen closure reference; silent onset discard; baseline admits magnets; stale encounter survives reverse/redeclare | **[D]** (the last one latent) |
| Recession closure; earliest-arrival guard; rearm/overlap rules; pre-RTB confirmation; second-sample timestamp | **[P]** |
| Background-offset cause and policy; recession constants; displacement evidence without IR; timing as qualification; Prop 6B status; never-completing encounter; flashed-binary identity; MM56/MM23 analyses | **[U]** |
| MM157 recession closure lands on the wrong feature (R1); F = 0.5 same-polarity slip (R2); Toby stall false MM109 and slip (R3); stride phase `serial % 5 == 1` | **[F]** |

---

## Appendix — replay programs

The **phase-aligned detector** is a copy of `CandidateDetector::onSample` (`NaviCandidate.h:52–68`), evaluated only where `serial % 5 == 1`:

```cpp
static bool pc=false,act=false; static int8_t ps=1; static uint64_t pus=0;
static bool det(int adc,float lvl,uint64_t us,navi_power::CandidateOnset&o){ float dev=float(adc)-lvl; bool cr=fabsf(dev)>=70.0f; int8_t s=dev>=0?1:-1; bool on=false;
  if(cr){ if(!act&&pc&&ps==s){o.openUs=pus;o.pol=s;o.levelAtOpen=lvl;on=true;act=true;} pc=true;ps=s;pus=us;} else {pc=false;act=false;} return on;}
```

The **variant simulator** (`sim.cpp`) uses the firmware `BaselineTracker` and `Tracing`, plus the detector above. The encounter closes on the firmware RTB rule (10 blocks within ±20 of the opening level), *or*, when F > 0, on |dev| < F × running peak. Judgement is polarity against `RouteMap` only, with timing profiles empty as on Oct 25. Usage:

```
./sim native_<boot>.bin <F> <declare_t_s> <last_mm> <end_t_s>
```

| Replay | Command | Result |
|---|---|---|
| R1 | `./sim native_7463C9467F2273E6.bin {0.3,0.5} 215.33 157 228` | §1 |
| R2 | `./sim native_C2C3E51F16AFA083.bin {0,0.3,0.5} 85.7 50 400` | F = 0: 176 confirmed, identical to the field |
| R3 | phase-aligned trace, 178–205 s | Reproduces the field's false opening at 192.407 s |

Native files come from `analysis/navi_rvm_shape_20261018/unified/u01_decode_nsr.py`, run on the NSR captures in `~/Pi_Log_Downloads`. Pass `9950012` as the loco argument for Toby. The scratch sources are not committed; this appendix is their record.
