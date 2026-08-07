# QUORUM baseline motion gate — problem and proposed fix (for review)

Date: 2026-08-06
Status: **proposed, awaiting Sam/CODEX review.** No firmware written.
Target: QUORUM 1.8 (one change, one commit, revertible alone)
Problem record: `docs/QUORUM_STATIONARY_BASELINE_POISONING.md`
Decision record: `docs/decisions/0017` (Proposed)
Operator ruling 2026-08-06: the problem is accepted and **must be fixed
before CTO3 Station Stop v1** (plan step 3), which parks locomotives at
mapped positions by design, four stations a lap.

---

## §1 The problem, in one paragraph

`updateBaseline()` ([QUORUM.ino:468](../firmware/QUORUM/QUORUM.ino)) pushes a
raw Hall sample into a 128-slot median every 500 ms **unconditionally** —
`detectorSample()`'s own comment says "every sample, in or out of an event."
The median is robust to magnets only because a *moving* locomotive sweeps
past them: at cruise a magnet contributes ≤1 of 128 samples. Parked, the same
magnet contributes **all** of them, and within ~32–64 s the learned baseline
*becomes the magnet*; `recomputeThresholds()` then re-centres the ±38-count
detection window on a reference that is off by up to 254 counts (observed,
2026-08-06 log — 6.7× the entry threshold). Every reading after departure is
judged against that corrupted reference until the median washes out.
Operator-observed twice on 2026-08-06: a powered, stationary locomotive
loses navigation on restart. The evening incident (`NAV_NO_QUORUM` at MM
100 after a multi-minute stop, scores tied 6–6, margin 0) matches, though
the park-position wasn't recorded, so §5's test remains the proof.

The failure has a second act the problem record did not spell out. Parked
*on* a magnet (take N, raw high), an event opens on arrival and cannot
close — until the migrating thresholds catch up with the parked reading,
at which point it closes with a tens-of-seconds duration. Departure is
worse than a single phantom. The swing back to true baseline crosses the
recentred window's *far* threshold and opens a phantom **S** event; and
because `EVENT_EXIT_HOLD_MS` is only 20 ms, each real **N** magnet the
locomotive then passes lifts raw into the recentred exit band for
~150 ms — long enough to *close* the open phantom and queue it — after
which the field's return re-opens the next one. Until the median washes
out (~30–60 s of driving), the navigator is fed a stream in which **every
real N magnet is delivered as an S reading and every real S magnet is
swallowed invisibly** below the window. Half-inverted, hole-riddled
evidence plus odometer slip is unscorable: the observed terminal snapshot
— scores `[6,4,5,4,4,6]`, everything about half right, no leader — is
precisely what the QUORUM vector looks like against it. This mechanism
doesn't just permit the observed `NO_QUORUM`; it predicts it specifically.

One more property matters operationally: **the evidence self-erases.**
`NO_QUORUM` fires after ~15 corrupted markers (~18 s), but the median
finishes healing in ~30–60 s of driving — so by the time anyone inspects,
`state/loopstat` shows a normal baseline again. A post-mortem look finds
nothing; only a capture spanning the dwell *and* the departure shows the
migration. This is why the failure read as sporadic for months.

## §2 Proposed fix: gate the median push on believed motion

```c
static void updateBaseline(int raw,unsigned long now){
  if(!medPrimed) primeMedian(raw);
  if(actualPwm <= MOTOR_DEAD_ZONE_PWM) return;   // NEW: no push while the
                                                  // wheels cannot be turning
  if(now-medLastPushMs < MEDIAN_SAMPLE_MS) return;
  ...unchanged...
}
```

One comparison, using state `detectorSample()` already reads on the hall
task for §3's PWM-at-detect capture (aligned 32-bit access, atomic on this
core — the existing pattern, no new threading).

**Why this condition.** `MOTOR_DEAD_ZONE_PWM` (20) is the tractive floor.
At or below it there is **no positive evidence of motion** — *not* proof of
rest: a locomotive can coast downhill at PWM 0 (this railway has Viaduct
Hill, and the SOLONAV motion bands classified "possible_downhill" from
PWM 14). The gate is stated that way round deliberately, because the two
error cases are asymmetric: pushing samples while wrongly assumed moving
can corrupt the reference (the whole problem), while *refusing* pushes
while actually moving merely postpones adaptation for the length of the
coast — bounded, and washed out by the median within ~30 s of powered
running. Freezing is safe in every uncertain case; adapting is not. Above
the floor the locomotive is believed moving, the regime in which the
median's magnets-are-outliers assumption holds. The gate does not freeze
the baseline in any new sense; it restores the tracker to the operating
regime its 128×500 ms design implicitly assumed.

**What replaces adaptation while parked:** nothing, deliberately. The boot
calibration (2 s, operator told "keep clear of magnets") establishes the
reference; motion maintains it; rest preserves it. A parked locomotive's
baseline is *whatever motion last proved*, which is exactly the NVS
principle of decision 0007 (persist only the pulse-proven state) applied to
a live reference.

## §3 Behavioural consequences, stated so review can check them

1. **Parking on or near a magnet becomes harmless to the reference.** The
   event that opens on arrival stays open for the whole dwell (thresholds
   no longer migrate to close it) and closes on departure as **one** marker
   event: correct polarity (opening pole), `detectedAtMs` stamped at
   arrival, `ms` large (saturating at 65535). The arrival magnet is counted
   once, at the right position — semantically correct. The sketch header's
   "an OPEN EVENT still can [stick] … Watch event_open_ms" note becomes an
   *expected* signature of a dwell-on-magnet, not a detector anomaly;
   the header comment should say so when 1.8 lands.
2. **No adaptation during long dwells.** Thermal/electrical drift while
   parked is uncorrected until motion resumes. Measured drift bound: 19
   counts across tonight's whole session *including* position changes —
   half the entry margin, washed out by the median within ~30 s of driving.
   Accepted.
3. **Hand-pushing a powered-off-throttle locomotive** (start-interval
   setup, IR bench work) no longer updates baseline. Conservative and
   correct: those samples were always taken at walking-pace over unknown
   track.
4. **Stall at PWM > 20 over a magnet still poisons** (believed moving,
   actually parked). Accepted residual risk: rare, requires a stall
   *precisely on* a magnet, and the honest fix is a real motion witness —
   the `motionWitnessSaysStopped()` hook (decision 0005) that CTO3's
   IR/Hall evidence will supply. The gate's condition is "believed moving";
   IR later upgrades belief to measurement without changing the structure.

## §4 Alternatives considered (and why not)

- **Excursion gate** — refuse samples beyond the entry threshold from
  current baseline ("a reading that looks like a magnet is not baseline
  evidence"). Rejected as primary: it is self-referential — a mis-primed or
  already-poisoned baseline refuses exactly the samples that would correct
  it — and it does nothing about *fringe-field* parking (offsets of ~30
  counts, under the threshold, still admitted and still migrating the
  reference by most of the entry margin). The motion gate covers both.
- **Re-prime on departure** (short recalibration when leaving rest).
  Rejected: discards a known-good baseline in the overwhelmingly common
  case to guard the rare one, trusts a single instant instead of a median,
  and adds a state machine where one comparison suffices.
- **Enlarge the median window.** Rejected: changes the poisoning time
  constant, not the fact of it. A 10-minute dwell defeats any window.
- **Operational rule only** ("don't park on magnets"). Rejected as sole
  remedy: an invisible constraint whose violation costs navigation, and
  Station Stop v1 will park locomotives at fixed mapped positions
  repeatedly — the rule would have to be engineered into the station map
  forever. Worth *also* doing where free (station stop targets can avoid
  magnet-adjacent dwell points), but the firmware must not depend on it.

**Relation to standing decisions.** This does not contradict 0004 ("filters
admit unconditionally") — 0004 governs admission of *measurements* into the
speed path; this gates maintenance of the *reference frame*, which is 0006's
territory ("envelope decay gates on signal activity"), and the proposal is
exactly 0006's principle applied to the Hall side. 0007's
"persist only the proven state" is the same doctrine at rest.

## §5 Verification plan

**Pre-fix (proves the diagnosis; ~6 min, no code change):**
park Otto with the sensor **on a magnet**, powered, throttle 0, capturing
`state/loopstat` + `mm/marker` + `state/nav` **continuously from before the
stop through at least 60 s after departure** — the corruption self-heals
(§1), so a capture that starts late proves nothing. Predictions: baseline
migrates tens-to-200+ counts within ~60 s; an event closes with monster
`ms` and extreme `drift` once thresholds catch up; departure yields
opposite-polarity readings at real-marker cadence with roughly half the
markers missing, DISAGREEs, and (if allowed to run) `EVALUATING` with a
flat score vector. Control: same dwell clear of magnets → baseline steady
(±1 observed tonight per position), clean departure. **If the magnet-park
baseline holds steady instead, the diagnosis is wrong and 1.8 is not
built.**

**Post-fix (1.8):** repeat both parks. Predictions: baseline frozen in both
(loopstat `delta` shows a large steady offset while on the magnet — the
*reading* is displaced, the *reference* is not); one arrival-stamped
long-duration marker on the magnet park; no phantom at departure; clean
AGREE run resumes immediately. Regression: a full lap with `miss_streak` 0
and `loop_max_gap_ms`/`hall_task_max_gap_ms` unchanged from the 2026-08-06
baseline (33–34 / 2–3 ms); confirm `LOW_PWM`-gated markers still navigate
(low-throttle crawl segment).

## §6 Scope

One guard line plus comments in `updateBaseline()`, a header note for the
open-event signature, version bump to `QUORUM_1_8`, implementation report.
No topic, payload, threshold, or navigator change. Nothing here touches the
bicameral boundary: the gate reads `actualPwm`, it never writes anything.

---

## §7 Self-review findings (2026-08-06, adversarial pass before Sam)

Requested by the operator. Method: re-derive every claim from source and
from the captured log rather than re-read the prose. Three corrections were
applied above (marked here); the rest verified clean.

**R1 — corrected (§2).** The original justification claimed PWM ≤ 20 means
"the wheels are not turning." False on grades — a locomotive coasts
downhill at PWM 0. The gate survives because the error cases are
asymmetric (refusing pushes while secretly moving is safe; pushing while
secretly parked is the bug), but the wording now says what is true: the
condition is *no positive evidence of motion*, and freeze is the safe
default under uncertainty.

**R2 — corrected (§1, §5).** With `EVENT_EXIT_HOLD_MS` = 20 ms, the
departure sequence is not "a phantom marker and some misjudged readings":
each real N magnet closes-and-requeues the open phantom S event, so the
navigator receives every N magnet as an S reading while every S magnet is
swallowed. This upgrades the diagnosis from "consistent with the incident"
to "specifically predicts the observed flat score vector
`[6,4,5,4,4,6]`" — and it upgrades the test predictions accordingly.

**R3 — new supporting evidence (problem record updated).** The 2026-08-06
capture contains the mechanism live at small amplitude. Parked 4 min at
MM 29, the baseline settled at that spot's local field level (2033–2034,
steady ±1). On departure at PWM 100 it slid to the loop-wide norm
(2016) over **~38 s** — matching the 128 × 500 ms median wash-out constant
the model predicts. 17 counts is 45 % of the entry margin and harmless;
under a −254-count magnet the identical dynamics consume the margin
several times over. The time constant is no longer theoretical.

**R4 — captured above (§1, §5).** The evidence self-erases: `NO_QUORUM`
fires (~18 s) before the median finishes healing (~30–60 s), so
post-incident telemetry looks innocent. The pre-fix test capture must span
the dwell and the departure, and this also explains months of
"sporadic" losses-on-restart.

**R5 — verified, no change.** `actualPwm` is `volatile`
([QUORUM.ino:315](../firmware/QUORUM/QUORUM.ino)) and already read on the
hall task for §3's PWM-at-detect — the gate adds no new threading
exposure. Threshold arithmetic cross-checked against the boot print
(2027 + 38 = 2065 = `Nent` ✓). Gate placement after the `medPrimed` check
is correct; `calibrate()` primes before `hallTask` exists.

**R6 — considered and rejected: also gating on `!evActive`** (skip pushes
during an open event), which would close the stall-over-magnet residual
(§3.4). Rejected because it removes an existing safety property: today a
stuck-open event — the sketch header's explicit watch item — is
eventually closed *by* baseline migration recentring the thresholds. With
pushes suppressed during open events, a stuck-open event while moving
would freeze the baseline and never close. The stall residual is the rarer
and better-bounded hazard; it stays accepted, awaiting the real motion
witness (decision 0005 hook).

Net: diagnosis stands, strengthened; the one-line fix stands; two
paragraphs of justification and prediction were wrong enough to matter for
review and are now corrected.
