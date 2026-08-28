# 0048 — Expectation tables are per-direction, because the railway has grades

**Date:** 2026-08-28
**Status:** Accepted
**Supersedes in practice:** the single-table assumption in `strengthPct[]` /
`durationMs90[]` as shipped

## Decision

NAVI's expectation tables are **per direction of travel**. One table cannot
serve both CW and CCW.

## Evidence

Toby, 2026-08-28, `QUORUM_1_13`, manual PWM 90, consist attached, 99 °F. Two
runs back to back on the same afternoon, same locomotive, same consist, same
speed, same air:

| run | crossings | proven | markers calibrated |
|---|---|---|---|
| CW | 1041 | 1035 (99.4%) | 170/171 |
| CCW | 2055 | 2043 (99.4%) | 170/171 |

Both exclude only MM128, which is
[physically failing](../../field-records/20260828_MM128_PROGRESSIVE_MAGNET_FAILURE.md).

Comparing the two tables, the **means agree** — strength CW-CCW mean +0.0,
median +1.0. There is no offset, no gain drift, no systematic bias. But the
spread is large (sd 13.5 points) and, decisively, **it is not scattered**:

```
Duration, CW minus CCW, across MM060-MM085
  +5 +24 +48 +53 +59 +51 +34 +48 +58 +47 +44 +40 +40 +42
  +66 +73 +62 +68 +48 +50 +38 +43 +38 +30 +33 +16

  net sign: +26 of 26
```

Every marker in a 26-long contiguous stretch is slower CW than CCW. The other
duration blocks behave the same way and in the opposite sense: MM027-031 is
**-5 of 5**, MM129-140 is **-12 of 12**. A run of 26 identical signs is not
variation.

## Why

At a held PWM the duration of a passage is the magnet's length divided by the
locomotive's speed over it. Speed at constant power depends on **grade**. A
stretch climbed CW is descended CCW, so the same magnets are crossed slower one
way and faster the other — by up to **73 ms**, nearly 60% at MM075.

`durationMs90[]` was never a property of the magnet. It is a property of the
magnet *and the local speed*, and local speed is direction-dependent on any
railway that is not flat. The strength differences are smaller and cluster on
curves (MM100-102, MM168-001), consistent with lateral offset and lean changing
the air gap by direction.

**Neither is a defect.** Both are the railway being measured correctly for the
first time.

## What it would have cost

Carrying the CW table while running CCW, tested against each marker's own
calibrated band:

- strength test refuses **26** markers
- duration test refuses **43** markers
- **52 of 170 refused on one test or the other**

NAVI ramps to a stop on the first refusal. It would not have completed a lap.
The converse is equally bad: the CCW table used CW refuses 51.

A single pooled table is worse still — averaging a +59 ms uphill against a
-59 ms downhill produces a number describing neither, then applies a tolerance
band too narrow for both.

## Consequences

- Ship `strengthPct[]`, `durationMs90[]`, `tolStrPct[]`, `tolDurPct[]` **twice**,
  indexed by `navDir`. Flash cost is ~1 KB against 77% used; not a constraint.
- NAVI already knows its direction — `nextMm(navMm, navDir)` takes it. Selecting
  the table costs nothing structurally.
- **A calibration run must state its direction, and runs of opposite direction
  must never be pooled.** Every table built before today mixed them, and is
  withdrawn for that reason on top of any other.
- A locomotive reversing mid-session switches tables at the reversal. The first
  crossings after a reversal are the least-evidenced on the railway and should
  be watched.
- Not settled here: whether the two directions need different *tolerance* bands
  as well as different centres. They are computed per direction already, so this
  falls out for free, but it has not been tested as a claim.
- Open: MM128 remains uncalibrated in both directions and wants physical
  inspection before either table is considered complete.
