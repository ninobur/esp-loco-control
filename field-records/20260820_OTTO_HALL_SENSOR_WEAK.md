# 2026-08-20 — Otto's confusions are a weak Hall sensor, not the map

Status: **Diagnosed from telemetry; Otto to be red-tagged at end of session**
(operator, 2026-08-20). Not yet confirmed on the bench.

## The complaint

Otto "got confused" three times during the morning session. Two episodes
reached full `NO_QUORUM` and stopped the railway; the shorter ones resolved
themselves.

## What the telemetry says

Eight navigation episodes today, and they are **not scattered** — they repeat
at the same places with the same ambiguity:

| time | mm | duration | outcome | viable set |
|---|---|---|---|---|
| 11:14:06 | 71→73 | 4 s | recovered | [−1,1,3,4] |
| 12:05:28 | **71→73** | 4 s | recovered | **[−1,1,3,4]** |
| 11:17:03 | 163→165 | 7 s | recovered | [−1,1,4] |
| 12:08:32 | **163→165** | 6 s | recovered | **[−1,1,4]** |
| 11:29:40 | 14→23 | 201 s | **NO_QUORUM** | [−1,1,2,3,4] |
| 12:32:49 | 8→17 | 288 s | **NO_QUORUM** | [−1,0,1,3,4] |

## The map and the magnets are exonerated

7,625 marker reads were compared against `NGR_DNA1` in firmware. The polarity
convention is confirmed by the data itself (map value 1 = N: 98.5% agreement;
the opposite convention scores 1.5%).

**Not one marker consistently disagrees.** Every disagreement at all 171
markers is intermittent, and the majority read at each one matches the map. A
wrong map entry or a flipped magnet would disagree on *every* pass, for *both*
locomotives. Nothing does.

## The instrument is not

| | wrong polarity reads | rate | sensor peak, correct reads | sensor peak, **wrong** reads |
|---|---|---|---|---|
| **Otto** | 102 / 3,797 | **2.69%** | 146 | **102** |
| **Toby** | 17 / 3,861 | **0.44%** | 189 | 209 |

Three facts, same magnets, same track, same session:

1. **Otto misreads polarity six times more often than Toby.**
2. **Otto's misreads happen on weak signals** — mean peak 102 on wrong reads
   against 146 on correct ones, a 30% weaker field on exactly the reads that
   fail. Toby shows no such relationship (209 vs 189, i.e. noise), because Toby
   barely errs.
3. **Otto reads weaker everywhere** — mean peak 146 against Toby's 189, roughly
   25% down across the whole railway.

Otto's worst zones — mm 10–19 (15 errors), 160–169 (13), 60–69 (11), 70–79
(11), 0–9 (10) — are where an already-thin margin drops below the decision
threshold. They match the episode locations above.

## Mechanism

Weak field → polarity decision goes wrong → `DISAGREE` → three consecutive
disagreements open a quorum evaluation → in stretches where several offsets
score alike, no unique winner emerges → `NO_QUORUM`, and the railway stops.

## What to do at the bench

Lower or realign Otto's Hall sensor until his peaks approach Toby's. **The
target is measurable: mean peak ~189 with no systematic gap between correct
and incorrect reads.** Check sensor height, lateral offset from the magnet
line, and that nothing has loosened or shifted.

Re-run this same comparison after the adjustment. A fix shows up as the error
rate falling toward Toby's 0.44% and the correct/wrong peak gap closing.

## Correction recorded

An earlier reading in-session proposed that Otto was **missing every other
magnet**, from a `dt_conserve_ratio` sitting near 2.0. That was wrong: Otto
read 3,797 markers to Toby's 3,861 in the same session. He sees essentially all
of them and misreads their polarity — a different fault with a different fix.

## Note on proportion

Two days of this investigation went to ESP-NOW packet loss. Otto's confusions —
the failures that actually stopped the railway today — are mechanical, are in
the locomotive rather than the radio, and were diagnosed from telemetry that
was already being recorded.
