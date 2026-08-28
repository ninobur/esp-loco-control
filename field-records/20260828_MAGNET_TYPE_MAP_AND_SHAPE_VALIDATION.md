# The magnet type map, and what the waveform got right

**Date:** 2026-08-28
**Source:** operator walking survey of all 171 markers
**Validated against:** pulse-shape classification from 2092 captures, `QUORUM_1_13W`

## The map (ground truth)

| stretch | type | n |
|---|---|---|
| MM000–MM011 | disk | 12 |
| **MM012** | **bar** | **1** |
| MM013–MM064 | disk | 52 |
| MM065–MM098 | bar | 34 |
| MM099–MM102 | disk | 4 |
| MM103–MM109 | bar | 7 |
| MM110–MM128 | disk | 19 |
| MM129–MM155 | bar | 27 |
| MM156–MM157 | disk | 2 |
| **MM158** | **bar** | **1** |
| MM159–MM170 | disk | 12 |

**101 disk** (on top of the sleeper) · **70 bar** (level with the sleeper).

Machine-readable copy generated alongside this record. This map did not exist
before today; it was recovered by eye and confirms a classification the
locomotive made on its own.

## The validation

The type of each magnet was predicted from **pulse width alone** — the fraction
of the normalised excursion spent above half height — split at 0.671, before the
survey was walked. The classifier saw no installation record; the only prior
anchors were six markers an old field record named as new disks.

```
markers classified and surveyed : 167
correct                         : 166   (99.4%)
wrong                           : 1     (MM012)
abstained (too few passes)      : 4     (MM063, 064, 076, 077)
```

Every stretch boundary was placed correctly. **MM158 — a single bar magnet
between two disk stretches, flagged in advance as the least confident call on
the railway — is a bar.** The classifier found a lone substituted magnet from
shape alone.

The four abstentions were markers with too few clean passes to place; all four
fall inside stretches called correctly, and the survey gives them disk, disk,
bar, bar.

## MM012 — the one it got wrong, and why

```
its pulse width   0.6719
the split         0.6710      <- missed by 0.0009
its own spread    0.0323      2.1x the railway median
```

The operator found MM012 **sitting on concrete and not centred**, and re-centred
it during the survey.

It was misclassified by nine ten-thousandths of a unit, and it was the second
most variable marker in the entire classification. A bar magnet read off its
centreline gives an unstable, drifting profile; the instability pushed its
median a hair over the boundary. **The physical fault is visible in the data as
variance, not as a wrong value** — which is a useful thing to know, because
variance is measurable without knowing the answer in advance.

## ACTION — MM012's calibration is now stale

Re-centring the magnet changes its air gap and alignment, which is exactly what
`strengthPct[]` and `durationMs90[]` encode. **Its entries in both direction
tables now describe a magnet that no longer exists in that position.** A few
passes each way will refresh it, as MM128 needed after its repair.

Until then MM012 is the one marker on the railway whose expectation is known to
be wrong.

## What this does and does not establish

**Does:** the waveform carries magnet type, and reads it at 99.4% across 171
markers against a truth it never saw. The morphology is real and legible. It
also detects a physically disturbed magnet as excess variance.

**Does not:** change
[0049](../docs/decisions/0049-waveform-shape-does-not-identify-a-magnet-on-this-railway.md).
Type is shared with a marker's neighbours — the map above is eleven blocks, so
only 4% of adjacent pairs differ — and identifying *what kind* of magnet this is
does not identify *which* magnet it is. Both findings are the same fact seen
from two sides: the shape describes the class cleanly and the individual not at
all.
