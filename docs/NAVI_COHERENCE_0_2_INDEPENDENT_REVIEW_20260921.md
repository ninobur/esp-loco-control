# NAVI_COHERENCE 0.2 — independent review of the extracted patch

**Correction (added during the 0.3 review, same day):** the "0 warnings"
result in the Build verification table below is **wrong**. `arduino-cli`'s
build cache served a prior compile's result for this unchanged source, so
`--warnings all` silently checked nothing. A `--clean` rebuild shows 15 real
`-Wformat` warnings, two of them a genuine argument-count mismatch in
`publishDecision()` that reproduces as an immediate SEGV on a host
`snprintf` with the same argument list/types (see
[the 0.3 review](NAVI_COHERENCE_0_3_INDEPENDENT_REVIEW_20260921.md) for the
full analysis and reproduction). This bug is **not** something 0.2
introduced relative to 0.3 or vice versa; it is present, unchanged, in both,
and this document's Build verification section should be read with that
correction in mind rather than trusted as originally written.

Independent review of `firmware/NAVI_COHERENCE/NAVI_COHERENCE_0_2_patch.zip`,
found sitting untracked in the working tree (appeared 2026-09-21 17:52, after
this session's own noon-run spatial-variability analysis was delivered — see
[MM_DECLARATION_SPATIAL_VARIABILITY_20260921.md](MM_DECLARATION_SPATIAL_VARIABILITY_20260921.md)).
Desk review plus independent build verification: no hardware access, no
locomotive commands sent, nothing flashed. The patch was extracted to
`firmware/NAVI_COHERENCE/NAVI_COHERENCE_0_2/`; the sketch itself was **not**
modified by this review.

## Scope

Read: `README.md` (the patch's own design/status doc), `NAVI_COHERENCE.ino`
(997 lines), `Navigator.h` (66 lines — the actual decision logic),
`RecoveryControl.h` (10 lines), `test_coherence.cpp` (23 lines), and
`docs/NAVI_COHERENCE_DEVELOPMENT_HISTORY_20260921.md` for the governing
design and the 0.1 review's prior findings. Diffed `RecoveryControl.h`
against `firmware/test-programs/NAVI_IR/RecoveryControl.h` to see exactly
what changed from the multi-hypothesis architecture. Not reviewed
line-by-line: `HallObserver.h`, `Ops.h`, `RouteMap.h`, `Stations.h`,
`MovementEvidence.h`, `LocoConfig.h` — none of these ship in the patch; all
five are borrowed unmodified from `firmware/test-programs/NAVI_IR/` to make
the sketch buildable (see Build verification).

## What 0.2 actually changed, checked against the 0.1 review's findings

`docs/NAVI_COHERENCE_DEVELOPMENT_HISTORY_20260921.md` records three defects
found in 0.1: the 500 ms timing fallback disappearing when IR was
unavailable, `Uncertain` able to feed a stale position to AUTO/station
logic, and Hall-callback-based (rather than mapped-landmark-based) recovery
history. All three were checked directly against the 0.2 code, not just
against the prose claiming they're fixed:

1. **Timing fallback with no IR anchor:** `Navigator::judge()`'s
   `!interval.usable()` branch checks `elapsed<MIN_MARKER_MS` before
   accepting — present. Confirmed.
2. **Uncertain feeding AUTO:** `stationService()` withdraws AUTO the instant
   `!navigator.positionKnown()`, and `loop()` separately withdraws on
   `NavState::Uncertain` — present, and redundantly so (two independent call
   sites reach the same conclusion). Confirmed.
3. **Mapped-landmark history:** `pushMapped()`/`acceptThrough()` push once
   per mapped marker walked (including inferred-missed ones, using the
   *expected* polarity for those and the *observed* polarity only for the
   final, real one) — not once per Hall callback. Confirmed.

All three hold up. This is a genuine, verified fix, not a rewritten claim.

## Confirmed issues

**MQTT client ID collides with NAVI_IR.** `NAVI_COHERENCE.ino:566`:
```
char id[48]; snprintf(id,sizeof(id),"NAVI_IR_%s",LOCO_NAME);
```
copied verbatim from `NAVI_IR.ino`. Both sketches target the same
locomotive (`LOCO_NAME` = Toby/9950012), so if NAVI_IR and NAVI_COHERENCE
are ever both pointed at the broker close in time — plausible this week,
given both are under active development on the same loco — whichever
connects second will make Mosquitto drop the first with the same client ID.
(NAVI_SIMPLIFIED has the same style of mismatch, `"NAVI_ONE_%s"` — this
project has a recurring habit of not updating the MQTT client-id string when
a sketch is renamed. This instance is worth fixing specifically because the
string it collides with, `NAVI_IR_9950012`, is a live, actively-tested
sketch on the exact same locomotive, not a retired one.)

**Boot serial banner still says "0.1".** `NAVI_COHERENCE.ino:876`:
```
Serial.printf("[BOOT] NAVI_COHERENCE 0.1 DEVELOPMENT — not field accepted.\n");
```
while `SKETCH_NAME` (line 40) is `"NAVI_COHERENCE_0_2"` and correctly flows
into the MQTT `state/bootid` record. MQTT telemetry will correctly say 0.2;
the raw serial console — what an operator watches directly during bring-up,
and this project's own established way of confirming "which build is
actually running" — will say 0.1. Low cost to fix, real value in fixing it:
this exact class of confusion (trusting a stale banner over the actual
build) is why the repo's convention is to make `SKETCH_NAME` "the ONLY thing
that tells telemetry which build is running" (the sketch's own comment,
line 37-39) — the serial line just doesn't follow that rule here.

**A missed-and-advanced acceptance silently drops its own polarity check.**
`Navigator.h:54-56`:
```cpp
s_.distanceConfirmed=true;uint8_t mm=s_.navMm;for(uint8_t i=0;i<steps;++i)mm=nextMarker(mm,s_.navDir);bool pol=hallSupports(o,polarityAt(mm));
if(steps>1){s_.missedSinceLast=uint8_t(steps-1);acceptThrough(mm,o,steps,EvidenceClass::MissedObservation);return Ruling::MissedAndAdvanced;}
acceptThrough(mm,o,1,pol?EvidenceClass::Confirmed:EvidenceClass::PolarityDiscrepancy);return pol?Ruling::Advanced:Ruling::AdvancedWithDiscrepancy;
```
`pol` is computed unconditionally, but the `steps>1` branch (a missed marker
followed by a later confirmation) never reads it. A single-step advance with
wrong polarity is correctly classified `PolarityDiscrepancy`/
`AdvancedWithDiscrepancy` (invariant #4 in the governing design: "Wrong
polarity at expected mapped location advances with a discrepancy record" —
no exception stated for the missed-marker case). A multi-step
missed-and-advance with wrong polarity on the *same* final event is
classified plain `MissedObservation`/`MissedAndAdvanced`, with no discrepancy
recorded anywhere in `NavStatus`, `Ruling`, or the rolling DNA's evidence
tag — even though the actually-observed (wrong) polarity is still pushed
into the DNA array's data, and the raw fields (`opening`/`window` in
`publishDecision`'s JSON) still carry the true observed polarity, so the
mismatch is reconstructable by a sufficiently careful downstream consumer,
just never classified or counted by NAVI itself. This is arguably the
higher-uncertainty case (a missed marker *and* a polarity mismatch,
together) and it's the one that gets *less* scrutiny recorded, not more.

## Worth flagging for awareness, not as defects

**`compareMovement()`'s diagnostic only ever evaluates 3 candidates, not
the 10 the navigator actually searches.** `NAVI_COHERENCE.ino:397`:
`for(unsigned step=0;step<=2;++step)`, publishing one `nav/ir_compare`
message per candidate — while `Navigator::judge()`'s real search goes to
`DNA_WORD=10` (`Navigator.h:52`). The loop bound (`2`) isn't derived from
`DNA_WORD` anywhere, so the two will keep drifting independently. Not a
safety issue — the decision logic doesn't consult this diagnostic stream —
but an operator or future reviewer watching `nav/ir_compare` to understand
a `MissedAndAdvanced` ruling at step 4 or beyond will see nothing that
explains it.

**Vestigial multi-hypothesis plumbing.** `publishDecision()` and
`compareMovement()` both keep a `for(uint8_t i=0;i<h.count();++i)` loop and
publish a `"branch"/"branches"` pair inherited from NAVI_IR's genuine
multi-hypothesis `HypothesisNavigator`. `RecoveryControl.h`'s
`stationConsensus()` similarly lost NAVI_IR's per-branch agreement check and
now just reads `hypothesis(0)` directly (confirmed by diff — this is the
one substantive simplification in that file, consistent with the README's
"single coherent position" model). None of this is a bug: `PositionView`
caps `count()` at 1, and `hypothesis(uint8_t)` ignores its argument and
always returns the one stored value, so the loops run 0 or 1 times and the
now-redundant `"authority":"ONE_COHERENT_POSITION"` label is accurate.
It's leftover complexity from the copy, and `hypothesis(i)` silently
ignoring `i` is a footgun if anyone reintroduces multiple branches later
without noticing the index is discarded.

**Two operator-facing warnings still say "NAVI_IR".**
`"AUTO disabled pending supervised NAVI_IR station acceptance"` (line 709)
and `"GO disabled pending supervised NAVI_IR acceptance"` (line 718) are
copied from `NAVI_IR.ino` unchanged. Cosmetic, but an operator reading the
warning on Toby's console while running NAVI_COHERENCE would be told to
wait on a different sketch's acceptance.

**The ±10% window's non-overlapping per-step structure has real dead
zones, and that is intentional here.** Consecutive candidate windows
(`cum×0.9` .. `cum×1.1` at each of the 10 steps) do not touch — there is a
genuine gap between one step's upper bound and the next step's lower bound.
A real magnet whose measured distance lands in that gap is rejected as
`NON_LANDMARK_HALL` even though nothing is physically wrong. This is not a
bug relative to the design: the development history doc and the sketch's
own README already anticipate it directly, citing this session's noon-run
finding that a flat ±10% single-pass window excludes about 10% of ordinary
intervals, and calling that acceptable for a deliberate first stress test.
Flagging it here only to connect it explicitly to the field-test objective
("real mapped magnets confirm arrival consistently") — a nontrivial rate of
transient `NON_LANDMARK_HALL` rejections on an otherwise-healthy lap should
be expected and is exactly what the test is designed to surface, not a sign
that something broke.

## Build verification performed here

The patch does not ship `RouteMap.h`, `MovementEvidence.h`, `Ops.h`,
`Stations.h`, `HallObserver.h`, or `LocoConfig.h`; none of the README's
"before flashing" instructions mention what to borrow or from where. By
tracing `#include` chains, the sketch needs, unmodified, from
**three different sibling sketch directories**:

- `firmware/test-programs/NAVI_IR/{RouteMap.h, MovementEvidence.h, Ops.h, Stations.h, HallObserver.h, LocoConfig.h}`
- `firmware/test-programs/NAVI_SIMPLIFIED/{LL_LocoConfig_9950011.h, LL_LocoConfig_9950012.h}` (reached via NAVI_IR's `LocoConfig.h`)
- `firmware/test-programs/NAVI_ONE_X22/{ExcursionDetector.h, MagnetRecognizer.h}` (reached via NAVI_IR's `HallObserver.h`)
- `firmware/common/*.h` and `firmware/QUORUM/credentials.h`

Also: **the patch's own folder name (`NAVI_COHERENCE_0_2`) doesn't match its
`.ino`'s basename (`NAVI_COHERENCE`)**, which the Arduino toolchain requires
to match — as delivered, neither Arduino IDE nor `arduino-cli` will treat it
as a valid sketch until the folder is renamed. And its current location,
`firmware/NAVI_COHERENCE/`, sits one level shallower than
`firmware/test-programs/NAVI_SIMPLIFIED/`, so `LocoConfig.h`'s
`../NAVI_SIMPLIFIED/...` relative include won't resolve there — it needs to
live inside `firmware/test-programs/` alongside its dependencies (which also
matches `CLAUDE.md`'s own artifact-classification rule that unaccepted
prototypes belong in `test-programs/`, not a new top-level `firmware/`
directory). `firmware/README.md`'s catalog has no NAVI_COHERENCE entry at
all yet, so this artifact is currently uncataloged as well as
un-relocated — a "librarian check" gap independent of the code itself.

With all of the above assembled into a correctly-named, correctly-placed
sketch folder (verification copy only — not part of the repo, discarded
after this review):

| Check | Result |
|---|---|
| `g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined` on `test_coherence.cpp` | **PASS** — 0 warnings, ASan/UBSan clean, all 6 asserted scenarios (early rejection, first-point establishment, spurious-lobe rejection, wrong-polarity-but-right-distance acceptance, stopped-duplicate rejection, missed-marker recovery + reversal) |
| `arduino-cli compile --fqbn esp32:esp32:esp32` | **PASS** — 1,000,447 B flash (76%), 68,028 B RAM (20%) |
| same, `--warnings all` | **0 warnings** |

The host test suite is real and passes cleanly, but its 6 scenarios don't
exercise either of the two confirmed issues above (neither a wrong-polarity
missed-and-advance, nor two sketches sharing a client ID) — passing tests
here should not be read as covering those cases.

## Documentation currency

`firmware/README.md` has no catalog row for NAVI_COHERENCE at all (checked
against the working tree's current, uncommitted copy). Per this repo's
librarian-check standing rule this should get an entry once the location
question above is settled — not added here, since where it belongs is
itself one of this review's findings and not mine to decide.

## Bottom line

The governing design is sound and 0.2 is a real, verified improvement on
0.1: all three defects the prior review found are genuinely fixed, the host
test suite passes clean under sanitizers, and the sketch compiles with zero
warnings once its (extensive, currently undocumented) dependency chain is
assembled. Two concrete, low-effort fixes are worth making before a field
flash: the MQTT client ID (`NAVI_IR_9950012` collides with an actively-used
sketch on the same locomotive) and the boot banner's hardcoded "0.1". One
logic gap is worth a decision either way: whether a missed-and-advance with
wrong final polarity should also record `PolarityDiscrepancy`, matching how
the single-step case already treats that exact combination of evidence.
Nothing found here contradicts the design doc's own stated first-test
objective or its explicit, already-acknowledged acceptance of a ~10%
transient-rejection rate from the deliberately tight window. Manual-only,
AUTO gated behind `NGR_ENABLE_EXPERIMENTAL_AUTO=0` (confirmed at its source
in `NAVI_IR/LocoConfig.h`, which this sketch borrows unmodified) plus the
unchanged `Ops.h` policy layer — the claim that AUTO is not authorized by
this build is backed by an actual compile-time default, not just the
README's prose.

## References

- [Patch README](../firmware/NAVI_COHERENCE/NAVI_COHERENCE_0_2/README.md)
- [Development history and governing design](NAVI_COHERENCE_DEVELOPMENT_HISTORY_20260921.md)
- [MM declaration spatial variability (this session's noon-run analysis, cited by the patch's own README)](MM_DECLARATION_SPATIAL_VARIABILITY_20260921.md)
- [NAVI_IR 0.2 independent review (house style and the sketch this patch is built alongside)](NAVI_IR_0_2_INDEPENDENT_REVIEW_20260920.md)
- `firmware/test-programs/NAVI_IR/` (source of every borrowed header)
