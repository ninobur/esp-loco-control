# Review — the bounded post-stop provisional-passage resolver

**Date:** 2026-09-12
**Reviewing:** `docs/NAVI_POST_STOP_PROVISIONAL_RESOLVER_DESIGN_20260912.md`,
`tests/post_stop_resolver_model.h`, `tests/gate_post_stop_resolver.cpp`,
`tests/run_tests.sh`
**Verdict:** **DO NOT INTEGRATE YET.** The experiment is honest and the
quarantine half of it is right. But the three field cases are all one
phenomenon, and it is not a magnet-timing phenomenon — it is an acquisition
artifact at the departure ramp. Two of the design's own rules are inert or
architecturally impossible as written.

I verified every field case against the raw telemetry
(`~/ngr-telemetry/pi/NGR/telemetry/all_20260910.log`, `all_20260911.log`) rather
than against the design report.

---

## 0. Two corrections I owe first

**My §2.2 speed arithmetic was wrong as applied to these events.** I computed
the implied speed as (marker spacing − envelope width) / close-to-open gap, with
an envelope width of about 40 mm taken from cruise. That is right at cruise and
wrong at a departure: the decoded waveform shows the post-departure excursion
lasting **1962 ms** and covering roughly 235 mm of a 300 mm interval. When the
excursion is that wide, envelopes touch and a 136 ms or 318 ms close-to-open gap
is reachable. The design report's objection at lines 17–19 is correct and mine
was not.

**X16-A — the unconditional 500 ms guard I recommended — would have struck at
Grillers.** Verified: `2026-09-11T20:51:07.478` mm=64, S, peak 252, ratio 1.385,
gap 113 ms, `post_stop_successor:1`, ADVANCED. Refuse that and the navigator
stays at MM63; MM65 (N) then arrives against an expectation of MM64 (S) and
strikes at `20:51:08.807`. My recommendation was wrong on real data. Withdrawn.

---

## 1. What the three cases actually are

All three verified from telemetry. The structure is identical at two stations,
two directions, two nights.

| | Bamboo X14, 2026-09-10 18:20 | Arches X15, 2026-09-11 23:44 | Grillers X15, 2026-09-11 20:51 |
|---|---|---|---|
| first passage after dwell | MM160 N peak **84** ratio 0.525 gap 41725 | MM106 S peak **84** ratio 0.469 gap 41578 | MM63 N peak 170 ratio 0.934 gap 4758 |
| the same passage on ordinary runs | ~170, ratio ~1.00 | 140–143, ratio 0.77–0.79 | — |
| second passage | S peak 239 ratio 1.513 gap 136, **duration 2495 ms** | N peak 74 ratio 0.413 gap 318, **duration 1962 ms** | S peak 252 ratio 1.385 gap 113, **duration 2463 ms** |
| third passage | N peak 155 gap 3622, duration ~132 ms | S peak 113 gap 58, duration ~120 ms | N peak 157 gap 1204, duration ~125 ms |
| neighbouring passages | 226–338 ms | 226–338 ms | ~125 ms |

**The second passage in every case is six to twenty times longer than any of its
neighbours.** That is the phenomenon. It is not a re-read (139–347 ms, 40–103 ms
long) and it is not a magnet.

### The decoded waveform settles it

The Arches strike triggered `withdraw()`, so the six-slot window was published. I
decoded it (`WavHeader`, `all_20260911.log` 23:44:42.657–.696):

```
slot 0  N  peak 74   ratio 0.413  gap 318    dur 1962 ms   494 samples, dec=4
slot 1  S  peak 84   ratio 0.469  gap 41578  dur  298 ms   MM106
slot 2  N  peak 159  ratio 0.888  gap 2177   dur  338 ms   approach
slot 3  N  peak 124  ratio 0.693  gap 2150   dur  226 ms   approach
slot 4  N  peak 178  ratio 0.994  gap 2114   dur  263 ms   approach
slot 5  N  peak 155  ratio 0.866  gap 2438   dur  258 ms   approach
```

Slot 0 is **a flat plateau, not an arc**: it opens at 65 counts, wanders between
51 and 76 for two seconds with no apex and no rise, then drops to 17 at the end.
Slot 1 (MM106) is the normal shape — rises 66 → 84, decays cleanly to 19. A
magnet gives an arc. Slot 0 does not.

Slot 1 also shows the damage: MM106's arc is only about **18 counts above a
66-count pedestal**, which is why it read 84 where the same passage reads 140–143
on every other Arches CCW departure that night.

### The mechanism, with the numbers

`HallCapture::updateBaseline` freezes the baseline at or below
`NAVI_BASELINE_ADAPT_PWM`, which is **24** for Otto
(`LL_LocoConfig_9950011.h:264`), and delays it further by `openMigrateMs` once a
passage is open. A departure ramps at `AUTO_STEP_UP_MS` = 62 ms per count, so the
baseline is frozen through the first ~1.5 s of every departure while traction
current climbs. The published baselines:

* **Arches:** 1910 (pwm 0–31) → 1856 (pwm 47) → 1855 (pwm 76). A **55-count**
  move.
* **Grillers:** 1962 (pwm 0–42) → 1981 (pwm 58) → 1918 (pwm 107). A **63-count**
  swing.

`entryMargin` is **38** (`HallCapture.h:33`). Both departures move the reference
by more than the threshold that opens a passage.

This is not a new observation on this railway. `fixtures_arches_departures.h`
records Arches departures with a "pre-roll→flank jump" of **35, 62 and 74
counts**, and `gate_flank_discard.cpp` exists because of it.

### Why the credential appears to work

When the artifact **swallows the next real magnet**, the recorded peak is
magnet + pedestal and the polarity is the magnet's — strong, and the expected
pole. Bamboo: the artifact spans 136–2631 ms after MM160's close, and the real
MM161 should arrive at about 2484 ms (measured from the 0911 Bamboo CW
departures); the third passage at +3622 ms lands where MM162 belongs (2484 +
1180 = 3664 ms). Grillers: same shape, MM64 swallowed. When it **does not**
swallow a magnet, the polarity is the artifact's own and the peak is the pedestal
alone — Arches: N, 74, closing 58 ms before MM105's real 120 ms passage opened.

So "expected polarity and exceptional strength" is a proxy for *the artifact
happened to contain the next magnet*. That is a real discriminator, not a
coincidence — which is the strongest thing I can say for the design. It is also
exactly why it cannot be trusted; see B2.

---

## 2. Blocking findings

### B1 — the artifact already spans nearly a whole marker interval, and the rule cannot see it
`post_stop_resolver_model.h:43` — `examine()` never looks at
`closedAtMs − openedAtMs`.

Measured spans: Arches 1962 ms at ~120 mm/s ≈ **235 mm** of a 300 mm interval;
Grillers 2463 ms at ~126 mm/s ≈ **310 mm** of a 325 mm interval. The artifact is
already the width of one marker gap. A departure that ramps harder or a stop
placed differently gives an excursion spanning **two** markers — and the rule
would accept it as one, advance once, and silently lose the other. Position would
then be wrong by one marker with no strike and no record.

This is the unbounded failure mode. Duration is the one measurement that
separates these events from every genuine passage on the railway (226–338 ms at
Arches, ~125 ms at Grillers) and the model discards it.

### B2 — the strength credential sits exactly at the median of the ordinary population
`post_stop_resolver_model.h:30`, `kStrongRatio = 1.0f`.

`gain` is the trailing median of accepted peaks, so a ratio of 1.0 means "at
least as strong as the median magnet". Measured over `all_20260911.log`: **4550
accepted advances, median ratio exactly 1.000, only 51.0 % at or above 1.0**
(10th percentile 0.852, 1st percentile 0.739).

Probe result: a close successor with the expected polarity at ratio **0.99** is
quarantined. The design's prerequisite 1 asks whether a genuine close successor
below 1.0 exists. I can now answer the survey half of it: across every station
departure on 2026-09-11 — roughly 100 of them — **exactly one** produced a
sub-500 ms successor (Grillers 20:51), plus the Arches artifact. Every other
departure's second marker sits at 950–2500 ms. So the evidence base for the
credential is n = 2 genuine (1.513, 1.385) against n = 1 artifact (0.413), and
the threshold is placed at the median of an unrelated distribution.

The separation is real but it is a property of the *pedestal*, not of the magnet.
A pedestal of the same sign as the magnet inflates the peak; a pedestal of the
opposite sign would deflate it — which is exactly what happened to MM106 (84
instead of 141) and to MM160 (84 instead of ~170). **The credential would refuse
a genuine close successor whose pedestal happened to oppose it**, and both
failures contain a passage that was deflated that way.

### B3 — the companion/replacement rule is dead code
`post_stop_resolver_model.h:50-55`; asserted at `gate_post_stop_resolver.cpp:64`.

The branch requires `anchorGap >= kGuardMs`, and the very next branch
(`:58`) accepts anything with `anchorGap >= kGuardMs`. Every input that reaches
`ReplaceQuarantined` would otherwise reach `AcceptOrdinary`, with the same
acceptance. I confirmed it by probe: the Arches South component moved to **+700
ms** after the quarantined lobe closed — outside the 64 ms companion window —
still returns `AcceptOrdinary`.

The Arches case does not need the companion rule at all. With the North
quarantined the anchor stays at MM106's close, and the real MM105 opens **2338
ms** later (318 + 1962 + 58) — comfortably ordinary. The assertion at line 64
tests an enum label, not a behaviour.

### B4 — the model puts map identity inside the recognizer's decision
`post_stop_resolver_model.h:43`, `examine(const Candidate&, uint8_t expectedPolarity)`.

The design's own prerequisite 2 (design lines 77–79) says the Hall task owns
timing and gain and Navigator owns expected identity, with no cross-thread
mutation. The model as written cannot be split that way: the quarantine /
replace / consume state machine needs the timing state and the expected polarity
inside one decision, and that decision runs on the Hall task.

This also crosses `Navigator.h:131` — "THE NAVIGATOR DOES NOT TOUCH THE
RECOGNIZER" — and its history note about exactly this class of cross-thread
coupling. It is not a deferred integration detail; it is the shape of the design.
The only splitting that preserves the contract is the one CODEX originally
proposed: the recognizer emits a flag and refuses, and Navigator alone decides —
which means giving up the quarantine/replace state machine.

---

## 3. Non-blocking

* **N1 — the Grillers replay is synthetic.** `gate_post_stop_resolver.cpp:44-50`
  uses 1000 / 1140 / 1253 / 1450 / 2654 / 2794, encoding a **197 ms** MM64
  passage. The real one is **2463 ms**. The peak, gain and 113 ms gap are
  faithful; the timestamps are not, and the design report calls all three
  "recorded timestamps, peaks, gains and polarities" (lines 55–56). Bamboo and
  Arches are exact — Bamboo's 2496 ms and Arches' 1962 ms match the log to the
  millisecond. Grillers is the one case where the duration would have shown the
  artifact, and it is the one case where the duration was invented.
* **N2 — the model has no ordinary guard.** `post_stop_resolver_model.h:58`
  returns `AcceptOrdinary` for *any* gap when `active_` is false. Probe: a 40 ms
  re-read outside a post-stop window is accepted. That is defensible for a
  wrapper around the real recognizer, but `gate_post_stop_resolver.cpp:89` labels
  its check "500 ms boundary remains ordinary and unconditional" when nothing
  unconditional exists in this file.
* **N3 — decision 0052 does not bound this artifact.** Design line 43 cites the
  false population as "no higher than 0.280". On Otto's gain of 179 the Arches
  artifact is **0.413**, above that band. It is a different population and 0052
  says nothing about it.
* **N4 — an experimental gate now gates the production suite.**
  `run_tests.sh:58-61` runs it unconditionally. A failure in an experimental
  model would block a production release, and a pass may be read later as
  production coverage. Suggest a separate target or an opt-in flag.

---

## 4. What I verified

* Full suite re-run from the working tree: all 12 gates green, plus the new gate
  at **9 checks, 0 failures**.
* Repository state: only `tests/run_tests.sh` modified, three new untracked
  files, nothing staged, nothing committed, no production header, sketch,
  profile or decision record touched. Confirmed.
* Bamboo X14 (`all_20260910.log` 18:20:59.537): `NOT_A_MAGNET`, `TOO_SOON`, obs
  S, expected S, peak 239, ratio 1.513, gap 136, gain 158 — **exact match** to
  the design.
* Arches X15 (`all_20260911.log` 23:44:42.619 and .805): peak 74 ratio 0.413 gap
  318 `post_stop_successor:1`; then peak 113 ratio 0.631 gap 58 — **exact
  match**, including the derived 1962 ms and 2338 ms.
* Grillers X15 (`all_20260911.log` 20:51:07.478): peak 252, gain 182, ratio
  1.385, gap 113, ADVANCED, `post_stop_successor:1` — substantively confirmed;
  see N1 on the timestamps.
* The claim that both extremes fail is **correct**, and I was wrong to dispute
  it.

---

## 5. Recommendation

**Fix the artifact, not the ruling on the artifact.**

Everything above points at one cause: the baseline is frozen below PWM 24 while
traction current ramps, the reference moves by 55–63 counts against a 38-count
entry margin, and a two-second flat excursion opens. That excursion is what X15
admitted at Arches, what the 500 ms guard refused at Bamboo, and what carried
MM64 at Grillers. No rule downstream of `HallCapture` can classify it reliably,
because by the time the recognizer sees it the magnet and the artifact are one
object with one open time, one close time and one peak.

Proposed order of work:

1. **Publish `dur_ms`** on the marker JSON. Twelve bytes in a 640-byte buffer.
   Every argument in this review needed a duration and had to reconstruct it from
   publish timestamps; the next event should not.
2. **Measure the artifact properly.** Otto's next controlled run, no rule change:
   log every departure's baseline trace and first three passages. Roughly 100
   departures a night, of which about one produces this. That is the sample the
   strength credential needs and does not have.
3. **Then fix acquisition** — the candidates are re-baselining at `DEPART`, or
   lowering `NAVI_BASELINE_ADAPT_PWM` so the reference tracks the ramp, or
   holding admission until the baseline has settled. Each needs its own
   measurement and its own one-change build.
4. **Keep the quarantine disposition regardless.** The part of this design that
   is unambiguously right is that an ambiguous post-stop passage should be
   ignored and recorded, never struck. Had only that been in force on 2026-09-11,
   Arches would have refused the artifact, accepted MM105 at 2338 ms and carried
   on — and Bamboo and Grillers would be unchanged from what they already did.

If the resolver is to go ahead anyway, B1 (duration bound), B3 (drop the dead
companion rule) and B4 (the cross-core split) must be closed first, and B2 needs
the survey in step 2 before 1.0 can be defended.

---

## 6. The ask

> Previously we made a decision that was recorded — decision 0052's taxonomy and
> decision 0081's 500 ms guard. This is the situation and the decision: the
> Arches, Bamboo and Grillers events are all the same departure-ramp acquisition
> artifact, and I want to recommend fixing acquisition rather than adding an
> exception to the guard. I face a similar decision. May I use this as a guide to
> make the current decision?

And: **may I have Otto's next run instrumented for step 2 before any resolver is
integrated?** Nothing else in this review needs the locomotive to move.
