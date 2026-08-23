# 0043 — The replacement host navigator carries a reversal shadow and confirms only on uniqueness

Status: Accepted  (2026-08-22)

Follows 0042, which unfroze the navigator as the candidate and left the
frozen acceptance harness as the thing it must satisfy. This record fixes the
design choices the replacement host navigator makes where
`docs/AUTONOMOUS_POSITION_ACQUISITION_SPEC.md` admits more than one reading,
so they are decisions on the record rather than implementation accidents.

Scope: the host model at `tools/navlab/hostnav/`. This record authorises no
firmware change, no flash and no train run.

## Context

The navigator was implemented against the harness frozen at `db9bb54`, which
was written before it. Five clauses of the specification admit more than one
implementation, and in each case the harness pins behaviour tightly enough
that the choice is load-bearing. Recording them here means the next person to
change one knows it was a choice.

## Decisions

### 1. The complete set carries a reversal shadow

The evidence record of §3.2 has no travel-direction field. A native reversal
is therefore unobservable to the navigator, and on this map it is
*polarity-invisible* at 45 of the 96 positions the reversal family exercises:
the marker behind carries the same polarity as the marker ahead, and the two
intervals differ by less than the timing envelope can separate. A
direction-preserving hypothesis set is consequently an under-approximation
across a reversal, and P5 forbids calling one `COMPLETE`.

**Decision.** The navigator propagates the same detections through two branch
lists. The **track** lane preserves direction and carries the navigation
claim. The **safety** lane additionally admits the reversed continuation at
every detection. The published set is their union, so it stays complete; the
navigation claim stays single-valued, so `POSITIONED` still means what §4.1
says it means. When the track chain dies and the reversal-admitting set does
not, the track set is replaced by it and the evidence window is dropped.

**Rejected:** admitting reversal in the tracked set. It makes `|H| > 1` at
roughly half of all detections on a clean stream, which by §4.1's exit rule
would drop the locomotive out of `POSITIONED` continuously.

**Rejected:** admitting reversal only when the forward chain dies. It is the
existing `maybe_reverse()` behaviour and it loses the truth in the 45
polarity-invisible cases, which is exactly the under-approximation S2 exists
to catch.

### 2. Confirmation is uniqueness only

§3.11 permits confirmation by collapse *or* by uniqueness. The collapse path
confirms from `|H| = 1` advancing coherently for `K_CONFIRM` detections, and
a locomotive that has reversed invisibly advances coherently down a wrong
chain for exactly that long before its polarity disagrees.

**Decision.** Uniqueness only, at the computed `W_dir` where a live operator
direction declaration holds and at `W_both` otherwise. Successor agreement
maintains an existing anchor but never creates one. The collapse path is not
used as a confirmation source in this build.

This is the stricter of the two the specification allows, and uniqueness
alone meets every usefulness gate on the generated families, so nothing is
bought by keeping collapse.

### 3. The lower distance bound is zero

**Decision.** `d_lo = 0`. Nothing in the evidence record excludes the
locomotive having been slower than nominal inside an interval the navigator
samples only at its ends, so any positive lower bound derived from nominal
speed is an under-approximation that can exclude the truth. Standstill is
handled separately and correctly: the same-marker candidate is admitted only
where the PWM profile leaves standstill possible.

This is not a weakening. A positive `d_lo` derived from a 25% band excludes
the true successor whenever unmodelled dwell inflates the interval — which
the generated ghost families produce directly — and that is precisely the
class of defect the withdrawn one-interval grant belonged to.

### 4. C1 (alone) is not established by silence

§4.2 requires the absence of a peer to be established by the decision-0031
membership rules and forbids inferring it from silence. The navigator
contract carries no membership channel.

**Decision.** C1 is never established from the absence of peer reports. On
generated evidence C2 is the only context that authorises acquisition motion,
and a locomotive with no peer information stands and publishes the reason
rather than moving. An explicit membership statement is wired in for when a
mechanism exists. This is stricter than the reading in `N3`'s note, which
assumed C1 would be the usable context on current hardware; it is the reading
§4.2's own sentence requires.

### 5. A contradiction latches a fault until the operator restarts

§5 says recovery from a contradiction is by uniqueness or declaration and
that neither is demanded. §7.5 says a GO is required after the locomotive
actually stopped, entered fleet hold, **hit a contradiction**, or cancelled
AUTO. §7.6 says an unscheduled stop requires deliberate operator restart
after investigation.

**Decision.** A contradiction orders a stop, latches a diagnostic snapshot,
re-seeds to all 342 bits, and suspends confirmation authority until
`operator('restart')`. The navigator keeps observing, reasoning and
publishing throughout — autonomous position reasoning is not suspended, only
the act of re-anchoring itself out of a model that has just been falsified.

## Consequences

- The generated acceptance suite reports PASS=49, FAIL=0, NOT_IMPLEMENTED=0,
  NOT_DEMONSTRATED=3 against this navigator. That is not validation: `N1`,
  `N2` and `N3` are the three things generated data cannot establish, and
  they are unchanged.
- Decisions 1 and 3 are the two that would most plausibly be revisited if a
  motor-direction field were added to the evidence record, which would make
  the reversal observable and the safety lane unnecessary. That is a firmware
  change and is out of scope here.
- Decision 4 will need revisiting the moment a membership or protected-region
  mechanism exists (open operator decision 4).
- None of these decisions is an operator policy. Open operator decisions
  remain in `navapi.Policy`; rulings already closed are not configurable.
