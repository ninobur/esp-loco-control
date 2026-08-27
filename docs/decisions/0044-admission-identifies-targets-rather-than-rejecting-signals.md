# 0044 — Magnet admission identifies expected targets rather than rejecting unqualified signals

Status: Accepted as provisional starting points (operator decision, 2026-08-27)

## Provisional status and revision triggers

Every value and choice in this record is a **starting point subject to
revision in the face of data**, not a settled position. Several were operator
answers to specific questions in a specific context and must not be
generalized beyond it. This section governs the rest of the record.

Named revision triggers:

- **The correction threshold will be determined empirically through live
  field testing.** The initial 67% value is an experimental starting point,
  not an approved safety boundary. Results from operation on the single-train
  loop will be used to adjust it. Falsification criterion for the starting
  value: *if it lets spurious signals through, it is too low.*
- **`durationAt()` proceeds on one locomotive, one day, tested against its
  own derivation session.** Cross-locomotive or multi-session disagreement
  invalidates it as a general table.
- **IR integration is interim** — a test car, an unmeasured link, an
  overcast-only optical result.
- **Pre-run calibration scope is unsettled** — whether it rewrites stored
  tables or only establishes session gain.

Nothing here is defended on the grounds that it was previously decided. Data
outranks this record.

## Decision

TEMPLATES' admission boundary changes from a universal signal-quality filter
to a per-marker target match. Instead of asking "is this signal strong enough
to be a magnet anywhere on the railway," admission asks "does this match the
specific marker we are already looking for."

The target profile is built from evidence the system already holds before the
magnet arrives: expected polarity (`dnaAt`), expected field strength
(`strengthAt`, scaled by current locomotive gain), expected PWM-normalized
duration (`durationAt`, new), expected arrival timing (`spacingMm` with
commanded PWM), accumulated sequence agreement, and IR corroboration when
available.

Four operator decisions settle the open questions from the design:

1. **The correction threshold will be determined empirically through live
   field testing**, not by a strict sequence-run match. The initial 67%
   value is an experimental starting point, not an approved safety boundary.
   Results from operation on the single-train loop will be used to adjust
   it. QUORUM's existing recovery machinery is the backstop that makes
   starting low acceptable to trial.
2. **`durationAt()` proceeds to field test** on a limited simulation rather
   than a dedicated measurement pass.
3. **IR is integrated now**, via ESP-NOW from the IR test car, as the
   necessary independent witness — not deferred until the car-side
   receiver-coexistence gate completes.
4. **A calibration run precedes automatic operations**, because magnet
   strengths are dynamic day to day.

## Context

Decision 0043 established that admission favors recoverable omission over
false inclusion. It did not specify *how* a doubtful response is identified,
and the implementation that followed used a single flat amplitude floor
(`ADMIT_MIN_PEAK` = 140 counts) applied identically to all 171 markers.

Field sessions on 2026-08-27 showed the cost. Genuinely weak but real magnets
at MM140–142 and MM69–71 were correctly refused by the floor, but each refusal
left the position counter behind physical reality, producing runs of up to 14
consecutive disagreements that resolved only at a lap wrap. The gate could not
distinguish "weak because this magnet is known to read at 0.69x median" from
"weak because this is not a magnet," because it consulted nothing about which
marker was next.

The information needed to make that distinction already existed in the
firmware and went unused at the admission layer: `strengthPct[]` predicted
MM140–142 as the weakest cluster on the railway before the session ran, and
`dt_expected` was already computed and published on every crossing.

A limited simulation over 2,038 field crossings found PWM-normalized duration
to be stable per marker (median coefficient of variation 0.047), corroborating
all 14 marginal rejects in the MM140–142 zone while separating cleanly from
genuine noise (ratios 0.09–0.14 against expectation).

## Alternatives considered

- **Lower the flat threshold.** Rejected: 140 sits in a density valley chosen
  because a single axis must carry the whole false-positive burden alone.
  Lowering it admits noise everywhere to rescue weak magnets in three places.
- **Slow the approach into weak zones.** Rejected on evidence: amplitude is
  speed-stable, and the near-identical peaks at PWM 90 and 118 show slowing
  would not lift the peak. It would also conceal weak infrastructure rather
  than diagnose it.
- **Strict sequence-run match for correction authority** (the map's
  provably-unambiguous length). Deferred rather than rejected: starting at
  the provable bound would never reveal where the practical boundary sits,
  and QUORUM's recovery machinery already exists to catch a wrong correction.
- **Defer IR until the car-side gate completes.** Rejected: IR is the only
  witness independent of the Hall/PWM chain, which is precisely what makes it
  able to break ties the other attributes cannot.
- **Full waveform morphology.** Out of scope: requires per-event sample
  retention, which the current 1.16R base lacks. Amplitude and duration
  jointly are adopted as the minimum viable morphology.

## Consequences

- The admission gate gains access to `navMm` and `navDir`, which were
  deliberately withheld to keep signal classification blind to navigation
  belief. This is the one architectural line the revision crosses, and it
  creates a new risk class: a wrong position hypothesis can produce a false
  match and entrench itself. The correction threshold and the preserved
  recovery machinery are the mitigations, and both are on trial. The
  threshold is empirical, to be set by single-train loop results.
- A scoring formula is now required — how six attributes combine into one
  percentage, and how absent attributes (missing IR, unusable position) are
  handled without counting against a candidate. This does not exist yet.
- `durationAt()` must be built. It is validated on one locomotive over one
  day, tested against the session it was derived from, and has not received
  the cross-locomotive validation `strengthPct[]` had.
- Automatic operation now depends on a preceding calibration run.
- RX 1.1 becomes a blocking prerequisite for IR runs: under RX 1.0 the
  transmitter cannot retire acknowledged records, and reached 22 of 192 free
  buffer entries in a single run. Eviction would convert a clean "vote
  unavailable" into silently lost evidence.
- Precision remains unmeasured. Every result supporting this decision
  addresses recall on Toby. Nothing has been tested against Otto's
  contaminated captures.

## References

- `docs/TEMPLATES_REVISION_3_TARGET_ACQUISITION.md` — full design
- `docs/decisions/0043-admission-favors-recoverable-omission-over-false-inclusion.md`
  — the doctrine this refines; not superseded
- `docs/TEMPLATES_REVISION_2_DESIGN.md` — prior revision
- Field sessions `9950012_20260827_114203`, `_123552`, `_134856`
- `firmware/test-programs/TEMPLATES/TEMPLATES.ino` — `strengthPct[]` (l.797),
  `strengthAt()` (l.808), `ADMIT_MIN_PEAK` (l.1051)
- CODEX IR evaluation, 2026-08-27 (overcast, upgraded TX)
