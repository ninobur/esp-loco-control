# Prior-aware candidate adjudication in QUORUM — investigation and design note

**Date:** 2026-08-11
**Trigger:** NO_QUORUM / HARD_BOUND, 2026-08-10, loco 9950011, towing the IR test car, wet rail.
**Capture:** `ngr-pi:/home/david/NGR/telemetry/runs/20260810_IR_SPEED_LOCAL_1_2_otto.log` (29 825 lines)
**Source:** `firmware/QUORUM/QUORUM.ino` @ `31d877a`
**Status:** investigation only. No code changed, no field behaviour changed.

---

## Recommendation

**REJECT** the prior-aware tie-break as scoped. **Neither +3 nor +4 was correct** — the locomotive
was at marker 18, offset **−5**, which the candidate fence cannot express. A rule that breaks that
tie adopts a position 8–9 markers (~2.5 m) wrong with a confident justification attached.

The tie is not evidence of a weak scorer. It is evidence that **the true hypothesis was never
scored**. That was true in all three NO_QUORUM events in this run.

Two conservative changes *are* warranted, and one of them needs no new algorithm — the firmware
already contains the exact code that would have identified the position. Details in §7–§8.

---

## 1. The incident, reproduced

```
1786420137.103  mm/no_quorum  {"mm":23,"since":12,"dir":"CW","sc":[5,3,5,5,6,6],
                               "ex":[0,0,0,0,0,0],"ld":3,"ru":4,"mg":0,"ev":12,
                               "ring":[["S",12],["S",13],["S",14],["S",15],["N",16],["N",17],
                                       ["N",18],["N",19],["S",20],["S",21],["N",22],["S",23]]}
1786420137.107  alert   reason:"HARD_BOUND"   dead_reckoned_mm:23  candidate_mm:26  viable:[-1,1,2,3,4]
1786420137.129  state/nav  event:"NO_QUORUM"  leader:3  runner_up:4  margin:0
```

Every element of the reported account verifies. Ring = `S S S S N N N N S S N S`; Hall health clean
(`hall_task_max_gap_ms` 2–3, `queue_drops` 0, `floor_rejects` 0). Recomputing the score vector from
`NGR_DNA1` (`QUORUM.ino:463`) through the scorer at `QUORUM.ino:996` with `MAP_CW=+1` reproduces
`[5,3,5,5,6,6]` exactly, so the scorer model is verified and is not implicated.

One correction to the framing: QUORUM entered EVALUATING at mm 14 (ts 1786420087.99), five markers
and 42 s **before** the Patio `DEPARTURE_COMPLETE`. The whole Patio approach, 15 s dwell and
departure ran inside an open evaluation — as reported.

---

## 2. The finding that reframes the request

### 2.1 The in-fence leader has no better-than-chance support

Scoring the ring against **all 171 alignments** of the route:

```
match  2:  4    match  5: 35    match  8: 13    match 11:  0
match  3:  9    match  6: 41    match  9: 11    match 12:  1  <-- unique perfect fit
match  4: 17    match  7: 38    match 10:  2
mean 5.994, sd 1.719  (clean binomial B(12,0.5) is 6.0/1.73)
```

The leader's 6/12 sits at the **62nd percentile** — 65 of 171 route positions explain the ring
*better* than the best in-fence candidate. The de Bruijn correlation caveat is real but negligible;
it does not lift the leader above chance.

More sharply: because the ring is an exact copy of a DNA window (§2.2), `score(c)` is exactly the DNA
autocorrelation at lag `5+c`. **The published score vector is a property of the map, not a
measurement of the locomotive.**

### 2.2 There is exactly one perfect explanation, and it is outside the fence

One alignment matches all twelve readings: **offset −5**, unique. The ring is byte-identical to
`DNA[7..18]`. The odometer read `mm 23`; the locomotive was at **marker 18**.

`QUORUM_OFFSETS = {-1,0,+1,+2,+3,+4}` (`QUORUM.ino:792`) cannot represent −5. The ring was never
corrupted — it is perfectly coherent. **The fence did not contain the truth.**

### 2.3 The same is true of all three NO_QUORUM events

| incident | line | stop `mm` | published `sc` | best fit anywhere | in fence? | truth |
|---|---|---|---|---|---|---|
| **A** | 14945 | 100 | `[7,3,7,8,4,6]` | **+8**, 12/12, unique | no (max +4) | marker 108 |
| **B** | 19608 | 87 | `[3,6,6,5,5,6]` | none ≥ 11/12 | n/a | unrecoverable (§4.2) |
| **C** | 25092 | 23 | `[5,3,5,5,6,6]` | **−5**, 12/12, unique | no (min −1) | marker 18 |

**Incident A has independent human ground truth.** After the stop the odometer read 102; the operator
walked to the train, read the physical marker, and declared **110** — exactly 102 + 8, matching the
polarity reconstruction. Drop bursts of 8 occur in the field, double the fence maximum.

Incident A is also a warning about loosening adoption: leader +2 scored 8/12 at margin **1**, one
point short, while the truth was +8. `QUORUM_MARGIN=2` is the only thing that prevented a confident
wrong adoption.

---

## 3. How incident C actually happened

Verified by a two-segment change-point fit: **0 mismatches across 39 events**, versus 8/39 for a
constant offset.

1. **Bamboo station stop.** `DWELL_BEGIN` 1786420020.65 (pwm 0) → `DWELL_COMPLETE` +15 s → throttle
   90 → `DEPARTURE_COMPLETE` at `actual_pwm` 29.
2. **The phantom rejector was switched off.** `GATE_LOW_PWM_FLOOR = 40` (`QUORUM.ino:795`) disables
   the conservation test below pwm 40, because the velocity model `v = 3.90·pwm − 99.2` goes
   *negative* below pwm 25.4. The gate published `LOW_PWM` and accepted unconditionally.
3. **Two phantom events were accepted** at lines 24662 / 24669 (pwm 12 and 19). Both near the
   detector floor: `peak` 39 and 40 against a run median of 80 and minimum of 38; one carried a
   1437 ms pulse against a p95 of 465 ms. The change-point lands exactly at **line 24669 / mm 160**:
   offset 0 before, **offset −2** after.

   Independently confirmed by geometry: this Bamboo departure covered 3 odometer positions in
   **4.32 s**, against **9.20–16.12 s** for the ten previous Bamboo departures in the same run.
4. **−2 is not in the fence.** The fence allows only −1 on the phantom side, by the explicit
   assumption at `QUORUM.ino:789`: *"a phantom inserts one spurious event, so the odometer runs at
   most one AHEAD."* **This run disproves that assumption** — phantoms arrive in pairs, through a
   gate hole, at station departures.
5. Disagreements at mm 1–3 → `missStreak` 3 → `QUORUM_OPEN` at mm 3.
6. **The fatal adoption** (line 24800):
   ```
   QUORUM_ADOPTED  scores:[2,1,1,2,4,2]  leader:3  runner_up:-1  margin:2  eval:4
                   old_mm:4  new_mm:7  offset:3
   ```
   **On that same 4-entry ring, the true offset −2 also scores 4/4.** The 2-point margin existed
   *only because −2 was absent from the candidate set*. This is the single most damning fact in the
   investigation: the scorer did not rank badly — the right answer was never on the ballot.

   Note also `beginNewEvaluation()` sets `evalCount = QUORUM_TRIGGER` (3) and calls
   `decideEvaluation()` immediately, so adoption can fire on **three** readings. It fired on four.
7. Odometer now **5 ahead**. The adoption "validated" on the first agreement — but
   `DNA[7],DNA[8],DNA[9] == DNA[2],DNA[3],DNA[4]` (all S), so the adopted and true hypotheses
   predicted **identical readings through the entire validation window**. Validation-by-agreement
   was structurally incapable of detecting the error.
8. The locomotive ran a **Patio station stop at the wrong place** — believed mm 15, was at mm 10.
9. A new evaluation opened at mm 14; at mm 23 the fence still could not contain −5; `evalCount` hit
   12 → HARD_BOUND → NO_QUORUM.

**The final refusal was correct and was the system working.** The defect is upstream.

---

## 4. Two distinct failure families

### 4.1 Displacement (incidents A, C) — the odometer is wrong, the readings are good
Recoverable in principle by a correct offset. In both cases the correct offset was outside the fence.

### 4.2 Corrupted observation (incident B) — the readings are wrong, no offset helps
Segment-2 change-point over 321 markers holds offset **0** at 96% (309/321); the odometer was fine.
The last 24 events tell the story:

```
mm 66 S | 67 S | 68 S | 69 S | 70 S | 71 S | 72 S | 73 S | 74 S | 75 S | 76 S | 77 S | 78 S ...
```

The detector was **latched at one polarity** through the stretch, at pwm 120, after a ~37 s stall at
the Grillers dwell marker. No alignment reaches 11/12; there is no offset to find. QUORUM refused
because the *readings* were garbage — the correct outcome.

**The slip hypothesis is supported here and disproved for incident C.** On the Bamboo→Patio leg,
current draw was 0.274 A against a 0.28 A baseline and per-marker `dt` sat within normal lap-to-lap
range — no slip signature. The incident-C error is **discrete event insertion, not continuous
distance drift**. Slip and stall are real, but they belong to episodes A and B, at Grillers.

### 4.3 Where the bad reads are — and an honest confound

Over spans where the odometer is independently known-good (1 813 reads), the run-wide misread rate is
**19/1813 = 1.05%**, and **18 of the 19 fall in the single stretch mm 66–82** (9.6% there vs 0.1%
elsewhere) — from just past Grillers (63) through Westpoint (72).

But speed and position are **confounded**: all 165 reads at pwm ≥ 110 in the entire run occur inside
that stretch, because the Grillers climb profile boosts PWM there. Splitting by matched speed:

```
pwm 90-109, mm 66-82      : 2/15  = 13.3%
pwm 90-109, rest of route : 1/1060=  0.1%
pwm >=110,  mm 66-82      : 16/165=  9.7%   (no out-of-zone comparison exists)
```

The matched-speed comparison supports a real position effect, but on 15 reads. **This run cannot
separate "defective markers at mm 66–82" from "detection fails above pwm ~110".** Both are
consistent. The discriminating experiment is in §9.

---

## 5. Could a bounded prior separate +3 from +4?

**No, and it must not try.**

- Neither is correct; the truth was −5, outside the fence. Ranking adopts a position 8–9 markers
  wrong.
- Separating them requires **6.5% distance accuracy over 4 605 mm**. The only distance model is
  `3.90·pwm − 99.2`, which the source itself calls provisional. Measured against reconstructed truth
  it over-predicts by **+12.9% run-wide** and **+25.1%** over this incident's ACTIVE intervals, with
  ~0.9 markers of 1σ scatter over 12 intervals — roughly three times too coarse.
- The bias runs the wrong way. Slipping or loaded wheels turn without moving the locomotive, so the
  model always **over**-estimates ground speed, manufacturing evidence for exactly the
  markers-were-dropped hypothesis it would be testing. A naive time-integrated prior over this
  window is off by **+90% (+10.8 markers)**, largely because the 31.3 s Patio dwell counts as one
  interval. **It would have confidently endorsed +4 — the worst candidate in the fence.**

Testing the obvious tie-break rules against all three incidents:

| rule | A (truth +8) | B (no truth) | C (truth −5) |
|---|---|---|---|
| prefer smaller \|offset\| | adopts 0 — wrong | adopts 0 — no basis | adopts +3 — wrong by 8 |
| prefer leader on any margin | adopts +2 — wrong | adopts 0 — no basis | adopts +3 — wrong by 8 |
| **today's behaviour** | **NO_QUORUM — correct** | **NO_QUORUM — correct** | **NO_QUORUM — correct** |

Base rate for this run: **1 adoption in 4 incidents, and it was wrong; in-fence coverage of the true
offset 0/3.** Three NO_QUORUMs, zero automatic recoveries, three human declarations — every correct
outcome in this run came from refusing to decide.

---

## 6. Local state, and how much of it is trustworthy

| state | location | usable as a prior? |
|---|---|---|
| `navMm`, `navDir`, `sessionDir` | 812, 451–452 | `navMm` is the thing under test |
| `lastConfirmedMm`/`Ms`/`markersSinceConfirmed` | 852–855 | **no — see below** |
| `previousAcceptedDt`, `lastMarkerMs`, `lastSegmentDt` | 840–843 | yes, inside ACTIVE only |
| `lastTimingGate`, `lastDtExpected`, `lastDtConserveRatio` | 846–848 | yes — they say when *not* to trust the rest |
| `spacingMm[]`, `spanMm()` | 475, 921 | **yes — a real distance routine already exists and is used** |
| velocity model | 802–803 | pwm ≥ 40, not mid-ramp; negative below pwm 25.4; +13–25% biased |
| station phase / route intent | 411 | **no** — all of it is *derived from* `navMm`, and `enterNoQuorum()` calls `stationReset()` (1132) |
| `agree`/`disagree` counters | published | **no — see below** |
| per-read quality: `peak`, pulse `ms`, `floor_rejects` | published per marker | **yes, and currently discarded** |

**`lastConfirmedMm` is not ground truth.** "Confirmed" means *one bit agreed with the map*, which
happens ~50% of the time at a wrong position. Verified twice: at the adoption the anchor was mm 0
while truth was mm 169 (`DNA[0]=DNA[169]=N`); at the final stop `lc_mm=11` while truth was mm 6
(`DNA[11]=DNA[6]=N`). All four post-adoption agreements were DNA-degenerate. The most attractive
prior input is precisely the one that inherits the error it is meant to detect.

**`agree`/`disagree` is not a confidence signal.** The 409/12 at the stop looks like a 3% long-run
error rate. In fact the counters were reset by a reboot 970 s earlier, after which there were 399
consecutive agreements and then **all 12 disagreements inside 48 s**. It is also degenerate between
"sensor is lying" and "odometer is displaced" — which demand opposite responses.

**There is no independent route intent.** `STATIONS[]`, `stPhase` and `stIndex` are all computed from
`navMm` via `offsetToCentre()`. Route intent cannot constrain `navMm` because it is downstream of it.

---

## 7. What the firmware already contains

`dnaMatch()` at `QUORUM.ino:1363` takes the last 12 polarity readings and searches for a **unique
exact 12-window match** within `±REACQ_WINDOW_MARKERS` (=5) of `navMm`, returning the marker or 255.
It is dead code — `__attribute__((unused))`, and the header at line 266 says so *by instruction*.

I verified the uniqueness claim at line 1348: **every window of length ≥ 10 is unique route-wide**
(W=9 has 4 collisions; W=10, 11, 12 have none). So an exact 12-match is globally unambiguous — it
identifies exactly one position or none.

Simulating it against the three incident rings:

| incident | `dnaMatch()` with window ±5 | unbounded exact search |
|---|---|---|
| A (`navMm` 100) | 255 — refuses (+8 is outside ±5) | **mm 108 — correct** |
| B (`navMm` 87) | 255 — refuses | **no match — correctly refuses** |
| C (`navMm` 23) | **mm 18 — correct** | **mm 18 — correct** |

**Zero false positives.** Because the match is exact and windows are globally unique, it either gives
the right answer or gives nothing. That is fail-closed by construction, in a way no probabilistic
prior can be. With the *existing* ±5 window it resolves tonight's incident exactly.

### Why it is dead, and why that caution still applies

The header is explicit about the lineage QUORUM replaced:

> the previous design asked "where am I?" from scratch at every marker … when the pattern matcher
> disagreed with the odometer, the matcher won — which is how a locomotive at MM133 came to believe,
> at certainty 1.000, that it was at MM105 travelling the other way.

That caution is sound and must be preserved. The proposal below differs in three ways: it runs
**once, at HARD_BOUND** (not every marker); it **publishes, never adopts**; and it demands an exact
unique match rather than a best score.

---

## 8. Recommended changes

### 8.1 Publish an exact-match position hint with the NO_QUORUM snapshot — *implement*
At HARD_BOUND only, run the existing `dnaMatch()` over the evidence ring and add the result to
`mm/no_quorum` as an advisory field (e.g. `"exact_match": 18` or `null`). **Still stop. Still require
operator declaration.** Nothing in the control path changes.

This converts "I am lost, come and find me" into "I am lost; the evidence uniquely indicates marker
18 — verify and declare." After incident A the operator had to walk to the train and read a physical
marker; this gives them a checkable hypothesis instead of a survey. It would have been correct in A
(with a widened search) and C, and silent in B.

Considering a modestly wider search window than ±5 is reasonable *for the published hint only*, since
exact matching is unambiguous route-wide. Auto-adoption from this hint is a separate decision with a
much higher evidence bar, and this note does not recommend it.

### 8.2 Close the low-PWM phantom hole — *implement*
**10.2% of accepted marker events in this run were never conservation-tested**, and the Bamboo
failure window had **seven consecutive ungated acceptances**. This is the direct cause of incident C.
The defence against phantoms is structurally absent exactly where phantoms are most likely.

Cheapest sufficient fix: enforce a minimum plausible inter-marker time during station dwell and
departure, derived from `spacingMm[]` at a floor velocity, instead of accepting unconditionally when
`pwm < 40`. The departure-window geometry gives a 2× margin (4.32 s observed vs 9.20 s minimum
normal), so this does not need the PWM velocity model to be accurate — only bounded.

### 8.3 Raise the adoption evidence floor — *gather evidence first*
Adoption can fire on three readings and did fire on four, producing the run's only adoption and a
5-marker error. But raising the floor alone is not obviously safe, and widening the fence with
today's floor would make confident wrong adoptions **more** likely — incident A came within one
margin point of exactly that. **Fence width and adoption floor must be redesigned together**, with a
decision record, and not before §8.1–8.2 have removed the conditions that generated the bad data.

### 8.4 Use the read quality already measured and discarded — *gather evidence first*
`scoreEntry()` is explicit: *"a weak or drifting read counts exactly as much as a strong one."* Yet
the phantoms carried near-floor `peak` values (39, 40 vs median 80) and an anomalous 1437 ms pulse,
and incident A's decisive reads had `peak` 180 and a 4 s pulse. Quality-weighting is local, invents
no position, and attacks the mode that dominated this run — but it changes adjudication, so it
belongs behind the same decision record as §8.3.

### 8.5 Distance-plausibility veto on adoption — *worth building, but after §8.1–8.2*
Strictly subtractive: eliminate candidates whose implied travel is physically impossible; adoption
still requires the existing margin among survivors. At the fatal adoption (anchor mm 0, 6 913 ms,
observed 190–225 mm/s, pwm 90, gate ACTIVE):

```
offset -1 -> 1000mm -> 145 mm/s  reject      offset +2 -> 1970mm -> 285 mm/s  reject
offset  0 -> 1315mm -> 190 mm/s  PLAUSIBLE   offset +3 -> 2285mm -> 331 mm/s  REJECT  <-- adopted
offset +1 -> 1640mm -> 237 mm/s  PLAUSIBLE   offset +4 -> 2585mm -> 374 mm/s  reject
```

Vetoing +3 leaves nothing at margin ≥ 2, so QUORUM keeps evaluating and stops near Southpoint 2
markers out instead of compounding to 5. It measures *marker count* between anchor and candidate, so
a constant anchor mislabelling cancels — which matters, given §6. It is the cumulative analogue of
the pairwise conservation test the codebase already trusts. It is a brake, not a compass: it could
not have found the truth in any of the three incidents.

### 8.6 The track — *gather evidence*
See §4.3: the misreads concentrate in mm 66–82, but speed and position are confounded. Run that
stretch at pwm ≤ 90 for a few laps and re-measure. If the errors vanish, it is a detection-speed
limit and the Grillers climb profile needs revisiting; if they persist, inspect the magnets.

### What would revive the original proposal
A ranking prior becomes worth revisiting only if, after §8.1–8.2, incidents still occur **with the
true offset inside the fence** and a genuine tie among *well-supported* candidates (leader ≥ 9/12,
not 6/12). That situation never arose in this run. §8.1 instruments for it directly: once the exact
match is published alongside every NO_QUORUM, the next incident answers this by observation.

---

## 9. Replay test suite

### 9.1 Harness
Extract the pure adjudication core — `scoreEntry`, `computeLeaderRunnerUpMargin`, `decideEvaluation`,
`beginNewEvaluation`, `handleFailedAdoption`, `adoptLeader`, `dnaMatch` — behind a header compilable
on the host, with `navMm`, `navDir`, the ring, `evalCount` and the DNA/spacing tables as explicit
inputs. Replay `(polarity, navMm, dt, pwm, gate)` tuples from a captured log.

**Cannot be replayed faithfully:** Hall ISR timing, queue-drop behaviour, real-time gate transitions.
Those need bench or field tests. State this in the suite README so coverage is not overstated.

### 9.2 Golden tests — the four real incidents

| test | input | must produce |
|---|---|---|
| `incident_A` | lines 14700–14960 | NO_QUORUM/HARD_BOUND at mm 100, `sc [7,3,7,8,4,6]`, margin 1 |
| `incident_B` | lines 19400–19620 | NO_QUORUM/HARD_BOUND at mm 87, `sc [3,6,6,5,5,6]`, margin 0 |
| `incident_C` | lines 25020–25120 | NO_QUORUM/HARD_BOUND at mm 23, `sc [5,3,5,5,6,6]`, margin 0 |
| `adoption_C` | lines 24780–24810 | today: adopt +3. Under §8.5: **no adoption** |
| `exact_match` | rings of A, B, C | §8.1 returns 108 / null / 18 — and **never** a wrong marker |

`adoption_C` is the discriminating test: the one place where correct behaviour differs from current
behaviour.

### 9.3 Regression corpus
Replay all 1 890 marker events. Freeze: 4 incidents (1 adoption, 3 NO_QUORUM), 2 `PHANTOM_REJECTED`,
and the full agree/disagree sequence. Any design must reproduce these exactly unless a named test
declares the change intentional.

### 9.4 Injected variants

| variant | mutation | must hold |
|---|---|---|
| slip | scale `dt` in a window by 0.6–1.8, markers unchanged | adoption rate must **not** rise |
| missed marker | delete 1, then 4 consecutive | offset +1 / +4; no adoption below the stated floor |
| missed burst beyond fence | delete 8 consecutive (reproduces A) | must reach NO_QUORUM; must **never** adopt |
| duplicate marker | duplicate at Δt = 40 ms | pairwise conservation rejects; `navMm` unchanged |
| phantom pair at dwell | insert 2 events at pwm 12–19 (reproduces C) | with §8.2: rejected. Without: offset −2 and NO_QUORUM, never adoption |
| latched polarity | force 13 consecutive identical reads (reproduces B) | must reach NO_QUORUM; §8.1 must return null |
| wrong polarity | flip 1, then 6 of 12 ring bits | 6-flip case must never adopt; §8.1 must return null |
| reversal mid-incident | flip `navDir` mid-evaluation | ring cleared or rescored; no adoption across the flip |
| **poisoned anchor** | set `lastConfirmedMm` 5 markers wrong | §8.5 verdicts **unchanged** — the key invariance test |
| stale route intent | `stationReset("MISSED")` then evaluate | no station state may influence adjudication |
| DNA-degenerate validation | adopt into a stretch where adopted and true predict identically | validation must not report success on non-discriminating evidence |

### 9.5 Invariants
1. With the new logic disabled, output is **bit-for-bit** identical to today.
2. §8.5 may only *remove* candidates; no input may make it promote one.
3. §8.1 may only *publish*; no input may make it alter `navMm` or the control path.
4. §8.1 must never return a marker that is not an exact unique match — a wrong non-null is a
   ship-blocking failure.
5. No input may turn a NO_QUORUM into an adoption unless the leader meets the stated evidence floor
   **and** margin ≥ 2.
6. Adoption count on slip-injected traces ≤ that on the clean trace.
7. NO_QUORUM remains reachable from every state.

### 9.6 Compounding test
Chain three synthetic incidents so each adopted position becomes the next one's anchor, with a
2-marker phantom injected before the first. Assert the accumulated error never exceeds the first
incident's error. This is the failure mode that actually occurred (§3 steps 6–8) and nothing in the
current suite covers it.

### 9.7 Acceptance
Ship only if: all three golden incidents still stop; `adoption_C` no longer adopts; `exact_match`
returns exactly 108/null/18; the regression corpus is otherwise unchanged; every §9.5 invariant
holds; and the poisoned-anchor test shows no verdict change. **Kill the change** if any variant
produces an adoption that today's code refuses, or if §8.1 ever returns a wrong marker.

---

## Appendix — reproduction

```bash
ssh david@192.168.68.142 "sed -n '25020,25120p' \
  /home/david/NGR/telemetry/runs/20260810_IR_SPEED_LOCAL_1_2_otto.log"
```

Analysis (exhaustive alignment search, change-point fits, per-marker error map, `dnaMatch`
simulation) is reproducible from the capture plus `NGR_DNA1` and `spacingMm[]` in `QUORUM.ino`.
Scripts are in the session scratchpad.
