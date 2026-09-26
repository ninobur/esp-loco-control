# NGR NAVI --- Architectural Lineage and Decision Rationale

**Draft v0.1 --- 2026-09-25**

**Purpose:** Durable orientation for David, Sam/ChatGPT, Claude, Codex,
and future NGR development.

**Status:** Working reference. Revise when later field evidence or
operator decisions refine or supersede a principle.

## 1. Governing lessons

NAVI developed through field tests, failures, reviews, and explicit
operator rulings. Reading only the newest sketch loses essential
context: some attractive approaches were already tested and rejected;
some current rules are provisional; some settled decisions remain
unimplemented; and some older mechanisms remain useful even though the
architecture around them failed.

The architectural spine is:

**DETECTION → HARD / PHYSICAL CONSTRAINTS → NAVI JUDGMENT**

A second governing principle emerged repeatedly:

**Sensors make observations. NAVI maintains knowledge.**

Hall reports magnetic observations. IR reports wheel movement. PWM
reports commanded/applied propulsion. The route map reports surveyed
geometry and polarity. None independently owns locomotive position or
train state.

Behavioral claims about the physical railway should be established
primarily on the actual NGR. Host tests prove software behavior; replay
tests focused hypotheses; track testing is primary behavioral evidence.

## 2. Development lineage

### NAVI_ONE

NAVI_ONE deliberately used severe rules: expected polarity advanced;
unexpected polarity struck. Its value was diagnostic. It exposed
disagreements rather than explaining them away. Field work also
demonstrated the danger of morphology, stitching, closure, and temporary
experimental rules acquiring permanent navigation authority.

### NAVI_SIMPLIFIED

NAVI_SIMPLIFIED restored judgment explicitly:

-   Detection reports a qualifying Hall opening.
-   Hard Protection asks whether another mapped magnet could physically
    have been reached.
-   Transparent Judgment interprets physically possible observations
    using context and history.
-   Morphology remains diagnostic only.
-   Temporary ambiguity is permitted.
-   Reasoning must be reconstructable.
-   Historical mixed-firmware logs generate hypotheses; the actual
    railway running the new architecture supplies primary behavioral
    evidence.

Its failed railway build was important evidence. Repeated readings of
one physical magnet could enter the ambiguity budget because physical
protection disappeared once position became unresolved. The train
stopped after one physical magnet while software believed it had
consumed ten subsequent observations. The build was rejected, but the
failure clarified that physical protection must survive ambiguity.

### X22 locked baseline

X22 replaced adaptive-rest baseline behavior with a baseline locked for
the MM interval. Baseline age and rejection telemetry were added without
granting baseline age navigation authority. Later findings challenged
acceptance details, not the core locked-per-interval principle.

### NAVI_IR

NAVI_IR introduced the unpowered IR wheel as independent movement
evidence.

-   Hall/map supplies landmark evidence.
-   IR supplies relative wheel travel.
-   IR does not name MMs.
-   Ambiguity may persist rather than forcing an immediate guess or
    stop.

NAVI_IR 0.1 exposed a conceptual error: unavailable IR evidence was
treated like contradictory evidence and consumed the ambiguity budget.
NAVI_IR 0.2 corrected that distinction. **Unavailable evidence is not
contradictory evidence.**

### NAVI_COHERENCE 0.4/0.5

The first 0.4 lap began with an operator declaration one MM wrong. NAVI
remained one MM wrong for the lap even though accumulated polarity
evidence strongly contradicted the declaration. Decisional inertia
therefore cannot mean permanent deference to the initial declaration.

The second 0.4 run showed that an IR window could reject a Hall
observation that was physically a real magnet. This exposed the need to
distinguish an early Hall event from a genuinely missed expected
landmark.

The 0.5 AUTO run demonstrated that IR could reject a false Hall event
that timing alone would accept, while Hall/map navigation could continue
when IR was unavailable.

### Twenty Questions

The Twenty Questions exercise consolidated and refined the architecture
rather than creating twenty exception rules. It formalized decisional
inertia, possible-before-probable recovery, rolling physical history, IR
as instrument/cyclometer, health vs readiness vs odometry vs MM
synchronization, irreversible IR Epochs, exact early/in-window/missed
semantics, the provisional 70-count detector, provisional 650-ms
Hall-only fallback, wrong polarity as retained evidence, and NAVI-level
failure reasoning.

### IR health / Epoch

IR_ARCHITECTURE_0_4 implemented independent health/readiness,
irreversible Epochs, explicit unavailable versus zero distance,
NAVI-owned MM references bound to an Epoch, Hall-time-aligned IR
observations, and transport/freshness discontinuity handling.

NAVI_COHERENCE 0.6 integrated this observation-only before changing
navigation.

### PROXIMAL_R1

The whole-route matcher produced the decisive MM65→166 failure. A
mathematically unique polarity match was accepted at a physically
impossible location.

PROXIMAL_R1 replaced global recovery with incumbent inertia, a ±10-MM
outer boundary, physical filtering before polarity scoring, rolling
ten-position history with UNKNOWNs, actual-observation denominator,
unique strictly better alternative, ties retaining the incumbent, and
history retained after correction.

Later audit found this incomplete: physical gating still depends heavily
on continuous IR; startup alternatives can remain too broad; established
trajectory strength is not graduated; bounded local uncertainty is not
explicitly represented.

### NAVI interpretation of STOPPED

Raw IR may truthfully report `INADEQUATE_CONTRAST` at rest. NAVI can
simultaneously use commanded PWM zero, applied PWM zero, confirmed
coupling, fresh unchanged observations, no pulse advance and no Hall
advance to conclude **STOPPED / 0 mm/s**.

Field testing confirmed this and confirmed renewed measured speed after
restart.

### Current-position operating requirements

A pause crossed Patio's old approach trigger while AUTO was paused. On
restart, the station stop was skipped even though NAVI knew the train's
position.

Operator ruling:

> Each MM segment has its own requirements. What is required at MM15
> should not depend on a trigger at MM25.

POSITION_STATIONS_R1 is the first implementation. The deeper direction
is that location determines the current requirement; procedure does not
create location.

### Measured-speed station work

The next work began replacing PWM-based station deceleration with
IR-speed/remaining-distance control. This follows naturally from the
architecture: physical speed is a better statement of desired motion
than PWM, while PWM is the actuator.

This work began before several already-decided NAVI foundation items
were implemented. Preserve the measured-speed ZIP as future work rather
than using it as the baseline for unrelated unfinished NAVI changes.

## 3. Adopted principles

1.  **Sensors observe; NAVI knows.**
2.  **Detection, physical constraint, and judgment are separate.**
3.  **Morphology has no navigation authority.**
4.  **Hall navigation occurs at opening, not closure.**
5.  **X22 locked per-interval baseline is the retained baseline
    architecture.**
6.  **Decisional inertia:** one contradictory observation modifies
    evidence, not the world view.
7.  **Possible before probable:** physical reachability precedes pattern
    scoring.
8.  **±10 MM is an outer recovery boundary, not a candidate list.**
9.  **Rolling physical history:** approximately ten MM / \~3 m; missed =
    UNKNOWN; observed wrong polarity remains evidence.
10. **IR is a cyclometer, not an agent or navigator.**
11. **IR health, readiness, odometry, and MM synchronization are
    separate facts.**
12. **An ended IR Epoch never reopens.**
13. **Loss of measurement continuity does not erase NAVI's other train
    knowledge.**
14. **NAVI may interpret STOPPED from system context without falsifying
    raw IR diagnostics.**
15. **Current position determines local operating requirements.**
16. **Track testing is primary behavioral evidence.**

## 4. Rejected, superseded, or expired ideas

  ------------------------------------------------------------------------------------------------
  Idea                       Why it seemed     What challenged it           Disposition
                             reasonable                                     
  -------------------------- ----------------- ---------------------------- ----------------------
  Morphology/shape as        Rich waveform     Stitch/shape field failures  **REJECTED** ---
  navigation authority       seemed                                         diagnostic only
                             discriminating                                 

  Closure-dependent          Convenient        Stops on magnets; delayed    **REJECTED** ---
  navigation                 passage boundary  judgment                     opening is event

  Stitching Hall fragments   Tried to          Experimental behavior leaked **EXPIRED/REJECTED**
                             reconstruct       into control and caused      
                             passages          shutdowns                    

  Quiet Hall window proves   Electrical quiet  Flat magnetic shelves and    **REJECTED**
  spatial clearance          looked like       stop-on-magnet behavior      
                             leaving a magnet                               

  Adaptive/rest baseline as  Could follow      Reference captured magnetic  **SUPERSEDED** by
  operative reference        drift             conditions                   locked baseline

  Universal hard Vmax/timing Simple protection Different                    **REJECTED** as
  rule                                         intervals/directions/locos   universal hard rule
                                               and unreliable premises      

  Immediate strike/stop on   Conservative      Real imperfect observations; **REJECTED**
  wrong polarity             simplicity        continuity need              

  Missing IR = contradiction No distance       NAVI_IR 0.1 exhausted        **REJECTED**
                             confirmation      recovery with no usable IR   
                             looked suspicious                              

  IR ten-magnet trust        Tried to validate Wrong model: instrument      **SUPERSEDED**
  earning                    IR before use     health is not agent trust    

  IR naming MMs/location     Distance looked   Cyclometer distinction       **REJECTED**
                             position-like                                  

  Whole-route 171-MM         Unique pattern    MM65→166 impossible teleport **REJECTED**
  polarity recovery          corrected                                      
                             declarations                                   

  Global 10/10 + ≥2          Protected against Still allowed impossible     **SUPERSEDED**
  correction                 one polarity      distant match                
                             error                                          

  ±10 as twenty equal        Easy bounded      Outer bound is not evidence  **REJECTED
  candidates                 search            of reachability              interpretation**

  Clearing history after     Simplifies state  Throws away physical         **REJECTED as general
  miss/correction/reversal                     evidence                     policy**

  PWM as speed               Convenient proxy  Grade/load/loco/ramp         **REJECTED as
                                               differences                  measurement**

  Station stop as            Required by old   Patio pause/resume skipped   **SUPERSEDED
  trigger-dependent scenario choreography      known stop                   directionally**

  Matching follower PWM to   Easy traffic      Physical speed differs;      **NOT ACCEPTED as
  leader as speed matching   response          rear-end contacts; slow      collision proof**
                                               ramp-to-zero                 

  Host/synthetic success as  Exhaustive and    Railway repeatedly           **REJECTED as primary
  behavioral validation      convenient        contradicted dry assumptions behavioral evidence**
  ------------------------------------------------------------------------------------------------

## 5. Provisional current decisions

### 70-count, two-sample Hall opening

Absolute departure ≥70 counts for two consecutive 1-kHz samples; detect
on sample two. Field evidence showed sub-70 false events, including the
38-count false advance, while weakest real magnets were near 70--72.

**Status:** decided experimental value, not yet implemented in current
COHERENCE; current Toby configuration still inherits 38 counts.

### 650-ms detection-to-detection Hall-only fallback

Used when usable IR/MM-referenced distance is unavailable.

Evidence: false events clustered around 402 ms; closest genuine Toby
next-MM event was about 894 ms. A PWM-dependent 700→1000-ms rule added
complexity without demonstrated benefit.

**Status:** accepted provisional decision, not yet implemented; current
code remains 500 ms.

### ±15% cumulative mapped-distance window

With valid MM-referenced IR:

-   before window: reject event and retain expected MM;
-   inside window: Hall observation is eligible;
-   complete window crossed without acceptable observation: expected MM
    becomes missed/UNKNOWN.

**Status:** partially implemented. Current code can discover a miss when
a later Hall event arrives rather than when the expected landmark's
window is traversed.

Near-boundary field refusals justify investigation of alignment,
quantization, and map geometry; they do not automatically justify
widening the window.

## 6. Implemented but incomplete

### Proximal recovery / decisional inertia

Remaining recorded gaps:

-   established trajectory strength is not graduated;
-   startup alternatives can remain broad without travel evidence;
-   physical constraints across an IR gap do not fully use elapsed time,
    recent motion, and motor/braking context;
-   explicit bounded local uncertainty is not represented.

Do not solve these by restoring 171 equal hypotheses.

### Failure-mode reasoning

NAVI should interpret combinations of measurement and operating state.
Some operational limits remain undefined and should be established from
real evidence rather than synthetic possibilities.

### Position-based station requirements

The principle is adopted. POSITION_STATIONS_R1 is a first
implementation, not the final operating architecture. It retains phase
identity, watchdogs, timed PWM pacing, dwell/departure procedure, and
MM-index rather than continuous within-segment destination control.

## 7. Field events that changed the architecture

1.  Morphology/stitch shutdowns → experimental morphology cannot retain
    navigation/control authority.
2.  NAVI_SIMPLIFIED one-magnet/ten-observation stop → physical
    protection must remain active through ambiguity.
3.  Stationary IR behavior → distinguish instrument health/readiness
    from NAVI interpretation.
4.  0.4 wrong declaration held for a full lap → accumulated evidence
    must be able to revise the incumbent.
5.  0.4 real magnets refused by IR window → distinguish early event from
    missed expected landmark.
6.  0.5 Arches false advance at peak 38 → support for stronger Hall
    opening threshold and IR physical evidence.
7.  IR car left behind at Patio → NAVI can continue on other evidence;
    IR unavailability does not erase navigation.
8.  MM65→166 global correction → pattern probability cannot precede
    physical possibility.
9.  Repeated stop/restart operation → loss of IR Epoch continuity is not
    loss of NAVI trajectory.
10. STOPPED field check → raw IR can be unavailable while NAVI
    coherently knows the train is stopped.
11. Patio pause/resume skipped stop → current position, not a past
    trigger, determines local requirement.
12. CTO follower rear-ending leader → coordination code engaging does
    not prove physical separation; PWM is not speed and a zero target is
    not instantaneous braking.

## 8. Outstanding already-decided NAVI work

These must not be mistaken for open architectural questions:

1.  Implement 70-count/two-sample Hall detection in the current
    COHERENCE lineage.
2.  Implement the provisional 650-ms detection-to-detection Hall-only
    fallback.
3.  Implement exact early / in-window / missed ±15% landmark semantics.
4.  Continue field verification of PROXIMAL_R1 and
    wrong-polarity/reversal/correction paths.

The measured-speed station ZIP should remain preserved while these
foundations are reconciled.

## 9. Genuine open questions

-   How should stronger-established-history semantics modify the current
    strictly-better correction rule without inventing an arbitrary vote
    margin?
-   How should physical reachability remain constrained across an IR
    Epoch gap using elapsed time, recent motion, direction, and
    propulsion context without converting PWM into measured distance?
-   What is the minimal useful representation of bounded local
    uncertainty?
-   Which failure combinations require AUTO withdrawal, warning, or
    operator acknowledgment, and with what field-supported time/distance
    bounds?
-   In the eventual destination model, what constitutes confirmed
    arrival at a physical destination rather than merely reaching PWM
    zero?
-   How should multi-train traffic constraints use physical speed,
    remaining distance, train extent, and closing behavior without
    resurrecting block logic or assuming equal PWM means equal speed?

## 10. Useful older mechanisms that should not be discarded merely because their parent system failed

-   X22 locked baseline.
-   Hall task/event architecture.
-   RouteMap and surveyed spacing.
-   AUTO/manual ownership concepts and E-stop priority.
-   Station dwell and smooth ramp mechanics where still applicable.
-   Preserving an underlying mission/request while a temporary traffic
    constraint limits movement.
-   Physical train extent rather than point-only separation.
-   Hall interval timing as a separate speed observation when its map
    association is credible.
-   Existing telemetry and event identity/provenance mechanisms.

Reuse requires checking assumptions against the current architecture;
inheritance is not automatic authority.

## 11. Change discipline going forward

Any architectural change should state:

1.  **Existing principle or decision affected.**
2.  **Observed field evidence motivating the change.**
3.  **Whether the change refines, supersedes, or leaves the existing
    principle intact.**
4.  **What is observation versus interpretation.**
5.  **What deterministic host tests can prove.**
6.  **What track test is needed for behavioral acceptance.**

A synthetic possibility alone is not sufficient reason to add control
complexity.

The development loop is:

**DESIGN → INSTRUMENT → TRACK TEST → OBSERVE → EXPLAIN → REVISE**

Replay and host testing support that loop; they do not replace the
railway.

## 12. Immediate orientation for the next build

The current measured-speed station ZIP is valuable but should be treated
as preserved forward work.

The working baseline should remain the pre-ZIP
`NAVI_COHERENCE_0_6_IR_HEALTH` lineage containing the IR health/Epoch
work, stopped-state interpretation, and PROXIMAL_R1/current-position
developments as applicable.

Before further station-motion architecture is layered on, reconcile the
already-decided NAVI foundations --- particularly 70 counts, 650 ms, and
exact ±15% semantics --- while preserving field-proven mechanisms and
avoiding unrelated redesign.

------------------------------------------------------------------------

## Status vocabulary for future edits

-   **ADOPTED** --- current governing architecture.
-   **PROVISIONAL** --- current working decision explicitly subject to
    field evidence.
-   **IMPLEMENTED / NOT FIELD ACCEPTED** --- exists in code but lacks
    required railway evidence.
-   **DECIDED / NOT IMPLEMENTED** --- policy settled; implementation
    pending.
-   **EXPERIMENTAL / EXPIRED** --- belonged to a bounded experiment and
    has no standing authority.
-   **SUPERSEDED** --- once valid, replaced by a later model.
-   **REJECTED** --- evidence or architecture says not to use it.
-   **OPEN** --- genuinely undecided.

This vocabulary is intended to prevent "not implemented" from being
mistaken for "not decided."
