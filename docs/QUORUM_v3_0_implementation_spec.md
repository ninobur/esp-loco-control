# QUORUM — SOLONAV v3.0 implementation specification

**Repo:** `~/esp-loco-control` **File:** `firmware/SOLONAV/SOLONAV.ino` **Tag:** v2.17 → v3.0
**Navigator name:** QUORUM **SKETCH_NAME:** `"SOLONAV_3_0_QUORUM"`

Revision 2. Revision 1 was reviewed by CODEX; all twelve findings are incorporated,
and four integration gaps CODEX did not reach are resolved in §6.

---

## The model, in one paragraph

The locomotive knows where it is. A magnet that disagrees is assumed to be a bad
read, not a lost position — so the locomotive holds its position, ignores the
reading, and watches whether the *next* magnets fit the pattern it already
expected. Only when several in a row fail does it ask the second question: if not
here, then where? And it asks against a short list, because it was right a minute
ago.

Operator's statement of the principle: *"I am on the tracks. I am not flying. I
knew where I was a minute ago."*

**Why the old layer goes.** `navConfidence` is a tally. It can express *how much
am I disagreeing* but not *which position might I be in*, so when it empties it
discards position and rebuilds from nothing. Run 3 (2026‑07‑29, 993 markers): one
phantom inserted a false count; the twelve good readings after it all scored wrong
because the map was being read one row out; confidence hit zero, LOST fired, the
buffer cleared, and thirteen markers later the recovery concluded `off = -1` — a
fact available at reading five.

---

## §0 Scope

Rewrite **LAYER 3 — NAVIGATOR** (lines ~390–670) in full.

Layers 1, 2 and 4 keep their behaviour. The edits outside Layer 3 listed in §0.1
and §6.1 are **required and permitted**. No others.

Do not change: detector thresholds, the `NGR_DNA1` array, `spacingMm[]` values,
the station state machine's logic, or the PWM authority rules
(`requestPwm()`/`requestPwmOver()` remain the only normal writers of
`commandedPwm`).

### §0.1 Permitted edits outside Layer 3

| Location | Change |
|---|---|
| `drainMarkers()` | add `dt_expected`, `dt_conserve_ratio` to the marker payload; raise `char b[224]` → 320 |
| `navPublishState()` | add `nav_state`, `miss_streak`, score vector, lead offset, margin; raise `char b[384]` → 512 |
| `cruiseForPosition()` | remove `navState==NAV_TRACKING` from the guard, keep `navDir!=MAP_UNSET` |
| LocoConfig headers | add `EXTENT_FRONT_MM`, `EXTENT_REAR_MM` |
| 15 `navState` call sites | mechanical substitution — see §6.1 |

---

## §1 Definitions — load-bearing, do not paraphrase

### navMm

**`navMm` is the marker the locomotive believes it has just reached.**

It **advances by exactly one on every ACCEPTED event**, including events whose
polarity disagrees. An event is *accepted* when it passes the timing gate in §3.
Rejected events do not advance it.

**"Hold position on a disagreement" means DO NOT RELOCATE. It never means do not
advance.** The train physically passed a magnet; the odometer reflects that. What
is withheld is any re-interpretation of *where* that magnet was.

So after a disagreement at `navMm = 154`, the next event is compared against
`dnaAt(155)`, not `dnaAt(154)`.

### offset

**An offset is a displacement in EVENT STEPS ALONG THE DIRECTION OF TRAVEL**, not
an arithmetic marker number.

- `offset = +2` — the true position is two markers further along than the odometer says; two events were missed.
- `offset = -1` — the odometer counted one event too many.

Apply it **only** as:

```c
routeMod((int32_t)navMm + navDir * offset)
```

Never as `navMm += offset`. Under CCW `navDir` is negative and bare addition gives
the wrong marker; near MM000 it also needs the wrap `routeMod()` provides.

Because `navMm` advances on every accepted event throughout EVALUATING, **the
offset is constant while evaluating**. Adoption is one correction applied once,
not an accumulation over the readings collected.

---

## §2 State machine

```c
enum NavState : uint8_t { NAV_UNSET, NAV_NORMAL, NAV_EVALUATING, NAV_NO_QUORUM };
```

Transitions, and no others:

| From | To | Trigger |
|---|---|---|
| `NAV_UNSET` | `NAV_NORMAL` | operator `cmd/start_mm` (`navDeclare`) |
| `NAV_NORMAL` | `NAV_EVALUATING` | `missStreak` reaches `QUORUM_TRIGGER` |
| `NAV_EVALUATING` | `NAV_NORMAL` | adoption accepted |
| `NAV_EVALUATING` | `NAV_NO_QUORUM` | §2.3 |
| `NAV_NO_QUORUM` | `NAV_NORMAL` | operator `cmd/start_mm` **only** |

There is no automatic exit from `NAV_NO_QUORUM`. The operator re-declares
position exactly as at the start of a session.

While `NAV_UNSET`, `navOnMarker()` returns immediately without advancing,
scoring or publishing — preserving current behaviour.

### §2.1 NAV_NORMAL — the common path, keep it one comparison

```
navMm = nextMm(navMm, navDir)
push (reading, navMm) onto the evidence ring        // §2.4
if reading == dnaAt(navMm):
    publish "AGREE";  missStreak = 0;  update last-confirmed
else:
    publish "DISAGREE";  missStreak++
    // nothing else. no scoring, no search, no relocation.
```

1.3% of readings are bad. Being lost is far rarer. One disagreement is free.

`missStreak == QUORUM_TRIGGER (3)` → `NAV_EVALUATING`.
Three consecutive failures at 1.3% is about one in half a million.

### §2.2 NAV_EVALUATING

Candidate offsets: **`{ -1, 0, +1, +2, +3, +4 }`**

Asymmetric by measurement. A phantom inserts one spurious event, so the odometer
runs at most one **ahead**. Dropped events arrive in bursts: run 1 logged
`queue_drops 0→4` and the recovery returned `off = +4` — one stall destroyed four
events and put the odometer four **behind**.

On entry, score retroactively over the ring entries since the streak began (3),
then score each new accepted event as it arrives.

For each candidate, score against:

```c
dnaAt(routeMod((int32_t)ring[i].navMm + navDir * offset))
```

using the `navMm` recorded **with that reading**, not the current one.

**ADOPT** when one offset is the unique maximum *and* leads the runner-up by
`QUORUM_MARGIN = 2`:

```c
navMm = routeMod((int32_t)navMm + navDir * offset);
clear scores; clear the ring; missStreak = 0;  → NAV_NORMAL
publish "QUORUM_ADOPTED"   // old mm, new mm, offset, final scores
```

Simulated against the real DNA at the measured 1.3% error rate: median 6 readings
to a correct adoption, 0.05% wrong-adoption rate.

**UNRESOLVED** — a leader exists but margin < 2 → publish `"QUORUM_TIED"` with
every viable candidate (§4) and keep collecting.

Run 3 held −1 and +1 level at 4/4 for four readings because the map alternates
N,S,N,S through MM154–158 and an alternating run looks identical shifted either
way. It resolved on the fifth. **This is not lost.**

### §2.3 The two routes to NAV_NO_QUORUM — there are no others

**(a) Hard bound.** `QUORUM_MAX = 12` accepted events have been scored in
`NAV_EVALUATING` with no adoption. This applies whether candidates are tied,
close, or all poor. Twelve readings without a two-point margin means the evidence
does not identify a position.

**(b) Repeated adoption failure.** An adoption contradicted by `QUORUM_TRIGGER`
(3) *consecutive* disagreements immediately following it: revert `navMm` to its
pre-adoption value, publish `"QUORUM_REOPENED"`, return to `NAV_EVALUATING` with
the failed offset excluded. If a **second** adoption also fails this way →
`NAV_NO_QUORUM`.

> Three, not one. A newly adopted position is held to the same evidence standard
> as NORMAL. A single bad read is free everywhere or it is free nowhere.

### §2.4 The evidence ring

A ring of `QUORUM_MAX` entries, each `{ uint8_t polarity; uint8_t navMm; }`.

**Maintained continuously during `NAV_NORMAL`**, not created on entry to
`NAV_EVALUATING` — otherwise the three readings that triggered evaluation are
already gone. Rejected events (§3) are **not** pushed. Cleared on adoption, on
`navDeclare()`, and on any direction change.

This ring **replaces** `dnaBuf`, `dnaPush()` and `dnaMatch()`. See §6.2.

### §2.5 NAV_NO_QUORUM behaviour

```
requestPwm(0, NORMAL_STEP_MS)          // controlled stop
```

**Retain** `navMm`, `navDir`, `autoRunning`, last-confirmed, and the evidence.
Nothing is cleared. AUTO is not dropped, but the locomotive does not resume.

Publish `"NO_QUORUM"` retained, carrying the last confirmed marker and landmark,
markers travelled since, and the bound from §4.

**Do not reduce speed on entering `NAV_EVALUATING`.** The locomotive keeps its
speed while evaluating; only `NAV_NO_QUORUM` stops it.

> The old LOST dropped PWM to 60. June 14 data, light engine: PWM 60 gives
> 2216 ms per marker median and 3583 ms worst, against a 2500 ms navigation
> floor. The LOST response was driving the train into the regime that collapses
> the baseline.

Delete `navConfidence` and every constant and path that used it. Do not
reintroduce it as telemetry.

---

## §3 Timing gate — an event must earn its advance

Delete the unconditional advance at line 640 and its comment.

```
v            = (3.90 * pwmForModel - 99.2) mm/s
expected_dt  = spacingMm[interval] / v
```

`pwmForModel` is **`actualPwm`, not `commandedPwm`**. `commandedPwm` is the ramp
target; during a 700 ms ramp — comparable to one interval — it describes motion
the locomotive has not yet received. Sample `actualPwm` at event time.

**Suspend the gate and advance normally** when any of these hold:

- `actualPwm < 40` — outside the model's fitted range
- `abs(actualPwm - commandedPwm) > 10` — a ramp is materially in progress
- `navDir == MAP_UNSET`
- the previous interval is unavailable (first event, or after a stop)

### Conservation test — the whole rule

```
if abs((dt + previous_dt) - expected_dt) <= DT_CONSERVE_TOL * expected_dt:
        // two events inside one interval's worth of travel:
        // one magnet, two events
        do NOT advance navMm
        do NOT push to the evidence ring
        do NOT touch missStreak
        publish "PHANTOM_REJECTED"   // both intervals, sum, expected, ratio
```

`DT_CONSERVE_TOL = 0.30`, provisional, named constant.

Run 3: `914 + 57 = 971` against neighbouring real intervals of 941, 956, 1008.
Two genuine consecutive intervals sum to about 2× expected and cannot trigger it.

> **No short-fragment condition, deliberately.** An earlier draft also required
> `dt < 0.40 × expected`, which would have missed an even split such as
> `420 + 530`. Conservation alone is the general test.

**Do not attempt to decide which of the two events was spurious.** The first has
already been consumed. Declining to advance on the second leaves the net count
correct either way. If the consumed one was the phantom, its polarity produces a
single disagreement, which costs nothing.

No missed-marker advance-by-*k* in this version.

---

## §4 Viability and published extent

**Viable candidate:** any offset scoring within `QUORUM_MARGIN` of the current
leader. A candidate one point behind has not been excluded — that is precisely
why adoption is being withheld — so the published bound must cover it. Do not
publish bounds covering only the tied maximum; that asserts more certainty than
the navigator holds.

**Extent.** New per-loco config, in **millimetres**:

```c
#define EXTENT_FRONT_MM   /* sensor to pilot */
#define EXTENT_REAR_MM    /* sensor to rear coupler face, consist included */
```

Otto with three small cars is roughly 600 / 1200 — operator to measure.

**Do not store extent as a marker count.** Intervals run 270–355 mm here, so four
intervals is 1095 mm at MM125 and 1345 mm at MM000. A fixed count under-reaches
exactly where the track is tightest, putting a follower nearer the last car than
intended.

Walk `spacingMm[]` outward from the current interval until accumulated distance
covers the extent. In `NAV_EVALUATING`, start from the most conservative viable
candidate in each direction before walking.

**Naming.** `front_bound_mm` and `rear_bound_mm` carry **marker indices**, not
millimetres — "mm" throughout this codebase means *mile marker*. Say so in a
comment beside the payload; do not rename the existing fields.

---

## §5 Telemetry

Every decision must be reconstructable from the log without guessing:
`nav_state`, `miss_streak`, the full score vector, the leading offset and its
margin, plus `dt_expected` and `dt_conserve_ratio` on every marker.

Stamp `lastConfirmedMs` from `e.detectedAtMs`, not `millis()`. A 49-second loop
stall was recorded 2026‑07‑29; a fix currently reports fresher than it is.
`navDeclare()` has no event — keep `millis()` there and comment why.

---

## §6 Integration — the parts that break the build

### §6.1 navState consumers outside Layer 3

Fifteen sites reference `navState`. Add two helpers in Layer 3 and substitute
mechanically:

```c
static inline bool navPositionUsable();   // NORMAL or EVALUATING
static const char* navStateName();        // "UNSET"|"NORMAL"|"EVALUATING"|"NO_QUORUM"
```

| Line(s) | Current | Replace with |
|---|---|---|
| 827, 896, 911 | `navState==NAV_TRACKING` | `navPositionUsable()` |
| 1118, 1131, 1222, 1293 | the three-way ternary | `navStateName()` |
| 1233, 1296 | `navState==NAV_LOST` (lost_ms) | `navState==NAV_NO_QUORUM` |
| 1245 | alert level ternary | `NO_QUORUM`→`"NO_QUORUM"`, `EVALUATING`→`"EVALUATING"`, `NORMAL`→`"CLEAR"`, else `"UNSET"` |
| 1276 | `nav_ready` | `sessionDir!=MAP_UNSET && navPositionUsable()` |
| 1394–1395 | GO refusal | refuse on `NAV_NO_QUORUM` and `NAV_UNSET`; **allow** on `NAV_EVALUATING` |
| 1479 | reconnect alert | `navState==NAV_NO_QUORUM ? "NO_QUORUM" : "CLEAR"` |

**Stations must keep working during `NAV_EVALUATING`** — hence
`navPositionUsable()` rather than a NORMAL-only test. Position is held and
probably correct; suspending station arming for 3–12 markers would drive past a
station, which is the failure the existing arming comment warns against.

### §6.2 Delete the superseded recovery machinery

`dnaBuf`, `dnaBufLen`, `dnaPush()`, `dnaMatch()`, `pendingMm`, `pendingValid`,
`pendingConfirms`, `REACQ_WINDOW_MARKERS`, `REACQ_CONFIRMS`, `lastOdomDisagreement`
and `DNA_W` are all superseded by the evidence ring and the hypothesis set.
Remove them. There is no LOST-state search in v3.0; recovery from
`NAV_NO_QUORUM` is by operator declaration.

`routeMod()`, `nextMm()`, `dnaAt()`, `spanMm()` and `offsetToCentre()` are all
still needed. Keep them.

### §6.3 Direction change

`applyDirection()` changes `navDir` mid-session. On any change: clear the
evidence ring, clear scores, reset `missStreak`, and return `NAV_EVALUATING` to
`NAV_NORMAL`. Readings collected in one direction cannot be scored against
candidates in another.

---

## §7 Do not

- Do not score hypotheses during `NAV_NORMAL`. The common path stays a single comparison; complexity runs only when something is wrong.
- Do not keep `navConfidence` in any form, including as telemetry.
- Do not add any stop condition other than `NAV_NO_QUORUM`.
- Do not add a budget, timeout, lockout or watchdog.
- Do not add missed-marker advance-by-*k*.
- Do not change Hall thresholds, `NGR_DNA1`, or `spacingMm[]` values — a survey commit is pending separately.
- Do not modify the station state machine's phase logic; only its `navState` guard changes.

---

## §8 Verify

- [ ] Compiles for `LL_LocoConfig_9950011.h` **and** `LL_LocoConfig_9950012.h`.
- [ ] **Isolated disagreement.** One bad reading followed by agreements. `navMm` **advances normally throughout** — the locomotive is moving. What must not happen is relocation: no offset applied, `NAV_EVALUATING` never entered, `missStreak` returns to 0 on the first agreement.
- [ ] **Direction and wrap.** CCW, `navMm = 2`, adopting `+4` yields **MM169**, not MM6. CW, `navMm = 169`, adopting `+4` yields **MM2**.
- [ ] **Timing gate.** Event at dt 57 ms whose predecessor was 914 ms, `actualPwm` 100 → `PHANTOM_REJECTED`. Relative to the state **immediately before processing the 57 ms event**, `navMm` and the ring are unchanged. The 914 ms event is **not** rolled back; it was consumed and was the real magnet.
- [ ] **Even split.** `420 + 530` at expected 950 also triggers `PHANTOM_REJECTED`.
- [ ] **No false trigger.** Two ordinary intervals, `917 + 917` at expected 917, do **not** trigger it.
- [ ] **Ramp suspension.** `actualPwm` 65, `commandedPwm` 100 → gate suspended, event advances normally.
- [ ] **Run 3 replay from MM154.** `NAV_EVALUATING` at the third disagreement, `QUORUM_TIED` while −1 and +1 are level, `QUORUM_ADOPTED` at −1 when the alternation breaks. No stop.
- [ ] **Post-adoption tolerance.** One disagreement after an adoption does **not** reopen it. Three consecutive do.
- [ ] **Hard bound.** 12 accepted events in `NAV_EVALUATING` with a persistent margin of 1 → `NAV_NO_QUORUM`. Not indefinite collection.
- [ ] **NO_QUORUM.** PWM 0; `navMm`, `navDir`, `autoRunning`, last-confirmed all retained; retained alert published; only `cmd/start_mm` returns it to `NAV_NORMAL`.
- [ ] **Stations survive evaluation.** A station arms and completes normally while `NAV_EVALUATING`.
- [ ] **Direction change** clears the ring and returns `NAV_EVALUATING` to `NAV_NORMAL`.
- [ ] Bounds during EVALUATING cover every **viable** candidate, not only the tied maximum.
- [ ] Payloads do not truncate.

```
git tag -a v3.0 -m "QUORUM navigator: hold position on disagreement, wake nearby
hypotheses only on a run of failures, conservation timing gate, stop only when no
candidate fits"
```
