# NAVI MM Hall-only detection-to-detection guard: timing evidence (2026-09-22)

**This is an evidence report, not a decision.** It does not modify firmware
and does not choose a guard value. It answers, from the railway's own
telemetry plus the firmware source that produced it, what the data support
for a detection-to-detection timing guard to use **when IR is unavailable**.
The choice belongs to David and Sam.

Requested by: operator (via Claude), 2026-09-22.

---

## 0. Executive summary

- The **only** timing guard presently running is a flat **`MIN_MARKER_MS =
  500`** (`Navigator.h`), applied solely on the Hall-only path (no usable IR
  movement evidence for that judgment), measured from the previous
  **accepted** opening to the new opening — i.e. exactly the "guard begins at
  the previous accepted MM detection" framing already assumed in the brief.
  When IR **is** usable for a judgment, there is currently no time floor at
  all; a distance window governs instead. Today's builds all ran with
  `guard_ms:0` in the diagnostic banner — that field is a separate,
  currently-unused placeholder, not the real guard; see §2.1.
- In **all of today's Toby data** (958 genuine consecutive-MM advances, 342 of
  them adjudicated on the Hall-only path), the closest a genuine advance ever
  came to any candidate guard was **894 ms** (PWM 100). The false-event
  population — 4 confirmed Hall-only rejections — sits at exactly **402 ms**,
  invariant across PWM 29–42. There is a clean, unclaimed **492 ms gap**
  between the two populations, and it does not move with PWM.
- Under **every** guard tested against this same event stream — the current
  500 ms flat floor, a fixed 700 ms floor, and the proposed 700→1000 ms
  PWM-scaled ramp — the retrospective result is **identical**: 342 genuine
  admitted, 0 suppressed, 0 false events slip through, 4 false events
  rejected. Today's evidence cannot distinguish between these three rules.
  None has a better retrospective score than the others.
- The false-event population is **not PWM-dependent**. All 4 instances land
  on exactly 402 ms regardless of throttle, which lines up numerically with
  the detector's 400 ms identification window plus one sample tick — and
  NAVI_COHERENCE explicitly runs that detector with its refractory delay set
  to **zero** (the underlying library's own default is 645 ms — the same
  number decision 0087 fixed the old guard at). See §2.4 for the source
  citation. This is the single most load-bearing finding in this report: it
  says the failure this guard exists to catch is a **fixed sensor/window
  artifact**, not a speed-dependent "reached the next magnet too fast"
  phenomenon — which is the opposite of what a PWM-scaled ramp is built to
  fix.
- Station approach/departure zones and the Grillers grade show **no
  suppression risk** in today's data — minimum observed intervals inside
  every station-tagged window (936–1346 ms) stay well clear of any candidate
  guard. The railway's own deceleration profile (smooth, ~one PWM count per
  marker) is the reason: PWM never changes by much within one marker's span,
  so "current PWM at the new event" stays a fair proxy for "the speed that
  covered this specific span" even during a ramp. §4.4 shows the mechanism
  in full so this can be checked against faster deceleration profiles later.
- Low-PWM sample sizes are **thin** (2–5 Hall-only samples at PWM ≤40). The
  "no risk shown" finding at low PWM is real but should not be read as
  "impossible" — only "not observed yet." §4.3.
- No current-architecture (NAVI_COHERENCE) Otto data exists as of this
  report. Otto's most recent comparable field data is 10 days old, under a
  **different, closure-anchored** event definition (`NAVI_ONE_1_0X15`).  It
  is reported separately in §5 and is **not pooled** with Toby's numbers. It
  independently confirms the same qualitative picture (a rare,
  fault-clustered false-event mode, cleanly separated from genuine advances
  by physical-plausibility checks) but its own guard-relevant reconstruction
  carries enough uncertainty that it should be read as corroboration, not as
  additional precision.
- §7 evaluates the proposed 700→1000 ms ramp against this evidence directly
  and asks whether a simpler rule is equally or better supported. It is not
  a recommendation.

---

## 1. Scope and ground rules (from the task)

- Evidence only. No firmware change. No design decision.
- Locomotive-clock (device `millis()`) timestamps only, never MQTT-broker
  receive time. Every timing number in this report is computed from
  `opened_ms`, the device's own clock at the Hall opening sample.
- Physical plausibility (mm/s, and PWM-conditional speed) is checked before
  any interval is allowed to argue for or against a constant.
- Known physical faults are identified and excluded, not folded in as
  "normal."
- Measurements taken under incompatible event definitions are not mixed
  without saying so. Toby's NAVI_COHERENCE data (opening-anchored, no
  closure) and Otto's X15 data (closure-anchored, the old architecture) are
  kept in separate sections throughout.

---

## 2. Event definition and methodology — verified against firmware source, not assumed

Every claim in this section was checked directly against the firmware that
produced the logs, not inferred from field behaviour alone; several of them
overturned an initial assumption (marked below). Sources:

- `firmware/NAVI_COHERENCE/NAVI_COHERENCE_0_4/NAVI_COHERENCE_0_4.ino`, `Navigator.h`
- `firmware/test-programs/NAVI_COHERENCE_0_5_AUTO_ENABLED/{NAVI_COHERENCE_0_5_AUTO_ENABLED.ino,Navigator.h,Stations.h,RouteMap.h,HallObserver.h}`
- `firmware/test-programs/NAVI_ONE_X22/ExcursionDetector.h`

### 2.1 What each field actually is

| Field (topic `state/nav`) | What it is | How verified |
|---|---|---|
| `opened_ms` | Device `millis()` at the Hall **opening** sample | `hallTask()`: `pending.openedAtMs = opening.detectedAtMs` |
| `pwm_open` | Commanded PWM (`actualPwm`) at that same instant | `hallTask()`: `pending.pwm = actualPwm`, set in the same block as `openedAtMs` |
| `gap_ms` | **Dead field.** `Judged.priorGapMs` is declared but never assigned anywhere in 0.4 or 0.5 | Every `state/nav` line in every one of today's logs prints `"gap_ms":0`; grepping both source trees for `priorGapMs` finds exactly two hits each — the struct declaration and the `snprintf` that publishes it. Nothing ever writes it. |
| `ruling` / `evidence` | The navigator's verdict for this Hall opening | `Navigator::judge()`, `Ruling`/`EvidenceClass` enums in `Navigator.h` |
| `mm` / `tgt` / `dir` | Current mapped position, next expected marker, direction | `NavStatus` |
| (topic `nav/discrepancy`) `distance_assessable` | Whether IR movement evidence was usable for **this** judgment | `s_.distanceAssessable = interval.usable()`, published verbatim |

The `gap_ms` dead-field discovery matters: it means detection-to-detection
timing cannot be read off the logs directly. Every interval in this report
was computed independently, by walking `opened_ms` in file order.

### 2.2 The guard that is actually running

```cpp
static constexpr uint32_t MIN_MARKER_MS = 500;   // Navigator.h, both 0.4 and 0.5

// no usable anchor yet:
if (!haveAnchor_) {
  if (lastAcceptedMs_ && uint32_t(o.openedAtMs - lastAcceptedMs_) < MIN_MARKER_MS)
    return reject();
  ...
}
// anchor exists, but IR movement isn't usable for this judgment:
if (!interval.usable()) {
  ...
  if (elapsed < MIN_MARKER_MS) return reject();
  ...
}
// anchor exists AND IR movement is usable: NO TIME FLOOR AT ALL.
// A +/-10% distance window over the mapped route governs acceptance instead.
```

Two consequences, both confirmed by direct source reading rather than
assumed:

1. **The guard is already exactly what the brief's provisional idea
   describes**: armed at the previous *accepted* opening, evaluated in
   `opened_ms` terms, with no reference to closure anywhere. NAVI_COHERENCE
   has no closure concept at all (`EVENT_CLOSED` does not exist in this
   codebase). The historical 500 ms constant was simply carried over
   unexamined into this different timing reference — which is precisely the
   mismatch the task brief flags.
2. **A Hall-only-path rejection and an IR-distance-path rejection publish
   the identical `ruling:"NON_LANDMARK_HALL"` / `evidence:"NON_LANDMARK_HALL"`
   string.** They are indistinguishable from `state/nav` alone. They ARE
   distinguishable by joining the separate `nav/discrepancy` topic's
   `distance_assessable` field (0 = Hall-only/time-gated path was in force
   for this judgment, for either sub-reason above; 1 = the IR-distance path
   was in force, and no time floor applied). This report joins the two
   topics by `(boot segment, event_serial)` for every event and treats
   `distance_assessable==0` as "this judgment is governed by the guard under
   discussion" throughout. `guard_ms:0` seen in every boot's diagnostic
   banner is a separate, unrelated, currently-unwired field — it is not
   `MIN_MARKER_MS` and should not be read as "the guard is disabled."

### 2.3 Genuine vs. false, operationally

- **Genuine** = ruling ∈ {`ADVANCED`, `ADVANCED_WITH_DISCREPANCY`,
  `MISSED_AND_ADVANCED`}. `MISSED_AND_ADVANCED` spans more than one mapped
  marker (the recognizer inferred a skipped magnet from IR distance); its
  interval and mapped distance are computed over the full span, not per
  marker.
- **False, guard-relevant** = ruling `NON_LANDMARK_HALL` **and**
  `distance_assessable==0` (rejected specifically by the time floor, not by
  an IR distance mismatch). A `NON_LANDMARK_HALL` with `distance_assessable==1`
  is a different mechanism (an IR-confirmed distance that didn't fit any
  mapped marker) and is reported separately, never pooled with the
  guard-relevant false population.
- Excluded from "clean consecutive-MM": the interval immediately following a
  `DECLARED`, `DIRECTION`, or reboot (its start is a command/boot timestamp,
  not a physical magnet passage), and `LOCATION_UNRESOLVED`/`NO_POSITION`
  events (recovery-state bookkeeping, not adjudicated Hall openings).
- Excluded from the **moving-transit** tables (but reported separately, never
  discarded): any genuine interval **> 6000 ms**. `STATION_DWELL_MS = 5000`
  (`Stations.h`) plus ramp overhead, and today's mix of MANUAL (operator
  throttle) and AUTO sessions means some intervals span a real stop — either
  a scripted station dwell or the operator simply pausing. A stop can only
  **lengthen** an interval, never shorten it, so excluding these cannot hide
  a suppression risk; it only stops a 30-second pause from distorting a
  median. The threshold itself is data-grounded, not arbitrary: the observed
  distribution jumps from a 4271–5129 ms cluster straight to a 7611 ms+
  cluster — a real gap, not a cut through the middle of anything.

### 2.4 Why the false events cluster at a fixed offset, not a PWM-dependent one

`hallConfig()` (`HallObserver.h`, both 0.4's inline copy and 0.5's) sets:

```cpp
c.departCounts = departure; c.refractoryMs = 0; c.lostMs = 0;
```

The shared acquisition library it wraps
(`firmware/test-programs/NAVI_ONE_X22/ExcursionDetector.h`) defaults
`refractoryMs` to **645** and `windowMs` to **400**. NAVI_COHERENCE
explicitly overrides the refractory period to **zero** — every boot's
`state/bootid` confirms `"window_ms":400` and this session never showed a
nonzero refractory. With no refractory delay, the detector can re-arm the
instant a 400 ms identification window closes. §4.2 shows the observed false
events landing at exactly 402 ms after the preceding accepted detection,
independent of PWM — consistent with a trailing-edge re-trigger right as
that window ends, not with "the locomotive covered ground too fast." This is
offered as the most consistent explanation available from the code and data
together, not as an independently confirmed waveform-level finding — nobody
captured a `diag/waveform` trace of one of these four events, which would
settle it directly.

### 2.5 Deduplication

`field-records/logs/20260922_navi_coherence_0_4_lap.log` is **excluded**
from every table in this report. Its entire productive content — 182
genuine advances — is a byte-for-byte duplicate (identical `opened_ms`/`mm`/
`pwm_open` triples, confirmed programmatically, not by inspection) of one
boot segment of `field-records/logs/20260922_navi_coherence_0_4_runs/
9950012_20260922_133918.log`. The `runs/…133918.log` copy is kept (it is the
recorder-native file with a matching `.meta.json`). Pooling both would have
silently doubled one session's weight in every table below — including one
of the four false events. A cross-file signature check for further
duplicates among the remaining five files found none.

### 2.6 Route and station geometry (used for mapped distance and regime tagging)

Transcribed verbatim from `RouteMap.h` (0.5 build — the copy actually present
in a committed directory; 0.4's own copy of this file is not present in its
directory on disk, see caveat below): 171 markers, CW spacing table, minimum
280 mm, maximum 355 mm, mean ≈304 mm, total loop 52150 mm. Station geometry
(4 active stops — Patio, Grillers, Arches, Bamboo — approach/zone/ramp/dwell
windows, `STATION_DWELL_MS=5000`) from `Stations.h`, same build.

**Caveat**: `NAVI_COHERENCE_0_4`'s own directory does not contain its own
copies of `RouteMap.h`/`Stations.h`/`HallObserver.h`/`Ops.h`, despite
`#include`-ing them by local name — they must have been present at
build/flash time and were not committed alongside that binary. The route
survey table is described in the file itself as long-stable ("unchanged in
value" since QUORUM) and is extremely unlikely to differ between the 0.4 and
0.5 builds; station stop-offset tuning is the part that has moved over time
per that file's own changelog. This affects only the qualitative
station/grade tagging in §4.4, not any interval or PWM number, which come
straight from the logs.

---

## 3. Files used

| File | Loco | Sketch | Genuine (clean) | Hall-only false | Notes |
|---|---|---|---:|---:|---|
| `field-records/logs/20260922_navi_coherence_0_4_runs/9950012_20260922_132859.log` | Toby | 0.4 | 0 | 0 | Stationary calibration boot, no Hall events |
| `field-records/logs/20260922_navi_coherence_0_4_runs/9950012_20260922_133918.log` | Toby | 0.4 | 182 | 1 | MANUAL, `auto_enabled:0` |
| `field-records/logs/20260922_navi_coherence_0_4_runs/9950012_20260922_134700.log` | Toby | 0.4 | 0 | 0 | Idle telemetry only |
| ~~`20260922_navi_coherence_0_4_lap.log`~~ | — | — | — | — | **Excluded — duplicate of 133918.log, §2.5** |
| `field-records/logs/20260922_navi_coherence_0_4_run2.log` | Toby | 0.4 | 73 | 0 | MANUAL |
| `field-records/logs/20260922_navi_coherence_0_5_auto_run.log` | Toby | 0.5 (`AUTO_ENABLED`) | 703 | 3 | First autonomous-driving test build, `auto_enabled:1` |
| **Total (Toby, primary)** | | | **958** | **4** | |
| `field-records/logs/20260913_otto_x16_floor82_session.log.gz` | Otto | `NAVI_ONE_1_0X15_DEPARTURE_DIAG` | 1735 | 5 (+3 different mechanism) | **Secondary, §5. Closure-anchored — not pooled.** |

No current-architecture (`state/nav` with `opened_ms`/`pwm_open`) Otto log
exists anywhere in `field-records/logs`. This was checked directly, not
assumed: every log file containing a `state/nav` topic was enumerated, and
every one from 2026-07-30 through 2026-08-20 (Otto, QUORUM/closure-era) and
2026-09-12/13 (Otto, X15, still closure-anchored) predates the opening-only
event model. Today's five productive NAVI_COHERENCE files are all Toby.

---

## 4. Primary dataset: Toby, NAVI_COHERENCE 0.4 / 0.5, 2026-09-22

### 4.1 Genuine detection-to-detection timing vs. current PWM (deliverable 1)

Two populations are reported. "ALL evidentiary paths" is the honest physical
answer to "how fast can a genuine transit be at PWM P" (a magnet doesn't
know which sensor confirmed it). "Hall-only path" is the subset any change
to this specific guard actually touches, since IR-confirmed advances are
never subject to a time floor today.

**Moving-transit genuine advances, ALL evidentiary paths (dwell/pause
excluded), n=932:**

| PWM bin | n | min | p1 | p5 | p25 | median | mapped mm range |
|---|---:|---:|---:|---:|---:|---:|---|
| ≥90 | 655 | 894 | 936 | 990 | 1070 | 1139 | 0–635¹ |
| 80 | 68 | 1047 | 1056 | 1101 | 1224 | 1292 | 295–330 |
| 70 | 55 | 1290 | 1292 | 1321 | 1396 | 1576 | 290–330 |
| 60 | 112 | 1450 | 1468 | 1632 | 1852 | 2056 | 290–330 |
| 50 | 21 | 1832 | 1858 | 1963 | 2019 | 2324 | 290–320 |
| 40 | 13 | 1358 | 1502 | 2079 | 2630 | 2831 | 300–330 |
| ≤30 | 8 | 3487 | 3496 | 3533 | 3704 | 4208 | 300–320 |

¹ The 0 mm floor in the ≥90 mapped-mm range is a boundary artefact of one
`MISSED_AND_ADVANCED` computation wrapping at the route origin, not a
zero-distance genuine advance; every individual step is a real mapped span
(280–355 mm per marker; `MISSED_AND_ADVANCED` sums two).

**Moving-transit genuine advances, Hall-only path only (the guard-relevant
subset), n=320:**

| PWM bin | n | min | p1 | p5 | p25 | median | mapped mm range |
|---|---:|---:|---:|---:|---:|---:|---|
| ≥90 | 239 | **894** | 927 | 973 | 1068 | 1139 | 280–355 |
| 80 | 18 | 1060 | 1065 | 1087 | 1203 | 1260 | 295–325 |
| 70 | 15 | 1307 | 1313 | 1338 | 1387 | 1533 | 290–330 |
| 60 | 37 | 1584 | 1639 | 1781 | 1979 | 2131 | 295–330 |
| 50 | 5 | 1963 | 1965 | 1971 | 2003 | 2019 | 290–300 |
| 40 | 4 | 1358 | 1401 | 1571 | 2425 | 2818 | 300–320 |
| ≤30 | 2 | 3618 | 3619 | 3624 | 3647 | 3676 | 300–320 |

**n at PWM ≤50 is thin (2, 4, 5 samples).** Treat those rows as indicative,
not as a settled distribution. The apparent non-monotonicity (PWM 40's
minimum, 1358 ms, sits *below* PWM 50's minimum, 1963 ms) is very likely
small-sample noise, not a real physical effect — it inverts exactly where
the sample counts are smallest.

Exact-PWM spikes worth naming directly (these are the railway's own
constants showing through: 60 = station-zone speed, 90 = base cruise, 97/100
≈ manual-driving equivalents of cruise, 105 = Patio curve cruise):

| PWM | n | min | p5 | median |
|---|---:|---:|---:|---:|
| 60 | 77 | 1671 | 1777 | 2086 |
| 90 | 371 | 939 | 1014 | 1137 |
| 92 | 51 | 973 | 1056 | 1227 |
| 97 | 29 | 1046 | 1089 | 1464 |
| 100 | 112 | 894 | 938 | 1116 |
| 105 | 16 | 941 | 953 | 1161 |

**Physical-plausibility cross-check**: the fastest genuine speed observed
anywhere in today's data is **335.6 mm/s** (PWM 100, 300 mm in 894 ms). That
is the ceiling this railway actually demonstrated today, at its highest
tested throttle.

Dwell/pause-spanning intervals (excluded above, reported here for
completeness — 26 of 958 total, 22 of them Hall-only-path):

| PWM bin | n excluded | interval range |
|---|---:|---|
| 60 | 2 | 15056–50119 ms |
| 50 | 6 | 12185–84392 ms |
| 40 | 10 | 19404–23149 ms |
| ≤30 | 8 | 7611–39407 ms |

### 4.2 The false/reread/fringe population (deliverable 2, 5)

**Every Hall-only-path false event in today's entire dataset, in full:**

| File | Line | PWM at open | Elapsed since last accept | Direction | Previous magnet's own polarity |
|---|---:|---:|---:|---|---|
| `runs/…133918.log` | 6226 | 29 | **402 ms** | CW | S |
| `…0_5_auto_run.log` | 10884 | 32 | **402 ms** | CW | N |
| `…0_5_auto_run.log` | 19099 | 35 | **402 ms** | CCW | S |
| `…0_5_auto_run.log` | 19317 | 42 | **402 ms** | CCW | S |

All four: **exactly 402 ms**, at four different PWM values (29, 32, 35, 42)
spanning both directions and two separate boots. There is no PWM dependence
visible in this population — it cannot be, from 4 points at a single
repeated value, but the invariance itself is the finding, and it lines up
with §2.4's mechanism, not with a speed argument.

A second, non-obvious check: **in all four cases the previous and the next
mapped marker share the same polarity** (verified against `RouteMap.h`'s
polarity table, not eyeballed — e.g. marker 53 and 54 are both South;
markers 110 and 111 are both North). This means polarity screening provides
**zero** protection against a same-magnet rebound at exactly these four
locations — `obs` matches `expected` in every one of the four false events,
so only the timing floor caught them. It is also a reason to expect the true
rate of this artifact is higher than 4-in-958: at an alternating-polarity
marker pair, the identical rebound would show up as an accepted
`ADVANCED_WITH_DISCREPANCY` rather than a rejected `NON_LANDMARK_HALL` if it
happened to land after 500 ms — but see the next paragraph, because that
doesn't seem to be happening either.

**The closest 20 genuine intervals of any kind, checked individually, show
no such contamination.** They range from 894–990 ms, all at PWM 90–100, and
roughly half are `ADVANCED_WITH_DISCREPANCY` — but with entirely ordinary
mapped distances (290–315 mm) and no proximity to the 402 ms cluster. The
discrepancy flag at these points is about polarity mismatch, not about
suspicious timing.

**Separation**: 402 ms (false, n=4) vs. 894 ms (closest genuine, n=1 of 320)
— a clean 492 ms gap with nothing in it, at any PWM tested.

Three further events are excluded from this population because
`distance_assessable==1` (an IR-distance rejection, a different mechanism,
not this guard) — reported for completeness but never pooled:
`NON_LANDMARK_HALL` with IR distance evidence occurred 15 times across the
five files; none of them are part of the timing-guard question.

### 4.3 Fixed 700 vs. the proposed 700→1000 ramp vs. current 500 (deliverables 3, 4, 7, 9)

The proposed rule was evaluated exactly as specified —
`guard_ms = clip(1150 − 5×PWM, 700, 1000)` — replayed against the *same*
ordered Hall-only-path event stream as the current 500 ms floor and a flat
700 ms floor, with cascading effects modelled properly (a candidate that a
stricter guard would newly reject changes the anchor the *next* candidate is
measured against). A reality-check replay of the current 500 ms rule against
the same model reproduces the logs' own recorded rulings with **zero
mismatches** across all 342 Hall-only-path judgments — the replay model is
verified correct, not merely assumed.

| Guard | Genuine admitted | Genuine suppressed | False admitted | False rejected |
|---|---:|---:|---:|---:|
| Current (500 flat) | 342 | 0 | 0 | 4 |
| Fixed 700 | 342 | 0 | 0 | 4 |
| Proposed ramp (700→1000) | 342 | 0 | 0 | 4 |

**All three are retrospectively tied.** Nothing in today's evidence favours
the ramp over a flat 700, or a flat 700 over the existing flat 500. Every
genuine Hall-only advance at every PWM tested has a comfortable margin over
even the most conservative candidate (700 ms at PWM≥90, 1000 ms at PWM≤30);
every known false event sits at 402 ms, comfortably under even the least
conservative candidate. Per the task's own instruction, a tied retrospective
score is not a reason to prefer the more complex rule.

Margin detail, moving-transit Hall-only genuine minimum vs. each candidate's
required floor at that PWM bin:

| PWM bin | Observed minimum | Ramp requires | Margin |
|---|---:|---:|---:|
| ≥90 | 894 ms | 700 ms | **194 ms** (tightest, but n=239 — most trustworthy bin) |
| 80 | 1060 ms | 750 ms | 310 ms |
| 70 | 1307 ms | 800 ms | 507 ms |
| 60 | 1584 ms | 850 ms | 734 ms |
| 50 | 1963 ms | 900 ms | 1063 ms (n=5) |
| 40 | 1358 ms | 950 ms | 408 ms (n=4) |
| ≤30 | 3618 ms | 1000 ms | 2618 ms (n=2) |

Read with the sample-size caveat from §4.1: the tightest, most reliable
margin is at the **high**-PWM end (194 ms on n=239), not the low-PWM end the
ramp is most aggressive about. The low-PWM margins look enormous, but rest
on 2–5 points each.

### 4.4 Station deceleration/departure and the "current PWM" question (deliverable 6, 11)

Regime-tagged moving-transit Hall-only genuine advances (tagged from
`Stations.h`'s own approach/zone/ramp windows and `RouteMap.h`'s Grillers
grade band, both endpoints of each interval checked):

| Regime | n | PWM range | interval min | interval median |
|---|---:|---|---:|---:|
| ORDINARY | 161 | 40–102 | 894 | 1138 |
| STATION: Arches | 40 | 37–100 | 936 | 1217 |
| STATION: Grillers | 39 | 30–97 | 1052 | 1792 |
| STATION: Bamboo | 39 | 34–100 | 988 | 1340 |
| STATION: Patio | 27 | 63–100 | 946 | 1202 |
| Grillers grade (CW) | 22 | 97–97 | 1346 | 1513 |

**No suppression risk observed anywhere near a station or the grade.**
Minimums inside every station-tagged window (936–1346 ms) stay clear of
every candidate guard discussed. This is worth explaining rather than just
reporting: task step 11 specifically asks whether using *current* PWM
causes over-protection when the locomotive covered most of an interval at a
higher PWM before decelerating right at the end. A concrete worked example —
the Bamboo CCW approach, one continuous deceleration, each row one mapped
marker:

| PWM at this marker | Interval (ms) | Ramp's required floor |
|---:|---:|---:|
| 90 | 1378 | 700 |
| 86 | 1360 | 720 |
| 80 | 1473 | 750 |
| 74 | 1592 | 780 |
| 69 | 1763 | 805 |
| 63 | 2075 | 835 |
| 60 | 1968 | 850 |
| 60 | 2144 | 850 |
| 60 | 1851 | 850 |
| 60 | 2019 | 850 |
| 52 | 1963 | 890 |
| 34 | 3618 | 1000 |

PWM drops by only 4–6 counts per marker here, because the railway's own
approach ramp is explicitly designed as "one count at a time" smoothing
(`Stations.h`/`RouteMap.h` comments, and the operator's own recorded
instructions behind them). Because deceleration is spread across roughly the
same granularity as marker-crossing, "current PWM at this marker" stays a
fair stand-in for "the speed that actually covered this marker's span" —
which is *why* no suppression shows up here, not just *that* none does. A
station-entry profile with an abrupt PWM step rather than this gradual ramp
would be a materially different, untested case; this report only speaks to
the ramp shape actually flown today.

### 4.5 Simplest data-supported alternative (deliverable 8, 10 — descriptive only)

Given §4.2's separation (402 ms false vs. 894 ms nearest genuine, PWM-
invariant on the false side) and §4.3's margin table, **the data support a
flat guard anywhere in roughly [450 ms, 850 ms]** at least as well as they
support the proposed PWM-scaled ramp, and a flat rule is a smaller, simpler
change than a 6-point piecewise ramp. This is a description of what the
numbers show, not a recommendation — precisely because a tied retrospective
score (§4.3) is explicitly not sufficient grounds to prefer either rule, per
the task's own instruction, and because the low-PWM margins this
observation leans on are thin (§4.1, §4.3). It is also worth noting for
context, without weight one way or the other: the current 500 ms value and a
notional ~650 ms flat value both sit inside that supported range, and 650
happens to coincide with the 645 ms figure decision 0087 fixed the old
detection-anchored guard at (§2.4) — that arithmetic coincidence is reported
because it exists, not because it resolves anything.

---

## 5. Secondary / cross-check dataset: Otto, `NAVI_ONE_1_0X15_DEPARTURE_DIAG`, 2026-09-12/13

**Different, older, closure-anchored event definition. Not pooled with
Toby's numbers anywhere in this report.** `guard_ms=500` here is measured
**close-to-open** (decision 0081's definition), and `pwm_close` is PWM
sampled at each event's own closure (~`dur_ms` after its own open), not at
open. `gap_ms` in this schema is a live, populated field (unlike
NAVI_COHERENCE's dead `priorGapMs`).

This is the most recent Otto field data available (2026-09-12/13, ~10 days
before this report) and the only Otto log with a directly usable
per-event ruling/timing/PWM structure. It is included because the task asks
for both locomotives and because it contains genuine ground-truth false
events (a firmware-labelled `TOO_SOON` ruling) that Toby's own data, being
nearly free of false events, cannot supply in volume.

### 5.1 Genuine `ADVANCED` gap_ms (previous close → this open) vs. `pwm_close`

n=1735 (deduplicated: the log double-publishes every event to `mm/marker`
and `state/nav`, identical payload — only `state/nav` counted, same
discipline as §2.5).

| PWM bin | n | min | p1 | p5 | median |
|---|---:|---:|---:|---:|---:|
| ≥90 | 1283 | 788 | 895 | 961 | 1116 |
| 80 | 115 | 0¹ | 998 | 1008 | 1149 |
| 70 | 104 | 0¹ | 1264 | 1274 | 1637 |
| 60 | 192 | 1626 | 1645 | 1848 | 2237 |
| 50 | 27 | 2050 | 2053 | 2073 | 2283 |
| 40 | 14 | 1811 | 1917 | 2341 | 3178 |

¹ Two `gap_ms=0` outliers (of 1735) are excluded from the "min" reading as
likely reset-boundary sentinels, not real measurements; the p1 column is
unaffected and is the more trustworthy floor for these bins.

These are **not** open-to-open intervals and are not compared numerically to
Toby's table above. They are reported to show the same qualitative shape
(tight clustering at high PWM, widening at low PWM) under a different clock
reference.

### 5.2 Ground-truth false events

**`TOO_SOON` (time-gated rejection — the same mechanism this report is
about), all 5 in the dataset:**

| Line | mm→tgt | Dir | pwm_close | gap_ms (close→open) | own dur_ms |
|---:|---|---|---:|---:|---:|
| 64503 | 92→91 | CCW | 90 | 52 | 95 |
| 64513 | 92→91 | CCW | 90 | 215 | 1200 |
| 64542 | 91→90 | CCW | 90 | 62 | 2446 |
| 77975 | 62→61 | CCW | 41 | 122 | 1558 |
| 78000 | 61→60 | CCW | 31 | 370 | 1007² |

² this row's own `dur_ms` is 871; 1007 in this column is the reconstructed
open-to-open figure explained below, kept in this column for reading
convenience.

**Three of these five (lines 64503, 64513, 64542) are not independent
samples.** They occur within 39 seconds of each other, all at mm 90–92, and
are immediately followed by a `WRONG_MAGNET` (polarity-mismatch) event at
line 64559 with `nav_state:"STRUCK"`. The accepted event immediately
preceding this cluster (line 64497) has `dur_ms=1569` — the single largest
value in the entire 1735-event genuine population (median 152, p95 299).
That combination — an abnormally long closure, followed by a burst of
rejected candidates, followed by a `STRUCK` polarity failure — reads as one
localized incident (a stall, a strike, or a rebound field at that specific
magnet), matching the "post-strike... four candidates while parked" pattern
already on record in decision 0087 for a different location (MM136). It is
reported in full rather than discarded, but should not be read as three
independent measurements of ordinary false-event timing.

The other two (lines 77975, 78000) look like ordinary, uncorrelated
instances.

Reconstruction: open-to-open(this, prev) ≈ gap_ms(this) + dur_ms(immediately
preceding accepted record). This is an **approximation** — it chains through
`dur_ms`'s own variability (§5.1's footnote territory: p95 299 ms, but this
specific chain hit the 1569 ms maximum) — reported, not treated as
equivalent-precision to a native open-to-open measurement:

| Line | Reconstructed open-to-open | Reliability |
|---:|---:|---|
| 64503 | 1621 ms | Low — chains through the 1569 ms outlier `dur_ms` above |
| 64513 | 310 ms | Low — same contaminated cluster |
| 64542 | 158 ms | Low — same contaminated cluster |
| 77975 | 428 ms | Higher — independent, uncorrelated instance |
| 78000 | 1007 ms | Higher — independent, uncorrelated instance |

The 1007 ms figure is the one number in this entire report that would slip
past the proposed 1000 ms ceiling if taken at face value. It does not
survive a physical-plausibility check: at `pwm_close=31`, a genuine 300 mm
step in 1007 ms implies **298 mm/s — 89% of the fastest speed Toby produced
anywhere today, at PWM 100–117** (§4.1), while running at under a third of
that throttle. Decision 0079 establishes Otto's own constants are measured
separately from Toby's (they are not assumed identical), so this cross-check
borrows Toby's ceiling as an approximation rather than an exact bound — but
the size of the mismatch (a 3.5×+ throttle gap producing near-parity speed)
is well outside what a reconstruction-noise explanation alone would need to
be treated as suspicious rather than confirmed.

**`WRONG_MAGNET` (polarity mismatch — a different mechanism, not
time-gated, excluded from the guard-relevant false population), all 3:**

| Line | mm→tgt | Dir | pwm_close | gap_ms | obs / expected |
|---:|---|---|---:|---:|---|
| 1091 | 151→150 | CCW | 90 | 558 | N / S |
| 64559 | 90→89 | CCW | 90 | 1107 | N / S |
| 78008 | 61→60 | CCW | 27 | 1606 | N / S |

All three passed the 500 ms time floor comfortably (558–1606 ms) and were
caught by polarity instead — a different screen catching a different
failure mode, consistent with §4.2's point that timing and polarity protect
against different things and neither substitutes for the other.

### 5.3 What Otto adds and does not add

- **Adds**: independent confirmation that the false-event mechanism is rare,
  clusters around specific incidents rather than scaling smoothly with
  speed, and fails a physical-plausibility check when it doesn't look
  obviously contaminated. Confirms polarity and timing are separate,
  complementary screens under the older architecture too.
- **Does not add**: precision. The closure-anchored reference frame, the
  `dur_ms` reconstruction noise, and the correlated 3-event cluster all
  argue against using Otto's numbers to tighten or loosen any specific
  millisecond value derived from Toby's data.
- **Gap**: no current-architecture Otto run exists. Everything here is
  10+ days old and under a different codebase. If Otto runs NAVI_COHERENCE,
  a repeat of §4's analysis on Otto's own data would be the direct fix for
  this, not a re-reading of X15.

---

## 6. Cited prior decisions (context, not re-derived here)

- **0081** ("The rebound guard is 500 ms", accepted 2026-09-10): fixed the
  500 ms constant, close-to-open, after a 436–438 ms event was wrongly
  treated as genuine — that event would have needed ≈473 mm/s (≈PWM 144) to
  be a real marker-to-marker transit at the shortest (280 mm) spacing,
  against a recorded fleet maximum throttle of 120. Explicitly declined a
  PWM-dependent extension at the time ("500 ms should be adequate...revisit
  only if field evidence shows problems at low speed").
- **0087** (proposed, 2026-09-16, not ratified): moved the guard's anchor
  from closure to detection, computing 645 ms = 145 ms (median
  detection→closure on Otto's QUORUM corpus at PWM 90, n=1117) + the
  original 500 ms. Explicitly still not PWM-dependent at that point either,
  and explicitly named "645 ms is a time proxy for spatial separation and is
  imperfect at low speed" as an accepted, unresolved cost.
- **0079** ("Otto's recognizer constants are measured on Otto"): the reason
  this report does not cross-apply Toby's PWM-speed ceiling to Otto without
  flagging it — the two locomotives' constants are measured, not assumed
  shared.

These are cited as background, not independently re-verified from raw logs
in this pass (0081/0087's own underlying 2026-09-10 log was not re-pulled
here) — treat the numbers above as sourced from the ratified/proposed
records themselves.

---

## 7. Direct answers to the deliverable list

1. **Genuine timing vs. PWM** — §4.1.
2. **False/reread timing vs. PWM** — §4.2 (Toby, native); §5.2 (Otto,
   cross-check).
3. **Fixed-700 performance** — §4.3 (tied with current 500 and the ramp:
   342/0/0/4 across the board).
4. **Proposed 700→1000 performance** — §4.3 (same: 342/0/0/4).
5. **Every genuine magnet each rule would suppress** — **none**, under any
   of the three rules tested, anywhere in today's data (§4.3).
6. **Every false event each rule would admit** — **none**, under any of the
   three rules tested (§4.3); all 4 sit at 402 ms, all 3 candidate floors
   start at 500 ms or higher.
7. **Is current PWM empirically useful for selecting the guard?** — Not
   demonstrated today. The false population is PWM-invariant (§4.2, §2.4);
   the genuine population's tightest margin against any candidate occurs at
   the *high*-PWM end, not the low end the ramp is built to protect (§4.3).
8. **Simplest data-supported alternative** — §4.5: a flat guard in roughly
   [450, 850] ms, offered descriptively, not as a recommendation.
9. **Reproducibility** — §8.

---

## 8. Reproducibility

- `tools/mm_guard_timing_toby.py` — primary Toby analysis. Run with no
  arguments from the repo root; reads the five files listed in §3 directly
  from `field-records/logs/`, writes
  `field-records/analysis/20260922_mm_guard_timing_toby_genuine.json` and
  `..._hall_only_false.json`. Re-run and verified clean from the committed
  paths (not from scratch state) before this report was written.
- `tools/mm_guard_timing_otto_x15.py` — secondary Otto analysis. Reads
  `field-records/logs/20260913_otto_x16_floor82_session.log.gz` directly
  (gzip-transparent). Same verification.
- `field-records/analysis/20260922_mm_guard_timing_toby_output.txt` and
  `..._otto_x15_output.txt` — full stdout of both scripts, including every
  individual event this report's tables were built from (all four false
  events, all 20 closest genuine calls, the full station/grade-tagged
  listing, the full dwell-exclusion listing).
- `field-records/analysis/20260922_mm_guard_timing_toby_genuine.json` — all
  958 genuine records with every field used (PWM, interval, mapped
  distance, evidentiary path, regime tags, source line).
- Both scripts' module docstrings restate the event-definition and
  exclusion rules from §2 in full, so the code is self-explaining without
  this document.
- Excluded from every table, with reason, throughout: `lap.log` (§2.5,
  duplicate), dwell/pause-spanning intervals >6000 ms (§2.3, §4.1, listed in
  full in the output text), first-interval-after-reset events (§2.3),
  IR-distance-path `NON_LANDMARK_HALL` events (§2.3, §4.2, different
  mechanism), `WRONG_MAGNET`/polarity events (§5.2, different mechanism),
  two Otto `gap_ms=0` sentinel outliers (§5.1).

---

## 9. Open questions this report cannot close

- No current-architecture Otto data. The single largest gap in this report.
- Low-PWM Hall-only sample sizes (2–5) are too thin to be confident the
  proposed ramp's generous low-PWM margins would hold up with more data,
  even though nothing today contradicts them.
- The §2.4 refractory-window mechanism for the 402 ms cluster is the most
  consistent explanation available from source + data together, but was not
  confirmed against an actual waveform capture of one of the four events —
  none was captured at the time.
- This report only speaks to the deceleration profile actually flown today
  (§4.4's "one count at a time" ramp). A faster or steppier approach profile
  is a different, untested case for the over-protection question.
