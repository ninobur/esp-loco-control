# QUORUM — SOLONAV v3.0 implementation specification

**Repo:** `~/esp-loco-control` **File:** `firmware/QUORUM/QUORUM.ino` **Tag:** v2.22 → QUORUM 1.0
**Navigator name:** QUORUM **SKETCH_NAME:** `"QUORUM_1_0"`

Revision 20. Thirteen review rounds with Sam (ChatGPT), then one with CODEX
reading against the actual source — which found four criticals thirteen rounds
of text review could not, because they live in the seams between this document
and `SOLONAV.ino`.
R1 — twelve, mostly under-specification. R2 — eight, of which three were genuine
bugs: reopening arithmetic, evidence rebasing, and rejected events poisoning the
timing predecessor. R3 — six, of which one was a bug: adoption validation never
terminated, so a long-confirmed correction could be undone by an unrelated
incident hundreds of markers later. R4 — six, of which one was a lifecycle risk:
recovery state had no defined end. R5 — three defects: reopening restarted the
incident it was meant to continue, the expected-interval formula was missing its
seconds-to-milliseconds conversion, and the conservation test indexed the
interval *leaving* the current marker instead of the one that ended at it.
R6 — five, of which one was fatal: `NO_PREV` invalidated the predecessor it had
just established, livelocking the gate so conservation could never run.
R7 — four, of which one was a gap: an accepted ACTIVE event never replaced the
timing predecessor, so it could go stale and the gate would stop comparing
adjacent intervals. R8 — two evidence-accounting gaps: `evalCount` not set to 3
after retroactive scoring, and the ring not stated to append during EVALUATING.
R9 — four: adoption before hard bound on the twelfth event, ordinary bookkeeping
on a confirming agreement, and two contradictory telemetry encodings. R10 — three:
the reopen path never ran the decision function, and stop-time predecessor
invalidation had no authorised call site. R11 — two: `NO_DIR` could not advance
`navMm` yet was told to accept, and `NAV_NO_QUORUM` was told both to close the
incident and to clear nothing. R12 — terminology only. R13 — one call-site
signature mismatch. **CODEX R1 — twelve, four critical:** the scoring operation
was never defined, `MarkerEvent` carries no PWM so event-time throttle was
unobtainable, a first-event `dt` of 0 would bootstrap a predecessor that rejects
the next genuine marker, and `cmd/start_interval` — what the console actually
sends — was not a recovery path. §4's extent work is deferred to M5 on its
recommendation. **CODEX R2 — eight, two critical:** the RAMP test still compared
an event-time value against a drain-time global, and PWM was to be captured at
event close while `detectedAtMs` comes from event open. Also `cmd/force_lost`
had no defined behaviour under QUORUM. Four integration gaps found in-house are
in §6.

**R17** — no new findings. Rebased onto v2.19, in which `loop()` can no longer be
blocked by MQTT. Three passages updated: §0 states the baseline, §3 re-justifies
the event-time PWM capture on correctness rather than on multi-second drain
delays, and §2.5 records why the snapshot may now safely precede the stop
request.

**R18** — CODEX round 3, seven findings, two critical: the 1 KB snapshot would
have truncated silently against v2.19's 512-byte queued payload, and a
non-retained snapshot would have been evicted from the drop-oldest queue during
exactly the network failure it documents. Also a station-machine strand on
`NO_QUORUM`, an incomplete permitted-edit list, stale line numbers, and a
missing parsing contract for `cmd/force_lost`.

**R19** — rebased onto v2.21, which split marker events onto their own publish
queue after 22,774 telemetry drops on 2026-07-30 left holes in the marker record.
No navigator logic changed. Three passages updated: §0 raises the baseline, the
new §5.1 assigns every QUORUM publish to its queue — in particular the retained
`NO_QUORUM` snapshot stays on `pub()` because `pubMarker()` forces
`retain=false` — and §8 verifies the assignment.

**R20** — CODEX round 4, seven findings, all transport/consistency; navigator
logic unchanged and §§1–4, 6, 7 remain as certified. The R19 claim that the
retain flag protects the terminal snapshot was false — retain acts at the
broker, not in the local drop-oldest queue — so §2.5 now specifies a desired
retained-state mechanism outside every queue, and §8's impossible eviction test
is replaced by the reconciliation test the mechanism actually passes. §0.1 and
§5.1 disagreed on whether scores ride `mm/marker`; the payload contract is now
stated once, exactly, with the `char b[320]` arithmetic shown. §2.5's station
reset names the existing `stationReset()` rather than a nonexistent overshoot
counter. Rebased onto v2.22, whose peek-publish-remove marker drain (capped at
8) closes the silent-discard hole CODEX finding 3 predicted and the 2026-07-31
outage test confirmed — markers 24 and 23 lost with `marker_pub_drops` reading 0.

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

**Baseline: v2.22.** This is written against a sketch in which `loop()` cannot be
blocked by the network. MQTT lives on its own task; `pub()` enqueues and returns;
the inbound command path is a queue drained by `loop()`. Measured after that
change: worst `loop_max_gap_ms` 80 ms across 1,117 samples, against 94,033 ms
before it.

v2.20 added the E-stop bypass (volatile flag, not the command queue) and bounded
the status drain to 4 per pass. v2.21 split marker events onto their own
publish queue — `pubMarker()` into `markerPubQueue` (64 slots, drop-newest) —
after status traffic on the shared queue evicted markers wholesale on
2026-07-30. v2.22 hardened that drain: `networkTask()` now drains markers by
**peek-publish-remove** — a marker leaves the queue only after `mqtt.publish()`
reports success, so a locally detectable publish failure retries next pass
instead of silently deleting the event — and the drain is **capped at 8 per
pass**, bounding the gap between `mqtt.loop()` calls so a marker backlog on a
degraded link cannot delay inbound E-stop reception. Status telemetry can be
dropped and re-sent; marker events happen once. The navigator publishes into
both paths, and §5.1 says which is which.

That matters for QUORUM specifically. The navigator's value is deciding in three
markers and acting on the sixth. Under the previous architecture a decision could
be reached and take ninety seconds to reach the throttle, which is worse than the
tally it replaces, because you would trust it. And the terminal-evidence
guarantees in §2.5 assume a transport that does not discard on locally
detectable failure. **Do not implement this on any sketch older than v2.22.**

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
| `drainMarkers()` | marker payload becomes exactly the §5.1 contract: existing raw event fields plus `timing_gate`, `dt_expected`, `dt_conserve_ratio` (`dt` is already present; `conf` is deleted with `navConfidence`). **Scores do not ride this message** — they ride the QUORUM decision events (§5.1). Raise `char b[224]` → 320; arithmetic in §5.1 |
| `navPublishState()` | add `nav_state`, `miss_streak`, score vector, lead offset, margin; raise `char b[384]` → 512 |
| `cruiseForPosition()` | remove `navState==NAV_TRACKING` from the guard, keep `navDir!=MAP_UNSET` |
| `struct MarkerEvent` + `detectorSample()` | add `uint8_t pwmActualAtDetect, pwmCommandedAtDetect`; capture **at event open**, alongside the existing `evStartBaseline` — see §3 |
| `commandedPwm`, `actualPwm` declarations | make both `volatile` — see §3 |
| `T_NO_QUORUM` | declare beside the other `T_*` topics; initialise in `buildTopics()` as `ngr/loco/%s/mm/no_quorum` |
| `cmd/force_lost` handler | replace entirely — see §6.5 |
| `serviceStations()` | expose `stationReset(const char* why)` for §2.5 |
| `servicePwmRamp()` | a static declaration and two statements at function entry, detecting the nonzero→zero edge on `actualPwm` and calling `invalidatePreviousAcceptedDt()` — see §3 |
| 15 `navState` call sites | mechanical substitution — see §6.1 |

---

## §1 Definitions — load-bearing, do not paraphrase

### navMm

**`navMm` is the marker the locomotive believes it has just reached.**

**Once position and direction are declared**, `navMm` **advances by exactly one
on every NAVIGATION-ACCEPTED event** — in `NAV_NORMAL`, `NAV_EVALUATING` and
`NAV_NO_QUORUM` alike, and including events whose polarity disagrees. An event
is *accepted* when it passes the timing gate in §3. Rejected events do not
advance it. In `NAV_UNSET` there is no position to advance and `navOnMarker()`
returns immediately (§2).

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

Because `navMm` advances on every navigation-accepted event throughout EVALUATING, **the
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
| `NAV_UNSET` | `NAV_NORMAL` | operator declaration — `cmd/start_mm` **or** `cmd/start_interval` |
| `NAV_NORMAL` | `NAV_EVALUATING` | `missStreak` reaches `QUORUM_TRIGGER` |
| `NAV_EVALUATING` | `NAV_NORMAL` | adoption accepted (incident stays open, §2.6) |
| `NAV_NORMAL` | `NAV_EVALUATING` | provisional adoption contradicted 3× — **reopen**, not a new incident |
| `NAV_EVALUATING` | `NAV_NO_QUORUM` | `evalCount` reaches `QUORUM_MAX` without adoption |
| `NAV_NORMAL` | `NAV_NO_QUORUM` | **second** provisional adoption fails validation |
| `NAV_EVALUATING` | `NAV_NORMAL` | direction change (§6.3) — full reset, evaluation abandoned |
| `NAV_NO_QUORUM` | `NAV_NORMAL` | operator declaration — `cmd/start_mm` **or** `cmd/start_interval` |

A direction change while in `NAV_NO_QUORUM` performs the §2.6 full reset of the
diagnostics but **does not leave the terminal state**. Only an operator position
declaration does. Readings collected in one direction cannot be scored against
candidates in another, but changing direction is not evidence about where the
locomotive is.

There is no automatic exit from `NAV_NO_QUORUM`. The operator re-declares
position exactly as at the start of a session. **Both** declaration commands
qualify: `cmd/start_mm` and `cmd/start_interval` each call `navDeclare()`, and
the console uses the latter. Recovery must not depend on which one the operator
happens to use.

While `NAV_UNSET`, `navOnMarker()` does not advance, score, push the ring or
update last-confirmed. The event **is still published** by `drainMarkers()` — §3
requires every received event to appear exactly once — with
`timing_gate = "NO_POSITION"`. It must not enter conservation, because `navMm`
holds no meaningful value to index `spacingMm[]` with.

### §2.1 NAV_NORMAL — the common path, keep it one comparison

```c
navMm = nextMm(navMm, navDir);
pushRing(reading, navMm);                    // §2.4

if (adoptionPendingValidation) {             // §2.3 owns the result entirely
    handleValidationResult(e, reading);      // never touches missStreak
    return;
}

if (reading == dnaAt(navMm)) {
    publish("AGREE");  missStreak = 0;  updateLastConfirmed();
} else {
    publish("DISAGREE");  missStreak++;
    // nothing else. no scoring, no search, no relocation.
}
```

The `missStreak` path applies **only** when `adoptionPendingValidation` is false.
During validation §2.3 owns the comparison outright.

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

**The scoring operation.** Scores are plain match counts over the entries
currently in the evaluation window:

```c
int8_t scores[6];                 // one per candidate offset
// initialised to 0 by beginNewEvaluation() / handleFailedAdoption()

for each entry in the evaluation window:
    for each non-excluded candidate c:
        if (entry.polarity == dnaAt(routeMod(entry.navMm + navDir * offset[c])))
            scores[c] += 1;       // match
        // mismatch adds nothing. No penalty, no weighting.
```

No `-1`, no quality weighting, no prior. A weak or drifting read counts exactly
as much as a strong one — quality flags are published but do not scale the vote.
Excluded candidates are not scored at all (§2.3).

`leader` is the highest score, `runnerUp` the second highest among non-excluded
candidates, `margin = leader - runnerUp`.

For each candidate, score against:

```c
dnaAt(routeMod((int32_t)ring[i].navMm + navDir * offset))
```

using the `navMm` recorded **with that reading**, not the current one.

**ADOPT** when one offset is the unique maximum *and* leads the runner-up by
`QUORUM_MARGIN = 2`:

```c
navMm = routeMod((int32_t)navMm + navDir * offset);
publishQuorumAdopted(oldMm, navMm, offset, scores, leader, runnerUp, margin);
clearScoresAndRing();  missStreak = 0;   // AFTER publishing
adoptionPendingValidation = true;  adoptionDisagreeStreak = 0;
→ NAV_NORMAL
```

Publish before clearing. Scores, leader, runner-up and margin are all gone
otherwise, and they are the record of why the adoption was made.

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

**(b) Repeated adoption failure.** An adoption contradicted by three
disagreements **while still under validation**. If a **second** adoption also
fails this way → `NAV_NO_QUORUM`.

> Three, not one. A newly adopted position is held to the same evidence standard
> as NORMAL. A single bad read is free everywhere or it is free nowhere.

**Validation ends at the first post-adoption agreement.** Without this the
adopted offset would remain provisional forever, and a dropped event hundreds of
markers later could "undo" a long-confirmed correction.

**While validation is pending it owns the comparison outright**, and a confirming
agreement must still do all the ordinary work — otherwise the locomotive holds a
confirmed position while its published last-confirmed marker sits behind the very
event that confirmed it.

```c
// on adoption
adoptionPendingValidation = true;
adoptionDisagreeStreak    = 0;

// §2.1 routes here before any missStreak logic runs
void handleValidationResult(const MarkerEvent& e, uint8_t reading) {
    bool agrees = (reading == dnaAt(navMm));
    if (agrees) {
        publish("AGREE");
        updateLastConfirmed(navMm, e.detectedAtMs);   // §5: detection time
        missStreak = 0;
        endSuccessfulIncident();    // clears provisional state, exclusions,
        return;                     // failure count, adoptedOffset (§2.6)
    }
    publish("DISAGREE");
    adoptionDisagreeStreak++;       // NOT missStreak — it stays 0 throughout
    if (adoptionDisagreeStreak == QUORUM_TRIGGER) handleFailedAdoption();
}
```

Without this precedence, three post-adoption disagreements would increment
`missStreak` *and* `adoptionDisagreeStreak`, firing `beginNewEvaluation()`
alongside `handleFailedAdoption()` — and the first would wipe the failure count
and exclusions the second needs.

So only `D D D` *immediately* after adoption proves it wrong. `A D D D` and
`D A D D D` both finalise the adoption at the `A`; the later streak is a new
incident that enters ordinary `NAV_EVALUATING` **without** removing the earlier
offset.

Post-adoption validation events are pushed to the ordinary evidence ring before
agreement testing, exactly like every other accepted `NAV_NORMAL` event.

**Reopening procedure — the arithmetic here is easy to get wrong.**

Three accepted events have passed since adoption. Those advances were real; the
locomotive moved. **Remove the correction from the current odometer. Do not
restore the marker held at adoption time.**

```c
navMm = routeMod((int32_t)navMm - navDir * adoptedOffset);   // correct
// NOT: navMm = preAdoptionMm;                               // discards 3 real advances
```

*Worked example.* `navMm = 154`, adopt `offset = -1` → 153. Three accepted events
→ 156. Adoption judged wrong. Removing the correction gives 157 — the position
the uncorrected odometer would now hold. Restoring 154 would throw away three
markers of travel.

**Rebase the evidence ring.** The three post-adoption entries recorded their
`navMm` in the *adopted* coordinate frame. Once the correction is removed they
are displaced by it, and scoring them unrebased tests every hypothesis against
the wrong map positions:

```c
for each retained entry:
    ring[i].navMm = routeMod((int32_t)ring[i].navMm - navDir * adoptedOffset);
```

Then, on reopening:

- the three post-adoption disagreements become the initial evaluation evidence
- scores are reconstructed retroactively over those three entries
- the failed offset is **excluded** from the candidate set
- the `QUORUM_MAX` counter restarts at 3
- `missStreak` resets to 0, having already served its purpose

**An excluded candidate is removed, not merely deprioritised.** It takes no part
in leader selection, runner-up selection, the margin test or the viability
bound. It is published as `null` in the score vector with its parallel
`excluded[]` entry set `true` (§5). Whatever integer remains in the underlying
array is irrelevant and must never enter any calculation — a stale numeric score
left in the vector could otherwise stay competitive and be re-adopted.

Publish `"QUORUM_REOPENED"` with the removed offset and the rebased frame.

**Both failures run one shared routine.** Scores were cleared at adoption, so
the second failure must *reconstruct* them before the terminal snapshot —
otherwise the snapshot reports an empty vector and the record of why the
locomotive stopped is lost.

```c
void handleFailedAdoption() {
    removeAdoptedCorrection();        // navMm -= navDir * adoptedOffset
    rebaseValidationEvidence();       // the 3 entries, same removal
    adoptionFailureCount++;
    excludeFailedOffset();
    clearScores();
    evalCount = 3;
    scoreEntries(the 3 rebased entries);
    computeLeaderRunnerUpMargin();

    if (adoptionFailureCount == 1) {
        navState = NAV_EVALUATING;    // same incident, continued
        publishQuorumReopened();      // record the failure first
        decideEvaluation();           // may adopt another offset at once
    } else {
        enterNoQuorum();              // snapshots the reconstructed vector
    }
}
```

The reconstructed three entries may already give a non-excluded candidate a
unique two-point lead. Calling `decideEvaluation()` here — the same function, not
a copy of its logic — keeps the reopen path consistent with
`beginNewEvaluation()`. Publish `QUORUM_REOPENED` before deciding, so the record
of the failed adoption survives even when a replacement is adopted immediately.

After the second failure the snapshot is internally consistent: `navMm` and the
ring entries share the uncorrected frame, the scores derive from those entries,
both failed offsets are excluded, and `evalCount` is 3.

The second failure is detected in `NAV_NORMAL`, since provisional validation
happens there. Normalising the frame before the snapshot matters — the retained
position must not sit in the coordinate frame the navigator has just rejected.

### §2.4 The evidence ring

A ring of `QUORUM_MAX` entries, each `{ uint8_t polarity; uint8_t navMm; }`.

**Once position is declared, every accepted event is appended — in
`NAV_NORMAL`, `NAV_EVALUATING` and `NAV_NO_QUORUM` alike.** Not created on entry
to `NAV_EVALUATING`; otherwise the three readings that triggered evaluation are
already gone. Rejected events (§3) are **never** pushed. Cleared on adoption, on
`navDeclare()`, and on any direction change.

Push once, then dispatch on state — the entry pushed is the entry scored:

```c
// after timing acceptance and navMm advancement
pushRing(reading, navMm);
switch (navState) {
    case NAV_NORMAL:      processNormalComparison();      break;
    case NAV_EVALUATING:  scoreNewestRingEntry();
                          evalCount++;
                          decideEvaluation();             break;
    case NAV_NO_QUORUM:   /* retained only, no scoring */ break;
}
```

**One decision function, adoption tested first.** `decideEvaluation()` runs after
the initial three-entry reconstruction, after a reopen reconstruction, and after
every new evaluation event — never duplicated inline:

```c
void decideEvaluation() {
    computeLeaderRunnerUpMargin();
    if (adoptionConditionMet())       adoptLeader();     // wins on event 12 too
    else if (evalCount >= QUORUM_MAX) enterNoQuorum();
    else                              publishQuorumTied();
}
```

Order matters on the twelfth event. If reading twelve is what finally produces
the two-point margin, the locomotive has identified its position and must adopt.
The hard bound means *twelve readings without a margin*, not *twelve readings
then stop regardless*.

The three triggering entries are already in the ring when `NAV_EVALUATING` is
entered. Retroactive scoring must **not** push them again.

Without this, the ring and the score vector describe different event sets, and
the terminal snapshot at the hard bound would not contain the twelve readings
`evalCount` claims.

This ring **replaces** `dnaBuf`, `dnaPush()` and `dnaMatch()`. See §6.2.

### §2.5 NAV_NO_QUORUM behaviour

```
requestPwm(0, NORMAL_STEP_MS)          // controlled stop
```

**Retain** `navMm`, `navDir`, `autoRunning`, last-confirmed, and the evidence.
Nothing is cleared. AUTO is not dropped, but the locomotive does not resume.

**On entry, in this order.** Step 1 is now a RAM write — it records the snapshot
into the desired-retained-state slot described below; nothing is enqueued and
nothing can block — so establishing it before requesting the stop costs nothing.
**If this mechanism is ever replaced by a synchronous publish, this order must
be reversed.**

1. **Snapshot the terminal evidence, before anything can overwrite it.** The ring
   holds only `QUORUM_MAX` entries, so deceleration events would otherwise
   destroy the very evidence that caused the stop — "nothing is cleared" is not
   the same as "everything is preserved." Snapshot once: the full score vector,
   the exclusion flags, all retained ring entries with their polarities and
   `navMm` values, leader, runner-up, margin, and `evalCount`. **One message,
   one topic, `ngr/loco/<id>/mm/no_quorum`, retained — delivered through the
   desired-retained-state mechanism below, never through a queue.**

   **It must fit 512 bytes**, matching `PubMsg::payload` (`char[512]`) so the
   same buffer discipline applies everywhere — anything longer truncates
   silently. Do NOT enlarge `PubMsg`: 32 slots at 1024 costs ~16 KB of RAM for a
   message published perhaps once a month. Encode compactly instead:

   ```json
   {"e":"NO_QUORUM","mm":123,"lm":"Bamboo","since":45,"dir":"CCW",
    "sc":[3,null,5,4,2,1],"ex":[0,1,0,0,0,0],"ld":2,"ru":3,"mg":1,"ev":12,
    "ring":[["N",120],["S",121],["N",122]]}
   ```

   Twelve ring entries at ~12 bytes is ~144; the rest is ~200. That leaves
   headroom. Short keys are acceptable here and nowhere else — this message is
   read by a human doing forensics, not by the dashboard.

   Assert the `snprintf()` return is under `sizeof(buf)` and publish a
   `SNAPSHOT_TRUNCATED` alert if it is not. A silently truncated forensic record
   is worse than a missing one.

   **The desired-retained-state mechanism.** R19 claimed the retain flag
   protected this snapshot in transit. That was false: retain is an instruction
   to the *broker* and takes effect only after the broker receives the publish.
   While the snapshot sat in the local drop-oldest `pubQueue` it was an
   ordinary, evictable entry — and `NO_QUORUM` fires preferentially when the
   broker is unreachable, which is exactly when that queue is churning.

   The snapshot therefore never enters a queue. One protected variable, outside
   every queue, holds the desired retained state of the `mm/no_quorum` topic:

   ```
   NONE      — nothing to reconcile
   SNAPSHOT  — the complete NO_QUORUM snapshot JSON is owed to the broker
               (regenerable from terminal state, which this section retains)
   CLEAR     — an empty retained clearing payload is owed
   ```

   Terminal entry sets `SNAPSHOT`. `navDeclare()` sets `CLEAR`. The network
   task, whenever it is connected and the state is not `NONE`, publishes the
   desired retained message; **only on publish success does the state return to
   `NONE`.** Routine telemetry cannot overwrite or evict it — it is in no
   queue — and after any reconnect the task reconciles the broker to the
   desired state automatically. Both directions get the same durability:
   establishing the snapshot and clearing it survive broker outages,
   reconnects, and queue churn identically.

   Retained still matters for its original reasons: the broker's copy survives
   the locomotive being switched off, which no in-RAM slot does, and a late
   subscriber sees it — matching `alert`, which is already retained.

   **Clearing on `navDeclare()`** is the `CLEAR` arm of the same mechanism, with
   the same guarantee. Without it the snapshot lingers as a ghost, exactly like
   the CTO2 r10 retained relics that were briefly mistaken for a second device
   on Otto's topics on 2026-07-30.
2. `requestPwm(0, NORMAL_STEP_MS)` — controlled stop.
2a. **`stationReset("NO_QUORUM")`.** The station machine must not retain a
   continuation that is no longer valid.

   Concretely: `NO_QUORUM` during `ST_DEPART` suspends station processing
   because position is unusable. The operator redeclares, `NAV_NORMAL` returns,
   and `ST_DEPART` resumes — but that phase assumes cruise was already
   requested and never requests it itself. `autoRunning` is still true, so `GO`
   refuses as already running. The locomotive sits stopped with a stale phase
   and no way forward. `ST_FINAL` fails similarly, resuming without its M+1
   timer, and every phase can resume against a coordinate that has since been
   redeclared.

   This is not new protective machinery. It stops an unrelated state machine
   from holding an invalid continuation after QUORUM has deliberately stopped
   the locomotive. Call the existing `stationReset("NO_QUORUM")` and clear the
   existing station-phase fields — `stationReset()` already returns the phase
   to `ST_IDLE`, disarms the station (`stIndex = -1`), and publishes the
   reason. No new station machinery.
3. Publish `"NO_QUORUM"` retained, carrying the last confirmed marker and
   landmark, markers travelled since, and the **list of viable candidate
   offsets** — not a computed occupancy bound, which is M5 (§4). The retained alert
   need not carry the whole snapshot; the snapshot has its own publication.

**Retain** `navMm`, `navDir`, `autoRunning`, last-confirmed, and all recovery
diagnostics — scores, exclusions, `evalCount`, leader, runner-up, margin. AUTO is
not dropped, but the locomotive does not resume. Call
`closeIncidentNoQuorum()`, never `endSuccessfulIncident()` (§2.6).

**Marker handling while in `NAV_NO_QUORUM`.** The locomotive is decelerating and
will pass more magnets. Those events are real and must not be pretended away:

```
continue applying the timing acceptance gate
accepted events continue advancing the nominal navMm
accepted events are appended to the evidence ring
do NOT score candidates
do NOT attempt adoption
do NOT update lastConfirmed
do NOT leave NAV_NO_QUORUM
```

Issue the stop request and the retained alert **once, on entry**. Do not reissue
either on subsequent markers. The ring may now overwrite freely — the snapshot
already preserved what mattered.

Advancing the odometer here records that the locomotive moved during
deceleration; it does not claim the position is confirmed, and the operator's
`cmd/start_mm` replaces it regardless. Freezing would be defensible, but it
would contradict §1's rule that every accepted event advances `navMm`, and one
rule with no exceptions is worth more than a marginally tidier stop.

**Do not reduce speed on entering `NAV_EVALUATING`.** The locomotive keeps its
speed while evaluating; only `NAV_NO_QUORUM` stops it.

> The old LOST dropped PWM to 60. June 14 data, light engine: PWM 60 gives
> 2216 ms per marker median and 3583 ms worst, against a 2500 ms navigation
> floor. The LOST response was driving the train into the regime that collapses
> the baseline.

Delete `navConfidence` and every constant and path that used it. Do not
reintroduce it as telemetry.

### §2.6 Recovery incident lifecycle

A **recovery incident** begins when `NAV_NORMAL` first enters `NAV_EVALUATING`.
Without an explicit end, a failed adoption today would make an unrelated failure
next week count as "the second" and stop the train — and an offset excluded
during one incident would stay excluded forever.

Recovery-only state:

```c
bool     adoptionPendingValidation;   // adoption not yet confirmed
uint8_t  adoptionDisagreeStreak;      // disagreements while pending
uint8_t  adoptionFailureCount;        // failed adoptions THIS incident
int8_t   adoptedOffset;               // NO_ADOPTED_OFFSET when none
bool     candidateExcluded[6];        // per candidate, THIS incident
uint8_t  evalCount;                   // accepted events scored, vs QUORUM_MAX
int8_t   scores[6];

constexpr int8_t NO_ADOPTED_OFFSET = INT8_MIN;
```

**Zero is a valid adopted offset**, so it cannot double as "none". Use the
sentinel, or treat `adoptionPendingValidation` as the validity flag and never
read `adoptedOffset` without it.

**Two distinct entry paths into `NAV_EVALUATING`. They must not share one
initialisation routine** — a reopen also arrives from `NAV_NORMAL`, and running
the incident-begin code there would erase the failure count and the exclusion
that reopening exists to preserve, making a "second failure" unrecognisable and
letting the failed offset be adopted straight back.

```c
void beginNewEvaluation();      // missStreak reached 3 in ordinary running
void handleFailedAdoption();    // provisional adoption contradicted 3 times
```

`beginNewEvaluation()` — a **new incident**:

```c
void beginNewEvaluation() {
    adoptionFailureCount = 0;
    clearCandidateExclusions();
    clearScores();
    // the three triggering entries are ALREADY in the ring — do not re-push
    scoreEntries(the 3 entries of the triggering streak);
    evalCount = 3;                        // NOT 0, and not 6
    decideEvaluation();                   // may adopt without a fourth marker
}
```

`evalCount` starts at **3** because three accepted events have already been
scored. Leaving it at 0 would allow twelve *more* readings, so the hard bound
would fire at fifteen rather than twelve.

Both entry paths end in `decideEvaluation()`. If those three entries already give
one candidate a unique two-point lead, there is no reason to wait for a fourth
marker — and using one function stops the entry path and the per-event path
drifting apart later.

`handleFailedAdoption()` — the **same incident, continued** (§2.3). It performs
the equivalent reconstruction and also leaves `evalCount = 3`.

**Returning to `NAV_NORMAL` after an adoption does not end the incident.** The
incident stays open through provisional validation and ends only at the first
confirming agreement. Do not run any incident-reset routine on the
`NAV_EVALUATING → NAV_NORMAL` transition.

**An incident ends** on any of:

| event | action |
|---|---|
| adoption receives its first confirming agreement | clear all recovery-only state; `adoptedOffset` invalid |
| `navDeclare()` | full reset, below |
| direction change | full reset, below |
| entering `NAV_NO_QUORUM` | snapshot first (§2.5), then **close** it — see below |

**Full reset** — `navDeclare()` and direction change both clear *everything*
recovery-related, not just the ring:

```c
clear the evidence ring;      clear scores[];
clear candidateExcluded[];    evalCount = 0;
missStreak = 0;               adoptionPendingValidation = false;
adoptionDisagreeStreak = 0;   adoptionFailureCount = 0;
adoptedOffset = NO_ADOPTED_OFFSET;      invalidate previousAcceptedDt;
lastMarkerMs = 0;                       // so the first dt after declaration is 0
```

`lastMarkerMs` must reset too. It currently survives `navDeclare()`, so the
first interval after a declaration would span a stop, a session gap, or an
operator carrying the locomotive to the track. Resetting it makes that first
`dt` zero, which §3 already refuses to bootstrap from.

**Two different endings. Do not use one routine for both:**

```c
void endSuccessfulIncident();   // after a confirming agreement: clears
                                // scores, exclusions, evalCount, failure
                                // count, provisional state, adoptedOffset
void closeIncidentNoQuorum();   // on NAV_NO_QUORUM: clears ONLY the
                                // provisional-adoption state, retains all
                                // diagnostics
```

`closeIncidentNoQuorum()` is not empty. It clears exactly two things, so no
stale provisional state survives into the stopped state:

```c
adoptionPendingValidation = false;
adoptedOffset             = NO_ADOPTED_OFFSET;
```

Entering `NAV_NO_QUORUM` must **not** call `endSuccessfulIncident()`. Scores,
exclusions, `evalCount`, the ring, leader, runner-up and margin are all retained
until `navDeclare()` or a direction-change full reset. `navState ==
NAV_NO_QUORUM` already prevents further processing, so no extra flag is needed.

Otherwise every `navPublishState()` while stopped would report an empty score
vector, and the diagnostic record of why the locomotive stopped would survive
only in the one-off snapshot.

**Within an incident**, on a failed adoption: `adoptionFailureCount++`, exclude
the failed offset, reopen (§2.3). On the second failure in the same incident →
`NAV_NO_QUORUM`.

Every configured offset becomes eligible again at the start of the next
incident.

---

---

## §3 Timing gate — an event must earn its advance

Delete the unconditional advance at line 640 and its comment.

```c
float velocityMmPerSec = 3.90f * pwmForModel - 99.2f;
float expectedDtMs = (1000.0f * spacingMm[conserveIntervalIndex])
                     / velocityMmPerSec;
```

**Units.** The velocity model is in mm/s while `dt` and `previousAcceptedDt` are
in milliseconds. The 1000 is not optional: at PWM 100, v = 290.8 mm/s over a
280 mm interval gives 0.963 **seconds**, and comparing 0.963 against a `dt` of
914 makes every conservation test meaningless. Name the variable `expectedDtMs`
so the unit travels with it, even though the payload field stays `dt_expected`.

**Both PWM values must travel with the event, and both must be sampled at the
same instant.** Add two fields to `MarkerEvent`:

```c
uint8_t pwmActualAtDetect;      // actualPwm
uint8_t pwmCommandedAtDetect;   // commandedPwm
```

**Capture at event OPEN, not close.** `detectedAtMs = evStartMs`, and `dt` is
opening-to-opening, so the aligned PWM sample is the one at opening. A magnet
event lasts 40–200 ms and longer at the low speeds this gate exists to diagnose;
during a ramp the throttle moves materially in that window. Capturing at close
would give the event a timestamp and a throttle from different moments.

There is already a precedent to follow exactly: `evStartBaseline` is saved when
`evActive` becomes true and used at close for `baselineDrift`. Add
`evStartPwmActual` and `evStartPwmCommanded` beside it, set them in the same
`if(!evActive)` block, and copy both into the event when it closes.

**Why both, not just actual.** An earlier revision captured only `actualPwm` and
left the RAMP test comparing it against the *global* `commandedPwm` at drain
time — two values from two different moments.

Note the justification has changed with v2.19 and the requirement has not. The
original argument was that a 49-second stall could separate detection from drain
by minutes; that gap is now under 35 ms and the argument is stale. The
requirement stands on correctness instead: `dt` is measured opening-to-opening,
so the throttle that produced that interval is the throttle at the opening, not
whatever it is when the event is processed. During a 700 ms ramp, 35 ms of drain
delay is still 5% of the ramp, and a 200 ms magnet event spans a great deal more
than that. The gate must be computed entirely from values captured at one
instant because that is what makes it a measurement rather than an approximation.

**Synchronisation.** `actualPwm` and `commandedPwm` are written on core 1 and
read here on core 0. Aligned 32-bit access is atomic on ESP32 hardware, but that
is not a compiler visibility contract — declare both `volatile`. Read them into
locals **adjacently, in the same statement pair**, so the two cannot straddle a
target change:

```c
evStartPwmActual    = (uint8_t)actualPwm;
evStartPwmCommanded = (uint8_t)commandedPwm;
```

Neither is used for control; a one-tick skew is immaterial. A skew of several
seconds, which the drain-time read allowed, is not.

**`conserveIntervalIndex` — the interval that ENDED at `navMm`, not the one
leaving it.** Conservation asks whether the previous accepted event and the
current event divided one physical interval into two pieces. `navMm` is the
marker the *previous* accepted event reached, so the interval under test is the
one behind it.

Verified against run 3: real magnet 153, phantom at +914 ms, real magnet 154 at
+971 ms. The interval split was **153→154**, and `navMm` was already 154 when the
57 ms event arrived.

Given `spacingMm[n]` = CW distance from marker *n* to *n+1*:

```c
uint8_t conserveIntervalIndex = (navDir == MAP_CW)
                              ? routeMod((int32_t)navMm - 1)
                              : navMm;
```

CW at MM50, the previous interval was MM49→MM50 → `spacingMm[49]`.
CCW at MM50, the previous interval was MM51→MM50 → `spacingMm[50]`.

This is the inverse of the naive "interval leaving the current marker," and an
off-by-one matters wherever surveyed spacings differ — 270 mm against 355 mm is
a 31% error, comparable to the whole tolerance.

**Gate evaluation, in this exact order.** `NO_PREV` is a *bootstrap*, not a
suspension — it establishes the predecessor rather than invalidating it. Getting
this backwards livelocks the gate: accept under NO_PREV, invalidate, next event
is NO_PREV again, forever, and conservation never runs at all.

**Two levels of acceptance, and they are not the same thing:**

- **received** — the detector event is real and is published. Nothing else.
- **navigation-accepted** — it also advances `navMm`, enters the evidence ring
  and participates in comparison or scoring.

Everywhere else in this document, an unqualified **"accepted event" means
navigation-accepted**. `NO_DIR` events are received only, and are the sole case
where the distinction bites.

```c
if (navState == NAV_UNSET) {
    gate = NO_POSITION;                        // direction may be known; position is not
    publishMarkerWithoutNavigationAdvance();
    return;                                    // no timing history to invalidate
}
else if (navDir == MAP_UNSET) {
    gate = NO_DIR;
    publishMarkerWithoutNavigationAdvance();   // received, NOT accepted
    invalidatePreviousAcceptedDt();
    return;
}
else if (e.pwmActualAtDetect < 40)                  { gate = LOW_PWM; navAccept(); invalidatePreviousAcceptedDt(); }
else if (abs(e.pwmActualAtDetect - e.pwmCommandedAtDetect) > 10) { gate = RAMP;    navAccept(); invalidatePreviousAcceptedDt(); }
else if (!previousAcceptedDtValid)            { gate = NO_PREV; navAccept();
                                                if (dt > 0) {          // <- REQUIRED
                                                    previousAcceptedDt = dt;
                                                    previousAcceptedDtValid = true;
                                                } }
else                                          { gate = ACTIVE;  conservationTest(); }
```

**`NO_DIR` cannot be navigation-accepted, because there is no direction to
advance in.** `MAP_UNSET` is `0`, so `nextMm(navMm, MAP_UNSET)` returns `navMm`
unchanged — it would not fail, it would silently fail to advance while still
pushing a ring entry at a duplicate position. Silent corruption, not a crash.

A `NO_DIR` event must not advance `navMm`, push the ring, touch `missStreak` or
`adoptionDisagreeStreak`, score a candidate, or update last-confirmed. It is
published with `timing_gate = "NO_DIR"` and nothing more.

**This is defensive handling, not a reachable operational path.** Both
`cmd/start_mm` and `cmd/start_interval` currently refuse to declare unless
`navDir` is set, invalid session directions are rejected, and NEUTRAL preserves
the previous direction. The branch exists so that a future ordering change
cannot produce silent duplicate ring entries — **do not reorder the declaration
handlers to make this branch reachable.**

**A zero `dt` must never become a predecessor.** The existing detector sets
`lastSegmentDt = 0` on the first event of a session, because there is no
preceding timestamp. Storing that zero as a valid predecessor makes the next
genuine marker test `0 + 917 ≈ 917` — one expected interval — and it is rejected
as a phantom. The first real marker of every session would be destroyed.

`LOW_PWM` and `RAMP` navigation-accept their event *and* invalidate timing
history — those intervals were traversed outside the model's validity and must
not seed a later test. `NO_DIR` invalidates but does not navigation-accept. An
event accepted under `NO_PREV` **establishes** the new predecessor; the event
after it runs `ACTIVE`.

### Two timestamp streams — do not merge them

`dt` and `previousAcceptedDt` have different lifecycles and the conservation test
depends on the difference.

| | advances on | computed where |
|---|---|---|
| `lastMarkerMs` / `dt` | **every received event**, including `NO_DIR` and `PHANTOM_REJECTED` | existing site in `navOnMarker()`, unchanged |
| `previousAcceptedDt` | only per §3's gate rules | the gate |

`dt` must remain *detector-event to detector-event*. Run 3 proves it: the 57 ms
interval is measured from the phantom to the real magnet, and if `lastMarkerMs`
had skipped the phantom the pair would never have summed to one interval. Do not
make `dt` accepted-event-to-accepted-event.

Publication ownership: `drainMarkers()` continues to publish every received
event exactly once, now carrying `timing_gate`, `dt_expected` and
`dt_conserve_ratio`. `publishMarkerWithoutNavigationAdvance()` is a description
of that same existing publication for a `NO_DIR` event, **not** a second
publisher — do not add one.

### Conservation test — the whole rule

```c
float combinedDtMs = (float)dt + (float)previousAcceptedDt;
float errorMs      = fabsf(combinedDtMs - expectedDtMs);

if (errorMs <= DT_CONSERVE_TOL * expectedDtMs) {
    // two events inside one interval's worth of travel: one magnet, two events
    // do NOT advance navMm
    // do NOT push to the evidence ring
    // do NOT touch missStreak
    // do NOT replace previousAcceptedDt          <- see below
    publish("PHANTOM_REJECTED");   // both intervals, sum, expected, ratio
} else {
    acceptEvent();                 // advance navMm, push the ring, compare polarity
    previousAcceptedDt      = dt;  // <- REQUIRED
    previousAcceptedDtValid = true;
}
```

**Use `fabsf`, not `abs`.** The Arduino `abs()` macro truncates floats and will
silently give the wrong comparison here.

**Every accepted ACTIVE event replaces the predecessor.** The variable means *the
interval of the last accepted event*, so it must advance with each acceptance —
otherwise a stale 914 keeps being tested against every later interval and the
gate stops comparing adjacent events at all. Only `PHANTOM_REJECTED` leaves it
unchanged.

`DT_CONSERVE_TOL = 0.30`, provisional, named constant.

### previousAcceptedDt — the name is the specification

**A rejected event must never become the timing predecessor.** The variable holds
the interval of the last *accepted* event, and rejection leaves it untouched.

Without this the gate destroys itself. Take the measured sequence
`914 → 57 (rejected) → 879`. If the rejected 57 replaced the predecessor, the
next test is `57 + 879 = 936` against an expected ~950 — inside tolerance, so the
**real** marker is rejected too. And since rejection also declines to update the
predecessor, it stays at 57 permanently and every subsequent event is rejected.
The odometer stops advancing altogether.

With the rule correct: `previousAcceptedDt` remains 914, the next test is
`914 + 879 ≈ 2 × expected`, and the real marker passes.

**Invalidate `previousAcceptedDt`** on:

- `navDeclare()`
- any direction change
- the locomotive coming to a stop, or `actualPwm` reaching 0
- a **received** `NO_DIR` event, or a **navigation-accepted** event under
  `LOW_PWM` or `RAMP` — never under `NO_PREV`

**The stop transition needs its own call site**, because it happens in the PWM
machinery with no marker event to hang it on. Three paths zero `actualPwm` — the
NEUTRAL interlock, the E-stop clamp, and the ramp decrement reaching zero — so
detect the edge once at the top of `servicePwmRamp()` rather than patching all
three — a static declaration and two statements:

```c
static void servicePwmRamp(){
    static int lastSeenActual = 0;
    if (lastSeenActual > 0 && actualPwm == 0) invalidatePreviousAcceptedDt();
    lastSeenActual = actualPwm;
    // ... existing body unchanged
```

Edge-triggered, not level-triggered — do not invalidate repeatedly while already
stopped. Detection lags by one loop pass (~35 ms), which is immaterial against
~900 ms marker intervals.

Without this, a dwell followed by a restart could leave the pre-stop interval
valid as an `ACTIVE` predecessor, and the first marker after the ramp settles
would be tested against an interval from before the stop.

After invalidation the next eligible event is accepted under `NO_PREV`, becomes
the new predecessor, and **cannot itself be conservation-rejected**. The event
after that runs `ACTIVE`. Clean restart, not a contaminated one.

**This model is explicitly provisional, and it contradicts the project's own
governing principle.** `ROAD_TO_CTO.md` states that PWM is a request, not a
result, and that what it produces varies with grade, load, battery state,
railhead condition and motor temperature. Using PWM as a velocity proxy is
therefore a stopgap for M1, not the answer — the answer is M2's wheel sensor.

Two consequences to expect and measure, not to design around:

- on a loaded climb a genuine split may fail to conserve against an optimistic
  estimate, and the phantom goes undetected — a missed catch, which is safe;
- on an unusually fast section, model error moves genuine timing nearer the
  rejection band — a false catch, which costs one marker.

The wide `DT_CONSERVE_TOL` exists to absorb this. Replay must cover load, grade,
direction and battery state, not merely odometer displacement.

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

**Viable candidate:** an offset is viable when

```c
leaderScore - candidateScore < QUORUM_MARGIN     // strictly less than
```

In words: **it trails the leader by fewer than two points.** Strict, not `<=` —
adoption fires the moment the leader is ahead by exactly two, so a candidate two
behind has by definition been defeated.

A candidate one point behind has *not* been excluded — that is precisely why
adoption is being withheld — so the published bound must cover it. Do not publish
bounds covering only the tied maximum; that asserts more certainty than the
navigator holds.

### DEFERRED TO M5 — do not implement in v3.0

Everything below this line in §4 is **out of scope for this change**, on CODEX's
recommendation and by the project's own filter.

`ROAD_TO_CTO.md` places published bounds and train extent in M5, after M1 and
M2, and "The filter" classifies consequence management as work to be minimised.
Consist-aware bounds also require physical measurements that do not yet exist —
Otto's numbers are estimates and Toby's are unknown — while §8 requires the
sketch to compile for both profiles. And it would be a second behavioural change
in a run that must attribute one.

**What v3.0 does publish:** the score vector, exclusions, leader, runner-up,
margin and the list of viable candidates. That is everything needed to *test*
M1. What it does not do is convert any of it into consist-aware occupancy
bounds.

Retained here because M5 will need it, and because the reasoning cost an evening
to establish.

---

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
margin, plus `dt_expected`, `dt_conserve_ratio` and `timing_gate` on every
marker.

**When timing values are unavailable** — no valid predecessor, gate suspended,
`e.pwmActualAtDetect < 40`, or direction unset — publish them explicitly rather than
omitting them:

```
dt_expected       = 0
dt_conserve_ratio = -1.0
timing_gate = "ACTIVE" | "NO_PREV" | "RAMP" | "LOW_PWM" | "NO_DIR" | "NO_POSITION"
```

More than one suspension condition can hold at once, so report the **first**
that applies, in this fixed priority — replay logs must be consistent:

```
NO_POSITION  ->  NO_DIR  ->  LOW_PWM  ->  RAMP  ->  NO_PREV  ->  ACTIVE
```

**Score vector encoding.** Keep the array homogeneous; excluded candidates are
`null`, with a parallel boolean array:

```json
"scores":   [3, null, 5, 4, 2, 1],
"excluded": [false, true, false, false, false, false]
```

A replay should read *why* the gate was inactive and *which* candidates were out
of play, rather than inferring either.

Stamp `lastConfirmedMs` from `e.detectedAtMs`, not `millis()`. A 49-second loop
stall was recorded 2026‑07‑29; a fix currently reports fresher than it is.
`navDeclare()` has no event — keep `millis()` there and comment why.

### §5.1 Publish-path assignment — every QUORUM publish, and its queue

v2.22 has two outbound queues with opposite semantics, plus the §2.5
desired-retained-state slot which is not a queue at all. Assigning a publish to
the wrong path is a silent defect: the wrong queue either evicts an
unrecoverable event or strips a required retain flag. The complete assignment:

| Publish | Path | Why |
|---|---|---|
| `mm/marker` per-event stream (from `drainMarkers()`) | `pubMarker()` | One-time events. Already moved in v2.21; v2.22's peek-publish-remove drain holds each one until confirmed handoff. Carries the marker payload of the contract below — **scores do not ride this message.** |
| QUORUM decision events — adoption, incident open/close | `pubMarker()` | Also one-time and non-re-derivable. A replay with the marker stream intact but the adoption event evicted is unreadable. Carries the decision payload of the contract below — this is where the score vector rides. |
| `mm/no_quorum` snapshot (§2.5) | desired-retained-state mechanism — **never a queue** | R19 routed this through `pub()` on the theory that the retain flag protected it; CODEX showed retain acts only at the broker, so a queued snapshot was evictable precisely when it mattered. It now lives in the §2.5 pending slot; the network task publishes it retained and returns the slot to `NONE` only on success. |
| Its empty clearing payload on `navDeclare()` | desired-retained-state mechanism | The `CLEAR` arm of the same slot — both directions get the same durability. |
| `nav_state`, loopstat fields, all periodic status | `pub()` | Current-value state; the newest is the truth and eviction of stale copies is correct. |

The rule generalising the table: **the terminal retained state has its own
mechanism and touches no queue; everything else splits by event-vs-state.**
One-time event → `pubMarker()`. Current-value state → `pub()`.

**The payload contract — exactly two payloads.** Earlier revisions let §0.1 and
this section disagree about where scores travel. The contract, stated once:

1. **`mm/marker`** (every accepted or rejected detector event): the existing
   raw event fields — `mm`, `landmark`, `obs`, `peak`, `ms`, `drift` — plus
   `dt`, `timing_gate`, `dt_expected`, `dt_conserve_ratio`. **Nothing else.**
   `conf` is deleted along with `navConfidence`. Scores, streaks, leaders and
   margins do NOT ride the marker message.

2. **QUORUM decision event** (adoption, incident open/close, via
   `pubMarker()`): `state`, `streak`, the full score vector, exclusions,
   `leader`, `runner-up`, `margin`.

**Buffer arithmetic for the marker payload (`char b[320]`).** Worst-case JSON,
field by field, using each format specifier's widest possible output
(`dt_conserve_ratio` prints `%.2f`, sentinel `-1.00`, clamped to `99.99`, so
five characters bound it; the longest landmark is `Southpoint`, 10 chars; the
longest `timing_gate` token is `NO_POSITION`, 11 chars):

```
{"mm":170,                       10     (uint8, 3 digits)
"landmark":"Southpoint",         24
"obs":"N",                       10
"peak":-4095,                    13     (12-bit ADC delta, sign + 4 digits)
"ms":65535,                      11     (uint16)
"drift":-32768,                  15     (int16)
"dt":65535,                      11     (uint16)
"timing_gate":"NO_POSITION",     28
"dt_expected":4294967295,        25     (uint32)
"dt_conserve_ratio":-1.00}       26
                                ---
                                173   + 1 NUL = 174
```

174 ≤ 320 with 146 bytes (46%) of headroom — enough that no realistic field
widening (a new landmark name, a wider gate token) approaches the boundary.
The §2.5 snapshot budget of 512 stands separately, sized against
`PubMsg::payload`.

---

## §6 Integration — the parts that break the build

### §6.1 navState consumers outside Layer 3

Fifteen sites reference `navState`. Add two helpers in Layer 3 and substitute
mechanically:

```c
static inline bool navPositionUsable();   // NORMAL or EVALUATING
static const char* navStateName();        // "UNSET"|"NORMAL"|"EVALUATING"|"NO_QUORUM"

static const char* navAlertLevel() {      // used at BOTH 1245 and 1479
    switch (navState) {
        case NAV_NO_QUORUM:  return "NO_QUORUM";
        case NAV_EVALUATING: return "EVALUATING";
        case NAV_NORMAL:     return "CLEAR";
        default:             return "UNSET";
    }
}
```

Reconnecting mid-evaluation must not report `CLEAR`.

**Locate every reference by search, not by line number.** The table below is a
semantic mapping. The line numbers in earlier revisions were taken from v2.17
and the source has moved substantially since — the station guard is now near 920
and command handling near 1380–1515. `grep -n navState` and work from the
containing function.

| Containing function | Current | Replace with |
|---|---|---|
| `cruiseForPosition()`, `serviceStations()` and its arming guard | `navState==NAV_TRACKING` | `navPositionUsable()` |
| `navPublishState()`, `publishAlert()`, `publishStat()` | the three-way ternary | `navStateName()` |
| `publishAlert()`, `publishStat()` — the `lost_ms` fields | `navState==NAV_LOST` | `navState==NAV_NO_QUORUM` |
| `publishAlert()` and the reconnect publish in `attemptReconnect()` | alert level | `navAlertLevel()` — one helper, both sites |
| `publishSimpleStates()` — `nav_ready` | | `sessionDir!=MAP_UNSET && navPositionUsable()` |
| the `cmd/go` handler | GO refusal | refuse on `NAV_NO_QUORUM` and `NAV_UNSET`; **allow** on `NAV_EVALUATING` |

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

`applyDirection()` changes `navDir` mid-session. On any change, perform the **full reset** of
§2.6 — every recovery-only variable, not merely the ring — and return
`NAV_EVALUATING` to `NAV_NORMAL`. Readings collected in one direction cannot be
scored against candidates in another, and neither can an exclusion or a failure
count.

### §6.4 Two acknowledged tradeoffs — deliberate, not oversights

Both are recorded here so a later reader does not "fix" them.

**Station operations run on the nominal odometer during EVALUATING.** If the
odometer is displaced, a station action may begin a marker or two early or late
until adoption corrects it. Accepted because most evaluation episodes begin from
a correct position, operations cannot simply stop for twelve markers, slowing on
entry is expressly unwanted, and the published viable-candidate list already exposes the
uncertainty for any peer.

**The conservation gate uses the nominal odometer position, not each
hypothesis.** During EVALUATING the odometer may be wrong by up to four markers,
so `expected_dt` may be drawn from the wrong interval. With surveyed spacings
spanning 270–355 mm, that is up to ~31% — comparable to `DT_CONSERVE_TOL`
itself. Per-hypothesis timing was rejected for v3.0 because it complicates the
common path for a second-order effect. This is an empirical question, so the
replay tests must exercise the gate with the odometer displaced by −1, +1 and +4.

---

## §6.5 A note for `M1_TEST_SPEC.md`

Not part of this change, but it will mislead a tester if left alone.

The test spec speaks of inducing **LOST** and reacquiring during a continuous
AUTO run. Under QUORUM that splits in two:

- `NAV_EVALUATING` — resolves while moving, which is what Test B actually
  exercises. Most induced incidents land here.
- `NAV_NO_QUORUM` — stops and requires operator declaration. There is no
  automatic recovery, by design.

### `cmd/force_lost` — exact contract for v3.0

`navEnterLost()` no longer exists, so the fixture needs defining rather than
porting. Topic name kept for compatibility with existing scripts.

**Payload is a signed integer *n*: displace `navMm` by *n* event-steps.**

```c
navMm = routeMod((int32_t)navMm + navDir * n);
// change NOTHING else — not navState, not missStreak, not the ring,
// not the scores. The locomotive does not know it has been moved.
publish("FORCED_OFFSET");   // n, old mm, new mm
```

This is the fixture that actually tests M1. The locomotive then discovers the
error the way it would discover a real one: readings start disagreeing, three in
a row wake `NAV_EVALUATING`, and adoption either corrects it or does not.
`n = -4` reproduces a queue-drop burst; `n = +1` reproduces a phantom.

**Do not** fabricate a disagreement streak, force `NAV_EVALUATING` directly, or
call `beginNewEvaluation()` over the last three readings. Those all start the
locomotive at offset 0 with three agreeing entries, which adopts 0 immediately
and tests nothing.

**Payload `"NOQUORUM"`** forces `NAV_NO_QUORUM` directly, for testing the
terminal state and its snapshot.

**Exact parsing contract.** `atoi()` returns 0 for any unparseable string, so
under the existing command style `cmd/force_lost garbage` would silently become
offset 0 and appear to succeed. Do not use it.

```
1. Trim whitespace. Empty  -> reject, publish FIXTURE_REJECTED with the reason.
2. Match "NOQUORUM" exactly, case-sensitive, BEFORE any numeric parsing.
3. Otherwise strtol() with endptr. Reject if endptr does not reach the end,
   if nothing was consumed, or if the value is outside -8..+8.
4. Reject displacement when navState == NAV_UNSET or navDir == MAP_UNSET —
   there is no position or direction to displace.
5. "NOQUORUM" IS permitted from NAV_UNSET: it is a state fixture, not a
   position operation, and testing the terminal state should not require a
   declaration first.
```

Every rejection publishes `FIXTURE_REJECTED` with the offending payload. A test
fixture that fails silently is worse than one that does not exist.

Testers should read `n` outside ±1..±4 as expected to fail: it is outside the
hypothesis set, and staying stopped is the correct outcome.

---

## §7 Do not

- Do not score hypotheses during `NAV_NORMAL`. The common path stays a single comparison; complexity runs only when something is wrong.
- Do not keep `navConfidence` in any form, including as telemetry.
- Do not add any stop condition other than `NAV_NO_QUORUM`.
- Do not add a budget, timeout, lockout or watchdog.
- Do not add missed-marker advance-by-*k*.
- Do not change Hall thresholds, `NGR_DNA1`, or `spacingMm[]` values — a survey commit is pending separately.
- Do not modify the station state machine's phase logic; only its `navState` guard changes.
- Do not implement the M5 extent work in §4. Publish the candidate telemetry and stop there.
- Do not reorder the declaration handlers to make the `NO_DIR` branch reachable.
- Do not make `dt` accepted-event-to-accepted-event. It is detector-event to detector-event.

---

## §8 Verify

- [ ] Built on v2.22 or later. `grep mqtt\.` returns hits only inside `networkTask()`, `attemptReconnect()` and `setup()` — never in Layer 3.
- [ ] No path out of `navOnMarker()` reaches a socket, a `delay()`, or any call whose duration depends on the network. Every publish goes through `pub()` or `pubMarker()`, both of which enqueue.
- [ ] Publish paths match the §5.1 table exactly. In particular: the `mm/no_quorum` snapshot and its clearing payload go through the desired-retained-state mechanism — `grep 'no_quorum'` must show the topic in the network task's reconciliation path and in **neither** `pub()` nor `pubMarker()` call sites — and every one-time nav event goes through `pubMarker()`.
- [ ] With the broker stopped, drive over markers: `navMm` advances, QUORUM adopts an offset if one is induced, and `loop_max_gap_ms` stays under ~80 ms.
- [ ] Compiles for `LL_LocoConfig_9950011.h` **and** `LL_LocoConfig_9950012.h`.
- [ ] **Isolated disagreement.** One bad reading followed by agreements. `navMm` **advances normally throughout** — the locomotive is moving. What must not happen is relocation: no offset applied, `NAV_EVALUATING` never entered, `missStreak` returns to 0 on the first agreement.
- [ ] **Direction and wrap.** CCW, `navMm = 2`, adopting `+4` yields **MM169**, not MM6. CW, `navMm = 169`, adopting `+4` yields **MM2**.
- [ ] **Timing gate.** Event at dt 57 ms whose predecessor was 914 ms, `pwmActualAtDetect` 100 → `PHANTOM_REJECTED`. Relative to the state **immediately before processing the 57 ms event**, `navMm` and the ring are unchanged. The 914 ms event is **not** rolled back; it was consumed and was the real magnet.
- [ ] **Even split.** `420 + 530` at expected 950 also triggers `PHANTOM_REJECTED`.
- [ ] **No false trigger.** Two ordinary intervals, `917 + 917` at expected 917, do **not** trigger it.
- [ ] **Ramp suspension.** `pwmActualAtDetect` 65, `pwmCommandedAtDetect` 100 → gate suspended, event advances normally.
- [ ] **Run 3 replay from MM154.** `NAV_EVALUATING` at the third disagreement, `QUORUM_TIED` while −1 and +1 are level, `QUORUM_ADOPTED` at −1 when the alternation breaks. No stop.
- [ ] **Post-adoption tolerance.** One disagreement after an adoption does **not** reopen it. Three consecutive do.
- [ ] **Adoption finalisation.** Adopt an offset, receive one agreement, then later three disagreements. The adoption is **not** reverted; the three disagreements begin a new ordinary evaluation.
- [ ] **Markers during NO_QUORUM deceleration.** Accepted events after entry advance `navMm` and append to the ring, but trigger no scoring, no adoption, no state exit, no repeated stop request and no repeated retained alert.
- [ ] **Reopening arithmetic.** Adopt `-1` at MM154 → 153. Three accepted events → 156. Trigger reopening. Result is **MM157**, not MM154 — the three advances are retained.
- [ ] **Reopening evidence rebase.** The three post-adoption ring entries are rebased out of the failed frame before scoring, and the failed offset is excluded from the candidate set.
- [ ] **Rejected-event timing history.** Process 914, reject 57, then present 879. The 879 event is **accepted**; `previousAcceptedDt` was never replaced by 57.
- [ ] **Suspended-gate reset.** An event accepted while `pwmActualAtDetect` and `pwmCommandedAtDetect` differ by more than 10 invalidates timing history, and the first later eligible interval cannot be conservation-rejected.
- [ ] **Viability boundary.** Scores 6, 5, 4 with `QUORUM_MARGIN = 2`: the 6 and 5 are viable, the 4 is not.
- [ ] **Conservation interval index.** With `navMm = MM000`: CW uses `spacingMm[170]` (MM170→MM000); CCW uses `spacingMm[0]` (MM001→MM000).
- [ ] **Units.** At PWM 100 over a 280 mm interval, `expectedDtMs` is ~963, not 0.963.
- [ ] **NO_PREV bootstrap.** With the predecessor invalid and every other condition eligible, the next event is accepted, stored as `previousAcceptedDt`, and the event after it runs `ACTIVE`. The gate does not livelock in `NO_PREV`.
- [ ] **Validation precedence.** During provisional validation, `D D D` increments only `adoptionDisagreeStreak`. `missStreak` stays 0, `beginNewEvaluation()` is never called, and the incident reopens with its failure count and exclusions intact.
- [ ] **Second-failure frame.** The second failed adoption removes its correction and rebases its three entries *before* the terminal snapshot. `NAV_NO_QUORUM` retains the uncorrected frame.
- [ ] **Adoption publishes before clearing.** `QUORUM_ADOPTED` carries a populated score vector, leader, runner-up and margin.
- [ ] **ACTIVE predecessor advancement.** Bootstrap 914 under `NO_PREV`, accept 879 under `ACTIVE`. `previousAcceptedDt` becomes **879**, so the next event is tested against 879, not 914.
- [ ] **Second-failure score reconstruction.** After removing the second rejected correction and rebasing its three entries, scores are reconstructed with both failed offsets excluded. The snapshot reports `evalCount = 3` and a leader and margin derived from that vector, not an empty one.
- [ ] **Reopening preserves incident state.** After a first failed adoption, re-entry to EVALUATING leaves `adoptionFailureCount` at 1 and the failed offset still excluded.
- [ ] **Gate under displacement.** Replay the conservation gate with the odometer displaced by −1, +1 and +4 and confirm no false rejections of legitimate intervals.
- [ ] **New-evaluation counter.** The three triggering disagreements are scored once and yield `evalCount = 3` — not 0, not 6. The hard bound therefore permits at most nine further accepted events.
- [ ] **Immediate adoption on entry.** If those three entries already give one candidate a unique two-point lead, adoption occurs inside `beginNewEvaluation()` without waiting for another marker.
- [ ] **Evaluation ring continuity.** Each accepted event during EVALUATING is appended exactly once, and the triggering three are not re-pushed. At the hard bound the ring holds the twelve events `evalCount` reports.
- [ ] **Twelfth-event adoption precedence.** Enter event 12 with no margin; let event 12 create a unique two-point leader. `QUORUM_ADOPTED` occurs and `NAV_NO_QUORUM` does not.
- [ ] **Validation confirmation bookkeeping.** The first post-adoption agreement publishes `AGREE`, updates `lastConfirmedMm` and `lastConfirmedMs` from `e.detectedAtMs`, zeroes `missStreak`, calls `endSuccessfulIncident()` and restores the full candidate set.
- [ ] **Reconnect during evaluation.** An MQTT reconnect while `NAV_EVALUATING` publishes level `EVALUATING`, not `CLEAR`.
- [ ] **Immediate decision after reopening.** Fail the first adoption and reconstruct the three rebased entries. If another non-excluded candidate already holds a unique two-point lead, `QUORUM_REOPENED` is published and then `QUORUM_ADOPTED`, without waiting for a fourth event.
- [ ] **Stop transition invalidation.** Establish a valid `ACTIVE` predecessor, bring `actualPwm` to zero with no marker in between, then restart. The first otherwise-eligible marker runs `NO_PREV` and cannot be conservation-rejected against the pre-stop interval.
- [ ] **NO_DIR position handling.** In `NAV_NORMAL` with `navDir == MAP_UNSET`, a marker reports `timing_gate` `NO_DIR` and invalidates the predecessor, but does not advance `navMm`, push the ring, alter either streak counter, score, or update last-confirmed.
- [ ] **NO_QUORUM diagnostic retention.** After the terminal snapshot and deceleration, ordinary state publications still report the terminal scores, exclusions, `evalCount`, leader, runner-up and margin. They survive until `navDeclare()` or a full reset; `endSuccessfulIncident()` is never called.
- [ ] **Scoring operation.** A match adds exactly 1, a mismatch adds 0, weak and drifting reads count the same as strong ones, excluded candidates are not scored.
- [ ] **Event-time PWM.** `e.pwmActualAtDetect` and `e.pwmCommandedAtDetect` are both captured in `detectorSample()` at event open and used by the gate. A 30-second drain delay with the throttle changed in between does not alter the gate's decision.
- [ ] **Zero-dt bootstrap.** The first event of a session has `dt == 0`, is accepted under `NO_PREV`, and does **not** set `previousAcceptedDtValid`. The second event also runs `NO_PREV`. The third runs `ACTIVE`.
- [ ] **Both declaration paths recover.** `cmd/start_interval` returns `NAV_NO_QUORUM` to `NAV_NORMAL` exactly as `cmd/start_mm` does.
- [ ] **navDeclare resets lastMarkerMs.** The first `dt` after any declaration is 0.
- [ ] **Provisional model under load.** Replay the gate on a loaded climb and a fast descent, not only under odometer displacement.
- [ ] **Event-time PWM pair.** Both values are captured in the `if(!evActive)` block at event open, beside `evStartBaseline`, and the gate uses only those two. A throttle change between detection and drain cannot alter the gate's decision.
- [ ] **RAMP from event-time values only.** A marker detected at steady 100/100, drained after a stop request set `commandedPwm` to 0, reports `ACTIVE` — not `RAMP`.
- [ ] **force_lost displaces only.** `cmd/force_lost -4` moves `navMm` by four event-steps and changes nothing else. `NAV_EVALUATING` is entered later, by ordinary disagreement, not by the command.
- [ ] **NO_POSITION.** A marker arriving in `NAV_UNSET` publishes with `timing_gate` `NO_POSITION`, does not advance, and does not enter conservation.
- [ ] **Snapshot fits the transport.** The `NO_QUORUM` payload is under 512 bytes as encoded, `snprintf()` does not truncate, and the queued copy matches the local buffer byte for byte.
- [ ] **Snapshot survives the failure.** Stop the broker. Force `NAV_NO_QUORUM`. Generate status traffic until `pubQueue` fills and cycles. Restart the broker. The retained snapshot appears on the broker after reconnect — the desired-retained-state slot reconciled it; no queue carried it. (R19's version of this test — expecting a queued retained publish to survive eviction — was proven impossible by CODEX and is withdrawn.)
- [ ] **Clear survives the failure too.** Recover navigation, `navDeclare()`. Stop and restart the broker mid-clear — after the `CLEAR` state is set but before it has been published — to prove the reconciliation direction. After reconnect, the retained message on the broker is empty; a fresh subscriber sees nothing.
- [ ] **Station machine reset.** Force `NO_QUORUM` during `ST_DEPART`, redeclare position, and confirm `stPhase` is `ST_IDLE` with no armed station. The locomotive is drivable without an `AUTO` toggle.
- [ ] **Fixture parsing.** `cmd/force_lost garbage`, empty, `+3x` and `99` are all rejected with `FIXTURE_REJECTED`. `-4` and `NOQUORUM` are accepted. `-4` is refused while `NAV_UNSET`; `NOQUORUM` is not.
- [ ] **Hard bound.** 12 accepted events in `NAV_EVALUATING` with a persistent margin of 1 → `NAV_NO_QUORUM`. Not indefinite collection.
- [ ] **NO_QUORUM.** PWM 0; `navMm`, `navDir`, `autoRunning`, last-confirmed all retained; retained alert published; operator declaration through **either** `cmd/start_mm` or `cmd/start_interval` returns it to `NAV_NORMAL`.
- [ ] **Stations survive evaluation.** A station arms and completes normally while `NAV_EVALUATING`.
- [ ] **Direction change** performs the full §2.6 reset — ring, scores, exclusions, failure count, provisional-adoption state, `previousAcceptedDt` — and returns `NAV_EVALUATING` to `NAV_NORMAL`. No state from the old direction survives.
- [ ] **Incident reset after successful validation.** Fail one adoption, exclude its offset, adopt another, receive one agreement. A later independent evaluation starts with `adoptionFailureCount = 0` and the full candidate set restored.
- [ ] **navDeclare reset.** Operator declaration yields a completely fresh incident with every configured offset eligible.
- [ ] **Terminal snapshot.** On entering `NAV_NO_QUORUM` the full evidence snapshot is published before any deceleration event can overwrite the ring.
- [ ] **Timing-gate priority.** With `pwmActualAtDetect = 30` and `commandedPwm = 100`, `timing_gate` reports `LOW_PWM`, not `RAMP`.
- [ ] The viable-candidate list published during EVALUATING contains every candidate within the margin, not only the tied maximum. (Occupancy bounds themselves are M5 and are not implemented.)
- [ ] Payloads do not truncate.

```
git tag -a v3.0 -m "QUORUM navigator: hold position on disagreement, wake nearby
hypotheses only on a run of failures, conservation timing gate, stop only when no
candidate fits"
```
