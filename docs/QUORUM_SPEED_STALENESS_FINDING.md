# `est_mm_s` never reports zero — speed staleness, measured

Status: **Finding, with a fix proposed but not specified in detail.**
Date: 2026-08-12
Source confirmation: CODEX, from QUORUM source.
Field measurement: Otto (9950011), QUORUM_1_13, one session, CW and CCW,
5,226 `est_mm_s` samples via `ngr_runlog.py` (decision 0028).

## The finding

`est_mm_s` is derived from a persistent `lastSegmentDt` with **no age
invalidation and no stopped-state invalidation** (CODEX, from source). It
therefore holds its last computed value indefinitely.

Measured consequence, this session:

```
samples with moving=0 :                     3290
   of those reporting est_mm_s > 0 :        3290   (100%)
reported speed while stationary :           min 59   median 133   max 561 mm/s
longest stationary interval :               1964 s  (32.7 min)
   reported throughout that interval :      133 mm/s, constant
```

**The locomotive never once reported zero speed while stopped.** Not in 3,290
opportunities. It sat still for thirty-three minutes publishing 133 mm/s.

## Why this is worse than it looks

The value is not merely wrong. It is *plausible*.

The session median speed while genuinely running is 150 mm/s. The median
reported while stationary is 133 mm/s. A consumer applying a physical sanity
bound — reject anything above the ~345 mm/s the IR spoke corroborates — catches
**one** of the 3,290 bad samples: the 561 mm/s case caused by the mm 60 phantom.

Every other stale value is indistinguishable from valid telemetry by inspection
of `est_mm_s` alone. Only `moving`, or the IR spoke, or looking at the
locomotive, reveals it.

## Interaction with the duration-floor proposal — read this before flashing

`QUORUM_MARKER_DURATION_FLOOR_PROPOSAL.md` proposes raising `floor_ms` 40 → 85.
CODEX confirms `EVENT_FLOOR_MS` rejection occurs **before** `MarkerEvent`
queueing, so the floor does suppress a phantom's speed sample as well as its
navigation effect. That is the proposal's §4 argument confirmed.

But applied to the mm 60 incident, the floor does not restore correct speed. It
changes what the estimate freezes *at*: instead of holding the corrupted 561
mm/s for 33 seconds, it would hold the previous valid **139 mm/s** for the same
33 seconds (CODEX).

**The freeze is not fixed. It is disguised.** 561 mm/s during `moving=0` is
detectably impossible. 139 mm/s during `moving=0` is not detectable by any bound
a consumer could reasonably apply.

The duration floor is still worth doing — it fixes marker phase, which is a real
defect with its own evidence. But it should not be mistaken for a speed fix, and
it slightly *reduces* the chance of noticing this one, because it removes the
single loudest symptom.

## Recommendation

Per CODEX: treat speed staleness as a **separate fix, landed before CTO consumes
`est_mm_s` operationally.**

Directions, not a specification — the shape is for whoever writes it:

1. **Invalidate on stopped state.** If `moving == 0`, `est_mm_s` should be 0 or
   explicitly invalid, not the last segment's value. This alone fixes 3,290 of
   3,290 observed cases.
2. **Age the estimate.** A segment time older than some multiple of the expected
   inter-marker interval is not evidence of current speed. This covers the case
   where the locomotive is crawling too slowly to trip `moving`, which stopped
   detection alone would miss.
3. **Publish validity, not just value.** The IR spoke already distinguishes
   `speed_valid` from `speed_mmps` and goes invalid rather than stale — see its
   payloads in the same log. Whatever QUORUM does, a consumer needs to be able
   to tell a current measurement from a remembered one.

Option 1 is the smallest change with the largest measured effect. Option 3 is
what makes the field safe for CTO to consume, because it moves the judgement to
the producer, which is the only party that knows.

## Why it was found

The phantom investigation put an impossible 561 mm/s next to an IR reading of
292 mm/s. That was visible only because the IR spoke is independent of the Hall
sensor, and only recorded because `ngr_runlog.py` subscribes to `ngr/#` rather
than the loco-only pattern it used to.

`ROAD_TO_CTO.md` says position and speed both derive from the Hall sensor, so a
bad read corrupts both and nothing internal can notice. This finding is a
variant: the estimate does not need a bad read to be wrong. It only needs time
to pass.

## References

- `docs/QUORUM_MARKER_DURATION_FLOOR_PROPOSAL.md` — the proposal this qualifies
- `field-records/20260812_otto_grillers_westpoint_disagree_cluster.md` — the mm
  60 / mm 64 phantom work that surfaced it
- `field-records/logs/20260812_ngr_all_topics.log` — the session, incl. the IR
  spoke's `speed_valid` handling for comparison
- `docs/ROAD_TO_CTO.md` — the shared-sensor hazard
- `docs/decisions/0028` — the logging that made the measurement possible
