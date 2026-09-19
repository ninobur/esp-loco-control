# NAVI_ONE — the fix-to-strike relationship across the whole variant line

**Date:** 2026-09-19
**Scope:** every NAVI_ONE variant in this repository from 0.1 (2026-08-29) to
X21 (2026-09-16), plus NAVI_ONE_SIMPLE and NAVI_SIMPLIFIED.
**Status:** Analysis. Nothing here is a decision, a ruling, or a proposal to
implement. No firmware was changed. Every claim cites a record already in this
repository; where I extrapolate beyond the record I say so.

---

## 1. Why this analysis is possible at all

Twenty-one builds, fifteen numbered field findings, four field verdicts and
about fifty decision records cover eighteen days of flying. Almost every build
carried one named change and was flown; almost every stop was decoded from
telemetry rather than guessed at. That is an unusually complete controlled
series, and it supports a question that a single build never can:

**not "why did this build stop" but "what does the *sequence* of stops say
about the shape of the problem".**

---

## 2. The lineage: what each fix was, and what struck next

The column that matters is the third one. In almost no case is the new failure
a regression of the fix. In almost every case the fix worked, and the failure
that followed it was *the same underlying fault surfacing through a different
test*.

| build | the change | what struck next, and why |
|---|---|---|
| **0.1–0.2** | thirteen-finding review; the strike latches | the ten-magnet witness turns out to be a tautology (decision 0059) |
| **0.3** | the 0.1 review closed; Gaussian shape ceiling 0.13 live | **F01** a retained `state/auto` outlives the boot and blocks startup. **F02/F03** the shape test refuses two real, full-amplitude magnets (resid 0.1422, 0.1811) → one-marker lag → strike three and four markers later. **F05/F06** a single ADC sample (+41, −43) latches polarity at entry, negates the whole passage, `TOO_WEAK`, lag, strike |
| **0.4** | decision 0064: polarity is the sign of the *summed* passage, not the entry sample — a direct fix for F05/F06 | **F07** the same single-sample artifact, now at sample 165 of 175, cannot flip the pole any more — so it inflates `peak` to 313 and breaks the shape fit instead. `WRONG_SHAPE`, lag, strike. *The fix worked and the artifact moved downstream.* |
| **0.5** | decision 0065: judgement reads a median-of-three copy; the recording is never filtered | **F08/F09** the baseline latch, caught whole: a 9-minute passage. `updateBaseline()` refuses to sample inside an open passage; a passage 99 % open starves the 41-sample median; the frozen baseline keeps the passage open. Markers crossed inside it are invisible. A declaration cannot clear it; only a reboot does. **Present identically in 0.3, 0.4 and 0.5** — not caused by the change it was blamed on |
| **0.6** | decision 0068: the station machine returns, stopping points tunable per station and direction | **F10** the baseline is captured by the magnet it parks on — Bamboo CW 1853 → **1899**, an error larger than the 38-count entry margin. And Grillers CW cannot climb away: 31 s at PWM 90, wheels spinning, the operator's hand required |
| **0.7** | two references, one sensor: baseline frozen below PWM 25 (the measured tractive floor); Grillers stop offset −1, depart 110 | The gate worked *exactly*: 1830 in, 1830 out through a 30 s dwell; Grillers launched unaided in 7.6 s. **F11** and the gate exposed what was behind it — Grillers CCW coasts two markers past its stop, parks on a magnet, and the dwell passage is refused on shape (resid 0.2773) → strike |
| **0.8 / 0.9** | stop offsets set from the measured landings | **F12** a derailed power car corrupts shape and leaves amplitude alone (resid 0.1455 against 0.086 worst-of-42-after) — a *physical* cause for the residual excursions. **F13** Arches CCW parks in a 41-count fringe field — three counts above the entry margin — holds a passage open for **36.7 s**, and the real MM106 arrives in its tail. The whole thing closes as one object. The remedy proposed for F11 would not have caught it |
| **1.0X** | decision 0070: a passage may not span a stop | **F14** the apex is lost when a stall ends in a lurch: a stitched waveform with a 56-count step at the join, `STITCHED_REFUSED`. 99 markers clean, one stop, no miscount — the build did exactly what it was told |
| **X2–X4** | Arches CW stop offset +1 → +2 → +1 → 0 | three builds moving geography. No failure class closed |
| **X5** | no passage may open, or span a dwell, while standing still | introduced the stop-episode widening that turns *any* refusal near a stop into a shutdown — removed eight builds later as a cause of shutdowns |
| **X6 / X7 / X8** | departure-side provisional retention; guard the discard inside a field; a passage paused on the approach ramp can resume | escalating capture machinery, each patching the previous one's boundary |
| **X9** | the archaeology: a split passage is two fragments of one arc, reconstructed under nine measured rules | built to survive corrupted records. Gated, never flown |
| **X11 "Epiphany"** | decision 0073: one Hall sample is the median of five ADC reads | **the corruption the archaeology was built to survive was a single conversion.** Bench: 4 bad reads in 3,731 (X9) → **0 in 5,113** (X11). The artifact class of F05, F06 and F07 disappeared at its source. **F15** — and X11's subtitle pushed `state/bootid` to 399 bytes, `snprintf` truncated it, and four boots published unparseable JSON |
| **X13** | decision 0074: shape is recorded, not refused; the X5 stop-episode widening removed | **the best run in the record.** Two hours, both directions, 3,284 magnets, **zero** DISAGREE, 76 of 76 station stops. And: **0 of 3,284 passages would have been refused by the old shape rule** — with whole flanks restored, the shape test had nothing to object to |
| **X15** | decision 0083: a post-stop opposite-polarity successor may bypass the guard | Arches, 2026-09-11: a 318 ms opposite-polarity passage bypassed the guard and struck. Decision 0052's measured taxonomy says opposite polarity at 139–347 ms **is** the re-read signature (13/13), while the genuine next marker is opposite only 52.6 % of the time. **The exception selects the false population.** The implementation was correct; the specification was wrong |
| **X16** | decision 0085: an 82 ms completed-passage floor, for the Bamboo 47 ms transient | two stops on 2026-09-13; **the floor rejected nothing at either**. The real mechanism was the Northpoint latch: a sub-threshold **+26-count shelf** entered the unscreened rolling median, the reference moved to 1980, a magnet opened against it, the line fell back ~60 counts, and with an exit margin of 25 the passage **could not close** — 1,569 ms, 1,200 ms, 2,446 ms. Real magnets merged inside it; fragments fell inside the 500 ms guard. **This is F08/F09 again, twelve builds later** |
| **X17** | fixed-after-prime baseline; the rolling median demoted to shadow | the pre-flash audit predicted the field outcome would be *a latched passage* — the symptom under investigation. It also found **B1: exactly one capture in the gate suite ran the flashed policy.** "All 12 gates passed" was evidence about X16 |
| **X18** | SET LOCATION-anchored lap baseline, ±2 counts per lap; 82 ms floor | the as-flown fallback. Known defects deliberately retained (`openMigrateMs` feeding magnet samples to the reference after 2 s; a sign problem in `adjustBaseline()`) |
| **X19** | no closure, no exit margin, no PWM, no duration floor: a 70-count local excursion against a trailing 300 ms level | Arches, first outing: **three magnets counted while Otto stood still at PWM 0**, 96–106 counts off his resting level, advancing mm 109 → 112 without moving. Plus two riders the audit found unauthorized: lap-baseline application silently disabled, and a **continuously adaptive measured rest** (`restRef_` / `noteRest()`) introduced under cover of a localRef correction |
| **X20** | a station dwell suspends detection; rest frozen at dwell; REST vs IN_OLD_FIELD | six clean station stops. **MM136**: a genuine South opening, then a positive shelf later in the same 400 ms window won the window-wide argmax, and the record was published **North** — contradicting its own departure sign. Strike |
| **X21** | polarity fixed at the opening; navigation queued immediately; the 400 ms window demoted to telemetry; guard 500 → 645 ms | **MM117**: a ~30 ms burst of jittering conversions opened an event, polarity fixed instantly as North against an expected South, strike — and the real MM116, arriving 330 ms later, fell inside the 645 ms guard and was never counted. **X20 would have been right here by accident: the window was quietly supplying burst immunity and nobody had named that as a function.** And three of the four stops were Grillers MM60 → 59: `noteRest` crept onto the magnet shelf during the **twelve-second zero ramp** (rest 2163/2176 against a quiet line of ~1950), freeze-at-dwell froze an already-corrupted value, and departure back to the true line read as a −190 South excursion — a false MM60, then the real MM60 judged against MM59 |
| **SIMPLE / SIMPLIFIED** | the subtractive restart: two samples at 70 counts, detection → hard protection → transparent judgment | first run failed on things that were never the navigator: the departure ramp ran at **62 ms/count** because `o.stepMs` landed in `requestPwm`'s *down* slot; `NAVI_BASELINE_ADAPT_PWM` demanded at compile time and then unused; `state/bootid` advertising a 500 ms guard that is not in the code; the alert near-indistinguishable from X18's |

---

## 3. Seven mechanisms that generated the strikes

### 3.1 The lag-to-strike amplifier hides every cause

Almost none of these strikes happens at the marker that caused it. One
missed or invented event displaces the count; the strike surfaces later, when
polarity finally disagrees. `ROUTE_POLARITY` has long same-pole runs —
MM107–113 is seven Norths, MM144–147 four — so the lag can stay invisible for
up to six markers, ~1.8 m (decision 0059). F02 struck three markers late, F03
three, F05 three, F12 six.

**Consequence for the whole program:** the observable is systematically
detached in space and time from the cause. Until the six-passage waveform
window existed (0.3, decision 0063), *every* diagnosis in this line was a
guess about the wrong marker. The single highest-leverage instrument ever
built here was the one that let a stop be traced backwards.

### 3.2 Fixing a test relocates the fault; fixing the source removes the class

The clearest controlled experiment in the corpus:

```
single-sample ADC artifact
  0.3  →  latches polarity at entry        (F05, F06)   strike
  0.4  →  polarity now summed; artifact inflates peak and breaks shape  (F07)  strike
  X11  →  one sample is the median of five                              gone
```

Two builds of symptom-treatment, then one line at the acquisition layer, and
the entire class vanished — and with it the justification for X9's nine-rule
archaeology. Decision 0069 ("improve operation at the layer where the cause
lives") is not a style preference; it is the difference between two builds
that each bought a new strike and one build that closed a family.

### 3.3 Every refusal rule bought a strike and, on this railway's measured
data, prevented nothing

| rule | strikes it caused | events it correctly refused, measured |
|---|---|---|
| Gaussian shape ceiling 0.13 | F02, F03, F07, F11, F13, four shutdowns on 2026-09-02/03 | X13: **0 of 3,284** |
| amplitude floor 0.34 | F05, F06 (via the polarity latch) | X21 run: **0 of 97** below the floor |
| 82 ms completed-passage floor | — | **0 at either X16 failure** |
| the 500 ms guard, unconditional | the real MM116 (X21) and the real MM105 fragments (X16) | the rebound/re-read population, genuinely |

Only the spacing guard has an earned keep. The others share a structure: a
refusal has a **certain** cost — a missed marker is a lag is a delayed strike —
and an unproven benefit. On a navigator whose position is a single counter
validated by a polarity chain, *omission is not recoverable*; it is exactly as
fatal as false inclusion, only later and harder to trace.

That observation sits against decision 0043 ("admission favors recoverable
omission over false inclusion"), which is a proposed record and not ratified.
I am not citing it as authority in either direction, and I am not proposing to
change it. **It needs your ruling**, because almost every refusal rule in this
line inherits from it.

### 3.4 The reference is the root cause, and every reference architecture has
failed in the same shape

Five distinct designs, each defeated the same way:

| design | build | how it failed |
|---|---|---|
| rolling median, ungated | 0.3–0.6 | captured by the magnet it parks on: 1853 → 1899 (F10) |
| rolling median, gated on PWM 25 | 0.7 | worked perfectly — and exposed F11/F13 behind it |
| rolling median + `openMigrateMs` | X16 | unscreened sub-threshold shelf → passage cannot close → latch (Northpoint) |
| fixed after prime | X17 | audited as *predicted to latch* before flight |
| continuously adaptive measured rest | X19–X21 | learns the Grillers shelf during a 12 s ramp; departure from the shelf reads as a magnet |

The invariant underneath all five: **the detector's decision is a difference
against a reference estimated from the same signal the magnets are in.** Each
design differs only in which rule decides when the signal is quiet enough to
become reference — and each rule is defeated by a field that persists longer
than the rule's own time constant. On this railway, a slow station approach
into a fringe field produces exactly that, routinely. Twelve seconds of ramp
beats a 945 ms hold; 36.7 s of dwell beats a 1,025 ms median window.

**There is no time constant that separates "a long approach into a magnet"
from "a new resting level", because the railway produces both.** This is the
deepest structural finding in the corpus, and it is why the reference problem
has now recurred in five architectures across eighteen days. It is not a
tuning problem.

### 3.5 The entry/exit asymmetry is a latch generator by construction

Entry margin 70 (Otto) or 38 (Toby); exit margin 25. A passage that opens
against a *displaced* reference must return to within 25 counts of that wrong
reference to close. If the displacement exceeds ~45 counts the passage can
never close by itself. That is precisely F08/F09 (a 9-minute passage), F13 (a
36.7 s passage) and Northpoint (1,569 / 1,200 / 2,446 ms). The asymmetry was
introduced for hysteresis and became the mechanism that makes a reference
error unrecoverable rather than merely noisy.

### 3.6 Two time scales, and the disagreement between them is the best signal
in the record

MM136 and MM117 are the same fact from opposite sides:

* **MM136 (X20):** the opening was right (South) and the 400 ms window was
  wrong (a later shelf won the argmax) → published North → strike.
* **MM117 (X21):** the opening was wrong (a 30 ms conversion burst) and the
  window was right (the real South magnet was inside it) → published North →
  strike.

X21 removed the window's vote to fix MM136 and thereby lost the burst immunity
X20 had been getting without knowing it. Neither vote alone is sufficient.
But note what the X21 record actually says: the instrumented disagreement
between them — `sign(depart)` vs `peak_signed` on `diag/excursion` — **fired
exactly once in 97 records, on the failure.** Two records in 266 on X20, and
the first was the strike.

That is a near-perfect discriminator already built and already measured, and
in both builds it was used as telemetry rather than as a reason to hesitate.

### 3.7 The stationary regime defeats everything, and it is not an edge case

Otto and Toby stop on magnets (F11, F13, X19 Arches, X21 Grillers), stop in
fringe fields three counts above the entry margin (F13), ramp for twelve
seconds into a field (X21), and stand still for eighty seconds before a
re-declaration (X21 stop 3). Every reference architecture has been defeated
here, and X20's entire station-dwell machine — authorized and correct —
exists to defend X19's adaptive rest against this regime.

**A standing locomotive produced navigation advances on X19 (three at Arches,
PWM 0).** Nothing physical justifies a position change with the wheels
stopped. That should be a structural impossibility, not a suppression rule
with a reference that can be corrupted before the suppression arms.

---

## 4. Two process mechanisms that are visible in the outcomes

### 4.1 One change per flight is measurable, not merely tidy

The two single, measured, one-variable changes in the line produced the two
best results in the record: **X11** (0 bad reads in 5,113) and **X13** (3,284
advances, zero disagreements, 76/76 stops). The build that carried unstated
riders — **X19**, which quietly turned a narrow event-framing experiment into
a reference-management architecture — produced the Arches standing-still
advance, and its riders are the direct cause of the Grillers failures a month
later. X20 and X21 then spent their whole complexity budget defending it.

**Complexity added to defend an architecture is evidence against the
architecture, not for the complexity.**

### 4.2 The evidence path is load-bearing and has never been gated like it

F04 (the runlog's UTF-8 decode destroyed the first waveform dump), F15 (X11's
399-byte `bootid`, four unidentifiable boots), X19's first flash halting on an
oversized boot record, X21's `win_ms` underflow, SIMPLIFIED's alert being
indistinguishable from X18's and its `bootid` advertising a guard that is not
in the code. In a program whose entire method is field evidence, the telemetry
contract earns the same one-change discipline and the same gates as the
navigation path.

And X17's blocker B1 is the sharpest version: the gate suite exercised the
adaptive baseline while the image flashed the fixed one. **A gate that does
not run the flashed policy is not evidence about the build.**

---

## 5. What the combined model says an effective NAVI_ONE would be

These are conclusions from the record, stated as design properties. None is
authorized and none is proposed for implementation.

**1. Nothing in the detection path may refuse.**
Measured: every refusal rule except the spacing guard cost strikes and caught
nothing. Detection should be permissive and cheap; judgement should be the only
layer that can decline to advance, and it must be a layer that can also
*recover* — which the present single-counter design cannot.

**2. Position must be a hypothesis that can be wrong once.**
The one-strike counter converts every detection error into a session-ending
stop at an unrelated marker, six markers downstream. Decision 0076 reaches for
this; NAVI_SIMPLIFIED's ambiguity window (the opening observation is count 0,
up to ten subsequent qualifying observations may be judged) is the first design
in the line that can survive one wrong observation and re-derive the truth from
the next few. The record says that is not a luxury — it is the only thing that
converts the lag-to-strike amplifier from fatal into diagnostic.

**3. The reference must be locked per interval, never continuously adaptive.**
`recognize N → get clear of N → establish a clean level for the N→N+1 interval
→ LOCK → detect N+1 against it`. A locked reference can be *wrong*; it cannot
*migrate onto a magnet during a slow approach*, which is the failure mode that
has now recurred in five architectures. The open question the audit names is
the right one: **when is the track demonstrably clear enough to establish that
level** — and the answer cannot be a quiet timer, because a 36-second dwell in
a fringe field is quiet.

**4. There must be a second physical channel, and it should be distance.**
Every false event in the record is separable by arithmetic the firmware does
not do in real time. Spans are 300 mm throughout. The MM117 burst sits at
326 mm/s in a run of 240–263 mm/s — a 25 % jump at unchanged PWM 80 — while
the late lobe it swallowed lands at 240 mm/s, dead on cadence. The 436 ms
"magnet" needed PWM 144. Decision 0059's six-marker bound is a distance
problem and says so. NAVI_SIMPLIFIED's Hard Protection layer is exactly this,
and its stated discipline is the right one: per locomotive, per direction, per
mapped interval, with a **permissive fallback** when interval evidence is
missing — never a railway-wide constant. This has been item 4 of
`NAVI_ONE_NEXT.md` for three weeks and is, on this evidence, the single largest
unclaimed improvement available.

**5. Keep both time scales, and treat their disagreement as the signal.**
Not opening-only (MM117), not window-only (MM136). Advance on the opening,
because immediacy is worth having; but when the window's later verdict
contradicts the opening's sign, that is the one measured condition that
selected the failure in both builds — 1 in 97, 2 in 266. It should raise
ambiguity, not be logged.

**6. A stationary locomotive must be structurally incapable of advancing.**
Not suppressed by a rule whose reference can be corrupted before it arms.
Freeze at the *start of the deceleration ramp*, not at PWM 0 — the Grillers
migration happens entirely inside those twelve seconds, and `ExcursionDetector`
states the choice that permits it outright ("the dwell begins at PWM 0 and not
one sample earlier").

**7. Symmetric margins, or an explicit un-latch.**
An entry/exit asymmetry with no bounded passage duration makes a reference
error unrecoverable. Either close the gap or give a passage a hard ceiling
after which it is abandoned and the reference re-established.

**8. One change per flight; the gate must run the flashed policy; the
telemetry contract is part of the build.**

---

## 6. What should not be re-learned

Carried forward from the X18–X21 audit and confirmed across the wider series —
do not restore without new evidence: closure authority; duration floors as
navigation authority; whole-window polarity authority; morphology as navigation
authority (decision 0080); PWM- or speed-derived magnet *classification* (as
distinct from physical-plausibility bounds); an arbitrary old-field timeout;
and any opposite-polarity successor exception, which decision 0052's taxonomy
shows selects the false population.

And the two that the series proves are real and were bought expensively:
**the median-of-five acquisition** (X11) and **shape as diagnostic rather than
refusal** (X13, 0 of 3,284).

---

## 7. Open questions this analysis cannot answer

1. **Decision 0043.** Is recoverable omission genuinely preferable to false
   inclusion on a navigator whose position is a single counter? The operating
   record suggests omission is not recoverable. This needs your ruling before
   any refusal rule is designed into the next build.
2. **When is the track demonstrably clear?** The locked-baseline model stands
   or falls on this, and no dataset in the repository yet answers it directly.
   The X18 recorder was built precisely to produce that data; the
   `NAVI_BASELINE_*` series of 2026-09-14/16 is the start of the answer.
3. **Does the disagreement discriminator hold beyond 2 events in 363?** Two
   builds, three records. Suggestive, not measured.
