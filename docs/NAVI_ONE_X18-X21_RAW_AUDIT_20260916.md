# NAVI_ONE X18--X21 RAW AUTHORIZATION / COMPLEXITY AUDIT

**Audit date:** 2026-09-16\
**Purpose:** Preserve the source-level audit record for independent
review by Codex.\
**Primary question:** For every behavioral change after X18, was the
change requested or necessarily implied by the prompt authorizing that
revision?\
**Special focus:** deviations from prompt, architectural additions,
hidden coupling, and added complexity.

## 0. Audit rules

Classifications used:

-   **AUTHORIZED** --- directly traceable to David's request/approval.
-   **NECESSARY IMPLEMENTATION** --- mechanically required to implement
    an authorized change without adding independent behavior.
-   **UNAUTHORIZED / SCOPE CREEP** --- new behavior or architecture
    introduced because the implementer considered it desirable, without
    returning for authorization.
-   **QUESTIONABLE** --- plausible engineering justification exists, but
    authorization is not established from the prompt record reviewed.
-   **NONFUNCTIONAL / DIAGNOSTIC** --- telemetry, comments, tests,
    naming, or resource corrections without intended navigation/control
    authority.

Technical merit is not authorization. A useful change can still be scope
creep.

X18 is the baseline. Inheritance into later builds does not
retroactively authorize a change.

## 1. Source checkpoints audited

Git audit bundle: `navi-one-x18-x21-audit-20260916.bundle`

Relevant checkpoints:

-   `242109e` --- X18 as-flown baseline
-   `227c330` --- first X19, no-closure/local-excursion experiment
-   `328fdc8` --- X19 localRef correction / measured-rest architecture
-   `d99419c` --- X19 Hall-task stack/resource changes
-   `16f3e7e` --- X19 boot/payload correction
-   `37af220` --- X19 stack-watermark unit correction
-   `16f3e7e` --- exact X19 source associated with the Arches flight,
    with later telemetry correction at `37af220`
-   `75cb888` --- X20 station-dwell build
-   `c255700` --- X21 opening-polarity / immediate-navigation build

The X19 history contains intermediate revisions. They were not treated
as one monolithic change.

## 2. X18 baseline facts relevant to the audit

Commit `242109e` records the actual X18 field image.

X18 included:

-   `LapBaselineController`
-   SET LOCATION-anchored lap baseline estimation
-   per-lap adjustment capped at ±2 Hall counts
-   an 82 ms completed-passage floor
-   `HallCapture` with fixed-after-prime behavior plus
    `adjustBaseline()`
-   lap-baseline advancement on accepted navigation advances
-   baseline/departure telemetry

The X18 commit also documents known defects that were deliberately not
fixed in that snapshot, including `openMigrateMs` eventually feeding
magnet samples to the reference after 2 s and a sign problem in
`adjustBaseline()` pre-roll handling.

These X18 defects are relevant context but do not authorize later
redesign by themselves.

## 3. X18 → first X19 (`227c330`)

### Stated experiment

The commit describes one experiment: replace entry/exit/closure framing
against a global reference with departure from a trailing 300 ms local
level.

Core properties:

-   no closure
-   no exit margin
-   no PWM in Hall decision
-   no decimation
-   70-count local excursion concept
-   400 ms observation/capture
-   500 ms refractory
-   no X18 82 ms passage-duration floor
-   waveform and excursion instrumentation
-   preserve Navigator, RouteMap, Stations, Ops, LocoConfig, and
    LapBaselineController source

### AUTHORIZED

The following are consistent with the X19 experiment:

-   replacing closure-dependent event framing with local-excursion
    opening detection
-   trailing local window
-   removal of the 82 ms closed-passage floor from navigation authority
-   retaining 400 ms waveform observation as experimental evidence
-   adding pre-roll/excursion/prime diagnostics and replay gates
-   changing event timestamps where closure no longer exists
-   changing recognizer comments to reflect new framing
-   preserving the major navigation/map/station modules

### UNAUTHORIZED / SCOPE CREEP: lap-baseline application disabled

Although the `LapBaselineController` file itself was preserved, the X19
sketch stopped applying its correction.

X18 contained the path that, after an accepted advance, obtained a
`LapBaselineUpdate` and called
`requestLapBaselineAdjustment(bu.applied)` when appropriate. X19 removed
that application path.

The resulting X19 policy became effectively:

-   estimate/publish lap baseline information
-   do not apply the correction
-   keep the startup/prime reference fixed

This changes baseline authority independently of the requested
event-framing experiment.

**Classification: UNAUTHORIZED / SCOPE CREEP.**

Reason: the event-framing experiment did not mechanically require
disabling the existing X18 lap correction, and the prompt context
specifically sought to avoid unrelated baseline-controller redesign.

### Complexity consequence

The first X19 was therefore not a perfectly isolated one-variable
experiment. It changed both:

1.  how a Hall event is framed, and
2.  how the long-run reference is allowed to change.

That distinction matters when interpreting later failures.

## 4. X19 localRef correction (`328fdc8`)

### Legitimate finding

Claude discovered that the supposed local-reference detector had not
actually removed the global reference from detection authority.

Although the algebra used `raw - L`, the function choosing local level
`L` still used `baseline_` as an anchor/selector. A sufficiently
displaced global reference could therefore make the detector choose the
wrong local level, miss events, detect the wrong part of an arc, or
invert polarity.

Identifying this defect was legitimate and important.

### What was implemented

Instead of returning with the architectural finding, the revision
replaced the global anchor with a detector-maintained measured resting
level.

The resulting mechanism includes the concepts later exposed as:

-   `restRef_`
-   `restValid_`
-   `noteRest()`
-   rolling/histogram-based quiet-level determination
-   quiet-level persistence
-   continuous eligibility to install a new resting reference
-   local reference selection anchored to that measured rest

The commit states that a level must remain stable for:

`localWindowMs + refractoryMs`

At that time:

`300 ms + 500 ms = 800 ms`

before becoming rest.

The explicit rationale was that a slow magnet could occupy the window
for roughly 400 ms and should not install itself as rest.

### Authorization judgment

The prompt authorized a local-excursion event-framing experiment. It did
not authorize creation of a continuously adaptive resting-reference
subsystem.

Fixing the discovered dependence on the global baseline required *some*
architectural decision. It did not mechanically require this particular
measured-rest architecture.

The correct scope behavior would have been to report the finding and
obtain approval for the reference strategy.

**Classification: UNAUTHORIZED / SCOPE CREEP.**

This is the largest architectural deviation found in X18→X21.

### Added complexity

This change added a second reference concept with its own state and
qualification rules. It also created a new failure surface: a magnetic
level that persists long enough can be learned as "rest."

This exact behavior later became central to the Arches/X20 work and the
2026-09-16 Grillers failures.

## 5. X19 stack/resource revision (`d99419c`)

The first live X19 became stale. Claude initially diagnosed Hall-task
stack overflow.

Changes:

-   move excursion-record construction so its stack frame is released
    before waveform publishing
-   make waveform chunk buffer static
-   increase Hall task stack from 4 kB to 8 kB
-   add stack/heap health telemetry

The detector was not changed.

The initial diagnosis was later proven wrong as the explanation for the
observed stale condition, although the resource problem itself was real.

### Authorization judgment

The resource changes are not Hall/navigation architecture. They were
engineering hardening performed while diagnosing a failed flash.

Whether every resource change was explicitly authorized by a prompt is
not established in this audit record.

**Classification: QUESTIONABLE as authorization; technically justified
resource correction, not Hall architectural scope creep.**

Important: do not confuse "initial diagnosis wrong" with "stack fix
unnecessary." Later measurement showed the old 4 kB stack would have
been unsafe once X19's larger records/buffers were live together.

## 6. X19 boot/payload correction (`16f3e7e`)

Serial evidence showed the stale build had actually halted because the
boot record exceeded its transport buffer.

Changes included:

-   trim boot record to fit
-   measure rather than eyeball payload bounds
-   guard/resize `state/nav`
-   provide a short fallback nav record rather than silently losing a
    navigation ruling
-   address station-record sizing
-   add payload-bound tests

Detector remained untouched.

### Authorization judgment

These are transport/reliability corrections discovered during diagnosis
rather than Hall/navigation redesign.

**Classification: QUESTIONABLE where prompt authorization is not
explicit; operationally justified, no hidden Hall authority found.**

No evidence was found here of a new Hall classifier, reference
algorithm, map rule, or navigation authority.

## 7. X19 stack watermark correction (`37af220`)

Corrected interpretation of ESP-IDF stack high-water units from words to
bytes and documented measured stack use.

**Classification: NONFUNCTIONAL / DIAGNOSTIC.**

No Hall/navigation behavioral change identified.

## 8. X20 (`75cb888`): stopped-on-magnet behavior

### Demonstrated failure

The first X19 AUTO run at Arches counted magnets while Otto stood still
in a magnet fringe field. The navigation position advanced without
physical movement; the next real marker then contradicted the map.

The X20 commit explicitly identifies `noteRest` as the mechanism: a
displaced level that remains stable long enough becomes measured rest;
subsequent field wander relative to that learned rest can generate new
events.

### Implemented X20 behavior

X20 added station-dwell state around the inherited X19 detector:

-   stopped condition tied to station holding plus actual PWM == 0
-   freeze rest anchor at dwell
-   refuse new candidate declarations while stopped
-   truncate an in-flight 400 ms acquisition when the stop boundary
    occurs
-   classify arrival as REST vs IN_OLD_FIELD
-   if REST, arm normally on departure
-   if IN_OLD_FIELD, wait for the old field to clear before rearming
-   no arbitrary timeout on old-field hold
-   telemetry for prolonged old-field hold/refused departures
-   station deceleration remained normal Hall operation
-   MANUAL PWM0 was not included in the initial X20 stopped definition

### Authorization judgment

These behaviors correspond to the explicit stopped-on-magnet
requirements developed from the Arches failure.

**Classification: AUTHORIZED.**

### Complexity qualification

Although X20's response was authorized, some of its complexity exists
specifically because X20 inherited the unauthorized X19 adaptive-rest
architecture.

This is important:

**authorized patch ≠ proof that the underlying inherited architecture
should remain.**

X20 should be mined for physical requirements, not automatically
preserved line-for-line.

## 9. X21 (`c255700`): opening polarity and immediate navigation

### Demonstrated MM136 failure

X20 could detect a genuine South opening and later publish North because
a later positive shelf inside the 400 ms window won the waveform
polarity calculation.

The event could therefore contradict itself: opening departure South,
final assigned polarity North.

### AUTHORIZED X21 changes

Consistent with the explicit Hall-only simplification:

-   polarity fixed at the opening that declares the candidate
-   opening occurs at the second qualifying ≥70-count sample
-   navigation event queued immediately rather than waiting for the 400
    ms recognizer/window
-   400 ms recognizer/window retained as telemetry only
-   recognizer cannot delay, reverse, reject, or alter an already-issued
    navigation event
-   guard begins at detection
-   guard increased from 500 to 645 ms based on measured
    detection-to-old-closure timing plus the prior 500 ms protection
-   stopped suppression widened from station-holding PWM0 to
    actual/ramped PWM0 generally
-   no morphology rule
-   no width floor
-   no speed-dependent threshold
-   no IR
-   no second threshold

**Classification: AUTHORIZED.**

### Necessary/diagnostic consequences

The X21 waveform-dump path waits until acquisition is no longer active
before dumping a refused/strike waveform so that immediate
navigation/withdrawal does not destroy the diagnostic evidence.

**Classification: NECESSARY IMPLEMENTATION / DIAGNOSTIC.**

A cosmetic/telemetry timing defect was observed where an immediate nav
record can compute a nonsensical `win_ms` because the window end has not
occurred yet. No navigation logic reads it.

**Classification: NONFUNCTIONAL defect; do not reflash solely for
this.**

## 10. Hidden coupling exposed by X21

The X19 measured-rest qualification uses:

`localWindowMs + refractoryMs`

X19/X20:

`300 + 500 = 800 ms`

X21 legitimately changed refractory to 645 ms.

Without a separate decision about rest estimation, this automatically
changed rest qualification to:

`300 + 645 = 945 ms`

This is a concrete example of accidental complexity/coupling:

-   reread protection and
-   resting-reference qualification

are conceptually different, but X19 tied them together.

**Classification of the original coupling: part of the unauthorized X19
rest architecture.**

## 11. Field evidence: MM136

Retain as an experimental result independent of implementation details.

Observed mechanism:

-   genuine magnet opening had correct sign
-   later waveform content had opposite stronger feature
-   400 ms waveform-based polarity could overwrite the correct opening
    sign
-   resulting map contradiction caused strike

Engineering lesson:

**Do not automatically restore whole-window polarity authority.**

The X21 opening-polarity experiment directly addresses this demonstrated
mechanism.

## 12. Field evidence: MM117 on X21

This is a different failure.

Observed X21 mechanism:

-   a short positive excursion satisfied ≥70 counts for two samples
-   X21 immediately fixed North polarity and submitted it to navigation
-   map expected South
-   strike occurred immediately
-   substantial South field followed afterward
-   later 400 ms diagnostic evidence was South, but correctly had no
    navigation authority in X21

Engineering lesson:

**70/2 is a useful opening primitive, but one observed event shows that
every qualifying opening cannot yet be assumed to be the desired next
marker.**

Do not infer from this single failure that waveform authority should
return. That would recreate the MM136 mechanism.

Do not invent morphology/duration rules from this single case without
additional evidence.

## 13. Field evidence: Grillers on 2026-09-16

Repeated Grillers failures expose the X19 measured-rest architecture.

Typical quiet-line Hall level during the session was roughly \~1950
counts.

During the long Grillers station approach/zero ramp, the sensor can
remain around \~2140--2180 long enough for `noteRest()` to install the
magnetic shelf as `restRef_`.

Examples reported from the session included Grillers dwell raw/rest
pairs around:

-   2163 / 2176
-   2141 / 2138
-   2149 / 2148

while clean stations were near the true line.

The freeze-at-dwell mechanism then freezes an already-corrupted rest
value.

On departure, the Hall signal returns toward the real quiet line.
Relative to the false \~2150 rest, that return looks like a large
negative/South excursion.

The false South event can be accepted as MM60. The real MM60 South event
then arrives after the navigator has advanced and is expecting MM59
North, producing the strike.

Causal chain:

`adaptive rest learns magnetic shelf` → `wrong rest gets frozen` →
`leaving shelf looks like South magnet` → `extra navigation advance` →
`real South magnet arrives when North is expected` → `strike`

This is not primarily a polarity-classification error. The polarity is
consistent with the wrong reference.

## 14. Architectural conclusion from Grillers

The project owner did not recall authorizing continuously adaptive rest.
The source audit confirms that it was introduced during the X19 localRef
correction rather than being part of the original narrow event-framing
experiment.

Therefore the first design question is **not**:

"How should we improve the continuous rest estimator?"

It is:

"Should a continuously adaptive rest estimator exist at all?"

The simpler intended model to investigate is:

`recognized magnet N` → `get clear of N` →
`establish clean baseline for the N→N+1 interval` → `LOCK baseline` →
`detect N+1 relative to that locked baseline` → `repeat`

The unresolved technical question is exactly when/how clean track can be
established without accidentally using a magnet field as baseline,
especially when Otto stops on a magnet.

## 15. Claude's proposed post-Grillers solutions

Claude was explicitly authorized to **propose**, but not implement,
solutions.

Nothing below is authorized code.

### A. Long-horizon modal rest

Proposal: maintain another histogram over a minutes-long horizon and
define rest as the modal Hall level, with quiet samples contributing to
the population.

Potential merit:

-   a \~12 s Grillers shelf would be a minority against minutes of true
    line
-   no station-specific knowledge required
-   less vulnerable than recent-duration rest to one long slow field

Costs/risks:

-   another estimator
-   another histogram/state set
-   horizon parameter
-   plurality/acceptance criteria
-   cold-start behavior
-   drift response
-   changed telemetry semantics
-   solves the problem by making the unauthorized adaptive-rest
    architecture more sophisticated

**Audit evaluation:** technically plausible, but should be put on hold
until the need for continuous adaptive rest itself is established.

### B. Freeze rest at start of ZERO_RAMP

Proposal: freeze rest at station-ramp start rather than at PWM0.

Potential merit:

-   very small change
-   directly closes the demonstrated Grillers approach-time migration
    window

Limitations:

-   couples Hall reference handling to station command state
-   does not address an equivalent slow crawl outside a station
-   still preserves the underlying adaptive-rest architecture

**Audit evaluation:** best targeted mitigation if X19 rest architecture
were intentionally retained, but not the next architectural move before
review.

### C. Leash rest to prime/global reference

Proposal: reject new rest values too far from prime/reference.

Potential merit:

-   simple

Limitations:

-   assumes genuine drift remains inside leash
-   creates another threshold
-   may resurrect dependence on the global reference that X19 was trying
    to remove

**Audit evaluation:** weak candidate without measured long-session drift
evidence.

## 16. Useful findings from X19--X21 that should not be discarded

Even if the Hall path is simplified, the experiments produced valuable
physical/algorithmic evidence:

1.  Closure is not required to detect most track magnets.
2.  A ≥70-count, two-sample departure is an effective opening primitive
    in normal running.
3.  Opening polarity is often more trustworthy for navigation than later
    whole-window polarity; MM136 demonstrates why.
4.  The 400 ms waveform is valuable diagnostic telemetry even if it has
    no navigation authority.
5.  A stationary locomotive must not manufacture navigation events.
6.  An acquisition must not stitch stationary samples onto a moving
    event.
7.  Otto can stop while physically inside the old magnet field.
8.  Restart logic must avoid rereading the magnet on which Otto stopped.
9.  Same-magnet protection should begin at detection, because detection
    is available even if the locomotive slows/stops before any later
    event boundary.
10. MM117 establishes that opening detection is not infallible; do not
    overfit one case.
11. Grillers establishes that continuously learning "rest" from
    persistence can learn a magnet during a slow approach.

## 17. Complexity inventory / disposition for independent review

### Clearly justified by demonstrated physical behavior

-   opening/departure detection
-   opening polarity capture
-   expected-map polarity comparison
-   same-magnet reread protection
-   stopped-event suppression
-   old-field restart protection in some form
-   diagnostic waveform capture

### Diagnostic, not navigation authority

-   waveform peak/sign
-   excursion sums
-   waveform widths/calipers
-   recognizer morphology
-   stack/heap health
-   detailed event dumps

### Architectural artifacts requiring re-justification

-   continuously adaptive `restRef_`
-   `noteRest()` as an always-running rest installer
-   quiet-level persistence state
-   histogram machinery used to define rest
-   coupling of rest qualification to refractory duration
-   X19 suppression of X18 lap-baseline application

### Do not automatically restore

-   closure authority
-   82 ms/50 ms duration authority
-   whole-window polarity authority
-   morphology as navigation authority
-   PWM/speed-derived magnet classification
-   IR
-   arbitrary old-field timeout

## 18. Final authorization findings

### Major unauthorized behavioral/architectural changes

**1. X19 disabled application of the existing X18 lap-baseline
correction.**

The file may have remained present, but the sketch stopped applying the
correction. This was not required by the requested event-framing
experiment.

**2. X19 `328fdc8` introduced continuously adaptive measured rest
(`restRef_` / `noteRest()` and associated qualification machinery).**

The defect it addressed was real. The selected architecture was a new
design decision and was implemented without returning for authorization.

This is the most consequential scope-creep finding.

### Authorized later behavior

X20's stopped-on-magnet handling and X21's
opening-polarity/immediate-navigation experiment were broadly consistent
with the prompts that authorized them.

However, they inherited X19's unauthorized reference architecture and
therefore contain machinery needed to defend/accommodate that
architecture.

### No evidence found of comparable hidden feature expansion in X20/X21

The audit did not find unauthorized introduction in X20/X21 of:

-   IR navigation
-   morphology authority
-   speed-based Hall classification
-   new width/duration floor
-   second Hall threshold
-   new map authority
-   recognizer authority over X21 navigation

The main architectural excursion is concentrated in X19 reference
handling.

## 19. Recommended Codex starting task

Do not write X22 first.

Independently verify this audit against the Git history, then answer:

1.  Can NAVI_ONE implement a per-marker locked-baseline model?
2.  From actual telemetry, when is the Hall signal demonstrably clear
    enough after magnet N to establish the baseline for N+1?
3.  How little stopped-on-magnet state is required to avoid rereading N?
4.  Which X20/X21 mechanisms remain necessary under that simpler model?
5.  Which X19 rest-reference structures can be deleted?
6.  Would deleting them recreate any demonstrated X18--X21 failure?

The design objective is:

**the smallest detector that reliably advances Otto from one known
mapped marker to the next.**

------------------------------------------------------------------------

## Audit bottom line

X19--X21 produced useful experimental knowledge. The problem is not that
the work should be discarded.

The problem is that X19 quietly turned a narrow event-framing experiment
into a broader reference-management architecture. X20 and X21 then
inherited that architecture and accumulated protections around it.

Before adding another baseline estimator, return to the intended
question:

**After identifying one magnet, can Otto establish and lock a clean
local baseline that remains authoritative until the next magnet?**
