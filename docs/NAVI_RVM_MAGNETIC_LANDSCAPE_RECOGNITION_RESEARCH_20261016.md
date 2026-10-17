# NAVI_RVM — Recognizing the magnetic landscape (research)

2026-10-16 · Claude (Opus 5.5) · Research and architectural recommendation only

> **Status.** This is a recommendation, not a decision. Nothing here is
> ratified, and nothing here authorizes implementation. No firmware, map,
> calibration, threshold, Pi service, MQTT setting or recording was changed.
> No device was flashed or operated. The October 15 source files were read
> only. Where this report touches an existing principle (decisions 0103–0108),
> it says so. It does not cite those records as authority (see §14).

> **Principles.** Per `NGR_DECISION_SYSTEM_DESIGN_PRINCIPLES.md` and
> `NAVI_DECISION_MODEL.md`, the design keeps the following:
> - NAVI is the sole judge;
> - pattern recognition is separated from geographic judgment;
> - evidence accumulates before revision;
> - PWM 0 has no navigation authority;
> - the baseline is spatial.
>
> It takes a deliberate position on one point: PWM is used as an upper
> *bound* that IR does not override (0103). That position, and every other
> point of contact with the principles, is listed in §14.

> *The future is known. The past is remembered. The present is interpreted.*

---

## 0. Answer first

**Recommended primary architecture: anchored, movement-constrained alignment
of a multi-resolution Hall observation strip against a stored, distance-indexed
landscape template, solved by online open-end dynamic programming (a
constrained DTW) inside a narrow cone around the single incumbent position.**

In plain terms:

1. **The future is known.** The route's Hall landscape is stored as a
   continuous template in *track distance* (mm) for each direction. It covers
   the magnets *and* the ordinary track between them. It is built offline from
   verified repeated passes and stays frozen during operation.
2. **The present is interpreted.** The live Hall signal is reduced, without
   discarding raw data, to a 10 ms block strip and a coarse-to-fine pyramid.
   It is aligned continuously to the template just ahead of the incumbent
   position. The alignment is a warp. Its slope is the locomotive's speed.
3. **PWM sets the admissible slope.** A speed envelope learned per PWM
   history from field data bounds the warp. PWM does not supply a distance.
   PWM 0 freezes the warp (0106).
4. **IR shapes the cost, not the constraint.** IR displacement is a soft term
   that saturates. A systematic 30–60 mm undermeasurement makes a path
   slightly more expensive. It cannot break an otherwise coherent Hall
   alignment.
5. **The past is remembered.** The DP keeps a fixed-lag history of a few
   seconds, plus a ring of raw samples and a list of completed ensembles. Each
   ensemble is interpreted *after* it completes, against its surroundings.
   That is where several lobes are found to belong to one magnet.
6. **Recognition is separated from judgment.** The recognizer exports one
   best path, its local cost profile, and per-landmark correspondences with
   their evidence. Geographic judgment is a separate explicit rule layer
   (NAVI Transparent Judgment). It decides advance, hold/watch, missed magnet
   (STOP) and loss of continuity.

**Why this fits NAVI:** NAVI's problem is not *"where am I among 171 places?"*
It is *"is what I am seeing the continuation of where I know I am?"* That is
a **sequence alignment with a known starting point and known physics**, not a
classification or a global search. Constrained DTW is the standard
mathematics for comparing two realizations of the same sequence traversed at
varying speed. The October 15 data show both properties it needs. First,
there is real speed variation at fixed PWM: ±17% at PWM 90 and ±20–25% at
PWM 50. Second, the landscape is highly repeatable in distance coordinates:
Hall opening positions repeat with a median SD of 2–3 mm between passes.

The main alternatives fall short for these reasons. Multiscale correlation
assumes locally constant speed. HMMs and particle filters maintain competing
locations by design. Event or threshold extraction is what the assignment
asks us to move beyond.

---

## 1. Evidence base used (October 15, 2026)

Sources (read-only):

- `~/NGR/NGR-Files/hall-ground-20261015/`: the "Find the Ground" ADC-only
  package. It covers the native NSR evening slice (SHA-256 `123e5111…eb6e8d`),
  frozen thresholds, 1,184 supported encounters, 288 uncertain intervals and
  277 censored edge fragments.
- `~/NGR/NGR-Files/navi-ewo-dart-experiment-1/field-records/20261015_IR_WAVE_1_ACCURACY/`:
  1,187 Hall-to-Hall IR intervals, session commands, and the cumulative and
  repeatability analyses.

The facts below were measured or quoted for this report. Units: ADC counts,
mm, s.

| Fact | Value | Source |
|---|---|---|
| Native Hall rate | ~5 kHz mean, bursts at ~22 kHz (45 µs) separated by ~0.8–1.1 ms gaps | `native_15549….bin` timing |
| Recording gaps | 134 + 12 gaps; max 1.49 s; 87,216 missing serials in boot 1 | `results_summary.json` |
| Dominant ordinary-track level | mode 1890.5; learned 50 ms basin 1876–1905 | `thresholds.json` |
| **Local** ordinary-track level varies along the track | e.g. ~1893 before MM72, ~1884 after; ~1899 after MM9; 1874–1887 around MM90 | profiles, this report |
| Supported encounter duration | median 0.25 s, p99 0.84 s (moving); up to 3.39 s (PWM 0) | `results_summary.json` |
| Raw baseline-band recrossings per encounter | median 58, p99 168 | same. A spike/threshold view sees many "events" per magnet |
| Map spacing (selected edges) | median 300 mm, IQR 300–305, range 280–360 | `interval_discrepancies.csv` |
| Speed at fixed PWM (map mm / Hall-to-Hall s), 5–95 % | PWM 50: 73–125 mm/s · PWM 90: 200–281 mm/s · PWM 110: 249–308 mm/s | computed here, n = 40–431 per stratum |
| IR minus map per interval | median abs 8.9 mm; 99.4 % within 40 mm; local repeatable shortfalls −30 to −59 mm (CW019→020, CW121→122); one +172.9 mm event | IR_WAVE_1 REPORT |
| Spatial repeatability of Hall features (IR coordinates) | opening position median SD 2.1 mm (CCW), 3.3 mm (CW); first-recorded-tick 6.8 / 8.6 mm | `comparison_summary.json` |
| Two-sided morphology repeats per magnet | all 8 encounters with two separate 20 ms excursions are **four magnets × two passes**: MM1, MM81, MM87, MM90 | `encounter_audit.csv`, this report |
| Single-sample transient | enc 14: one sample at 2026 among ~1895 (10 ms "interval") | raw, this report |
| Grillers stall | minutes 6–11 of boot 1: PWM 90, IR motion 0, Hall flat at ~1973 for ~5 min | `timeline.csv` |

Three consequences shape the design.

- **The landscape is nearly periodic in spacing** (300 ± 12 mm). Spacing
  alone cannot identify a magnet. Identity comes from *continuity*, polarity
  order and morphology. The dominant failure is a **one-magnet slip**, and the
  dominant safety question is the **missed magnet**.
- **Movement estimates are good enough to stop a one-magnet slip, but only
  when used together.** At PWM 90, one spacing takes 1.07–1.50 s. Two spacings
  take 2.1–3.0 s. Those ranges do not overlap. IR separates 300 mm from 600 mm
  easily. Each alone, however, has a failure that the other covers: the
  Grillers stall defeats PWM, and the +172.9 mm event defeats IR.
- **Ordinary track is part of the landscape.** Its local level changes by
  10–25 counts from place to place. A fixed global baseline therefore mislabels
  real structure, and "flat displaced ADC is not ground" (as the ground
  package's own protocol says).

---

## 2. Q1 — Multi-resolution representation

**Principle:** keep everything, but *decide* at the scale where the physical
phenomenon lives. A magnet is about 60–100 mm of track. A spike is one
conversion. A lobe is a sub-structure of a magnet's field.

| Level | Content | Rate / grid | Purpose |
|---|---|---|---|
| **R0 raw** | every native sample: serial, µs, ADC, PWM, direction | ~5 kHz | Preserved unchanged (recorder stream, on-device ring of 2–4 s). Never smoothed in place. |
| **R1 block strip** | per 10 ms: count, mean, min, max, first/last serial, PWM, IR pulses, gap flag | 100 Hz | Primary observation unit. Absorbs single-conversion transients into min/max without letting them move the mean much. Gaps are marked, never filled. |
| **R2–R4 pyramid** | box/Gaussian smoothing of R1 deviation in the **progress coordinate** at ~10, ~30, ~90 mm (factor 3) | derived | Scale-space: lobes merge at coarser scales; a magnet's ensemble survives to R4; a spike does not survive R2. |
| **E ensemble record** | closed ensemble: extent, polarity sequence of persistent extrema, area, peak(s), pre- and post-level, matched template landmark, local alignment cost | per magnet | Long-term memory and the unit of geographic judgment. |

The **feature** that is aligned is the deviation of R1 from a *slowly tracked
local reference*, not from a global constant. A short-window slope channel is
added beside it. The reasons are as follows.

- The Hall zero drifts with time ("Hall zero: age, not value"). Absolute level
  is therefore weak evidence. Relative structure over 100–300 mm is strong
  evidence.
- The template stores the expected *local level step* across each magnet (for
  example ~1893 before MM72 and ~1884 after). A level change becomes a
  recognizable feature rather than a baseline failure.

**Why scale-space, not wavelet thresholding:** the question *"are these two
excursions one magnet?"* is answered by **persistence across scale**. In the
Witkin/Lindeberg sense, two extrema that merge into one structure before the
~90 mm scale belong to one ensemble. An extremum that disappears by ~10 mm is
a transient. This replaces "N consecutive samples over 70 counts" with a
structural test whose scales come from physical sizes (magnet extent and
spacing), not from detector tuning. An undecimated Haar/B-spline à-trous
pyramid is an equivalent and cheap implementation.

**Progress coordinate.** R2–R4 are defined in mm, but the observations arrive
in time. Smoothing is done on the template side, which is in mm. On the
observation side it is done with the time scale implied by the current
alignment slope. The alignment (§3) supplies the slope, so the pyramid and the
warp are solved together, coarse to fine.

---

## 3. Q2 — Comparison methods

| Method | Handles variable speed | Uses whole pattern | Needs competing locations | Failure mode on NGR | ESP32 cost | Verdict |
|---|---|---|---|---|---|---|
| Threshold / opening events (current NAVI: openedAtMs, peakCounts, polarity) | n/a | **No.** Three scalars | No | 58 median recrossings per magnet; spikes; two-sided undershoots; stalls on a magnet | trivial | Baseline to compare against |
| Fixed-scale cross-correlation | No. Assumes constant speed in window | Yes | No (if local) | ±17–25% speed spread smears peaks over 300 mm | low (FFT or direct) | Coarse check only |
| **Multiscale / scale-searched correlation** (stretch factors) | Piecewise: one stretch per window | Yes | No | Cannot follow a speed change inside the window (164 PWM-increase, 225 decrease intervals) | moderate | Good *coarse* layer, not primary |
| **Constrained DTW** (Sakoe–Chiba band / Itakura slope; open-end; subsequence) | **Yes**, non-uniformly | **Yes** | No: one path | Pathological warps (singularities) if unconstrained; endpoint ambiguity | O(N·W), small | **Primary, with physical constraints** |
| Derivative DTW / shapeDTW (Keogh & Pazzani 2001; Zhao & Itti 2018) | Yes | Yes, local shape descriptors | No | Amplifies noise without smoothing | ~2–4× DTW | Adopt as **cost features**, not as a separate method |
| Symbolic / PAA / SAX | Partly | Lossy | No | Quantization hides small lobes and the level step | very low | Telemetry and compression only |
| Left-to-right HMM / Viterbi on the map | Yes | Yes | **Implicitly yes** (state lattice) | Equivalent to DTW when banded; unbanded it is a global search | moderate | Same mathematics as banded DTW; framed as DTW to keep the single-incumbent discipline |
| Particle filter / Monte Carlo localization | Yes | Yes | **Yes, by design** | Violates the constraint in ordinary navigation; tends to self-reinforce on periodic landmarks | high | **Rejected** for ordinary navigation; possibly a host-side recovery tool by David's decision |
| Matched filter per magnet | No | Per magnet | No | Reduces back to event detection | low | Use only as a template building block |
| Kalman filter on position with landmark fixes | Yes | **No.** Needs events first | No | Inherits event extraction | low | Use only for the movement-estimate fusion inside the cone (§4) |

### Recommended formulation

The template is a multi-channel vector at a 5 mm grid in direction *d*:

`T_d(x) = [ deviation@R2, deviation@R3, deviation@R4, slope@R2, expected local level step ]`

The observation is the R1 strip, `O(t)`, at 10 ms. The path `x(t)` is
non-decreasing in the travel direction. Each block step must satisfy

`Δx ∈ [ v_min(PWM history) · Δt , v_max(PWM history) · Δt ]` (§4)

Cost per cell:

`c(t,x) = Σ_scales ρ( (O_s(t) − T_s(x)) / σ_s(x) ) + λ_IR · ψ( Δx − Δx_IR ) + λ_acc · (Δ²x)²`

- `σ_s(x)` is the **template's own between-pass variability** at that place
  and scale. It is learned from repeated passes, not chosen. Places where
  passes differ (amplitude varies with speed) count less.
- `ρ` is robust (Huber/Tukey), so a single unexplained block cannot dominate.
- `ψ` is a *saturating* IR penalty, flat within IR's measured tolerance
  (§5).
- The `Δ²x` term discourages physically implausible acceleration. It
  replaces arbitrary DTW step weights.

The solution is **online open-end DTW**. One column is added per block. The
cone is centered on the incumbent's predicted position. Backpointers are
kept for a fixed lag (e.g. 4 s). Coarse-to-fine refinement (FastDTW style) is
applied: the R4 alignment fixes a corridor, and R2 refines it inside the
corridor.

*Note on "competing locations":* the DP evaluates many cells, but they are
**alignment offsets within a cone that movement evidence bounds to well under
half a magnet spacing** (§6). They are never alternative geographic
identities, and only the single best path leaves the recognizer. David should
decide whether this satisfies the constraint (§14, Q-A).

---

## 4. Q3 — PWM as mandatory, independent movement evidence

PWM is treated as a **speed envelope, not a speed**. The October 15 data show
why. At fixed PWM 90 the 5–95% speed is 200–281 mm/s. During the Grillers
stall, PWM 90 coexisted with zero movement for five minutes. PWM therefore
yields bounds, and its lower bound must be allowed to reach zero.

**Construction (offline, from field data):**

- For each PWM history class, record the distribution of observed speed. A
  class is the PWM held for at least 1 s, or an increase or decrease within
  the last *k* s. The input is Hall-to-Hall map distance over elapsed time.
  The October 15 strata are tabulated in §1.
- Use the 1st–99th percentile as `[v_min, v_max]`. Add a lag model for ramps:
  after a PWM step, the envelope widens toward the old and new classes over
  the measured ramp time. (Ramp-travel templates already exist:
  `NAVI_RAMP_TRAVEL_TEMPLATES_20260916_C3B93D0B.md`.)
- **Upper bound** (`v_max`): always usable. A train cannot outrun its PWM.
  This bound makes a one-magnet skip *impossible* in the alignment when the
  elapsed time is too short.
- **Lower bound** (`v_min`): usable only when *some* independent evidence
  confirms the train is moving, such as IR increments or Hall non-stationarity
  (the variance of moving ordinary track differs from stationary track; see
  the flat 1884 ± 1 after MM72 at 139.6 s versus the ±5 moving wobble before
  it). Without that evidence, `v_min` drops to 0. That is the stall case.
- **PWM 0:** `v_min = v_max = 0`. The warp freezes. Hall/IR changes are
  recorded but not aligned (0106 as written).

PWM stays independent of IR because the envelope is learned against *Hall
landmarks and the map*, not against IR. The combination rule for "the train
has certainly moved at least D" is §8.

---

## 5. Q4 — IR without letting undermeasurement invalidate Hall coherence

IR is the best direct displacement measurement (0103), but October 15 shows
three kinds of IR error:

- frequent small errors (median 8.9 mm);
- **repeatable local shortfalls** of −30 to −59 mm at specific edges in both
  directions;
- rare large anomalies (+172.9 mm, unassigned cause).

IR therefore enters as a **saturating soft cost on the warp slope**,
`ψ(e) = 0` for |e| ≤ τ_IR, rising linearly, then flat beyond τ_sat:

- `τ_IR` = the measured per-interval IR tolerance (e.g. the 95th percentile
  absolute IR−map error, 28–30 mm, **scaled to the alignment horizon**).
- **Saturation** means that IR disagreement, however large, adds at most a
  bounded cost. A Hall alignment that is coherent at R2–R4 over several
  magnets is never overturned by IR alone.
- IR **direction/polarity agreement is not required** (as the assignment
  states). Only the magnitude of forward progress is compared.
- IR has two roles beyond the soft term:
  - It **unlocks `v_min`** (positive increments confirm movement, §4).
  - It is one of the two legs of the missed-magnet demonstration (§8).
- **Known local biases** (CW019→020, CW121→122) can be stored *as template
  annotations* of expected IR-per-segment, taken from repeated passes. That
  removes them from the residual without "fixing" IR. This is optional and
  needs David's decision, because it encodes a measurement bias into the map
  (§14, Q-D).

---

## 6. Q6 — Continuity from a trusted start, not a global search

- **Initialization** happens only from an authoritative reference: an
  operator declaration (e.g. `start_interval 071-072`), or the boot condition
  per 0107. The cone is placed at the declared interval. Its width is the
  declared interval itself, about 300 mm.
- **Steady state:** each recognized ensemble **re-anchors** the cone. Its
  width collapses to the ensemble's localization precision, which the
  repeatability data put at about 3–6 mm SD. Between anchors the cone widens
  with the PWM envelope and the IR tolerance. Over one 300 mm interval at
  PWM 90 that is about ±40–60 mm. **This stays well inside ±150 mm (half a
  spacing), so the neighbouring magnet is never inside the cone in ordinary
  navigation.** That is the structural reason no competing location is
  needed.
- **The cone may approach half a spacing** only during long low-information
  spans: stalls, PWM-0 handling, IR loss combined with PWM ramps. That is
  exactly when continuity is at risk, and §8 governs it.
- The 171-location search is **never** run in ordinary navigation. If David
  wants a recovery aid, a whole-lap subsequence DTW could be run *host-side
  and advisory*. It would serve the operator's declaration and would not
  replace it.

---

## 7. Q5 — Completed ensembles interpreted retrospectively

An **ensemble** is opened when the R3 deviation leaves the template's
ordinary-track envelope at the current alignment. It **closes** when the
alignment has carried the train past the template's landmark extent *and*
the signal has met the expected post-landmark ordinary track (local level and
moving-track variance) for a span of movement. That span is measured in mm
along the path, not in time (0107: RTB is recognized from the phenomenon, not
a timer).

**On closure the ensemble is re-read as a whole, with its surroundings:**

1. **Scale-space grouping.** Persistent extrema that merge before ~90 mm are
   one ensemble. Sub-lobes and undershoots are kept as its *morphology*, and
   no one of them counts as a separate event.
2. **Template agreement.** The ensemble's polarity sequence and lobe layout
   are compared with the template landmark *including its known
   multi-lobe/undershoot signature*. On October 15, MM1, MM81, MM87 and MM90
   all show a main lobe plus a sub-baseline undershoot, and they show it **on
   both passes**. That signature is a property of the magnet, not noise.
3. **Movement between sub-structures.** If two apparent excursions are
   separated by an "apparent baseline", three things are checked:
   - the movement between them (PWM envelope, IR), set against the map gap
     to the next landmark;
   - whether the intervening level equals the expected local ordinary track
     or is displaced from it;
   - whether the intervening interval is **stationary** (low variance) or
     moving.

   A displaced or stationary "baseline" between excursions, with less
   movement than one spacing, means **one magnet**. An intervening span with
   ordinary-track level, moving variance and about one spacing of movement
   means **two magnets**.
4. **PWM 0.** Excursions at PWM 0 are recorded as an ensemble *with no
   navigation authority* (0106). They never advance the path.

Retrospective interpretation is the *confirmation*. A **provisional**
correspondence can be emitted as soon as the leading structure aligns. This
matters for station stops (§14, Q-B).

---

## 8. Q7–Q8 — Inconsistent evidence, watchful waiting, loss of continuity

### 8.1 Local cost is judged against its own history

Each template segment carries the **distribution of alignment cost per mm**
observed on verified passes. An observation is **atypical** when its local
cost falls outside that distribution. The cut-off is a quantile chosen by
David once, explicitly; it is not a hand-tuned count. This is the only kind
of decision threshold the recognizer needs. The rest of its structure is
physical (envelopes, extents, spacing).

### 8.2 Recognizer outputs (pattern recognition)

| Output | Meaning |
|---|---|
| `MATCH(k, cost, offset)` | ensemble aligned to landmark *k*, cost typical |
| `ATYPICAL(k, cost, what)` | aligned, but the cost is atypical (amplitude, shape, extra structure, level) |
| `UNMATCHED_OBSERVED(span)` | persistent structure with no template counterpart in the cone (an extra excursion) |
| `UNOBSERVED_TEMPLATE(k, why)` | the path passed landmark *k* with no persistent counterpart. `why` ∈ {data gap, PWM 0, low movement certainty, none} |
| `FROZEN` | PWM 0, or both movement legs absent |

### 8.3 Geographic judgment (separate layer, explicit rules)

| Recognizer evidence | Movement evidence | Judgment |
|---|---|---|
| MATCH | consistent | **ADVANCE** to *k*; re-anchor |
| ATYPICAL | consistent | **ADVANCE + WATCH**: position held as incumbent; anchor *not* tightened; record |
| UNMATCHED_OBSERVED, does not persist to R4 | any | **TOLERATE** (transient, e.g. enc 14); record |
| UNMATCHED_OBSERVED, persistent | consistent | **WATCH**: an unexplained physical structure; record (no silent magnets: it has a findable cause) |
| UNOBSERVED_TEMPLATE, why = data gap | any | **UNOBSERVED**, not missed. Hold, record, and resolve at *k+1* (policy, §14 Q-C) |
| UNOBSERVED_TEMPLATE, why = none | **demonstrated** passage (§8.4) | **MISSED MAGNET → STOP** |
| UNOBSERVED_TEMPLATE, why = none | not demonstrated (e.g. IR 0 under PWM: stall) | **HOLD** (stall). No advance. Cone keeps widening |
| cone width ≥ ½ spacing, or no authoritative reference after PWM-0 handling | — | **CONTINUITY LOST → STOP**; requires declaration or next accepted MM per 0106 |

### 8.4 "Demonstrated" passage of landmark *k*

All three legs are required:

1. **PWM leg:** even the slowest admissible path (lower envelope, *unlocked*
   by independent movement evidence) has carried the train past the end of
   landmark *k*.
2. **IR leg:** IR displacement since the last anchor exceeds the distance to
   the end of *k* minus IR's tolerance. Undermeasurement makes this leg
   conservative, which is the safe direction.
3. **Hall leg:** the observed strip over the expected location of *k* is
   ordinary moving track (not a gap, not stationary), *or* landmark *k+1* has
   been matched at the position predicted by having passed *k*.

The STOP is therefore issued, at the latest, at *k+1*. If *k+1* is also
unobserved, the same rule fires there, or continuity is lost: STOP either
way. A demonstrated miss is never "repaired" by sliding the path to explain
it away. The DP's monotonic, envelope-bounded path makes that slide
impossible, because skipping *k* would require exceeding `v_max` or IR
saturation.

### 8.5 Preventing self-reinforcing errors

- **The template is frozen in operation.** It is rebuilt only offline, from
  passes verified by lap closure and repeatability, and it is versioned.
- **The local reference adapts** only over movement-spanning spans that the
  alignment labels ordinary track with typical cost (0107). It never adapts
  during WATCH, at PWM 0, or across gaps.
- **WATCH does not re-anchor.** The cone keeps widening from the *last
  trusted* anchor. The questionable alignment therefore cannot become the
  reference against which it is itself verified.
- **Review uses new, independent data.** When *k+1* (and if needed *k+2*)
  arrive, the fixed-lag DP re-solves from the last trusted anchor through the
  questionable span to the new evidence. If the incumbent path is still best
  and typical, WATCH is cleared, and the earlier record is annotated, not
  rewritten. If it is not, the result is CONTINUITY LOST. It is never a jump
  to another location.
- **WATCH is bounded** by movement (e.g. two landmarks). It does not persist
  indefinitely (0104's OBSERVE → HOLD → RESOLVE).

---

## 9. Q9 — ESP32 computation and memory

Assumptions: classic ESP32, 240 MHz dual core, ~300 KB usable SRAM,
4 MB flash, no PSRAM. Hall sampling and Wi-Fi/MQTT stay on their current core.
The recognizer runs on the other core at low priority.

| Item | Size / rate | Estimate |
|---|---|---|
| R0 raw ring (2–4 s at ~5 kHz, int16) | 20–40 KB RAM | optional but recommended for retrospective re-read |
| R1 strip (100 Hz × 8 B), 30 s history | 24 KB RAM | |
| Template, both directions (52.264 m / 5 mm ≈ 10,450 cells × 5 ch × int16) | ≈ 105 KB **flash** (const) | read through a small window; or int8 channels ≈ 52 KB |
| Template variability σ_s(x) + annotations | +50–100 KB flash | |
| Cone width | ±60 mm / 5 mm ≈ 25 cells (typ.); ≤ 60 cells worst | |
| DP update per block | ~60 cells × ~4 predecessors × ~6 ops (cost) ≈ 1.5 k ops | at 100 Hz ≈ 0.15 M ops/s ≈ **<0.2 %** of one core |
| Fixed-lag backpointers (4 s × 60 cells × 1 B) | 24 KB RAM | 2-bit packing → 6 KB |
| Coarse R4 pass + review re-solve (4 s horizon) | ~25 k cells | a few ms, on event only |
| Ensemble memory (last 32 × 64 B) | 2 KB | |
| **Total RAM** | **≈ 60–100 KB** | must be checked against the actual NAVI build heap (E8) |

Floating point is unnecessary: the costs fit in int32 fixed point. The
dominant risk is not CPU. It is **heap headroom and deterministic latency
alongside the existing tasks**, and that has to be measured.

---

## 10. Worked examples (October 15, boot `15549e9385faa4d5`)

All values are 20 ms means of native ADC unless stated. Times are source
seconds in that boot.

### 10.1 MM70 → MM71 → MM72 at PWM 110, then a stop: lobes, a spike, and a displaced "baseline"

```
t (s)   136.44 136.58  136.70     137.48 137.61  137.74     138.52   138.84  139.14   139.52  139.7→
ADC      1890   1751    1902       1886   2034    1879       1899     1738    1900     1872   1884±1
         ───── MM70 (−) ─────     ───── MM71 (+) ─────  spike    ───── MM72 (−) ─────  dip   flat
level     ~1893 before                ~1887–1891 between              ~1895        after: 1884, std ≈ 1
```

What an event detector sees:

- three openings;
- a separate 10 ms "uncertain interval" at 138.53 s, which is **one sample at
  2026** among ~1895;
- an unselected supported excursion at 139.48 s (enc 16, 1872, about 100 ms);
- 24 recording gaps of 2–10 ms in 4.8 s, one of 49 serials at 137.973 s.

What the landscape aligner sees:

- **MM70→71→72 spacings** are mapped at 295 and 295 mm. Elapsed times are
  1.038 s and 1.177 s, which is 284 and 251 mm/s. Both fall inside the PWM 110
  envelope (249–308 mm/s), so the warp is admissible without help. The second
  lap over the same edges (1094.8–1096.8 s) gives 0.98 and 0.96 s; the
  difference is absorbed by the warp. IR errors on these edges are −5.4 and
  +13.9 mm, inside τ_IR.
- **The 2026 sample** is visible in R1 max but does not move the R1 mean
  (~1897). It does not exist at R2. → TOLERATE, recorded.
- **The gaps** are marked in R1 and not filled. None covers a landmark, so
  there is no UNOBSERVED_TEMPLATE.
- **MM72** shows its main negative lobe, a positive overshoot to 1900 at
  139.14 s, and then the 139.5 s dip to 1872. At R3–R4 these merge into one
  ensemble whose post-level (1884) differs from its pre-level (~1895).
  Retrospectively the dip is a candidate *part of MM72's trailing
  structure*. The template, built from the clean second-lap pass, decides
  that. Here, though, the dip coincides with the locomotive **stopping**.
- **After 139.6 s** the Hall becomes extraordinarily flat (20 ms means
  1883–1885) while **PWM is still 110** and IR shows no positive motion.
  This is the stationary-under-power signature. `v_min` is not unlocked, the
  PWM leg is not "demonstrated", and the judgment is **HOLD (stall)**. MM73 is
  *not* declared missed, even though PWM 110 for 1+ s would "predict" it.
  **This is exactly why PWM must be an envelope with an evidence-gated lower
  bound.**

### 10.2 PWM-0 ensemble with an intervening apparent baseline (enc 17, 18)

```
t (s)        184.4 ──── 187.8      187.8 ────────── 201.1      201.1 ─────────── 202.8     after
PWM            0                     0                           0
ADC (50 ms)  1884 →1834 (plateau)   1879–1880                  1880 →1833 →1868 →1821    1891–1892
             →1843 →1821 →1774        flat, in basin             (two sub-lobes)            flat
             →return                  "GROUND" in ADC-only        → return
```

- Both excursions are negative-going, the polarity of MM72. They contain
  **plateaus**, the step-like movement of a handled locomotive. They occur at
  **PWM 0** with no IR positive motion.
- The 13 s between them lies inside the learned basin with low variance. The
  frozen ADC-only classifier therefore calls it ground. Its level, however,
  is **1879–1880**: 4–5 counts below the level before (1884) and 11–12 below
  the level after (1891). It is a *displaced, stationary* level, not ordinary
  track.
- Ensemble interpretation: one magnetic structure (MM72's field), visited
  repeatedly while stationary or handled, with a displaced intervening level.
  It is **not** two magnets, and under 0106 it carries **no navigation
  authority** in any case. → FROZEN, recorded in full.
- **Continuity:** the reference is broken by handling at PWM 0. The operator
  declared `start_interval 071-072` at 263.45 s (`session_commands.csv`), and
  **MM72 was re-selected at 275.88 s** (PWM 48–54, ARCHES station). The
  following 72→73 interval agreed with the map to −0.8 mm. This is the
  intended 0106 path: continuity restored by declaration and the next accepted
  MM, not by inferring displacement from PWM-0 Hall changes.
- *Interpretation caveat:* that the train was handled back through MM72's
  field is **consistent with** the record (polarity, plateaus, PWM 0, the
  declaration, the re-selection). It is not established by it. The operator's
  recollection would settle it.

### 10.3 Two-sided signatures repeat per magnet

| Magnet | Passes (enc) | Main lobe peak | Undershoot | Notes |
|---|---|---|---|---|
| MM81 CW | 30, 608 | 2079, 2079 | ~1875–1877 | identical within 1–2 counts |
| MM87 CW | 36, 426 | 2085, 2084 | ~1873–1875 | |
| MM90 CCW | 880, 1095 | 2075, 2077 | ~1873–1874, ~100 ms | |
| MM1 CCW | 980, 1195 | 2154, 2149 | ~1869–1873 | |

Seen through a fixed baseline band, each of these is "two excursions", and a
threshold detector could re-arm on the undershoot. Seen as an ensemble
aligned to a template that contains the undershoot, each is one landmark with
a known shape. **The magnets that look most ambiguous to events are the most
distinctive to a pattern recognizer.**

---

## 11. Experiments against the October 15 recordings

All of these are host-side replays on the existing files. Per AGENTS.md §8
they test whether the software does what the design says, and whether the
design's assumptions hold *on recorded passes*. They are not behavioral
proof. Track tests follow (§12).

| # | Experiment | Data | Pass/fail evidence (stated before running) |
|---|---|---|---|
| **E1** | **Template build and cross-pass recognition.** Build the CW template from complete lap 1, test on lap 2 and the partial third pass, then swap. CCW: split-half by section (6 vs 7). | laps per `comparison_summary.json` | Landmark position error (aligned vs NAVI-selected opening, in IR mm) against the 2–3 mm repeatability SD; count of MATCH / ATYPICAL / UNOBSERVED; **zero** one-magnet slips |
| **E2** | **Speed robustness.** Repeat E1 restricted to PWM-change intervals (164 inc / 225 dec), 38 departures, PWM 50 sections | interval strata | Cost distribution in these strata vs cruise; no slips |
| **E3** | **PWM envelope holdout.** Learn `[v_min, v_max]` on even 30 s blocks, apply to odd blocks (the ground package's holdout) | intervals | Fraction of held-out intervals outside the envelope; any outside → record, do not widen to fit |
| **E4** | **IR stress, real cases.** CW019→020 (5 passes, −30 to −50 mm), CW121→122 (−59 mm), the +172.9 mm event, the two snapshot-association failures | IR_WAVE_1 FAILURES | Hall alignment unchanged where Hall is coherent; IR leg never triggers STOP alone |
| **E5** | **Ensemble interpretation.** MM1/81/87/90 two-sided passes; §10.1 (spike, MM72 trailing dip, stall); §10.2 PWM-0 ensembles | encounters listed | One ensemble per magnet; spike TOLERATE; stall HOLD; PWM-0 FROZEN |
| **E6** | **Stall vs miss.** Grillers stall (minutes 6–11, PWM 90, Hall ≈1973 flat) | boot 1 | HOLD throughout; no MISSED MAGNET despite PWM predicting ~75 m |
| **E7** | **Missed-magnet rule (deterministic only).** Delete one landmark's samples from a replay copy *as a software test of the rule*. Also leave one landmark out of the template (the converse: UNMATCHED_OBSERVED) | lap 2 copies | STOP at *k* or *k+1*; never a silent slide. **Explicitly a synthetic code test, not evidence that misses occur** |
| **E8** | **ESP32 budget.** Run the core DP on recorded R1 strips on a *bench* ESP32 | needs David's authorization (flashing) | Per-block latency, heap high-water, coexistence with sampling |
| **E9** | **Gap handling.** All 146 native gaps: confirm none is labelled a miss, and list any landmark covered by a gap | gaps CSV | UNOBSERVED, not MISSED; list produced |
| **E10** | **Template label independence.** Rebuild the template using only lap-closure-consistent passes. Report how many template landmarks depend on a single NAVI selection | all | A list of landmarks whose identity rests only on NAVI's own choice. That is the circularity risk (§13) |

Primary reported metrics for every experiment:

- slip count (must be 0);
- per-landmark offset distribution;
- local-cost distributions by segment;
- the full list of non-MATCH outputs with raw excerpts.

Nothing is discarded, and no parameter is tuned to make a count come out.

---

## 12. Track tests the replay cannot replace

Each needs David's design and authorization.

- **T1.** A repeat of IR_WAVE_1's suggested synchronized pass through
  MM019–020, MM121–122 and CCW143–142. It tells us whether the local IR
  shortfall is spacing, opening location or wheel travel. That answer decides
  whether IR annotations belong in the template (Q-D).
- **T2.** A bounded, operator-controlled **real missed-magnet** test (e.g. a
  temporarily shielded or lifted magnet at a quiet location, at PWM 50). This
  is the only real evidence for §8.4. It is optional, and it is David's call
  whether it is worth the disturbance.
- **T3.** A deliberate short stall and an operator handling episode, with
  dashboard notes, to confirm the stationary-variance and PWM-0 signatures
  used in §10.1–10.2.
- **T4.** A second locomotive (Toby), per 0107's "general model before
  locomotive-specific calibration".

---

## 13. Risks and limitations

- **Circularity.** The template and its landmark labels come from passes
  whose identities are *NAVI-selected* (IR_WAVE_1 is explicit about this).
  Lap closure (−8 / +21 mm CW) and cross-pass repeatability support them, but
  physical identity is not independently proven. E10 quantifies this.
- **Template staleness.** Magnets move, track is relaid, and Hall mounts
  change. A frozen template must be versioned and re-verified after any
  physical change. Persistent ATYPICAL at one landmark is the signal to do so.
- **Station-stop latency.** Retrospective confirmation adds roughly one
  ensemble length (~75–125 mm at PWM 90) of lag. The existing stop logic may
  need the provisional correspondence (Q-B).
- **Only one evening, one locomotive (Otto).** The speed envelopes and the
  σ_s(x) are Otto-on-October-15 quantities.
- **No real missed magnet in the corpus.** §8.4 is untested on physical
  evidence (T2).
- **This is a large architectural change.** Current NAVI reads three scalars
  per opening. The proposal changes what NAVI's recognizer *is*. It should be
  run first as instrumentation beside the existing navigator, if David wants
  it at all. That too is his decision.

---

## 14. Decisions for David (none are made here)

- **Q-A (constraint interpretation).** Does an alignment DP that evaluates
  offsets within a movement-bounded cone (< ½ spacing), and exports only one
  path, respect "no collection of competing geographic locations"? If not,
  the fallback is a greedy online aligner. It is less robust to atypical
  passes but holds strictly one estimate.
- **Q-B (stop latency).** Should station-stop logic act on the *provisional*
  correspondence (lobe onset aligned) or only on the retrospective
  confirmation?
- **Q-C (gap policy).** Is a landmark covered by a data gap "unobserved, hold
  to *k+1*", or STOP? The design treats it as unobserved. "No silent magnets"
  is respected because the cause (the gap) is recorded.
- **Q-D (IR annotations).** Should repeatable local IR biases be stored in the
  template? Doing so is honest about them, but it encodes a not-yet-explained
  bias into the map.
- **Q-E (the one tunable).** Which quantile of the per-segment cost
  distribution defines ATYPICAL? This is the only non-physical parameter, and
  it should be set once, explicitly, and recorded.

**Relationship to existing principles.** These are not cited as authority;
this records how the proposal touches them, and the implication of each.

- **0103 (direct measurement before proxy).** The proposal gives PWM a
  *mandatory* role, as the assignment requires. It does so as a *bound*, so
  IR stays the direct displacement measure. *Implication:* if PWM's upper
  bound ever conflicts with IR (IR shows more travel than PWM permits, like
  the +172.9 mm event), the design trusts the bound and the event is
  recorded. That is a deliberate choice of proxy over measurement in one
  direction, and it should be ratified or rejected knowingly.
- **0104 (accumulate before revising).** WATCH → review at *k+1* is that
  principle in mechanism form. *Implication:* errors are confirmed about one
  landmark late.
- **0105 (no voting).** The cost function *sums* terms. This is not
  evidence-voting: the terms are residuals of one physical alignment, and the
  geographic decision stays an explicit rule table (§8.3). David should
  confirm he sees it the same way.
- **0106 (PWM 0).** Adopted as written: the warp freezes and PWM-0 Hall/IR
  carries no authority. The design also addresses 0106's open question, as a
  rule for David to accept or reject: a motionless PWM-0 stop is detected
  from flat-stationary Hall plus no IR, and **does not** break the reference
  when Hall resumes at the same alignment.
- **0107 (spatial baseline, RTB from the phenomenon).** Adopted in spirit.
  "Ordinary track" becomes part of the template, and RTB is the alignment
  re-entering it over movement. *Implication:* this replaces a held scalar
  baseline (D8's open question) with a continuously maintained, map-indexed
  model.

---

## 15. Provenance

Measurements in §1 and §10 were computed in this session with read-only
scripts against:

- `hall-ground-20261015/derived/native_15549e9385faa4d5.bin`, using the
  package's own `NATIVE` dtype;
- `encounter_audit.csv`, `thresholds.json`, `results_summary.json`,
  `comparison_summary.json` and `timeline.csv`;
- the IR_WAVE_1 files `interval_discrepancies.csv` and `session_commands.csv`.

The speed envelope uses intervals with equal start and end PWM and an elapsed
time under 5 s. No source or derived file was modified. The research ZIP and
its freeze file were not regenerated.
