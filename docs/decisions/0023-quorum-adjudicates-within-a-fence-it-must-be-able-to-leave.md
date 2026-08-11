# 0023 — QUORUM will not rank candidates it cannot trust; the defect is the fence, not the scorer

Status: Accepted in part (operator review, 2026-08-11)

Accepted and implemented: the rejection of a ranking prior, and the
exact-or-silent HARD_BOUND advisory (commits 70cab5b, 160eff5, 723f0b4).
Approval covers the replay harness and the diagnostic advisory only.

NOT approved by that review, and still proposal-only:
  * closing the low-PWM phantom hole — see
    `docs/QUORUM_LOW_PWM_PHANTOM_DESIGN_PROPOSAL.md`, which recommends
    gathering a targeted sample before any rule is implemented;
  * the joint fence-width / adoption-evidence-floor redesign, which owes its
    own record;
  * flashing or field deployment. Nothing has been flashed.

## Decision

QUORUM will **not** gain a prior that re-ranks the six candidates in
`QUORUM_OFFSETS {-1,0,+1,+2,+3,+4}`. The proposal to break close ties — specifically the
+3/+4 tie in the 2026-08-10 NO_QUORUM — is rejected.

Instead, at HARD_BOUND only, QUORUM will publish the result of the existing dead
`dnaMatch()` — an exact, unique 12-window match — as an advisory field on the
`mm/no_quorum` snapshot. It will still stop, and the operator will still declare. Separately,
the low-PWM hole that lets phantom events in at station departures will be closed.

Fence width and the adoption evidence floor are recognised as defective but are **not**
changed here; they must be redesigned together, under their own record.

## Context

On 2026-08-10 loco 9950011 stopped at NO_QUORUM/HARD_BOUND with `sc [5,3,5,5,6,6]`, offsets
+3 and +4 tied at 6/12. The request was for a bounded prior able to separate them.

Analysis of the raw capture shows the premise does not hold. The published evidence ring is
byte-identical to `DNA[7..18]`: the locomotive was at marker 18, offset **−5**, which the
fence cannot express. Offset −5 fits 12/12 and uniquely among all 171 route alignments,
while every in-fence candidate sits at or below chance (empirical mean 5.994). Because the
ring is an exact DNA window, the score vector is the map's autocorrelation, not a measurement
of the locomotive.

This was not a one-off. In all three NO_QUORUM events in that run the true offset was outside
the fence (+8; unrecoverable; −5). The +8 is corroborated by the operator, who read the
physical marker and declared 110 against an odometer reading 102.

The proximate cause was traced by change-point fit (0 mismatches over 39 events): two phantom
events accepted during the Bamboo departure at pwm 12/19, where `GATE_LOW_PWM_FLOOR` disables
the conservation test, put the odometer +2 ahead. Since −2 is outside the fence, QUORUM
adopted +3 on a four-entry ring where **−2 also scored 4/4** — the two-point margin existed
only because the true answer was absent from the ballot — compounding the error to −5. The
final refusal was correct.

## Alternatives considered

**Rank the tied candidates using a bounded prior.** Rejected. Neither tied candidate was
correct, so ranking converts a correct stop into a confident 8–9 marker error. Tested against
all three incidents, every simple tie-break rule is wrong wherever it fires.

**Anchor a prior on `lastConfirmedMm` / distance travelled.** Rejected as a position source.
"Confirmed" means one bit agreed with the map, which happens ~50% of the time at a wrong
position; it was false twice in this run by DNA degeneracy. The only distance model
(`3.90·pwm − 99.2`) is biased +13% run-wide and +25% in the incident window, needs 6.5%
accuracy to separate the tied candidates, and is biased in the direction that manufactures
evidence for the hypothesis it would be testing.

**Use route or station intent.** Not possible: `STATIONS[]`, `stPhase` and `stIndex` are all
derived from `navMm`, so they cannot constrain it. `enterNoQuorum()` also calls
`stationReset()`.

**Re-enable `dnaMatch()` as an authority.** Rejected. That is the lineage QUORUM replaced —
the matcher overriding the odometer is how a locomotive at MM133 concluded, at certainty
1.000, that it was at MM105. The advisory form above keeps the matcher a witness, not a judge.

**Widen the fence now.** Deferred. The truth was outside the fence three times, so the fence
is the defect — but widening it under today's adoption floor (which can fire on three
readings) would make confident wrong adoptions more likely. Incident A came within one margin
point of adopting +2 when the truth was +8.

## Consequences

- NO_QUORUM remains the only automatic outcome when QUORUM cannot resolve position, and
  operator declaration remains the only recovery. This run produced three NO_QUORUMs, zero
  automatic recoveries, and three correct human declarations.
- The published advisory must be exact-or-silent. A wrong non-null value is a ship-blocking
  defect, since its purpose is to save the operator a walk, and a wrong hint is worse than
  none. With the existing ±5 window it returns marker 18 for this incident and refuses on the
  corrupted ring.
- Closing the low-PWM hole is now on the critical path: 10.2% of accepted marker events in the
  run were never conservation-tested, and the failure window had seven consecutive ungated
  acceptances.
- A follow-up record owes a joint redesign of fence width and adoption evidence floor. Until
  then the adoption path stays as-is, including its ability to fire on three readings.
- Adjudication-affecting changes now require the replay suite defined in the design note,
  including the compounding test — no such test exists today, and compounding is what happened.

## References

- `docs/QUORUM_PRIOR_AWARE_ADJUDICATION_DESIGN_NOTE.md` (2026-08-11) — full investigation,
  reproduction, and replay test suite
- Capture: `ngr-pi:/home/david/NGR/telemetry/runs/20260810_IR_SPEED_LOCAL_1_2_otto.log`,
  incidents at lines 14945, 19608, 25092; the causal adoption at line 24800
- `firmware/QUORUM/QUORUM.ino`: `QUORUM_OFFSETS` :792, `scoreEntry` :993, `decideEvaluation`
  :1168, `dnaMatch` :1363, `GATE_LOW_PWM_FLOOR` :795, velocity model :802
- Supersedes nothing; constrains any future change to `scoreEntry()` or `QUORUM_OFFSETS`
