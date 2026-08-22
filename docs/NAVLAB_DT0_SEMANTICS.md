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

Each clause has a synthetic test in `test_reachability_nav.py`. Those tests are
**development tests, not validation** — see the review below.

---

# Review of the rule (operator correction item 3, 2026-08-22)

## The one-interval grant is an assumption, not established physical truth

**A reset event does not prove that only one interval elapsed.** It proves that
the firmware re-anchored its timer and that the locomotive is at some magnet.
The elapsed time is unknown, and unknown time is unbounded: the locomotive may
have covered many intervals — after a hand reposition, after a re-declaration
following a long stretch of missed markers, or after any period the detector
did not report.

Rule 2 called the one-interval grant "bounded, never unlimited". That is true
and it is the wrong axis. Bounding growth protects against relocating anywhere;
it does nothing to keep the true position *inside* the corridor. A corridor is
only meaningful as an **over**-approximation. The one-interval grant is an
**under**-approximation of reachability whenever the unknown time covered more
than one interval, so it can exclude the truth.

## Rule 4's "fail safe" claim is refuted

`tools/navlab/probe_dt0_unknown_time.py` (development probe: synthetic events
on the real committed map and the committed envelopes) sweeps all 171 start
positions for several true gap sizes. Committed result:
`tools/navlab/results/iter3_probe_dt0_unknown_time.json`.

| true intervals during the unknown time | confirmed correct | **confirmed WRONG** | stopped | unresolved |
|---|---|---|---|---|
| control (no reset) | 171 | 0 | 0 | 0 |
| 0 (same-magnet reread) | 168 | 0 | 0 | 3 |
| 1 | 168 | 0 | 0 | 3 |
| 2 | 16 | **7** | 81 | 67 |
| 3 | 1 | **20** | 93 | 57 |
| 4 | 0 | **19** | 95 | 57 |
| 6 | 0 | **25** | 95 | 51 |
| 10 | 0 | **15** | 96 | 60 |

Inside the assumed domain (0 and 1 intervals) the rule behaves as written, and
the control run is clean, so the failures are attributable to the rule and not
to ordinary corridor behaviour. Outside that domain the *usual* outcome is a
stop — but in 7–25 of 171 start positions the navigator instead **confirms a
marker the locomotive does not occupy**, off by 1 to 11 markers. Failure is not
always a stop; sometimes it is a confident wrong position. Rule 4 as written is
false, and the missed-marker and prolonged-unknown-time cases are exactly where
it breaks.

## What the safe representation would be

Not an unconditional one-interval grant. Unknown time cannot be converted into
a small distance by assumption. Two representations are sound; both are
recorded here as candidates and **neither is implemented** — implementing one
would be a navigator redesign, and tuning the grant to preserve the Otto replay
is explicitly not the goal:

1. **Bounded hypothesis expansion with suspended confirmation authority.** Keep
   the pre-reset hypotheses, expand them forward by one interval per successor
   event as evidence arrives, and mark the chain as gap-bearing: while the flag
   is set, a collapse to a single candidate may not CONFIRM. Position is
   re-established only by the uniqueness argument the route-wide path already
   uses (the 12-window), never by three consecutive collapses inside a corridor
   that may not contain the truth.
2. **Explicit unknown-position state.** Treat a reset as loss of positional
   authority outright and require re-acquisition. Safer, and it forfeits track
   continuity across every ordinary re-declaration, which is a real cost.

Option 1 keeps continuity while removing the false-confirmation pathway; option
2 is simpler and more expensive. The choice belongs to iteration 4 and needs
data that does not exist yet.

## Consequence for the record

The claim "dt=0 handling is fail-safe" is withdrawn. The dt=0 rule is a
**candidate design assumption with a demonstrated counterexample**, and
condition C1 (never outside the physically reachable set) is scored FAIL on
that counterexample, not on replay behaviour.
