# NAVI_TRACER_0_2 — Calibration corpus review, dataset inventory, and relative-speed reference plan

**Status: review and inventory only. No evaluator experiment run. No
firmware. No adaptive-window or new filtering mechanism proposed or
implemented.** Produced in response to Sam and David's request to review
Experiments 1–3, incorporate the Codex relative-speed study, and
establish selection criteria and coverage before any further evaluator
work.

**CORRECTED 2026-10-23, see
`NAVI_TRACER_0_2_PWM60_PWM90_RUN_REPRODUCIBILITY_20261023.md`:** the
parser behind §2's passage counts and §5's reference-compatibility
numbers had a real bug — it silently dropped every landmark-named marker
line (`MM063 Grillers`, `MM072 Westpoint`, and six others), including
MM063, which sits inside the MM060-079 section this whole investigation
is about, and it caused runs passing through a landmark to be
artificially split in two. The counts and ratios below are superseded by
the corrected analysis in that document; left here unedited as the
record of what was actually reported, per this project's standing
practice of not editing a report away. Corrected totals: 10,503 raw
lines (not 10,014), 9,063 filtered passages (not 8,225). The
reconciliation conclusion — Codex's tight PWM≥70 compatibility claim
still doesn't reproduce, the MM060-079 directional finding still does —
is unchanged by the fix.

## 0. Responding to the Experiment 3 critique directly

Three points, taken in order, without softening them:

1. **The 49→48 stall is a data-selection problem, not evidence against
   segment speed.** Correct, and I conflated the two. A 63-second stop
   folded into a segment's average speed doesn't show segment-based speed
   estimation is unreliable — it shows that segment needed to be excluded
   from a *normal-running* speed estimate before using it. That's a
   filtering requirement, not a verdict on the method. Withdrawn as
   stated in Experiment 3.
2. **Retrospective waveform duration is not a prospective predictor.**
   Also correct, and an important distinction I elided. Experiment 3
   measured each waveform's own actual duration *after the fact* and
   showed it correlated with the window that worked — which explains a
   completed observation, not something NAVI could have known in advance.
   A real predictor has to be built from information available *before*
   the next magnet is reached: PWM, direction, position on the route, and
   whatever the locomotive has already learned about that combination
   from prior passages. The plan in §4 is built around that distinction.
3. **Two recordings, one of them disputed, is too thin a basis for
   conclusions about PWM or segment speed generally.** Agreed, and this
   review's job is to go find the larger evidence base before drawing
   those conclusions again.

## 1. Two categories of evidence, kept separate

**Movement-calibration data** — mm-to-mm distance, elapsed time, PWM,
direction, locomotive identity. No raw Hall waveform required. Supports a
relative-speed reference even without a usable Hall recording alongside
it.

**Waveform-evaluation data** — continuous native Hall recordings with
trustworthy confirmed encounters and real intervening background. Needed
to test the line-versus-curve evaluator itself; a relative-speed number
alone can't validate or refute an observation-window choice.

## 2. Movement-calibration inventory

### `field-records/cal/*.txt` — the June 29–30 corpus Codex used

**Located, already in this branch's working tree** (not something I had
to fetch): 16 non-empty files, 10,504 lines, committed at `8c573836`
("field-records: certification runs through 2026-08-03"), covering
2026-06-29 17:39 through 2026-06-30 21:04, Otto (9950011, 13 files) and
Toby (9950012, 3 files). `field-records/README.md` documents them as "as
captured," sixteen non-empty recordings (28 zero-byte starts on the Pi
are already accounted for and not missing data).

**Format**, one line per marker passage:
```
MM<n> obs=<N|S> expected=<N|S> pos=<EXACT_LOCKED|STALE|DECLARED|CONTESTED>
      dir=<CONFIRMED|DECLARED|CONTESTED> dt=<ms> dt_expected=<ms>
      timing_mode=<MOVING|SPEED_TRANSITION|STOPPED_INFERRED>
      effective_pwm_avg=<n> seg_mm=<n> speed_mm_s=<f>
      peakN=<n> peakS=<n> hallms=<n>
```

**Prior analysis exists and is directly relevant**:
`CURRENT/docs/HALL_SELECTIVITY_AND_GATE_TIMING_FROM_CAL_20260630.md`
(2026-09-18, read-only, no firmware changed) used three of these same
files (5,021 passages after its own filter: `pos=EXACT_LOCKED`,
`obs==expected`, `timing_mode=MOVING`, `seg_mm` 250–360, `dt>0`, plus a
neighbor-speed artifact filter removing double-count short-`dt` records)
to establish PWM-vs-speed and Hall-amplitude findings. Its stated caveat
matters here too: **this is 2026-06-30 data; Otto's Hall sensor
subsequently degraded and the board was replaced** (`§4`). The doc argues
§1/§2 (PWM-vs-speed physics) survive the vintage because replacing a Hall
board doesn't change how fast the locomotive moves — I'd extend that same
reasoning to a relative-speed profile for the same reason: it's a motion
question, not a sensor-health question. Worth stating as an assumption,
not a proven fact, since nobody has directly re-verified it.

**My own independent re-parse** of all 16 files (not just the three the
2026-09-18 doc used), applying the same documented filter plus the same
neighbor-speed artifact filter:

| Step | Count |
|---|---:|
| Raw marker-passage lines, all 16 files | 10,014 |
| After `EXACT_LOCKED` + `obs==expected` + `MOVING` + `seg_mm` 250–360 + `dt>0` + adjacent direction known | 8,268 |
| After neighbor-speed artifact filter | **8,225** |

By locomotive: Otto 7,116, Toby 1,109. By direction: CW 5,528, CCW 2,697.
By PWM decade: concentrated at 40–69 (4,542 of 8,225), with usable but
thinner coverage from 70 up through 240.

**This does not match Codex's stated 6,017.** My filter is the same one
documented in the precedent analysis; Codex's is described only as
"strict, stable-PWM adjacent-magnet passages after excluding contested,
inferred-stop, non-adjacent, and transitional observations" without the
exact criteria spelled out (e.g., whether "stable-PWM" requires PWM
constancy across a short run of consecutive records, which I did not
test for). I'm reporting the discrepancy rather than adjusting my filter
to match their count — the exact filter Codex used needs to be stated
before the two analyses can be reconciled.

### Other movement data mentioned but not yet located

- **"David has had segment-derived speed displayed on the NGR dashboard"**
  — I haven't located where this is persisted (if at all) versus
  computed live and discarded. Worth a direct answer from David: is there
  a retained log of dashboard-displayed segment speeds beyond what's
  already in the cal corpus and session telemetry?
- **Toby 9950012 coverage is thin** — only 1,109 of 8,225 filtered
  passages, all CW, concentrated at PWM 50–69. Matches Codex's own
  statement that cross-locomotive invariance can't yet be validated.

## 3. Waveform-evaluation inventory

### What's already been used (Experiments 1–3)

- **Fresh CW/PWM~90 run, 2026-10-22 22:49:53 PDT** — clean, 22 consecutive
  magnets MM034→MM056, no stops, no disputed geography. **Still usable**
  under the new criteria; nothing about this recording is disqualified.
- **Oct 22 CCW/PWM~35-60 Run 2, 15:31:48–15:41:57 PDT** — contains the
  63-second stall and the disputed MM045 identification. **Per this
  instruction, no longer used as evaluator test data.** (Its segment
  speed data, where independently confirmed and excluding the disputed
  and stalled portions, remains legitimate movement-calibration evidence
  — those are different uses of the same recording, and §1 above is the
  reason that distinction survives this exclusion.)

### Catalogued, not yet surveyed

All on the Pi at `/home/david/NGR/navi_sync/`:

| File | Size | Span | Note |
|---|---:|---|---|
| `navi_sync_20261015_100403.nsr` | 13.4 GB | 2026-10-15 10:04 → 10-22 15:42 (~7 days) | The file Experiments 1–3 pulled narrow slices from. **Almost entirely unexamined** — I've only looked at ~11 minutes of this ~7-day capture. Real candidate for clean lower/mid-PWM sessions without the Oct 22 Run 2 problems, but finding them means scanning, not assuming. |
| `navi_sync_20261022_163743.nsr` | 23.7 MB | 2026-10-22 16:37 → 22:51 | Contains the fresh PWM-90 run already used. Rest of its span (16:37–22:49) not examined. |
| `navi_sync_20260927_191922.nsr` | 10.4 MB | 2026-09-27 19:46 | Pre-dates NAVI_TRACER_0_1; era and build not confirmed compatible. |
| `navi_sync_20260927_195603.nsr` | 1.1 MB | 2026-09-27 20:00 | Same caveat. |
| `navi_sync_20260927_200313.nsr` | 15.1 MB | 2026-09-27 20:17 | Same caveat. |
| `navi_sync_20260927_214310.nsr` | 13.4 MB | 2026-09-27 21:54 | Same caveat. |
| `otto_navi_sync_20260928_101705.nsr` | 72.5 MB | 2026-09-28 11:13 | Same caveat. |

The native-Hall binary format (`REC_HALL_NATIVE`, parsed by
`navi_sync_format.py`) hasn't changed across this period as far as I
know, so these are plausibly readable the same way — but I haven't
confirmed any of them actually contain loco 9950011 native Hall data at
a clean, stable, non-transitional PWM, in either direction, which is
what the criteria below actually require.

## 4. Selection criteria for waveform-evaluation recordings

Stated explicitly, per the request:

1. Consecutive, independently trustworthy magnet encounters (confirmed
   `CLOSE_AND_ADVANCE` sequence, no disputed identification).
2. Continuous movement — no prolonged stops, no inferred-stop intervals,
   no interruption requiring re-declaration.
3. Stable PWM over the interval examined — not mid-ramp, not a
   deliberate bracketed-speed trial crossing multiple PWM regimes inside
   one segment.
4. Coverage across multiple PWM settings, both directions, and more than
   one section of the route, where available.
5. Complete Hall waveform with identifiable background on both sides —
   the raw native recording has to actually be present and extractable,
   not just the confirmed-close telemetry.

### Coverage against these criteria, right now

| Condition | Have it? |
|---|---|
| PWM ~90, CW, one section (MM34–56) | Yes — the fresh run |
| PWM ~90, CCW | No |
| PWM 30–70, any direction, *clean* (no stall/dispute) | **No** — the only PWM~35-60 waveform recording in hand is the excluded one |
| A second route section at any speed | No |
| Toby (9950012) waveform data | No — Experiments 1–3 only used Otto |

That's a real, specific gap, not a vague one: **there is currently no
clean low/mid-PWM native Hall recording to test the evaluator against at
all.** The movement-calibration corpus can describe *speed* at those PWM
settings; it says nothing about waveform shape, because it was never
designed to carry raw Hall data.

## 5. Reconciling the reference sections — what I independently found

Re-derived, from my own 8,225-passage filtered set, not from Codex's
table, using the same two candidate sections (MM035–055, MM115–125) and
grouping by locomotive/direction/PWM band:

- **PWM≥70, matched loco/direction/PWM-band groups (n=7):** ratio
  (MM115-125 median ÷ MM035-055 median) median **0.914**, range
  **0.821–1.118**. Codex reported median 1.006, range 0.989–1.044 (n=17).
- **PWM<70 (n=5):** ratio median **1.010**, range **0.777–1.195**. Codex
  reported this range as *less* compatible (0.828–1.091) than PWM≥70 —
  my own numbers show the opposite ordering: my PWM<70 groups cluster
  *closer* to 1.0 than my PWM≥70 groups do.

This does not reproduce Codex's headline claim that the two sections are
tightly compatible (±1-4%) at PWM≥70 and diverge below it. My own
re-derivation, using 3 broad PWM bands (70–99, 100–149, 150–249) rather
than Codex's presumably finer PWM matching (their n=17 "matched
conditions" suggests per-decade or per-setting matching, not broad
bands), shows real spread in both regimes and no clean crossover at
PWM 70.

**MM060–079 directional asymmetry — this part I can confirm
qualitatively.** My own numbers show the same *direction* of effect Codex
reports: CW consistently slower than the MM035-055 reference in this
section (ratios 0.422–0.895 across bands), CCW consistently faster
(1.008–1.449). That pattern is real and reproduces independently. The
*magnitudes* don't match Codex's table closely in every band (e.g., my
CW 70–99 ratio is 0.582 against their reported 0.78), which again points
at a PWM-binning or matching-criteria difference rather than a
disagreement about whether the effect exists.

**My assessment:** the qualitative finding — this railway has a real,
direction-dependent speed effect concentrated around MM060–079, large
enough to matter — is independently supported and should be trusted. The
*precise* reference-compatibility numbers and per-band ratios need
Codex's exact filter and PWM-matching criteria stated before they're
reconcilable with mine, and I'd treat the specific numbers in both
tables as provisional until that's done, not as a settled reference yet.

## 6. Proposed relative-speed reference, with limitations stated plainly

Given §5, I'm not proposing Codex's table or my own re-derivation as a
trusted reference yet. What I think is defensible to carry forward:

- The *existence* and *direction* of a route-dependent, direction-
  dependent speed effect (MM060–079 foremost) is established by two
  independent re-derivations from the same raw corpus.
- The *magnitude* at any specific PWM/direction/section combination is
  not yet settled to the precision either table implies, pending
  methodology reconciliation.
- Cross-locomotive transferability is unestablished — Toby's 1,109
  passages are all CW, PWM 50–69 only.
- No relative-speed number here has been connected to waveform duration
  or observation-window selection yet — that's §7's explicit purpose,
  not something to assume follows automatically.

## 7. Plan for connecting this to observation-window prediction (not executed)

Per the correction in §0.2, this has to be built as a **prospective**
predictor, tested against data the predictor didn't see.

Proposed design, for review before any of it runs:

1. **Build the predictor from information available before the
   encounter only:** last confirmed PWM, direction, and the mapped
   segment about to be travelled, combined with the relative-speed
   reference (once §5's reconciliation is resolved) to produce an
   *expected* speed for that specific segment, then an expected waveform
   duration from expected speed and the known physical magnet width
   (~34 mm, per the June corpus's own fitted figure in
   `HALL_SELECTIVITY_AND_GATE_TIMING_FROM_CAL_20260630.md` §3).
2. **Test against held-out clean recordings**, per §4's criteria — not
   the recording the prediction was built from. This is exactly why §4's
   gap (no clean low/mid-PWM waveform recording) has to be closed first;
   there's nothing to test the predictor against yet at those speeds.
3. **Report prediction error, not just correlation** — how far off was
   the *predicted* window from the window that Experiment 2's method
   actually needed for that encounter, known only after measuring it.
4. **No mechanism is authorized by this plan.** It's a test of whether a
   prediction is possible and how accurate it would be — not a proposal
   to build it into NAVI.

## 8. Immediate next step

Given §4's gap, I think the next concrete, boundable task — still no
evaluator run, no firmware — is scanning `navi_sync_20261015_100403.nsr`
for clean, stable-PWM, non-stalled stretches at PWM bands other than ~90,
in both directions, using the confirmed-close telemetry (not the Hall
trace itself) to find candidates before pulling any raw Hall data. That
directly closes §4's stated gap rather than leaving it as a named
limitation. I'd like confirmation before starting that scan, since it's
a new piece of work beyond this review.
