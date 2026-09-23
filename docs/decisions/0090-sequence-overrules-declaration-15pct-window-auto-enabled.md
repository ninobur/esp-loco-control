# 0090 - A 10-magnet sequence overrules a bad declaration; IR window 15%; AUTO enabled

**Whole-route recovery implementation superseded by0098** (2026-09-23).
The historical AUTO/15% rulings below are retained; global matching is removed
from the PROXIMAL_R1 candidate after the documented MM65->166 failure.

**Superseded in part by 0092** (2026-09-22): see 0092 for the NAVI / IR decision model the operator adopted with Sam.
Status: Proposed. Operator rulings given in chat 2026-09-22; mechanisms
implemented in NAVI_COHERENCE_0_5_AUTO_ENABLED; not flashed. Not
authoritative until the operator reviews and ratifies this record.
Decided by: David (the three rulings and the build name). Claude chose the
mechanisms that enforce them, listed separately below and not attributed to
the operator.

## The operator's rulings (policy)

Verbatim, 2026-09-22, after two NAVI_COHERENCE 0.4 field runs:

> Operator declaration is the truth, but a 10 magnet sequence is also the
> truth. The bad initial declaration should be over-ruled. We can try 15%.
> Please authorize auto. We can still run more manual only. But Auto matters
> more and will reveal more. _AUTO_ENABLED is more descriptive to me. these are
> all field tests with aspirations of being production.

1. **Sequence authority.** A 10-magnet observed sequence has the authority to
   overrule the operator's declaration when they conflict.
2. **IR eligibility window ±15%** (was ±10%).
3. **AUTO enabled** in the next build.
4. **Naming:** the build is suffixed `_AUTO_ENABLED`, which the operator finds
   more descriptive than `_FIELDTEST`. These builds are field tests intended to
   become production.

Follow-up rulings, same day:

> The correction should work at any time. That is much more powerful. It also
> helps if a derailment or handling or a misplaced magnet alter the true count.
> 2. Correction during station stops should just rely on the working model.
> 3. Thanks for killing the bug.

5. **Corrections apply at any time.**
6. **During station stops, rely on the working model:** no stop and no reset
   on a correction.
7. **The direction-before-declaration fix is accepted.**

## Evidence behind the rulings

- Lap 1 (`docs/NAVI_COHERENCE_0_4_FIRST_LAP_20260922.md`): declared 041–042,
  physically 040–041 (the operator's selection). The navigator ran +1 all lap.
  Observed polarity matched the map 184/184 at the true offset and 86/184 at
  the declared one, and nothing escalated.
- Run 2 (`docs/NAVI_COHERENCE_0_4_SECOND_RUN_20260922.md`): the ±10% window
  refused 4 of 78 real magnets (IR/map 1.158, 1.158, 0.845, 0.827). Accepted
  spans ranged 0.901–1.076.

## Mechanisms (engineering choices, not the rulings)

**Sequence authority.**
- The navigator keeps the last 10 observed polarities at 10 consecutive mapped
  points. The window restarts on declaration, on reversal (the first point
  repeats the one just passed), and after any multi-step advance (the skipped
  points were not observed).
- With the window full, position moves only when **exactly one** offset on the
  whole route matches **10/10**, and the current position disagrees on **at
  least 2**.
- Why 2: 108 of the route's 171 windows are one polarity flip from another
  window, so one Hall misread could imitate a different place. Requiring 2
  disagreements means a single misread can never move position. Every window
  differs from its ±1, ±2 and ±3 neighbours on at least 2 points, so a real
  small offset is always corrected on the 10th magnet.
- Host test: every start × both directions × offsets ±1..3 is corrected
  (2,052 cases). Every single misread in 20 magnets holds (6,840 cases).
- On correction: trust becomes `SEQUENCE_RECOVERED`; the sketch publishes a
  retained `SEQUENCE_CORRECTED` nav event and a sticky warning. Nothing stops
  and nothing resets. The station machine recomputes its offset from the
  corrected MM on its next tick. An Idle machine arms only when the offset
  lands exactly on −10, so if a correction lands inside an approach (−9..−6)
  the machine is armed there (`armAfterCorrection`). Without that, a
  correction of 2–3 magnets could jump past the arming point and run through
  the station.
- `seq_matches` now reports agreement at the current position (it was always
  0 in 0.4). `seq_len` is the observed window length.

**Window.** `IR_WINDOW_FRACTION` = 0.15. One-step and two-step windows cannot
overlap until about ±33% on this route (spans 280–355 mm).

**AUTO.** `NGR_ENABLE_EXPERIMENTAL_AUTO 1` in the sketch. The existing
admission rules are unchanged: declared position, no e-stop or low voltage,
forward only. Unresolved position still withdraws AUTO.

**Direction-before-declaration fix (included because AUTO makes it a
hazard).** In 0.4, `setDirection()` from UNSET reported TRACKING at MM0.
`admitAuto()` reads `positionKnown()`, so with AUTO on, a loco that had a
session direction but no declaration could have been admitted to AUTO at an
MM0 nobody declared. In 0.5, a direction set before a declaration records the
direction only.

## Implications and unintended-consequence risks

- **The sequence can now overrule the operator.** That is the intent. The risk
  is a false correction: it needs at least 2 Hall polarity misreads within 10
  magnets whose result exactly matches one other window. Neither run today
  misread a polarity (258 events). A degraded Hall sensor would change that.
  The correction is loud, and the operator can see it and re-declare.
- **The mechanism applies at any time.** Operator, 2026-09-22: "The
  correction should work at any time. That is much more powerful. It also
  helps if a derailment or handling or a misplaced magnet alter the true
  count." The risk is the same false-correction path as above, now open for
  the whole run rather than just after declaration.
- **A correction during a station stop moves the stop.** A correction of −1
  seen in Zone moves the ramp one magnet later; a +1 correction can trigger
  the zero-ramp at once, or put the train past the overshoot limit, which
  reports MISSED and resumes cruise.
- **In the first 10 magnets after a declaration, the position is still the
  declaration.** A wrong declaration can drive station logic for up to 10
  magnets in AUTO before it is corrected.
- **For 10 magnets after a reversal or a two-step advance there is no sequence
  authority.**
- **±15%** accepts a spurious Hall opening in 255–345 mm on a 300 mm span,
  where ±10% accepted 270–330. The observed spurious openings (magnet
  re-crossings) were at 19–30 mm, far outside either window.
- **AUTO has never run under this navigator.** Station approach, braking and
  departure are untested. The first AUTO run should be supervised with a hand
  on stop.
