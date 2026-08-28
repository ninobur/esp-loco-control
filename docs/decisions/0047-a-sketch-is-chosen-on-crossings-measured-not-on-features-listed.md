# 0047 — A sketch is chosen on crossings measured, not on features listed

**Date:** 2026-08-28
**Status:** Accepted
**Locomotive:** Toby (9950012)

## Decision

Toby runs **QUORUM_1_13**, already flashed. The proposed move to 1.16Rb is
declined, and the reason is the sample size behind it, not the design.

## Context

Toby showed 34 disagreements across 1426 markers. 1.16Rb was recommended — by
this assistant — on the strength of what it contains: quarantine holds a
doubtful event and lets its successor judge it instead of promoting it into the
record. That is a real improvement in design over 1.13.

It was recommended before anyone measured it.

Attributing every crossing from 26-28 August to the sketch that produced it,
using the retained bootid timeline, and restricting to the regime Toby actually
operates in (PWM 85-95, `timing_gate` ACTIVE, pulling a consist):

| sketch | crossings | disagree | rate |
|---|---|---|---|
| QUORUM_1_12C | 1700 | 15 | 0.9% |
| QUORUM_1_13 | 864 | 19 | 2.2% |
| QUORUM_1_16R_IR_TEST_A | **60** | 5 | 8.3% |
| TEMPLATES_0_2 | 2209 | 153 | 6.9% |
| TEMPLATES_0_3B | 58 | 10 | 17.2% |

**1.16R has sixty crossings.** That is not evidence of anything. It cannot be
called better and it cannot be called worse.

Two further facts decided it:

1. The 34 disagreements were mostly **not QUORUM**. The TEMPLATES lineage
   supplies the bad rows. The QUORUM builds are the two best.
2. 1.13's 2.2% is dominated by one short session immediately after its flash —
   15 of its 19 disagreements. The long run that followed, 10:15-11:04 on
   2026-08-28, was **4 disagreements in 733 crossings: 0.5%**, and it is the
   best-evidenced result on the railway.

Reflashing would have replaced a measured 0.5% with an unmeasured unknown, and
cost a session to do it.

## The rule this sets

A sketch is chosen on crossings measured in the operating regime, not on the
features it lists. When the candidate has no sample, the honest report is "no
sample" — not a recommendation dressed in its changelog.

This is [0024](0024-a-request-counter-is-not-a-measurement.md) again in a new
place. There, a counter of send *requests* was read as proof of send *success*.
Here, a list of *intended* improvements was read as proof of *achieved*
reliability. Both substitute the description of a thing for its measurement.

## Consequences

- Toby stays on 1.13. No flash. Running may begin immediately.
- 1.16Rb is not rejected on merit and remains available. It needs crossings
  before it can be preferred — if it is ever flashed, it should be given a long
  cruise session before being judged.
- The comparison above is repeatable: bootid timeline joined to `mm/marker`,
  filtered to the cruise regime. It should be re-run before any future sketch
  change, and it costs nothing but the data already on the Pi.
- Not addressed here: disagreements cluster at the START of running — 13.0% in
  the first 25 cruise crossings, 5.8% through 100, 2.5% after. No sketch choice
  in this record affects that, and it is the larger effect. It wants its own
  investigation.
