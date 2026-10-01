# NAVI proposal — identify ordinary track directly from Hall data

Date: 2026-10-01
Status: **Architectural proposal for the next NAVI build. Not a decision.**
Proposed by: David and Sam.
Recorded by: Claude, at David's request.

This document authorizes no firmware change, flashing, or deployment
(`/AGENTS.md` section 2). It does not amend any decision record. If adopted, it
would reopen the part of [0113](decisions/0113-the-navi-ewo-hall-reference-is-spatial-and-is-replaced-from-the-leading-field-boundary.md)
that fixes the 0–100 mm / 100–200 mm geometry, and the part of 0113's
resolution of 0107 D4 that declined to recognize RTB from the waveform. That
reopening would itself need a new decision record.

Part A is the proposal as written by David and Sam, unchanged. Part B is
Claude's review comments; they are recommendations and open questions, not
established decisions.

---

## Part A — The proposal (David and Sam)

David and Sam want the next NAVI build to reconsider how NAVI determines that
a magnetic encounter has ended and where a valid Hall reference should be
measured.

This is an architecture/design task first. Do not implement until the
proposed recognition rule has been reviewed.

### Problem with the current method

Current EWO uses IR distance after a confirmed magnet to determine where
Hall-reference collection occurs:

```text
confirmed magnetic boundary → 0–100 mm clearance → collect Hall reference over
100–200 mm → replace reference at 200 mm.
```

That works by using distance traveled as a proxy for the physical condition
NAVI actually cares about:

> Has the locomotive left the magnetic field and returned to ordinary track?

Recent IR testing has reinforced the weakness of depending on a proxy when the
underlying phenomenon is directly observable by the Hall sensor.

### Proposed principle

NAVI should investigate determining the end of a magnetic encounter directly
from Hall observations.

Conceptually:

```text
magnetic departure
→ magnetic encounter
→ Hall observations cease exhibiting the magnetic field
→ sustained ordinary-track Hall behavior
→ magnetic encounter is over
→ ordinary-track observations become eligible for reference measurement.
```

The Hall data should tell NAVI when the magnetic field has ended rather than IR
distance telling NAVI when it ought to have ended.

### Important authority boundary

This proposed ordinary-track/return recognition has **no location authority**.

It does not:

- identify an MM;
- advance position;
- select among alternative MMs;
- correct sequence;
- determine polarity;
- declare a Missed Magnet.

Its statement is only:

> The preceding magnetic encounter has ended, and the present Hall
> observations are representative of ordinary track.

Target identity remains a separate NAVI judgment.

### Reference measurement

Once NAVI has directly established an ordinary-track region, it can measure
the Hall reference from that region.

The desired principle is:

> Measure baseline because the Hall data demonstrate ordinary track—not
> because another sensor estimates that the locomotive has traveled an
> arbitrary number of millimeters.

Do not simply substitute another fixed timer, sample count, or arbitrary
distance for the existing 100-mm rule.

The reference population should remain NAVI-owned and spatial/observation
representative rather than being weighted by how long the locomotive happens
to remain at one ADC value.

### Potential second use: physical re-arm

Investigate whether the same observation can provide a non-time-based guard
against treating one sustained magnetic field as multiple magnets.

After NAVI accepts a magnetic encounter:

> Do not permit another magnetic encounter until Hall has demonstrated that
> the previous field ended and ordinary track was traversed.

This would replace the conceptual weakness exposed by the former 650-ms guard:

```text
same sustained field → wait 650 ms → falsely accept another magnet.
```

Instead:

```text
magnetic encounter → locked against another encounter → demonstrated ordinary
track → re-armed → subsequent new departure may be considered.
```

Again, ordinary-track recognition itself has no MM authority.

### Central design question

Before coding, answer:

> What evidence in the native Hall stream is sufficient for NAVI to conclude
> that a magnetic field has ended and that it is observing ordinary track,
> without circularly depending upon a baseline that NAVI is currently trying
> to establish or replace?

Examine actual field Hall data surrounding known magnet passages.

Look specifically at:

- departure from the existing reference;
- field peak/plateau;
- return trajectory;
- stability after the field;
- behavior on both polarities;
- variation between magnets;
- acceleration/deceleration;
- stops within or near magnetic fields;
- reversal within an encounter;
- ordinary track variation;
- whether a simple direct rule emerges from the observations.

Prefer the simplest observable physical distinction that the data support.

### Relationship to IR

Do not remove IR or redesign current EWO merely because this proposal exists.

The immediate objective is to determine whether Hall can directly recognize:

```text
magnetic field ended → ordinary track present
```

better than the current IR-distance proxy.

If successful, this could strengthen EWO even when IR is healthy and also
provide one necessary building block should Hall ever need greater independent
navigation capability.

### Deliverable

Before modifying firmware, provide David and Sam:

1. Field evidence showing Hall behavior through complete magnetic encounters.
2. A proposed definition of ordinary track / encounter ended.
3. False-positive and false-negative risks.
4. Behavior during stops and reversals.
5. How the observation would trigger reference collection.
6. How it could provide a non-time-based re-arm without acquiring navigation
   authority.
7. Comparison with the current 0–100/100–200-mm IR-distance method.
8. Any case in which the Hall data cannot reliably make this distinction.

Stop for David and Sam's review before implementation.

---

## Part B — Review comments (Claude, 2026-10-01)

These are comments on the proposal, made before any new data analysis. Nothing
here is a field finding; where a point rests on an existing record, the record
is cited.

### B1. The proposal is consistent with the record, and reopens one thing

- It is a direct application of
  [0103](decisions/0103-direct-physical-measurement-before-proxy.md) (direct
  measurement before proxy). 0113 itself records that the 100 mm clearance is
  "an agreed geometry, not a measured one" and that the test evidence "does
  not set" the distances.
- It revives [0107](decisions/0107-hall-baseline-is-spatial-rtb-is-recognized-from-the-phenomenon.md)'s
  RTB methodology ("observe the real waveform → identify RTB from the
  phenomenon → …"), which 0113 declined for EWO on the ground that waveform
  closure has no independent authority (0110). The proposal's explicit
  no-location-authority boundary appears to answer that objection: 0110 is
  about navigation authority, and this recognizer would hold none. David and
  Sam should confirm that reading, because it is the hinge on which adopting
  the proposal is consistent with 0110 rather than a reversal of it.
- The 650-ms description is accurate per
  [0116](decisions/0116-ewo-ir-distance-is-authoritative-and-hall-only-navigation-is-withdrawn.md):
  the fallback "confirmed MM13, MM12 and MM11 from one sustained Hall field
  without IR pulse progression." 0116 already withdrew that fallback, so the
  re-arm proposal is not fixing a live defect in the current candidate. Its
  value is structural: it would make "one field counts once" a property of the
  Hall observation rather than an accident of IR being required.

### B2. The circularity question has a likely non-circular answer — and a trap

Two kinds of Hall evidence are available without the reference being
replaced:

1. **Shape-only evidence** (slope and spread per unit of travel, absence of
   trend). This needs no reference at all.
2. **The locally observed pre-departure level** — the ordinary-track level
   NAVI actually saw immediately before this encounter's departure. This is
   not the reference being replaced; it is a separate observation from the
   same trip, typically a few hundred millimetres earlier.

Shape-only evidence is probably **not sufficient on its own**. The Otto record
describes the magnet tail as "a 15–18-count return-flux shelf, not a decay"
(0107 context, from `NAVI_BASELINE_TIMING_20260916_C3B93D0B.md`). A shelf is
flat. A rule that says "flat and quiet = ordinary track" would accept the
shelf and collect a reference biased by 15–18 counts — the exact error the
100 mm clearance was meant to avoid.

The hypothesis I would test first is therefore: **encounter ended = Hall has
returned to within ordinary-track spread of the pre-departure level and is
flat at that level, sustained over travel.** Whether that is true, and what
"within spread" and "sustained" must mean, is for the data to decide, not
this comment.

### B3. "Sustained" has to be measured in travel, which keeps IR in the loop

The proposal asks the Hall data, not IR distance, to decide when the field has
ended. But a stationary locomotive produces a perfectly flat, perfectly quiet
Hall trace at whatever level it stopped at. The 2026-09-27 spatial-reference
capture contains the clearest example: a 22,399-sample stationary interval
between MM109 and MM110 sat at about **2003 counts against an ordinary-track
level of about 1825** (`field-records/analysis/20260927_navi_eyes_wide_open_spatial_reference_report.md`).
That is a stop in or near a field, and its trace satisfies any shape-only
"stable" test.

So "sustained ordinary-track behavior" must mean sustained **over physical
travel**, and the reference population must stay spatial — both of which the
proposal already requires. Both need a distance measurement, which today is
IR. The accurate framing is therefore:

- Hall decides **whether** the field has ended (replacing the 100 mm guess);
- IR still decides **how much** ordinary track has been observed and supplies
  spatial weighting for the reference population.

That is still a real improvement over 0113, but it is not Hall independence.
Under PWM = 0, Hall is already non-actionable (0112), so an at-rest stretch
cannot count as "ordinary track traversed" in any case. If the longer-term
goal of Hall-only capability matters, a Hall-intrinsic motion signal would be
a separate research question; I would not assume one exists.

### B4. The re-arm lock trades false positives for false negatives

Locking out a second encounter until ordinary track is demonstrated removes
the "one field counted many times" failure. It introduces the opposite: if
two real magnets are close enough that no qualifying ordinary track appears
between them (for example, overlapping return-flux lobes — the 2026-09-16
record already notes "a leading lobe precedes the next magnet"), the second
magnet would be suppressed. Under EWO that would surface as a Missed Magnet on
IR distance. Whether any NAVI-mapped magnet pair is that close is a map
question that can be answered before any firmware work.

There is also a stuck-locked case: if the recognizer never sees ordinary track
(sensor drift, a misjudged pre-departure level, a stop inside the field
followed by reversal back out), NAVI would stay locked. The proposal should
say what releases the lock in that case, and the answer must not be a timer.

### B5. Reversal inside an encounter

A reversal inside a field means the locomotive leaves through the same side
it entered. The Hall trace on exit will be the mirror of the entry, and the
pre-departure level is the correct comparison (same physical stretch of
track). That case looks tractable. The harder case is a reversal on the
shelf: the trace may look like a short return followed by a second excursion
— superficially a second magnet. With the re-arm lock it is correctly
suppressed only if the shelf is not mistaken for ordinary track (B2).

### B6. Available evidence, and what is missing

- `field-records/analysis/rtb_evidence_20260926/` already holds 14 native-rate
  Otto and Toby passages across both polarities and low/mid/high PWM, plus
  three "difficult" cases. It is a good start for shape and polarity, but it
  is **time-domain only** (±2000 samples around an anchor, no IR distance):
  Otto's capture predates IR, and Toby's is a QUORUM trace.
- The synchronized Hall+IR capture that would answer the distance-domain
  questions — `/home/david/NGR/navi_sync/navi_sync_20260927_214310.nsr` (Toby,
  622,320 samples, 6,207 IR snapshots) — is on the Pi and **not in the
  repository**. Only its derived JSON is.
- No synchronized Otto Hall+IR capture from the EWO runs of 2026-09-29/30 is
  in the repository that I found.
- On 2026-09-30 Otto's IR housing was dislodged (branch
  `claude/wonderful-pascal-55tbkf`). Any capture from that run after the
  failure is not usable distance-domain evidence.

To produce Deliverables 1–8 properly, the `.nsr` capture (via Git LFS, as the
RTB pack did) and preferably one Otto synchronized lap are needed. Producing
the analysis is a substantial piece of work; under `/AGENTS.md` section 4 it
should be explicitly requested before it is started.

### B7. A cheaper first test

Before building a general recognizer, one bounded comparison would show
whether the problem it solves is real on NGR: for every confirmed passage in
the `.nsr` capture, compute the median Hall level over 0–100 mm and over
100–200 mm after the leading boundary, and compare both with the
pre-departure level. If the 100–200 mm window already sits within
ordinary-track spread everywhere, the current proxy is adequate in practice
and the proposal's value is mainly the re-arm. If some passages are still on
the shelf at 100 mm, that is direct evidence for the proposal and identifies
which magnets matter.

### B8. Bookkeeping

This proposal is not yet listed in `NAVI_EWO_GOVERNING_DOCUMENTS.md`. Proposals
are not canonical authority, so that is deliberate; if David wants it indexed
under a "proposals under review" heading, that is a separate change.

The repository's EWO state is split: `main` does not contain the EWO decision
set (0109–0116), which lives on `codex/ewo-ir-authoritative`, while the
2026-09-30 run verdict is on `claude/wonderful-pascal-55tbkf`, branched from
`main`. This proposal is on a task branch from `codex/ewo-ir-authoritative`
because that is where the documents it refers to live. No reconciliation was
attempted.
