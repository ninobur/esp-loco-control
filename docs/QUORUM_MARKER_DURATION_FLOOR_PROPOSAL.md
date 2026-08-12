# Raising the marker duration floor — proposal for review

Status: **Proposed. Not implemented. No firmware changed.**
Date: 2026-08-12
Evidence: one session, Otto (9950011), QUORUM_1_13, 13 laps, 2,368 marker reads,
captured by `ngr_runlog.py` on ngr-pi (decision 0028).

## 1. What is being asked

Raise `floor_ms` from **40** to **85**.

That single number rejects every spurious marker read observed in the session
(9 of 9) and rejects none of the genuine ones (0 of 2,359).

A second, larger change — making the floor scale with measured speed rather than
being a constant — is described in §7 and is **not** part of this request.

## 2. The observation

Nine marker reads in the session sit just above the ±38 entry threshold:

| time | mm | obs | peak | ms | dt | PWM trend |
|---|---|---|---|---|---|---|
| 15:06:13.676 | 87 | S | 51 | 62 | 263 | steady 90 |
| 15:12:23.069 | 89 | N | 42 | 70 | 178 | steady 90 |
| 15:18:02.188 | 65 | N | 41 | 59 | 38975 | **accel 38→120** |
| 15:24:28.783 | 86 | N | 41 | 52 | 259 | steady 90 |
| 15:36:38.122 | 89 | N | 44 | 61 | 189 | steady 90 |
| 15:42:38.169 | 87 | S | 39 | 65 | 209 | steady 90 |
| 15:54:56.212 | 100 | S | 41 | 46 | 234 | **decel 85→62** |
| 16:06:29.287 | 76 | N | 43 | 49 | 190 | steady 120 |
| 16:12:45.708 | 87 | S | 39 | 45 | 215 | steady 90 |

They are one class. Every one has a duration of 45–70 ms and, apart from the
post-stop case, arrives 178–263 ms behind a genuine marker — secondary lobes of
a magnet the sensor has just passed.

`mm 87` produces one on three separate laps; `mm 89` on two.

**Seven of the nine occur at steady PWM.** This is not an acceleration
transient. It is a property of specific locations on the railway.

## 3. Duration separates them; amplitude does not

| population | n | ms range | 1st percentile |
|---|---|---|---|
| genuine (peak ≥ 70) | 2,359 | **122 – 775** | 127 |
| spurious (peak < 70) | 9 | **45 – 70** | — |

```
        45 ────────── 70          122 ──────────────── 775
        └── spurious ──┘   (gap)  └────── genuine ──────┘
```

Nothing occupies 70–122 ms. Any floor in that window is exact on this data:

| floor_ms | spurious rejected | genuine lost |
|---|---|---|
| 80 | 9 / 9 | 0 / 2359 |
| 85 | 9 / 9 | 0 / 2359 |
| 95 | 9 / 9 | 0 / 2359 |
| 120 | 9 / 9 | 0 / 2359 |

**Amplitude cannot do this job.** The systematic phantom at mm 65 reads a median
peak of 135 against an all-marker median of 139 — statistically
indistinguishable. Raising the peak threshold cannot separate spurious from
genuine without destroying genuine markers; the weakest real marker (mm 140)
medians 102, only 2.7× the current threshold.

`dt` cannot do it either: the genuine minimum is 195 ms against a spurious
maximum of 263 ms. They overlap.

## 4. Why this matters for CTO — the speed estimate

This is the part that reaches beyond four DISAGREE reads.

`est_mm_s` was sampled 5,226 times in the session. Median 150 mm/s, p95 302
mm/s. The IR spoke independently corroborates a true maximum near **345 mm/s**.

Ten samples exceed 700 mm/s:

```
15:06:14  1140      15:36:38  1613      16:06:29  1578
15:12:23  1713      15:42:38  1435      16:12:46  1395
15:18:03  1564      15:54:56  1282
15:24:29  1274      15:54:57  1282
```

**All ten map one-to-one onto the nine spurious reads.** Every phantom injects a
speed estimate four to five times the true value, because a ~200 ms interval
across a ~300 mm nominal spacing computes to ~1,500 mm/s.

The locomotive cannot detect this. Position and speed both derive from the Hall
sensor, so a bad read corrupts both at once and every internal signal stays
self-consistent — the hazard `ROAD_TO_CTO.md` describes. Only the independent IR
measurement reveals it.

If CTO coordination consumes `est_mm_s`, it is currently consuming ten
impossible values per session, at unpredictable moments, seven of them while
running at constant throttle.

## 5. What the change fixes, and what it does not

**Fixes.** All nine short-duration secondary lobes, including the 15:18 read
that fragmented an existing phantom into two crossings, shifted marker phase by
one, and produced the session's only four DISAGREE events. And, consequently,
all ten corrupted speed samples.

**Does not fix — important.** The *systematic* phantom at mm 65 survives
untouched. It is the return-flux lobe of the magnet at mm 64, it fires on every
pass, and its dwell is 416–712 ms — far above any floor proposed here. See
`field-records/20260812_otto_grillers_westpoint_disagree_cluster.md`.

That is deliberate, and it is the safe outcome: **the established position map
does not change.** mm 65 is currently carried as a landmark. Rejecting it would
renumber every marker after it, which is a far larger change than this proposal
and is not being requested.

This proposal removes reads the map never absorbed. It leaves the map alone.

## 6. Risk

**The failure direction is bad if we overshoot.** A rejected genuine marker
shifts phase exactly like an admitted phantom does. The floor must never clip a
real read.

**The gap is a ratio, not a window.** Both populations scale inversely with
speed, so the 122/70 = **1.74×** separation is preserved at any speed — but a
*constant* floor only sits inside the gap over a limited speed range:

| floor_ms | speed increase tolerated before clipping genuine markers |
|---|---|
| 75 | 1.63× |
| **85** | **1.44×** |
| 95 | 1.28× |
| 120 | 1.02× — none |

85 is chosen to sit above the spurious maximum (70) with 15 ms of margin while
retaining ~1.4× speed headroom. At this session's ~345 mm/s ceiling that covers
roughly 500 mm/s.

**If Otto is to run substantially faster than this session, 85 is not safe** and
§7 becomes necessary first.

**Sample limitations.** One locomotive, one session, predominantly CW, PWM 45–120.
2,359 genuine reads is a good sample for the separation itself; the *speed
envelope* is narrow, and that is the axis the risk lives on.

## 7. The durable version — not requested here

A constant floor is the wrong shape for a speed-dependent quantity. The firmware
already computes `est_mm_s`, so the floor could be expressed as a fraction of
expected dwell at the current speed, preserving the 1.74× separation at every
speed instead of one band of speeds.

Reasons to defer it: it is a larger change, it depends on `est_mm_s` — which is
the very signal these phantoms corrupt, creating a feedback path that needs
thinking about — and the constant is enough to remove every phantom observed so
far. Raised so the constant is understood as a first step, not an end state.

## 8. Validation before flashing

1. **CCW running.** The strongest available test, and the operator has proposed
   it independently. If these are return-flux lobes, direction reverses which
   side of each magnet the sensor traverses first: a *trailing* lobe in CW should
   appear as a *leading* lobe in CCW — before the genuine marker rather than
   after. Confirmation would establish the mechanism; absence would mean the
   model is wrong and this proposal rests only on a duration statistic.
2. **A speed sweep**, to fix the real minimum dwell at the fastest speed the
   railway is actually operated at. The 122 ms figure is the binding constraint
   and it comes from PWM ≤ 120.
3. **Toby.** Everything here is Otto. Hall mounting is stated identical across
   both locomotives, but that is an assumption this proposal has not tested.
4. **A rollback image**, per existing practice.

Items 1 and 2 are ordinary running with the logger enabled — no special protocol.

## 9. For the reviewer

- Is `floor_ms` applied before or after the value is used for speed estimation?
  The proposal assumes rejection also suppresses the speed sample; if the
  estimator sees the read regardless, §4 does not follow and the change is worth
  much less.
- Is 15 ms of margin above the observed spurious maximum enough, given only 9
  observations?
- Does anything else consume marker duration such that raising the floor has a
  second-order effect not considered here?
- Is the mm 65 phantom being left in the map the right call, or should
  renumbering be taken on deliberately while the railway is already being
  examined?

## 10. References

- `field-records/20260812_otto_grillers_westpoint_disagree_cluster.md` — the mm
  64/65 return-flux analysis this came out of
- `docs/QUORUM_LOW_PWM_PHANTOM_DESIGN_PROPOSAL.md` — earlier phantom proposal;
  note its finding that a *peak* threshold is not viable, which this proposal
  independently confirms from a different direction
- `docs/QUORUM_STATIONARY_BASELINE_POISONING.md` — ruled out as the mechanism
  here; baseline moved 2 counts across the session
- `docs/decisions/0025` — a phantom that proved to be maintenance, not firmware
- `docs/decisions/0028` — the continuous logging that produced this evidence
- `docs/ROAD_TO_CTO.md` — the shared-sensor hazard §4 is an instance of
