# Summary — 2026-08-13 to 08-20 Otto NO_QUORUM watch (source log deleted)

The source capture, `20260813_otto_1_13_noquorum_watch.log`, was 95.3 MB and
297,617 lines. It was deleted 2026-08-29 at the operator's direction: *"Old field
logs become less relevant every day. Especially from discarded models. If there
is any lesson to take from the file, summarize it, then delete it."* The
navigator it records — QUORUM 1.13 with the full recovery machinery — was
deleted by decisions 0053 and 0056.

This is what it said.

## The run

| | |
|---|---|
| span | 2026-08-13 14:40 → 2026-08-20 05:37 |
| duration | **159.0 hours** |
| locomotive | Otto (9950011), QUORUM 1.13 |
| marker detections | 41,546 |
| position declarations | 51 |

## The lesson: the recovery machinery was not a rare fallback

| nav event | count |
|---|---:|
| AGREE | 40,117 |
| DISAGREE | 647 |
| **QUORUM_TIED** | **357** |
| QUARANTINED | 133 |
| QUARANTINE_DISCARDED | 120 |
| **PHANTOM_REJECTED** | **100** |
| QUORUM_OPEN | 96 |
| **QUORUM_ADOPTED** | **85** |
| QUORUM_CLOSED | 82 |

**Position was relocated by committee vote 85 times.** Each adoption moved
`navMm` without a magnet having authorised it — the scoring committee decided
where the locomotive was. The committee deadlocked (`QUORUM_TIED`) 357 times,
more than four times as often as it reached a verdict.

100 events were refused as phantoms and 133 quarantined, on a locomotive whose
disagreement rate was 1.6% (647 in 40,764 judged).

`NO_QUORUM` was entered at `miss_streak: 3` — three consecutive disagreements
ended navigation, at MM160 CCW on 08-14 and MM033 CCW on 08-15 among others.

## Why this is worth remembering after the machinery is gone

The argument for offsets and adoption was always that they were an emergency
path, rarely taken. Over 159 hours of ordinary running they fired more than
800 times. Every firing was the navigator writing a position that no magnet had
confirmed, and the 2026-08-27 record shows where that ends: 254 markers of
believed advance against 47.0 m of wheel travel, roughly half a lap never
driven.

The scale is the point. A mechanism that exists to survive bad data will be
used constantly, because bad data is constant — and each use is indistinguishable
from the failure it was built to prevent.

## What was lost by deleting the source

Per-event waveform detail, voltage telemetry (34,151 samples) and throttle
history for a superseded navigator on the locomotive that is not the current
test subject. No decision rests on re-reading it; the counts above are the
evidence, and they are reproduced here in full.

Note: deleting the working-tree file does not shrink `.git`, which still carries
the blob. Reclaiming that space requires a history rewrite, which has not been
done.

## References

- decisions 0053, 0056; `docs/research/20260828_WHAT_THE_HALL_SENSOR_SEES.md`
