V4 is materially stronger than V3. I would approve it for implementation review. The changes have closed the two concerns I raised on V3 and also tightened several authority-edge cases that were still implicit. NGR_DASHBOARD_AUTHORITY_ALIGNMENT_SPEC.md

The biggest improvement is that the specification now consistently separates UI sequencing from authority enforcement. P6 is now correctly phrased as “the console shall not issue cmd/auto 1 until ORIENTATION and LOCATION have been supplied,” while P11/R15 make those conditions firmware invariants. That is the right division of labor: the console helps the operator do things in the right order; the locomotive decides whether enlistment is valid.

The E-STOP design is also much better. The broadcast-set/per-locomotive-clear asymmetry is exactly the right answer to the mixed-state problem. A universal emergency stop has coherent semantics; a universal clear does not. V4 also fixes the state-display issue I raised by explicitly making retained state/estop authoritative and prohibiting both optimistic UI state and “last command sent” inference. NGR_DASHBOARD_AUTHORITY_ALIGNMENT_SPEC.md

R13/P14 is a significant architectural decision, but I think it is internally sound. Preserving DIRECTION while relying on zero propulsion means E-STOP recovery no longer destroys operating context. More importantly, V4 identifies the hidden UI hazard this creates: if the console slider retains its old value, clearing E-STOP could effectively resurrect a stale throttle request. P5 explicitly zeroing the slider closes that loop. That connection between firmware state and UI state is easy to miss, and V4 handles it.

P13 is another strong addition. The original station dedup mechanism was designed to suppress repeated state-machine events, not operator-command responses. Allowing an identical refusal to disappear merely because its event+offset matched the previous one creates an observability failure. A repeated command should produce a repeated response. Separating refusal publication from transition dedup is architecturally cleaner than weakening the dedup globally.

I also agree with the narrower language in P11. “De-energised” is what the firmware can actually prove. It cannot prove that the locomotive is physically motionless merely from propulsion state. Explicitly recording the pushed/coasting case as a residual rather than pretending the guard solves it is exactly the right treatment.

One remaining specification issue

There is one inconsistency I would correct before implementation.

§4.4 still says:

“Firmware position (console deliberately stricter, no firmware change needed)…”

That statement is now stale because R15/P11 explicitly does move R9 into firmware:

* ORIENTATION unset → firmware refusal
* navigation not ready → firmware refusal

So §4.4 describes the Draft-3 architecture while §3 and §6 describe Draft 4.

I would change that paragraph to something like:

R9 is enforced in both layers for different purposes. The console sequences ORIENTATION → LOCATION → enlist and does not issue the enlistment request prematurely; QUORUM independently refuses cmd/auto 1 unless ORIENTATION is set and navigation is ready. The console behavior is usability; the firmware behavior is the authority invariant.

That would remove the only substantive internal contradiction I see.

A smaller wording issue

§8 still says:

“The console’s pre-flight refusals (P6) are the one deliberate console-side strictness…”

After the P6 rewrite, the console does not really “refuse enlistment.” It withholds the request.

I would change that phrase to “the console’s pre-flight sequencing” or “pre-flight request suppression.”

That matters because V4 has otherwise become very precise about who is entitled to refuse what.

T6 is now particularly valuable

T6 has evolved into a strong end-to-end authority test. It verifies:

E-STOP ALL → throttle UI zero → authoritative state/estop → per-loco clear → DIRECTION preserved → BEGIN resumes → no release/re-enlistment.

That is no longer merely an E-STOP test. It proves that an emergency interruption does not accidentally transfer authority or destroy the locomotive’s operating setup.

T10 likewise correctly proves the complementary distinction:

PAUSE changes activity; END changes operator.

Those two tests together exercise the heart of the authority model.

Implementation sequencing

I agree with V4’s decision that the console work need not be held hostage by P11 firmware timing. The handoff problem in §7 is independently valuable and testable. Shipping the console with T2 explicitly blocked pending P11 is preferable to either delaying the entire handoff or pretending the motion invariant exists before firmware enforces it.

I would, however, keep that dependency very visible in the implementation notes: until P11 lands, R6 is policy but not an invariant. A non-console MQTT source could still enlist an energised locomotive.

Recommendation

My disposition would be:

APPROVE V4 FOR IMPLEMENTATION after two editorial corrections:

1. Update §4.4 because its “no firmware change needed” statement is superseded by R15/P11.
2. In §8, replace “console-side pre-flight refusals” with wording that reflects P6’s actual role: sequencing/withholding the request.

I do not see a reason for a Draft 5 architecture cycle solely for those corrections. They are consistency edits, not unresolved design.

The architecture itself has converged around a very clean rule:

The console requests and represents authority; QUORUM owns and enforces it; locomotive intelligence operates inside that granted authority.

V4 now follows that principle remarkably consistently. NGR_DASHBOARD_AUTHORITY_ALIGNMENT_SPEC.md
