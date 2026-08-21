# Otto's confusion: root cause — label-slip episodes, not bad sensing

Date: 2026-08-21. Sources: full 30 h capture (`20260820_morning_session.log`,
14,564 marker reads), QUORUM.ino as flashed, six-agent adversarial analysis
(code read, capture mine, three hypothesis-refutation passes, prior-art sweep).

## The verdict

Otto's "confusions" are **position-label slip episodes**. About 60% of his
wrong-polarity reads — in BOTH directions, including the entire 11:50:43
cascade — are **correct pole readings scored against a label that has slipped
1–3 markers**. Proven by sliding-window fits: the 11:50 cascade matches
`obs == DNA[mm−2]` at 17/20; one reverse episode fits −2 **perfectly across
30 consecutive windows**, then drifts to −3, +2, −1 as the quorum part-corrects.
A global ±1 test sits at chance — the offsets are episodic and drifting, which
is why they hid from aggregate statistics.

The sensor is largely innocent. The magnets (post-tamping) are innocent.
The bookkeeping slips, and then honest readings get marked wrong.

## The four seed mechanisms (how the label slips)

1. **The conservation gate predicts instead of measuring** (QUORUM.ino:2495):
   `expectedDt = spacing / (3.90×pwmDuty − 99.2)`. During deceleration the
   duty model under-reads true speed; two genuine intervals sum into the
   ±30% reject band and the gate calls a real magnet PHANTOM_REJECTED. Worse,
   a rejection freezes `previousAcceptedDt` while `lastMarkerMs` advances
   (2343-2345 vs 2521/2526), so at stable cadence the sum is CONSTANT and the
   gate rejects **runs** of genuine markers: five straight (Toby, mm 101,
   exp 2104–2226 vs real ~1450), **fifteen straight** (Otto, 08-21 19:08Z,
   mm 130). 90 PHANTOM_REJECTED in the capture; the majority followed by
   DISAGREE bursts or a quorum open.
2. **The LOW_PWM hole**: below duty 40 the gate is off entirely
   (`dt_expected=0`) — every crawl seed lands unexamined.
3. **Fringe-lobe ghosts, Otto only**: opposite-pole excursions of 40–66
   counts flanking real magnets. Trailing ghosts ~0.2 s behind strong markers
   at ALL speeds (mostly caught by quarantine; the 90-count gate killed them,
   70 readmits some). A **leading** lobe is directly visible in loopstat
   before the 11:50:20 event: sustained +46..+56 counts for ~4 s while
   creeping onto the mm 18 S-magnet — the 5,641 ms event then OPENS as N.
   Polarity is decided by the opening pole (detector contract), so at crawl
   the lingering lobe names the event wrongly; at speed it is milliseconds
   and averages away. Toby produced **zero** sub-90 events in 6,171 reads —
   this generator is a property of Otto's sensor/mount.
4. **Dwell double-counts** at departure after parking on a magnet.

## The structural bug that turns slips into NO_QUORUM

`QUORUM_OFFSETS = {−1, 0, +1, +2, +3, +4}`, applied as `navMm + navDir×offset`.
A label that ran **2 ahead** in forward travel is a true offset of **−2 —
outside the candidate set**. The quorum can literally never name the right
answer; it ties, drifts, and declares NO_QUORUM (mm 37 episode). The same
physical slip in reverse maps to **+2, inside the set** — quorum adopts and
recovers. That asymmetry is why forward slips hard-fail while reverse
"confusion" churns through adopt-cycles, and why 7 of Otto's 8 NO_QUORUMs
were preceded by a >1.5 s (crawl/dwell) event.

## What was falsified

- **Baseline corruption**: Toby dwelt on magnets 25 times (1.5–65 s) — baseline
  moved ≤1 count, zero errors after. Otto's analog baseline (loopstat) moved
  12 counts across the 5,641 ms event, against a ±70 entry threshold and
  150–209 peaks. Dead. (`env` in alerts is the occupancy envelope, not the
  sensor baseline — do not confuse them again.)
- **Fixed reverse indexing offset**: all fixed-shift fits sit at chance;
  0 of 60 repeatedly-read reverse markers is always-wrong. Dead.
- **"Speed rescues reverse"**: not supported as stated. Speed prevents NEW
  seeds (model accurate at cruise, lobes sub-threshold), so forward improves
  4.15%→2.67% — but an already-slipped label stays slipped at any speed; the
  fast-reverse windows carry the slip until quorum adopts or chokes.

## Prior art — this was found ten days ago

The PWM-model defect was identified 2026-08-11 (1.13 beta verdict: "the
conservation gate cannot see them… expectedDt is derived from 3.90×pwm−99.2").
Decision 0024 ("the conservation gate measures rather than predicts") was
written the same day — status still Proposed. A concrete fix exists:
`docs/QUORUM_TIMING_EXPECTATION_PROPOSAL.md` (expectedDt from the measured
predecessor interval); CODEX limited it to harness-only, and 0035 then shipped
measured medians for quarantine while explicitly leaving the conservation
gate's PWM model untouched. The operator's "there is a glitch in the firmware"
was already in the repo, diagnosed and shelved.

## Recommended fixes, in order

1. **Implement 0024**: expectedDt from the trailing median of accepted
   intervals (quarantine already maintains it), duty model as cold-start
   fallback only. Prove first by shadow-replaying the capture through the
   harness with the one-line substitution: the 14 frozen-predecessor rejection
   runs must disappear while true phantom rejections survive.
2. **Widen QUORUM_OFFSETS to a symmetric set** (add −2, −3): converts
   unrecoverable forward slips into ordinary adoptions. Small, cheap, and it
   removes the failure the operator actually sees.
3. **Tonight's oscilloscope session has a concrete prediction**: Otto's sensor
   should show opposite-pole excursions of ~40–66 counts flanking each main
   lobe (~0.2 s behind at speed), and a leading lobe that lingers at crawl.
   No such lobes ≥70 counts falsifies mechanism 3.
4. Re-examine the entry threshold against post-tamping magnet strengths: with
   the weakest marker now ~140+, a 90-count entry may again be safe and would
   re-kill the readmitted trailing ghosts.
