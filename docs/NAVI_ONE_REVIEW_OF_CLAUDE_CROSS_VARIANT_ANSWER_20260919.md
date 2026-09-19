# NAVI_ONE — review of Claude's cross-variant answer

**Date:** 2026-09-19  
**Scope:** Review Claude's summary answer against the repository evidence.  
**Status:** Analysis only. No firmware was changed.

## Verdict

Claude's central diagnosis is strong:

> Most successive strikes were not regressions of the preceding fix. A fix
> removed one expression of a deeper fault, after which the fault appeared at
> the next authority boundary.

The single-conversion lineage is the clearest supported example. Entry-sign
polarity in 0.3 converted an impulse into F05/F06; summed polarity in 0.4
prevented that polarity error but allowed the same impulse to inflate the peak
and trigger F07's shape refusal; median-of-five ADC acquisition in X11 removed
the source population. The distinction between symptom-layer and source-layer
repairs is valid and important.

The answer is nevertheless too categorical in three places, contains one
definite decision-status error, and turns two promising design hypotheses into
conclusions stronger than the available evidence. Its direction is useful;
its strongest wording should not be adopted as a specification.

## Findings

### 1. Decision 0043 is Accepted, not proposed

Claude says decision 0043 is "proposed" and "not ratified," and therefore
leaves omission versus false inclusion for an operator ruling.

The repository says the opposite:

```text
# 0043 — Magnet admission favors recoverable omission over false inclusion
Status: Accepted (operator decision, 2026-08-24)
```

This is a factual error in both the answer and the underlying cross-variant
analysis. A later architecture may justify revisiting or superseding 0043, but
the existing operator decision cannot be treated as unsettled.

There is also a crucial architectural qualification in 0043 that the answer
omits: omission was chosen because downstream navigation was said to be able
to compensate for omitted valid markers. The NAVI_ONE single-counter behavior
did not actually provide that recovery. The conflict is therefore not simply
"field evidence disproved 0043." It is:

1. 0043 assumes recoverable omissions;
2. NAVI_ONE converted omissions into persistent lag and delayed strikes;
3. therefore NAVI_ONE violated the recovery premise that made 0043 safe.

The correct design question is whether to restore bounded omission recovery,
change the admission policy, or both.

### 2. "Every refusal rule caught nothing measurable" is false

The shape evidence is accurately summarized but rhetorically overstated. X13
shows that, after complete flanks were restored, the old shape ceiling would
have refused 0 of 3,284 accepted passages. It does not prove that shape could
never reject an artifact; it proves that the rule had no demonstrated benefit
in that run and had previously rejected real passages at fatal cost.

The amplitude statement is weaker still. Zero of 97 X21 records below the
floor establishes that the floor was inactive in that run. It does not measure
how many false events the floor rejected before a navigable record existed.

Most importantly, the 82 ms duration floor demonstrably caught false events.
`NAVI_BAMBOO_TRANSIENT_ANALYSIS_20260912.md` records that it:

- rejects the 47 ms Bamboo false event that caused withdrawal;
- rejects the measured 40–58 ms phantom band;
- retains all 195 Toby primary passages;
- retains the shortest known genuine Otto passage at exactly 82 ms, with zero
  safety margin.

It is true that the floor rejected nothing at either X16 failure. That proves
that it did not address the X16 mechanism, not that it "caught nothing
measurable."

The spacing guard also has measured value: X13 reports 19 `TOO_SOON`
refusals, all re-reads of the magnet just passed.

A defensible synthesis is:

> Refusal rules can reject real artifacts, but in an irreversible
> single-counter navigator every false rejection is potentially fatal. A
> screen needs both measured selectivity and a downstream state model that can
> survive its mistakes.

That is materially different from eliminating all physical admission rules.

### 3. The reference argument is right in kind but overcounts field failures

The key observability claim is sound: a time-only quietness rule cannot
distinguish a stable ordinary track level from a stable magnetic shelf. Both
can be quiet longer than any selected adaptation constant. Changing the time
constant therefore cannot solve that classification problem.

But "five reference architectures, one failure" is stronger than the table
supports:

- the ungated rolling median was observed capturing a stopped-on magnet;
- the PWM-gated reference worked at the reported 0.7 dwell and exposed a
  separate shape refusal rather than itself failing there;
- `openMigrateMs` and adaptive rest have observed shelf/latch failures;
- fixed-after-prime X17 was predicted by preflight audit to latch, not flown to
  an observed field failure in the cited lineage.

The combined evidence strongly rejects continuous time-based adaptation as a
complete solution. It does not establish that every listed reference design
failed in the field in the same way.

The locked-per-interval alternative is promising, but it retains the hard
problem: a flat fringe field can satisfy a low-spread acceptance gate. X22's
own implementation report labels this a known open condition. Locking prevents
migration during an interval; it does not prove that the value being locked is
ordinary track.

### 4. Opening-versus-window disagreement is a valuable detector, not yet a
general solution

Claude correctly joins MM136 and MM117:

- at MM136 the opening sign was right and the later window verdict was wrong;
- at MM117 the opening event was false while the later window contained the
  real magnet.

The disagreement flag selected the known failure once in 97 X21 records and
twice in 266 X20 records, with MM136 the first X20 case. This is excellent
evidence that disagreement should create ambiguity rather than being ignored.

It is not enough to call the signal "near-perfect" in a general sense:

- the population contains only these two failure mechanisms and few positive
  cases;
- the second X20 disagreement is not characterized as a strike in the cited
  summary;
- failures where opening and window agree on the same wrong interpretation
  will not be detected;
- the test was recognized retrospectively from the same runs used to assess
  it, so it needs prospective validation.

There is also an implementation tension in "advance on the opening" and
"raise ambiguity when the later verdict contradicts it." An advance must be
provisional, transactional, or represented as competing hypotheses. Otherwise
the later ambiguity arrives after irreversible navigation effects have
already occurred.

### 5. Independent distance evidence is promising; Hall cadence is not an
independent second channel

The post-hoc timing arithmetic is persuasive evidence that motion plausibility
can reject events such as MM117. But a speed inferred from intervals between
the same magnet events under judgment is partly circular: an inserted or
omitted event changes the inferred interval.

The claimed 326 mm/s versus a 240–263 mm/s run is also a 25 percent anomaly,
not an impossibility by itself. Real-time rejection needs measured envelopes
by locomotive, direction, interval, PWM, acceleration state, and uncertainty,
as Claude notes later in the same paragraph.

An independent distance or motion sensor would genuinely add another physical
channel. It should have asymmetric authority: it may veto an implausible
advance or require a safe stop, but should not invent magnet identity or route
position. Until such a sensor exists, cadence is useful corroborating evidence
but not independent truth.

### 6. The stationary invariant is correct, but must begin before dwell

The answer is right that a stationary locomotive must not advance position.
X19's three Arches advances while standing make that non-negotiable.

It is also right that PWM zero is too late as the protection boundary. The
Grillers reference migration occurred during the twelve-second deceleration
ramp. Protection must cover the station approach/ramp state and must not rely
on a reference that can already have been contaminated before the gate arms.

"Structurally incapable" should mean that stationary or station-zone Hall
observations can be retained as evidence but cannot commit route position
without subsequent motion-consistent confirmation.

## Assessment of the proposed combined model

Claude's proposed direction is substantially correct:

- preserve credible physical observations rather than collapsing them into a
  binary accepted/refused location update;
- make position a recoverable hypothesis rather than an irreversible counter;
- lock the reference during each detection interval;
- retain opening and later-window evidence and expose disagreement;
- prevent station/ramp observations from directly advancing position;
- add independent motion or distance evidence when available.

Two amendments are required.

First, "permissive detection with no refusal authority" should be replaced by
layered authority:

1. acquisition may suppress demonstrated electrical conversions and
   same-magnet re-reads while preserving diagnostics;
2. the observation layer records credible passages, including ambiguous ones;
3. the estimator decides which route hypotheses those observations support;
4. the safety controller stops when plausible hypotheses disagree about a
   required action.

Second, position recovery is not optional. It is the missing premise behind
both Claude's recommendation and accepted decision 0043. Without bounded
insertion, omission, duplicate, and ambiguous-polarity hypotheses, changing
the detector merely chooses which single observation will cause the next
irreversible error.

## Bottom line

Claude identified the dominant architectural lesson: successive code changes
often moved a fault between authority boundaries, while source-layer fixes
closed fault families. The answer is especially good on the ADC lineage, the
reference observability problem, MM136/MM117 duality, and the need to prevent
stationary advances.

The corrected conclusion is narrower and stronger:

> NAVI_ONE should not remove all filtering. It should stop allowing any one
> imperfect filter or polarity vote to make an irreversible position decision.
> Admission rules require measured selectivity; observations require retained
> uncertainty; and navigation must be able to recover from the omission or
> insertion errors that every practical detector will eventually make.

## Evidence reviewed

- `docs/NAVI_ONE_FIX_TO_STRIKE_CROSS_VARIANT_ANALYSIS_20260919.md`
- `docs/decisions/0043-admission-favors-recoverable-omission-over-false-inclusion.md`
- `docs/NAVI_BAMBOO_TRANSIENT_ANALYSIS_20260912.md`
- `docs/NAVI_ONE_X13_FIELD_VERDICT_20260904.md`
- `docs/HALL_OPENING_ANALYSIS_20260915.md`
- `docs/X22_LOCKED_BASELINE_IMPLEMENTATION_20260919.md`
- `docs/NAVI_SIMPLIFIED_LOCKED_BASELINE_FINDINGS_20260919.md`
- `docs/NAVI_ONE_X18-X21_RAW_AUDIT_20260916.md`
