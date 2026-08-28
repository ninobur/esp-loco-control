# 0049 — Waveform shape does not identify a magnet on this railway

**Date:** 2026-08-28
**Status:** Accepted — negative result, measured
**Data:** Toby (9950012), `QUORUM_1_13W`, 2092 captures, PWM 90, consist attached, 99 °F

## Decision

The normalised Hall excursion **does not carry usable magnet identity** on this
railway. It is not adopted as an identification test. The capture firmware and
the survey are kept; the conclusion is recorded so it is not re-derived.

## What was asked

Not "which of 171 is this" — NAVI never asks that. The operator's framing was
decisive and correct: NAVI verifies **one expected target in context**, the way
you recognise a colleague on your own ward. So the test is: *expecting X, could
a reading of X±1 or X±2 pass as X?*

Each curve was normalised in time (resampled — speed removed) and in amplitude
(divided by its own peak — gain, temperature and air gap removed). What remains
is form: symmetry, shoulders, rise against fall.

## What was measured

| band k | genuine crossings refused (leave-one-out) | stops per lap | neighbour pairs shape rejects |
|---|---|---|---|
| 3 | 4.5% | 7.7 | 8% |
| 4 | 1.8% | 3.0 | 5% |
| 5 | 0.7% | 1.2 | 1% |
| 6 | 0.2% | 0.3 | 1% |

At any band wide enough not to stop the train, shape rejects **about 1%** of the
neighbour pairs that strength and duration already let through.

**The sampling explanation was tested and failed.** A template from three
curves is a poor estimate, and a poor template forces a wide band. So the survey
was doubled: CW went from 3 passes per marker to 6, and leave-one-out from
n=44 to n=1016. **The numbers did not move** — 4.5% at k=3 before and after.
Whatever limits this, it is not template noise.

## Why — and this is the part worth keeping

```
distance of any pass to the RAILWAY-WIDE average curve : 0.0346
distance of a pass to its OWN marker's template        : 0.0130
```

Shape **is** marker-specific: a marker's template explains 62% of the deviation
from the average curve. It is not noise, and the curves are highly repeatable
(within-marker spread 0.013 on a normalised scale).

The failure is subtler. What shapes the pulse — sensor height, track geometry,
local speed, ballast — **varies smoothly along the track**. Adjacent magnets sit
in nearly identical surroundings and therefore produce nearly identical curves.
Median distance to the nearest same-polarity neighbour is 0.019 against a
within-marker spread of 0.013: a ratio of 1.4, where a usable test needs
several. Shape is most alike exactly where NAVI needs it most different.

A fingerprint that distinguishes distant magnets and not adjacent ones solves a
problem the railway does not have.

## Consequences

- **No firmware depends on this.** The discipline held: the analysis ran before
  a line of identification firmware was written, which is what it was for.
- The **ten-magnet polarity word stands as the identification mechanism**. It
  guarantees detection of a wrong position within ten markers, and nothing here
  weakens it.
- The **per-direction tables** ([0048](0048-expectation-tables-are-per-direction-because-the-railway-has-grades.md))
  stand: 171/171 calibrated, and strength and duration remain the per-crossing
  screens they always were.
- Per-crossing certainty is **not available from this Hall sensor as the railway
  is currently built**. Saying so plainly is the point of this record.

## What would actually deliver it

Identity has to be *given* to the magnets; it cannot be extracted from magnets
that were installed to be identical. The operator's own principle — "make the
magnet's job easy so it can perform well" — points the way:

1. **Deliberately varied strength classes.** Strength is currently a continuum
   in which neighbours overlap. Three well-separated, deliberately-set levels
   would make it a *symbol* rather than a noisy scalar, adding ~1.6 bits per
   marker and shortening the confirming word.
2. **Deliberately varied spacing.** The leg-asymmetry test built today
   (`field-records/20260828_MM128_PROGRESSIVE_MAGNET_FAILURE.md`) measured a
   66 mm displacement from timing alone and matched the operator's physical
   observation. Spacing is already measurable to that precision; deliberately
   varying it would make it carry information.
3. **A second independent sensor**, which is what makes real balises work.

## Method note, recorded because it nearly cost a wrong decision

The in-sample false-refusal rate at k=3 was **0.0%** — the shape band looked
free. Leave-one-out on the same data gave **7.6%**, thirteen stops a lap. The
first number was three curves fitting themselves. Every figure in this record
is leave-one-out. In-sample validation of a per-marker template is not evidence.
