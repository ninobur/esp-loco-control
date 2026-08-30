# A clean lap: 172 detections, 172 matches, zero errors

**Date:** 2026-08-29
**Locomotive:** Toby (9950012), `NAVI_2_0`
**Run:** 18:02:19 to 18:05:48, manual, no position declared
**Evidence:** `field-records/logs/20260829_navi2_first_lap/`

## What happened

Toby ran a full circuit under manual control with `nav_state = UNSET`. The
navigator therefore did nothing: all 172 marker detections published
`RECEIVED_NO_POSITION` and none moved position. That is rule 1 working — but it
also means the detection layer ran a whole lap completely unjudged, which makes
the record a clean test of detection alone.

## The result

The observed polarity sequence was matched against `NGR_DNA1` over every
rotation and both directions:

> **172 / 172 detections matched, starting at MM41 travelling CW.**

172 detections from MM41 returns to MM41 — exactly one circuit of the
171-marker route.

The match is self-proving. A missed magnet shifts every subsequent reading by
one and destroys the alignment; a false detection does the same in the other
direction. Only an exact one-to-one correspondence survives, and the run
retroactively establishes the starting position without anyone having declared
it.

## What the detector was running

The 2026-08-28 ruling, in the field for the first time:

- entry threshold **38 counts** (boot: `baseline=1834 Nent=1872 Sent=1796`)
- **no** amplitude floor above it (the 60-count magnet test deleted)
- **no** duration floor above the 40 ms electrical screen
- no position gate, no conservation gate, no quarantine

## Measurements from the lap

| | |
|---|---|
| detections | 172 |
| peak, min / median / max | 146 / 205 / 323 counts |
| peaks below 140 counts | **0 of 172** |
| interval, min / median / max | 898 / 1109 / 5956 ms |

The 5956 ms interval is a stop. The minimum of 898 ms is the number that
matters for the debounce.

## What NAVI 2.0 would have done

Both of the navigator's tests are checkable against this data directly:

| test | evidence | outcome |
|---|---|---|
| polarity vs `nextMm(navMm, navDir)` | 172/172 matched | pass, every event |
| ≥ 500 ms of moving time since last accept | minimum interval 898 ms | pass, every event |

Declared at MM41 CW, the navigator would have advanced 172 times by exactly one
marker, refused nothing, and stopped the locomotive never.

This is the first end-to-end field evidence for the contract in decision 0053.

## PROVENANCE: the track changed this morning

**The operator replaced the large disk magnets before this run** — the ones
identified in the 2026-08-28 survey as the source of the rebound population.
This lap therefore ran over a partly different magnet set from the one every
2026-08-28 measurement describes.

That is almost certainly why no peak fell below 140 counts today when 1.68% did
yesterday, and it means the two data sets must not be pooled. The 172/172 result
stands on its own — sequence matching does not care what the magnets are made
of — but the *amplitude* and *rebound* numbers here describe the new track, not
the surveyed one.

A correction to the record while this is in view: the list of magnets proposed
for swapping on 2026-08-28 (004, 061, 063, 080, 081, 100, 109, 144) was partly
wrong. MM004 at least is a regular magnet, not a large disk. The list should not
be relied on; the operator's own inspection governs.

## The caveat that keeps this honest

One lap is one lap, on freshly changed hardware. The lesson is not that a
140-count floor would have been safe today — it is that the floor moves with air
gap, battery, and now demonstrably with the magnets themselves, which is why the
ruling puts no floor above the entry threshold rather than trying to place one
cleverly.

Nor does this lap test the failure paths. Nothing was refused, so nothing
exercised the stop-on-identity-failure branch, the re-declaration path, or the
behaviour after a missed magnet. Those remain proven only in the host suite.

## References

- decision 0053; `firmware/test-programs/NAVI_2/NAVI_2.ino`
- `docs/research/20260828_WHAT_THE_HALL_SENSOR_SEES.md`
