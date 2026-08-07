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
*on* a magnet, an event opens on arrival and cannot close (raw never
returns inside the exit band) — until the migrating thresholds catch up
with the parked reading, at which point the event closes with a
tens-of-seconds duration. On departure, the swing *back* to true baseline
crosses the far threshold of the now-recentred window and opens a **phantom
event of opposite polarity**. So one parked dwell can produce: a
monster-duration event with an extreme `drift` field, a phantom
inverse-polarity marker, and a stretch of misjudged real markers. All three
are testable signatures (§5).

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

**Why this condition.** `MOTOR_DEAD_ZONE_PWM` (20) is the tractive floor —
at or below it the wheels are not turning (the same physical fact the v1.6
direction-change rule rests on). Above it the locomotive is believed
moving, which is precisely the regime in which the median's
magnets-are-outliers assumption holds. The gate does not freeze the
baseline in any new sense; it restores the tracker to the operating regime
its 128×500 ms design implicitly assumed.

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
`state/loopstat` + `mm/marker`. Predictions: baseline migrates tens-to-200+
counts within ~60 s; an event closes with monster `ms` and extreme `drift`
once thresholds catch up; departure yields a phantom opposite-polarity
marker and DISAGREEs. Control: same dwell clear of magnets → baseline
steady (±3 observed tonight), clean departure. **If the magnet-park
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
