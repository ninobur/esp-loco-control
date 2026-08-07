# The parked-locomotive baseline problem — QUORUM Hall median has no activity gate

Date: 2026-08-06
Status: **mechanism confirmed in source; root-cause link to the 2026-08-06
NO_QUORUM incident is a strong hypothesis, not yet proven.** The decisive
test is specified at the end and has not been run.
Reported by: operator (two independent observations, same day)
Firmware: QUORUM_1_7 (mechanism is unchanged since QUORUM 1.0)

---

## The operator's observation

Twice on 2026-08-06, independently:

1. During afternoon IR testing — "If I stopped Otto and left him on, he was
   lost when I re-started."
2. Evening run — Otto sat on the track for several minutes, continued
   forward, and entered `NAV_NO_QUORUM` shortly after. He then ran a
   complete lap in the terminal state; the dashboard MM tile blanked
   (correct — `NO_QUORUM` is not in the dashboard's `USABLE_NAV`).

The common factor is **sitting still while powered**, not distance, speed,
direction, or reversal. An initial reversal-odometry hypothesis was raised
and **withdrawn**: the operator confirms the reversal happened *after*
`NO_QUORUM`, not before.

## The mechanism (confirmed in source)

`updateBaseline()`, [QUORUM.ino:468](../firmware/QUORUM/QUORUM.ino):

```c
static void updateBaseline(int raw,unsigned long now){
  if(!medPrimed) primeMedian(raw);
  if(now-medLastPushMs < MEDIAN_SAMPLE_MS) return;   // 500 ms
  medLastPushMs=now;
  medRing[medIndex]=(int16_t)raw;                    // pushed UNCONDITIONALLY
  medIndex=(uint8_t)((medIndex+1)%MEDIAN_WINDOW);    // 128 slots
  int m=medianOfRing();
  if(m!=baselineCounts){ baselineCounts=m; recomputeThresholds(); }
}
```

`MEDIAN_WINDOW` 128 × `MEDIAN_SAMPLE_MS` 500 ms = **a 64-second window**, and
the median crosses over at roughly half that. **Nothing gates the push on
motion, on PWM, or on signal activity.** A parked locomotive keeps feeding
the tracker whatever its Hall sensor is currently sitting over.

If that is a magnet — or anywhere in a magnet's fringe field — the median
migrates to the magnet's level within about 32–64 seconds, and
`recomputeThresholds()` re-centres the whole detection window on it:

```c
northEnter = baselineCounts + 25 + 13;   // +38
northExit  = baselineCounts + 25;
southEnter = baselineCounts - 25 - 13;   // -38
southExit  = baselineCounts - 25;
```

**The magnitudes are not close.** From the 748 `state/loopstat` samples in
`field-records/logs/20260806_quorum17_otto_run.log`:

| Quantity | Value |
|---|---|
| Entry threshold | **±38 counts** from baseline |
| Observed magnet excursion (`delta` extremes) | **−254 / +182 counts** |
| Baseline movement across the session (all stops clear of magnets) | 2015–2034, **19 counts** |

A magnet displaces the reading by up to **6.7× the entry threshold**. Parked
over one for a minute, the learned baseline ends up further from true than
the entire detection window is wide. On pulling away, the field swings back
toward true baseline — which is now itself a threshold crossing — and every
subsequent real magnet is judged against a badly offset reference. Wrong
polarity decisions and missed detections follow, `missStreak` reaches
`QUORUM_TRIGGER`, and the navigator goes to `NAV_EVALUATING` and then
`NAV_NO_QUORUM`.

Note that even parking *clear* of magnets moved the baseline 19 counts —
half the entry margin — so the tracker is demonstrably position-sensitive at
rest even in the benign case.

## Why it is intermittent

Entirely dependent on **where** the locomotive stops. This predicts, and
matches, the 2026-08-06 evening session:

- Stopped at MM 29 and later MM 112, both clear of magnets: baseline sat
  rock-steady (2034 and 2016 respectively, raw within ±3), and the
  subsequent lap was **perfect — 259 markers, 250 `ACTIVE` gates, 7 `RAMP`,
  2 `NO_PREV`, `miss_streak` 0, zero new disagreements.**
- The earlier stop, position unrecorded, produced `NO_QUORUM`.

So a stop-restart cycle is *usually* fine, which is exactly why this took
months to surface as a pattern rather than a one-off.

## This is the Hall twin of decision 0006

Decision 0006 — *envelope decay gates on signal activity* — established for
the IR sensor that an adaptive reference **must not keep adapting when there
is no signal activity to justify it.** The Hall median has the identical
flaw and has never had the corresponding guard.

QUORUM's own header lists this among its deliberate removals:

> Deliberately absent: baseline freeze, tracker recovery state, stuck-event
> guard, settling qualifier … Every one of those existed to prop up a
> detector that had to be right on its own. Give it a navigator that can
> absorb being wrong and they are all unnecessary.

That reasoning is sound for the failure it was aimed at — **random** misreads
at ~1.3%, which QUORUM absorbs beautifully (782 agrees against 5
disagreements this session). It does not hold for a **systematically biased**
detector. A navigator that can absorb noise cannot absorb a reference frame
that has quietly moved 250 counts, because every reading after that is wrong
in the same direction. This is not an argument to restore the old
freeze machinery wholesale; it is an argument that one specific guard was
load-bearing for a case the removal rationale did not consider.

There is a second, sharper way to see it. `calibrate()` at boot prints:

```
[CAL] 2 s baseline — keep clear of magnets
```

The firmware already knows that establishing a baseline over a magnet is
wrong, and says so to the operator. `updateBaseline()` then silently redoes
that same calibration, continuously, wherever the locomotive happens to be
standing — with no such condition.

## The decisive test (not yet run)

Cheap, ~6 minutes, no code change:

1. Park Otto deliberately **with the Hall sensor over a magnet**, powered,
   throttle 0.
2. Watch `state/loopstat` `baseline` for 2 minutes. **Prediction: it
   migrates toward the magnet's level (tens to ~200+ counts) rather than
   holding.** Holding steady falsifies this account outright.
3. Drive away and watch `mm/marker`. Prediction: immediate misreads,
   `DISAGREE` events, and likely `EVALUATING` within a few markers.
4. Repeat with the sensor parked clear of any magnet as the control.
   Prediction: baseline holds, clean run.

`baseline`, `raw` and `delta` are already in `state/loopstat` every second,
so the existing capture command records everything needed.

## If confirmed, the fix is a design decision, not a patch

**Update, same evening:** the operator accepted the problem and ruled it
must be fixed before Station Stop v1. The proposed fix (motion gate) is
specified in `QUORUM_BASELINE_MOTION_GATE_SPEC.md` and recorded as decision
0017 (Proposed), awaiting Sam/CODEX review. The options below are preserved
as the original analysis:

- **Gate the median push on motion.** Do not push while `actualPwm == 0`, or
  while no marker has been seen recently. Closest analogue to 0006. Risk: a
  locomotive that legitimately needs to re-baseline while stopped (thermal
  drift over a long dwell) never does.
- **Gate on excursion.** Refuse to push samples that are themselves beyond
  the entry threshold — a reading that looks like a magnet is not baseline
  evidence, by definition. Cheap, local, and does not need a motion concept.
- **Re-prime on departure.** On the transition from stopped to moving,
  re-run a short calibration before trusting polarity.
- **Do nothing in firmware; make it operational.** "Do not park on a
  magnet." Rejected as a sole remedy — it is an invisible constraint with a
  navigation-loss consequence, and station stops (CTO3 §6) will deliberately
  and repeatedly park locomotives at mapped positions that may sit on
  magnets.

The last point matters for CTO3 directly: **Station Stop v1 (plan step 3)
parks a locomotive at a mapped location for a dwell.** If dwelling near a
magnet poisons the baseline, station stops will manufacture exactly this
failure on every stop, at four stations a lap. This should be resolved
before Station Stop v1, not after.

## References

- `field-records/logs/20260806_quorum17_otto_run.log` (748 loopstat samples,
  259 marker events with 1.7's `pwm`/`v` fields)
- Retained NO_QUORUM snapshot: entry at MM 100, `scores [6,4,5,4,4,6]`,
  leader −1, runner-up +4, **margin 0**, 12 evaluations — an honest tie, not
  a wrong answer
- Decision 0006 (IR envelope decay gates on signal activity)
- `docs/decisions/0001` (QUORUM holds position on disagreement)
