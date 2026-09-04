# NAVI_ONE — amplitude-floor sweep: is there a robust gap?

**Date:** 2026-09-04
**Asked by the operator** after finding 09 A showed the residual had uniquely
refused one observed non-magnet (ratio 0.36–0.38 against the 0.34 floor).
**Question:** does a single amplitude-ratio floor separate every observed real
magnet from every observed non-magnet, robustly, with the firmware's evolving
gain? **Answer: a gap exists on the observed data, but it is not robust to
gain adaptation. No build.**

Nothing was changed. This is an evidence check of one existing physical test.

## Sources

| source | records | gain history |
|---|---:|---|
| 2026-08-28 survey waveforms, `rej` labelled, field order | 351 | firmware recognizer, evolving, 168–200 |
| bespoke run 2026-09-03, every passage, field order | 1,805 | evolving, 176–236 |
| `mm/marker` events, Toby, 2026-08-30 → 09-03, navigation active | 15,215 accepted, 129 refused/struck | the locomotive's own gain, 169–278 |
| gate 12 real fixtures F09A, F09B, F11, F13 | 4 | 190–192 |
| bench session 2026-09-02 23:46 (X9, stationary, PWM 0) | 5 | 190 |

Passages judged with navigation unset (NO_POSITION) are excluded: nobody
labelled them.

## 1. Highest ratio among observed non-magnets

| ratio | peak | gain | where | what |
|---:|---:|---:|---|---|
| **0.453** | 86 | 190 | bench, 2026-09-02 23:46:13, stationary, before declaration | artifact, 668 ms, dec 2, resid 0.149 |
| 0.395 | 75 | 190 | bench, 23:46:28, stationary, just declared | artifact, 414 ms, resid 0.142, refused WRONG_SHAPE |
| 0.379 | 72 | 190 | railway, 2026-09-01 16:26 and 16:41 | finding 09 A, latched offset, refused WRONG_SHAPE |
| **0.341** | 73 | 214 | **railway, 2026-09-02 11:26:12, Arches CW approach at PWM 60, X2** | slow 2.5 s rise 36→74 that never fell, resid 0.268, refused WRONG_SHAPE |
| 0.279 | 53 | 190 | railway and bench, several | TOO_WEAK, the amplitude floor's own population |

The 0.341 event is a second railway case where shape was the only refuser,
and it sits 0.001 above the present floor. Under X13 as built it is
admitted. It was a stop-adjacent acquisition artifact of the X2 era (before
the discard branch and 0075 existed); whether it recurs under 0075 is
unmeasured.

Not counted as non-magnets: 0.443 (Arches CCW 14:14:40, X6) and the 0.558 /
0.574 stitched Arches CCW records. Those were real magnets damaged by the
capture, the faults 0070/0075 addressed.

## 2. Lowest ratio among genuine magnets

| ratio | peak | gain | where |
|---:|---:|---:|---|
| **0.624** | 133 | 213 | MM108 CW, Arches ZONE, bespoke run seq 244 |
| 0.632 | 120 | 190 (bootstrap) | MM132 CW, 2026-08-31, NAVI_ONE 0.4 |
| 0.641 | 132 | 206 | MM108 CCW, 2026-09-02 |
| 0.650 | 132 | 203 | MM106 CCW stitched, paused 36 s |

The survey's two records at 0.395 and 0.426 are labelled `rej=0` but carry
`mm=0`, are truncated (`tr=1`), 600 ms long at PWM 91, and sit 240–335 ms
ahead of MM127/MM128. They are not identifiable as magnets and are excluded
from the minimum; they are the only "real" records below 0.62 anywhere.

The 2026-08-31 DISAGREE strikes at 0.559–0.579 were NAVI_ONE 0.3/0.4
reading the pole from the entry sample (fixed by 0064); their peaks were
read on the wrong side of the passage and do not measure the magnet.

**Limiting magnet: MM108, peak 131–145 counts, both directions, Arches
zone, every build.** Under bootstrap gain the limit is MM132 CW at 0.632.

## 3. Is there a gap?

On the observed populations, yes: nothing observed between **0.453** and
**0.624**. A floor of 0.50 sits 0.05 above the strongest observed artifact
and 0.12 below the weakest observed magnet, at the gains those were seen at.

## 4. Field-order verdict changes at candidate floors

| floor | survey (351) | bespoke run (1,805) | marker history, accepted (15,215) | observed non-magnets newly refused |
|---:|---:|---:|---:|---|
| 0.36 | 0 | 0 | 0 | 0.341 |
| 0.40 | 1 (the 0.395 unlabelled record) | 0 | 0 | + F09A 0.379, bench 0.395 |
| 0.44 | 2 (+ the 0.426 unlabelled record) | 0 | 0 | same |
| 0.46 | 2 | 0 | 0 | + bench 0.453; also refuses the real amputated 0.443 |
| 0.50 | 2 | 0 | 0 | all observed |
| 0.56 | 2 | 0 | 0 | also refuses the real stitched 0.558 |
| 0.60 | 2 | 0 | 0 | also refuses the real stitched 0.574 |

No accepted railway passage in five days of navigation changes verdict at
any floor up to 0.60.

## 5. What sets the limit

MM108 in the Arches zone, peak about 133 counts. At the bespoke run's
highest gain (236) it reads 0.56. At the gain the locomotive reached on
2026-08-31 15:33 (278, after a run of 246–326-count peaks around MM0–MM11)
it would read **0.478**, below a 0.50 floor.

## 6. Can gain adaptation or bootstrap collapse the separation?

Yes, and this is the finding that matters. The artifacts and the magnets
are both fixed in counts; the ratio divides both by a gain that has ranged
169–278 in the field. The same 86-count artifact reads 0.31 at gain 278 and
0.51 at gain 169. The same 133-count magnet reads 0.48 at gain 278 and 0.79
at gain 169. The two ranges cross. A ratio floor that separates them at one
gain fails at another the locomotive has actually run at.

In absolute counts the observed populations do not cross: non-magnet peaks
top out at 86 (bench) and 73 (railway); real magnet peaks bottom out at 120
(MM132, once) and 131 (MM108, routinely). That is a 1.4× gap in counts,
where the ratio gap is 0.05 at the worst observed gain. Decision 0054 chose
a relative floor because a fixed 140 would have refused MM012 on Otto; the
observed Toby minimum is 120. This is reported as evidence, not proposed.

Bootstrap does not collapse it: under gain 190 the weakest accepted real is
0.632 and the strongest artifact 0.453.

## Conclusion

A floor of 0.50 separates every observed real from every observed
non-magnet, with margins of 0.05 and 0.12 at the gains they were seen at.
It is not robust: the gain excursion to 278 observed on 2026-08-31 would put
MM108 below it, and the gain minimum of 169 would put the bench artifact
above it. By the operator's criterion, that is not a robust separation.
Stopping here; nothing built, nothing revised in X13.
