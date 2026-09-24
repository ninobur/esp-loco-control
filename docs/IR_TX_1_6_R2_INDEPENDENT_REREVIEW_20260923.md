# IR TX 1.6 R2: Independent Re-review

Recorded 2026-09-23. Reviewed commit: `3887f2a`.
Provenance: independent assessment supplied by the operator as pasted text;
reviewer identity is not specified in the attachment. Preserved below verbatim.
This is not a new review performed by Codex or a claim of rerun tests.

Disposition: **independent re-review passed for supervised IR-car-only bench
evaluation; not approved for NAV/AUTO field operation.** No flash authorization.
No detector changes in this recording step. Physical evidence comes before
any decision to develop R3.

## Supplied Assessment

I re-reviewed R2 at commit 3887f2a, including the response document, the actual TX sketch, IrMovementDetector.h, and the new/adversarial tests. My assessment is:

R2 is ready for the next gate: supervised IR-car-only bench testing. I would not send it directly to a train run.

The important thing is that the earlier hard objections appear to have been addressed rather than merely documented around.

What changed my assessment

1. The hidden-continuity problem is substantially fixed.

The strongest R1 problem was that the detector could lose a real pulse yet later present apparently continuous usable evidence. R2 now makes discarded detector continuity observable through aborts/openAborts, and the revision test verifies that this breaks the odometry epoch and invalidates an existing MM distance reference even when the bad intermediate report never reaches Toby.

That is exactly the property I wanted: the consumer doesn’t have to be lucky enough to receive the transient bad state.

The test covers amplitude fault, mid-edge stop, inadequate moving contrast, and saturation.

2. Cold retention is no longer established too easily.

Requiring two compatible completed cycles before stationary retention is important. The new test explicitly demonstrates:

* one completed cycle → cannot establish retention;
* two agreeing cycles → can establish retention.

That makes retained SIGNAL_STALE evidence much more defensible.

3. Retention has been narrowed to the intended condition.

R2 no longer lets the retained thresholds dominate when useful live contrast exists. Retention is specifically bridging the quiet plateau; moving contrast returns to the live envelope.

That is an important conceptual improvement. We’re no longer asking a “stopped-wheel” mechanism to interpret normal movement.

4. The wildcard about frozen measurements is handled correctly.

The compiled regression is stronger evidence than merely discussing the older IR_SPEED_LOCAL behavior.

It demonstrates the distinction we discussed:

fresh radio packet ≠ fresh optical measurement.

A packet with a non-advancing capture timestamp becomes NO_NEW_SAMPLE, then LINK_STALE; it does not perpetuate a valid speed indefinitely.

And the test demonstrates a genuine stationary measurement can produce zero. Those are now explicitly different cases:

* measured zero → zero;
* frozen measurement → unavailable;
* stale link → unavailable.

That is the correct semantic separation.

5. Default behavior is protected.

The 4,000,000-sample differential test with retention disabled is particularly valuable. It gives good evidence that this work hasn’t quietly changed the ordinary detector path while fixing the opt-in stationary-retention path.

The remaining 23/25 sunlight result

I agree with Codex’s treatment of this.

It is not a reason to call R2 unsuccessful, because R2 hasn’t made the inherited behavior worse. But it absolutely prevents us from saying that R2 has established reliable optical odometry under changing illumination.

Two real wheel cycles disappear during that synthetic illumination transition, and there is at least one interval in which the endpoints look ready without the existing continuity counters revealing those missing cycles.

That’s a genuine unresolved observation-integrity problem.

I would not change the algorithm again before bench testing, however.

The proposed possible remedy—an envelope-change continuity fence—is exactly the sort of thing that could solve this case while unnecessarily fragmenting epochs during perfectly normal outdoor illumination changes. We don’t yet have physical evidence establishing where that boundary should be.

The bench should tell us whether this synthetic weakness occurs in the installed sensor/wheel system and what the actual waveform does when illumination changes.

One semantic issue I would keep on the review record

openAborts has become broader than its name suggests.

It now represents not merely an abandoned open pulse, but also certain continuity-invalidating detector events. That is functionally useful—the epoch consumer needs precisely such a durable counter—but future code could misunderstand the name.

I would not hold the bench test for a rename. I would make sure the design documentation explicitly says something like:

openAborts is a monotonic continuity-loss indicator; it is not exclusively a count of timed-out open optical pulses.

Codex has documented this, which is sufficient for this gate.

The older/newer consumer discrepancy also remains real

The response correctly says the older MovementEvidence path and the newer health/epoch path don’t have identical semantics around stopped SIGNAL_STALE.

I don’t think R2 should attempt to solve that. That’s a consumer architecture question, not a reason to complicate the detector again.

It should remain an explicit issue for NAVI_COHERENCE.

My disposition

I would change the status from:

R2 — independent re-review pending

to:

R2 — independent re-review passed for supervised IR-car-only bench evaluation; not approved for NAV/AUTO field operation.

That is deliberately narrower than “approved.”

The bench should now attack exactly the things simulation cannot establish: real stationary noise, sunlight/shade transitions, plateau stability, single-sample disturbances, very slow rolling, mid-edge stops, restart behavior, and whether the physical system produces the 23/25-style hidden undercount.

Most importantly, I see no reason from this re-review to send Codex back for R3 before collecting that physical evidence.

The development sequence now looks right:

R1 adversarial review → R2 fixes → independent re-review → physical IR-only bench → evaluate evidence → only then decide whether R3 is necessary.

Nothing in this review authorizes flashing or running the train.
