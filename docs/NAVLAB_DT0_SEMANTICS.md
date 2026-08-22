# dt=0 semantics — the rule, specified before implementation

Iteration-3 item 5. Applies to `tools/navlab/reachability_nav.py` only.

## Protocol meaning

A marker event with `dt == 0` is a **dt-chain reset**: the firmware
re-anchored its inter-event timer (declaration, boot, or re-declaration).
It means *traversal time unknown* — never *zero physical travel*. The event
itself is real evidence: the locomotive is at SOME magnet, with a measured
pole and peak.

## The rule

On a dt-chain-reset event:

1. **No timing veto.** Unknown time cannot refute any candidate, so the
   empirical-envelope test is skipped for this event only.
2. **Bounded corridor grant, never unlimited.** The corridor gains exactly
   ONE interval's spacing beyond its current reach. Rationale: unknown time
   could be long, but each reset event is a single magnet boundary; treating
   unknown time as unlimited travel would allow relocation anywhere, which
   is precisely the route-wide search the plan forbids. Repeated resets each
   grant one further interval — growth stays linear and auditable.
3. **Both hypotheses survive.** The new hypothesis set is
   (stay: current positions whose pole matches — the same-magnet-reread
   hypothesis) ∪ (advance: pole-matching candidates within the granted
   corridor). The successor event arbitrates, exactly like every other
   pending resolution.
4. **Fail safe.** If neither hypothesis matches, the event is held as a
   suspect (position unchanged); if the successor also fits nothing, the
   run CONTRADICTS and stops. Reachability that cannot be established is
   never invented.
5. **No borrowed information.** The rule consumes no MQTT receipt-time gap
   and no firmware position label. Reversal handling (motor evidence) is
   independent and unaffected.

## What the rule must NOT do (tested)

- expand the corridor without bound on repeated dt=0 events;
- confirm a position purely because unknown time made everything reachable;
- treat a same-magnet reread as forward travel;
- lose a genuine traversal that the reset happened to straddle;
- mask a phantom that follows a reset;
- disturb reversal modeling near a reset.

Each clause has a synthetic test in `test_reachability_nav.py`.
