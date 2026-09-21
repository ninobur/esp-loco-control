# NAVI_IR 0.4 — independent review

Reviewed the patch supplied as `NAVI_IR_0_4_patch.zip` plus
`README_0_4_DELTA.md` (from `~/Library/Mobile Documents/.../NGR-Files/NAVI_IR/`).
Applied it to the repo tree, then independently ran the real host suite and
both ESP32 compiles the delta doc said it could not run. **The files are in
the working tree; nothing described here has been committed**, pending the
operator findings below.

## Build status: the named blocker is resolved

Copying the three patched files (`NAVI_IR.ino`, `Navigator.h`,
`HypothesisNavigator.h`) into `firmware/test-programs/NAVI_IR/` alongside the
repo's existing `MovementEvidence.h`/`firmware/common/IrMovementWire.h`
resolves the missing-dependency issue the delta doc flagged. Both ESP32
builds compile clean, no non-library warnings:

| Build | Flash | RAM |
|---|---:|---:|
| Default (AUTO gated off) | 1,010,107 B (77%) | 75,164 B (22%) |
| `NGR_ENABLE_EXPERIMENTAL_AUTO=1` | 1,010,627 B | 75,164 B |

(0.2 for comparison: 1,008,819 B / 75,020 B — a small, expected increase.)

The existing host suite (`tests/test_navi_ir.cpp`, `console_states.cpp`,
`replay_core.cpp`) does **not** compile as shipped against 0.4: it references
`WaitingDistance` (state, ruling, and the `waitingDistance()` accessor),
which 0.4 removes entirely, per the delta doc. This isn't a defect in the
patch — the patch correctly didn't touch tests it wasn't asked to touch — but
it means the suite could not, in fact, have been run before this review.
I updated the four affected test files to compile and pass against 0.4's
actual (verified, not assumed) behavior; see "Test suite" below.

With those updates: **46 native checks pass under ASan/UBSan, the console-
contract cross-check against the live `server/ngr_app_v1_11_2.py` passes,
and the noon replay passes clean: 513/513 tracking, 0 ambiguous, 0 lost, 0
committed-MM differences from the historical record.**

## Finding 1 — sustained IR absence is now invisible to the ruling stream

This is the most consequential thing I found, and I think it deserves an
explicit yes from you rather than surfacing later in the field.

I ran all 30 iterations of the suite's IR-unavailability loop under all three
unavailability reasons (`NO_SOURCE`, `STALE`, `OPTICAL_INVALID`) with
correct, sequential Hall polarity provided throughout. In every single case,
every one of the 30 observations returned `Ruling::Advanced`, `navMm`
advanced every time, `positionKnown()` stayed true throughout, and
`unresolvedCount` never left 0. **Hall-only navigation proceeds exactly as if
IR did not exist — for as long as IR stays unavailable, with no ambiguity, no
withdrawal, and no ruling-visible difference from ordinary tracking.** The
only trace is the quiet telemetry counters (`irWaits`, `irIssue`,
`distance_assessable:0`).

This is a faithful implementation of the rules as written — rule 1 ("NAVI
remains the primary navigator... using Hall polarity plus timing/physical
gates") and rule 7 ("IR-invalid... never becomes zero movement") both point
exactly here. But the practical consequence is stronger than the delta doc's
prose conveys: **if the optical sensor is unpaired, broken, or permanently
out of contrast for an entire session, 0.4 provides zero additional
protection against the same-polarity phantom-magnet failure mode IR was
built to catch** (decision 0089's stated motivation). It degrades silently
and exactly to plain Hall-only NAVI. Rule 3's reality gate (`TOO_EARLY`) can
only ever fire when IR is actually present and valid — absence is never
itself suspicious. Given today's field history (the afternoon IR segment was
`INADEQUATE_CONTRAST` for 516/535 snapshots), this is not a hypothetical
case.

I'm not proposing a change here — I think this is worth you actively
confirming rather than something I should second-guess by patching. A cheap,
non-architectural mitigation if you want one: surface `irWaits`/time-since-
last-assessable-distance on the console so an extended IR outage is visible
to the operator even though it no longer affects NAVI's own ruling.

## Finding 2 — movement-reseed fires on ordinary ambiguity, not just LOST

`reseedFromMovement()`'s own comment describes it as "movement-assisted
**recovery**," and the delta doc frames rule 5 as something that happens
when "NAVI loses confidence" after a run of trouble. In the code, the trigger
is simply `(result==Lost || result==Ambiguous) && haveConfirmedAnchor_` —
with no distinction between "we've been stuck a while" and "the local
hypothesis set just now split into two for the first time." I confirmed this
concretely with two traced scenarios:

- **The suite's "exactly half a mapped span" case** (a deliberate 0-vs-1-
  interval tie): locally this is a real, symmetric tie — `classifyDistance`
  correctly flags it (`closest==3`, tied). But because a confirmed anchor
  already existed, `judge()` immediately tries `reseedFromMovement`, which
  independently walks the whole map from that same anchor, finds exactly one
  polarity-supported candidate, and reports `MovementReseeded` with
  `distanceConfirmed:true` and `Trust::Sequence` — on the very first
  observation. The tie is real, but it never surfaces as a tie; it resolves
  immediately and is reported at the *highest* confidence tier.
- **The suite's recovery-exhaustion loop**, i=2: a local two-candidate split
  (missed-marker vs. false-observation) gets discarded and replaced by a
  reseed hit within the same single `judge()` call, again reported as
  `MovementReseeded`/`Trust::Sequence` on the first attempt — not after ten
  clean observations, not after a sustained recovery window.

Separately, `Navigator::judge()` sets `s_.trust=Trust::Sequence` on **every**
ordinary reestablishment now, not only when `hypothesis(0).clean>=10` — 0.2
gated this explicitly; 0.4's rewrite dropped the gate along with the
`sequenceLength` field it read. `trustName(Trust::Sequence)` publishes as
`"CONDITIONAL_SEQUENCE"`, which the sketch's own README (still describing
0.1/0.2) defines as earned by "ten subsequent clean observations forming a
unique word on this map." Under 0.4 it now means "resolved from ambiguity at
all, by any means, including a bare majority vote on the very first try."

None of this contradicts rules 5/6 as literally written — rule 5 does say
"uncertainty," not "terminal loss," and rule 6 says no second ten-magnet
qualification is required. So this may be exactly what you intended. But the
gap between "movement-assisted **recovery** after NAVI loses confidence" and
"replaces the local hypothesis machinery on the first ordinary split" seems
worth confirming deliberately, and the `CONDITIONAL_SEQUENCE` label is, I
think, actually wrong now regardless: it's describing something with a much
higher bar than what currently earns it. I'd suggest either gating
`Trust::Sequence` behind an actual clean-streak count again, or renaming/
splitting the telemetry so "resolved via reseed" and "ten-observation-
verified" aren't the same word.

## Two concrete bugs (telemetry-only; no navigation-decision effect)

Both are in `compareMovement()`'s `nav/ir_compare` publish in `NAVI_IR.ino`,
not in the decision path itself.

1. **`ir_hard_verdict` never actually changes.** A `hardVerdict` local is
   computed (`TIMING_TOO_EARLY` / `IR_TOO_EARLY` / `IR_POSSIBLE` /
   `CANNOT_ASSESS`) and passed to `snprintf`, but the format string still has
   `\"ir_hard_verdict\":\"CANNOT_ASSESS\"` as a **literal string**, not a
   `%s`. `hardVerdict` is passed as an unused trailing vararg — harmless per
   the C standard, but the field always reads `CANNOT_ASSESS` regardless of
   what actually happened. (I did wonder if `-Wformat-extra-args` would
   catch this; it doesn't fire for this compiler/case, and it wouldn't be
   `-Werror`-blocking either way since extra varargs aren't a format-string/
   argument-type mismatch.)
2. **Even once fixed, the priority is wrong.** The three conditions are
   written as `if(...)TIMING_TOO_EARLY; if(...)IR_TOO_EARLY; else if(...)
   IR_POSSIBLE;` — the second `if/else if` pair isn't chained off the first,
   so whenever timing says too-early but IR (if valid) disagrees, the second
   pair unconditionally overwrites `TIMING_TOO_EARLY` with `IR_POSSIBLE`,
   silently dropping the timing-gate's verdict from this field.

Fixing bug 1 without also fixing bug 2 would make bug 2 start actually
manifesting in telemetry, so they should be fixed together.

## Minor: `MAX_FAULTS` is now dead

Rule 6 ("ONE STRIKE is removed as terminal authority") is correctly and
thoroughly implemented — I checked all three candidate-creation branches in
`HypothesisNavigator::observe()`; none of them gate on `source.faults <
MAX_FAULTS` anymore (0.2 gated all three). `MAX_FAULTS = 1` is still
declared but no longer read anywhere. Harmless, but worth deleting or it will
read as a real cap to the next person who greps for it.

## Test suite

Updated `tests/test_navi_ir.cpp`, `tests/console_states.cpp`,
`tests/test_console_contract.py`, and `tests/replay_core.cpp` to compile and
pass against 0.4. I want to be precise about what kind of update this was,
since some of it is more than a rename:

- Symbol removal (`WaitingDistance` state/ruling/accessor): mechanical.
- Several assertions encoded **0.2/0.3 behavior that 0.4 deliberately
  changes** (first-observation-advances-on-Hall-alone; same-polarity-too-
  early now rejects outright as `Retained` instead of creating `Ambiguous`;
  sustained IR outage no longer holds position). I did not guess these — I
  instrumented a scratch copy of the suite to print actual rulings/status at
  each disputed point, verified my understanding against that output, then
  wrote the real assertions to match confirmed behavior. Every changed
  assertion has an inline comment saying what changed and why.
- The recovery-exhaustion loop (`Assessable contradictions still exhaust
  recovery`) now traces a specific, verified sequence that happens to
  exercise `MovementReseeded` partway through (see Finding 2) before real
  conflicting evidence drives it to `RECOVERY_EXHAUSTED`. I annotated this
  as data/map-dependent rather than a general property — it's the honest
  trace of what this map's polarity pattern produces at these indices, not
  a minimal isolated unit test of either mechanism.
- I did **not** write new, independent unit tests for `reseedFromMovement`
  or `classifyDistance` beyond what these (updated) integration-style
  assertions incidentally exercise. Deciding what correct behavior looks
  like for the new relocalization mechanism in isolation is a design
  question, not a compatibility fix, so I left it rather than guessing at
  a spec on your behalf.

## What I did not change

`MovementEvidence.h`, `HallObserver.h`, `RecoveryControl.h`, `RouteMap.h`,
`Stations.h`, `Ops.h`, `LocoConfig.h`, and the sketch's own `README.md` are
untouched — the patch didn't include them and nothing above requires
changing them. The sketch `README.md` still describes 0.1/0.2's pairing and
authority-model text and does not yet mention 0.4 exists or reflect rules
1-7; that's a real doc-currency gap but rewriting it is an authoring task
with real judgment calls (how to represent the new authority model,
telemetry additions, pairing steps), so I flagged it rather than doing it.
`README_0_4_DELTA.md` (from the patch) is now sitting in the sketch
directory alongside the stale main README.

## Bottom line

The named blocker — "couldn't compile because `IrMovementWire.h` wasn't in
the upload" — is resolved; 0.4 builds clean for both ESP32 variants once
placed in the repo tree, and the host suite (now updated) passes 46/46 plus
a clean noon replay. I did not find anything that looks like a build defect
or a sanitizer-detectable memory/logic error. What I found instead are two
small, real telemetry bugs (easy fixes, zero navigation impact) and two
behavioral properties — silent Hall-only fallback under sustained IR outage,
and eager, over-confident movement-reseed on ordinary ambiguity — that
correctly implement the rules as literally stated but have bigger practical
consequences than the delta doc conveys. I'd want your explicit sign-off on
those two before calling this flash-ready, independent of the mechanical
build status.

Nothing here has been committed. The patched sketch files, the delta doc,
and the four updated test files are sitting in the working tree.

## Reproduction

```sh
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/test-programs/NAVI_IR
arduino-cli compile --fqbn esp32:esp32:esp32 \
  --build-property "compiler.cpp.extra_flags=-DNGR_ENABLE_EXPERIMENTAL_AUTO=1" \
  firmware/test-programs/NAVI_IR
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I firmware/test-programs/NAVI_IR firmware/test-programs/NAVI_IR/tests/test_navi_ir.cpp \
  -o /tmp/test_navi_ir && /tmp/test_navi_ir
c++ -std=c++17 -Wall -Wextra -I firmware/test-programs/NAVI_IR \
  firmware/test-programs/NAVI_IR/tests/console_states.cpp -o /tmp/console_states
python3 firmware/test-programs/NAVI_IR/tests/test_console_contract.py /tmp/console_states
c++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -fno-omit-frame-pointer \
  -I firmware/test-programs/NAVI_IR firmware/test-programs/NAVI_IR/tests/replay_core.cpp \
  -o /tmp/replay_navi_ir
python3 firmware/test-programs/NAVI_IR/tests/replay_noon.py /tmp/replay_navi_ir
```

## References

- `README_0_4_DELTA.md` (in the sketch directory)
- [NAVI_IR 0.2 independent review](NAVI_IR_0_2_INDEPENDENT_REVIEW_20260920.md)
- [0.1 first-startup field record](NAVI_IR_FIRST_STARTUP_20260920.md)
- [Decision 0089](decisions/0089-navi-ir-joint-evidence.md)
