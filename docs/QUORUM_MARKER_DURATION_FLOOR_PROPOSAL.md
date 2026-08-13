# Raising the marker duration floor — proposal for review

Status: **Proposed. Not implemented. No firmware changed.**
Date: 2026-08-12
Evidence: Otto (9950011), QUORUM_1_13, CW and CCW running, captured by
`ngr_runlog.py` on ngr-pi (decision 0028).

> **Revision 2, same day.** CCW running was added after the first draft, as
> validation §8.1 asked for. It found four more phantoms, all rejected by the
> proposed floor, and one finding materially worse than anything in revision 1 —
> a corrupted speed estimate that **persisted for 33 seconds across a stop**
> rather than spiking for a single sample. See §4b. Revision 1's directional
> prediction was also wrong; see §8.1.

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

## 4b. CCW validation, and a worse failure than §4 describes

CCW running produced four further phantoms:

| time | mm | obs | peak | ms | dt | follows real marker |
|---|---|---|---|---|---|---|
| 16:46:30.587 | 79 | N | 38 | 78 | 168 | mm 79, **S**, peak 169 |
| 16:59:09.039 | 89 | S | 42 | 40 | 334 | mm 90, **N**, peak 152 |
| 17:05:34.646 | 89 | S | 38 | 74 | 271 | mm 90, **N**, peak 168 |
| 17:06:11.001 | 60 | S | 40 | 42 | 561 | mm 61, **N**, peak 213 |

Every one is the opposite polarity to the real marker it follows — four for
four, matching all nine CW cases. Every one has a duration well under the floor.

**Running total: 13 phantoms, 13 rejected by `floor_ms=85`, 0 genuine markers
lost, across both directions.**

Note also what they follow: peaks of 169, 152, 168, 213 — the *strong* magnets.
A stronger pole face throws a stronger return-flux lobe. This is a caution about
"fix it with a bigger magnet": the replacement disk at mm 64 took that marker
from ~120 to ~211, which by this pattern raises rather than lowers the phantom
risk at that location.

### The 33-second frozen speed estimate

The mm 60 phantom did not produce a one-sample spike. It produced a wrong value
that persisted across an entire station stop:

```
17:06:10.906  est_mm_s=139   last good value, decelerating
17:06:11.001  marker mm=60 admitted — peak 40, ms 42, dt 561
17:06:11.906  est_mm_s=561   300 mm / 0.561 s ≈ 535 mm/s
17:06:21.910  moving=0       ← locomotive STOPPED
17:06:11 .. 17:06:43         est_mm_s = 561, unchanged, for 33 s
17:06:36.908  moving=1       ← departed, estimate still 561
17:06:44.906  est_mm_s=9     recovers, 33 s after it went wrong
```

For roughly fifteen of those seconds the locomotive reported **561 mm/s while
`moving=0`** — a stationary locomotive publishing a speed 1.6× its true maximum.

This is worse than §4 in kind, not only degree. Ten transient spikes can be
filtered by a consumer with a sanity bound. A value that is wrong, stable, and
self-consistent for 33 seconds, spanning a stop and a departure, cannot be — it
looks exactly like valid telemetry. Any CTO coordination consuming `est_mm_s`
would have believed it.

The internal signals do not contradict each other: `moving` and `est_mm_s`
disagree, but nothing reconciles them. Only the IR spoke, or the fact that the
wheels are visibly still, reveals it.

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

1. **CCW running — DONE, see §4b.** The mechanism is confirmed: phantoms occur
   in both directions, at opposite polarity to the magnet just passed, with
   marginal peak and sub-80 ms duration.

   **Revision 1's prediction was wrong and is corrected here.** It predicted a
   trailing lobe in CW would present as a *leading* lobe in CCW, arriving before
   the genuine marker. It does not. In both directions the phantom arrives
   **after** the real marker, 168–561 ms behind it.

   The likely reason is that the entry lobe is absorbed into the main detection —
   the detector latches on the approach and does not release until the field
   falls back through the exit threshold — whereas the exit lobe fires as a
   separate event once it has released. Only the exit lobe can become its own
   marker, and there is an exit lobe in either direction of travel.

   This does not weaken the case; it corrects the model. The proposal never
   depended on the directional signature, only on the duration separation, and
   that separation held on entirely independent data.
2. **A speed sweep**, to fix the real minimum dwell at the fastest speed the
   railway is actually operated at. The 122 ms figure is the binding constraint
   and it comes from PWM ≤ 120.
3. **Toby.** Everything here is Otto. Hall mounting is stated identical across
   both locomotives, but that is an assumption this proposal has not tested.
4. **A rollback image**, per existing practice.

Items 1 and 2 are ordinary running with the logger enabled — no special protocol.

## 9. For the reviewer

- **Is `floor_ms` applied before or after the value is used for speed
  estimation?** This is the question the proposal most needs answered, and it
  cannot be answered from telemetry. If rejection also suppresses the speed
  sample, §4 and §4b follow and the change is worth a great deal. If the
  estimator sees the read regardless, the change fixes marker phase only and the
  33-second frozen estimate in §4b survives it — in which case a separate fix is
  needed and is more urgent than this one.
- **Why did `est_mm_s` hold 561 for 33 seconds rather than decaying?** A speed
  estimate that does not age out while `moving=0` is arguably a defect
  independent of phantoms. Should it be bounded by `moving`, or by a staleness
  timeout, regardless of what causes the bad value?
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
