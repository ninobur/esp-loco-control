# NAVI_TRACER_0_1 architectural continuity audit — 22 October 2026

**Status:** Research and documentation only.  This is not an approval for a
firmware change, a threshold change, or a new recognition model.

## Question

Why did `NAVI_TRACER_0_1` adopt a frozen local-linear reference and timed
baseline/return rules after earlier work had identified the risk that a frozen
reference could prevent return-to-baseline recognition?

## Short answer

The implementation was authorized as a narrow, faithful port of the
PWM-90 target-acquisition research at commit `6efc9522`, not as a synthesis
of the CPR or NAVI_EYES_WIDE_OPEN research.  Its direct-port status is real:
the C++ core preserves the research state machine, thresholds, frozen
pre-candidate reference, and lack of timeout.

That provenance explains the implementation; it does not resolve the
architectural conflict.  The port carried a mechanism validated on Otto at
PWM 90 into a speed regime that the governing research explicitly said not to
advance into without further review.

## Implementation and authorization trail

| Item | What it authorized or found | Relevance |
|---|---|---|
| `98db802d` — normal-curve research | Local linear baseline, 20 ms bins, 110 ms evidence gate, and six-bin return persistence in an offline recognizer. | Source of the line-versus-curve mechanics later reused. |
| `a1f836aa` / `6efc9522` — target-acquisition research | `CLOSE → TARGET → BASELINE → ACQUIRE → CLOSE AND ADVANCE`; frozen pre-candidate reference; 70 ADC/two-bin opening; no automatic timeout. | Direct algorithmic source for TRACER.  Validated against the restricted PWM-90 Otto research set. |
| Decision 0023, commit `fb8c3840` | Authorized a direct C++ port for an observation-only Otto field trial; selected the 20 ms duration-floor configuration; omitted ΔBIC because it was diagnostic rather than a gate. | Authorizes the port, not a general redesign and not a claim of PWM-60 validation. |
| `fca9e5b8` — CPR research | Found frozen local-linear baseline handling unable to rearm after the Toby background shift; clipped EMA performed better on those recordings but was not field-ready. | Prior negative evidence directly relevant to the present failure family. |
| `7cc9e602` — NAVI_EYES_WIDE_OPEN | Separated observation from judgment; a spatial reference was measurement-only and explicitly had no elapsed-time navigation gate. | Records the intended direction: ordinary behavior must be observed, not inferred from elapsed time. |

Decision 0023 is the documented authorization for the implementation.  The
source comments in `NaviTracerCore.h` accurately describe a direct port, but
comments alone are not treated here as architectural approval.

## The 650 ms requirement: origin and scope

`NaviTracerCore.h` declares:

```cpp
static constexpr double MIN_BASELINE_SPAN_S = 0.65;
static constexpr int MIN_BASELINE_BINS = 8;
```

The immediate source is `target_acquisition.py` at `6efc9522`.  That research
said the value was borrowed from Decision 0093 and noted that it was less than
the 0.72 s preceding-baseline minimum measured in its PWM-90 corpus.

Decision 0093 itself (`f8bcaeb1`,
`docs/decisions/0093-provisional-navi-coherence-hall-only-guard-650-ms.md`)
has a materially narrower scope:

- it is for **NAVI_COHERENCE only**;
- it is a provisional **open-to-open marker guard**;
- it applies when IR movement evidence is unavailable;
- it explicitly states it was not yet implemented in that lineage.

It is not an authorization for a baseline-estimation warmup, a claim that
650 ms establishes ordinary Hall behavior, or a portable rule for Otto at all
speeds.  The reuse in target-acquisition research was a research choice; the
later direct port preserved it.

This distinction matters because a duration can establish only that samples
were observed for a duration.  It cannot establish that they were ordinary
background.  The code's baseline-collection path has no Hall-derived
ordinary-track test before fitting the line; see
`firmware/test-programs/NAVI_TRACER_0_1/NaviTracerCore.h:210-259`.

## Earlier architectural findings

### Decision 0107: RTB is a phenomenon, not a timer

Decision 0107 (`f903a625`, 27 September 2026) states that baseline is spatial
information and defines RTB as the Hall signal returning to ordinary-track
behavior.  It lists agreement with known baseline information, ordinary-track
spread/noise, disappearance of systematic trend, persistence, and subsequent
waveform structure as possible evidence.  It expressly leaves the method for
recognizing RTB open and says not to start from a predetermined time or
locomotive-specific answer.

The current core instead makes closure contingent on six bins near the frozen
opening reference.  That is an implementation of persistence, but it is not
an independent recognition of ordinary behavior.  The two ideas coincide only
when the frozen reference still represents ordinary background.

### CPR: frozen local-linear baseline failure on Toby

The CPR report at `fca9e5b8`,
`research/NAVI_CPR_0_1/REPORT.md`, records Toby's background moving from about
1915 to 1860 ADC near 56 s.  The frozen local-linear reference remained tied
to the earlier level, reached its horizon, and stayed unresolved.  The report
states that it then missed later genuine-looking excursions.

CPR's comparison found clipped EMA better than that frozen local-linear
implementation on the evaluated recordings.  It did **not** authorize EMA as
an operational solution: it documents that EMA can absorb slow weak signal,
cannot resolve a persistent changed background from an unfinished plateau, and
has an unverified startup baseline.  CPR is evidence against assuming that
either “freeze” or “continuously update” alone solves the problem.

### NAVI_EYES_WIDE_OPEN: observation before judgment

`NAVI_EYES_WIDE_OPEN`, commit `7cc9e602`, was an unflashed clean-slate
observation build.  Its `SpatialHallReference` represents samples by measured
distance bins and returns a robust median candidate.  It is explicitly
measurement-only: it has no timer, refractory period, cadence gate, or
navigation authority.

That code was not adopted by TRACER and is not presented as a drop-in answer.
Its relevance is conceptual: the project had already separated “collect a
reference” from “declare that an encounter closed.”

## Line-versus-curve and ΔBIC history

The normal-curve research at `98db802d` used a local linear baseline and a
line-versus-Gaussian ΔBIC diagnostic.  The target-acquisition research retained
the diagnostic but explicitly treated it as supportive waveform evidence, not
an independent acceptance gate.

Decision 0023 then authorized omitting ΔBIC from `NAVI_TRACER_0_1` because it
was diagnostic only in the research.  The C++ port therefore retains no
candidate sample history beyond the six-sample closure tail.  This omission is
documented and authorized; it did not itself create the PWM-60 failure.

Its practical consequence is narrower: the fielded core has less retained
waveform structure available to distinguish a genuine return to ordinary track
from an accidental numerical crossing.  That is an observation about
instrumentation and model scope, not a request to restore ΔBIC.

## Time-based mechanisms in the fielded recognizer

| Mechanism | Classification | Evidence status |
|---|---|---|
| 20 ms raw-ADC averaging and 50-read minimum | Measurement integrity | Represents the physical input; missing blocks are censored rather than invented. |
| Two consecutive 70-ADC same-sign bins | Recognition gate | A persistence requirement based on two observed departures.  It depends on a valid baseline. |
| Eight samples spanning 650 ms before baseline fit | Recognition gate | Time is being used as a substitute for ordinary-track evidence.  The MM044 replay shows the resulting failure. |
| 20 ms default acquisition floor | Recognition gate, but redundant | Equal to the two-bin opening condition; it adds no further persistence in the enabled build. |
| Legacy 110 ms acquisition floor | Recognition gate | The PWM-90 research found it rejected genuine encounters; it is not the default. |
| Six 20 ms baseline-consistent bins for closure | Recognition gate | Persistence is physically relevant, but validity depends on the reference actually describing ordinary track. |
| PWM equals zero | Context gate | Decision 0106: a non-navigation interval; not a PWM-60 timing gate. |
| Motor ramp, sampling, Wi-Fi/MQTT, recorder, and reporting clocks | Infrastructure | Do not classify Hall encounters and have no input path into the reconstructed failure. |

## Architectural finding

The October 22 failure is continuous with the earlier Toby/CPR failure in one
specific sense: a reference that no longer represents ordinary background
prevents a candidate from completing, and downstream navigation cannot
progress.

The immediate causes are not identical.  CPR/Toby involved a persistent
background displacement; Otto/MM044 demonstrates a newly fitted local-linear
reference contaminated by the slow approach of the next field.  Both expose
the same unresolved architectural question: how does NAVI establish, protect,
and recognize ordinary Hall behavior without either absorbing a field or
remaining tied forever to an obsolete reference?

## Unresolved questions

- Whether the MM31, MM152, and MM063 stalls reproduce the same local fit
  contamination demonstrated for MM044.
- What observable combination of Hall level, trend, variability, and later
  structure is sufficient to classify a region as ordinary track.
- How an ordinary-track reference should persist across real baseline shifts
  without absorbing a developing magnetic field.
- Whether a general model can be validated across Otto and Toby before any
  locomotive-specific accommodation is justified.

These questions remain open.  This audit makes no firmware proposal.
