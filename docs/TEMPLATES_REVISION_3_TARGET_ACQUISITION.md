# TEMPLATES Revision 3 — Target acquisition, not threshold rejection

**Status:** Design synthesis with operator decisions incorporated.
**Authority:** Design-only. This document authorizes no firmware change,
build, flash, field operation, or IR control authority.
**Date:** 2026-08-27
**Evidence baseline:** field sessions `9950012_20260827_114203`,
`9950012_20260827_123552`, `9950012_20260827_134856`; offline captures
`toby_ccw_20260824`, `otto_ccw_20260824`, `otto_cw_20260824`,
`otto_change1_20260825`; `firmware/test-programs/TEMPLATES/TEMPLATES.ino`
as flashed 2026-08-27.
**Operator decisions incorporated:** §6 correction threshold determined
empirically, starting at 67%;
`durationAt()` validated by limited simulation then field-tested; IR
integrated now via ESP-NOW from the IR test car; pre-run calibration
required.

## 1. Result first

TEMPLATES' admission gate asks the wrong question. Today it asks "is this
signal strong enough to be a magnet, in general" — one flat amplitude floor
(140 counts) applied identically to all 171 markers, blind to which marker
is next, blind to direction, blind to everything the system already knows.

The system already knows a great deal. Given a starting position and
direction, it knows the expected polarity of the next marker (`dnaAt`),
that marker's expected field strength relative to the railway median
(`strengthAt`, measured, wired, and currently used nowhere upstream of
quarantine), the physical distance to it (`spacingMm`) and therefore,
combined with commanded PWM, roughly when it should arrive (`dt_expected`,
already computed and published today).

Revision 3's governing change: **stop asking "does this clear a universal
floor" and start asking "does this match the specific target we are already
looking for."** A weak-but-expected read (MM141 at 125, against an expected
~144) is evidence *for* MM141, not noise to discard. A strong-but-unexpected
read is a warning, not automatic confirmation. The admission decision moves
from a one-axis filter to a multi-attribute match against a target profile
the system possesses before the magnet arrives.

This is not a new philosophy replacing the existing one. It is the same
asymmetry the current design already enforces — false negatives recoverable,
false positives corrupt the map — applied with more of the available
evidence, so fewer real magnets get treated as if they were noise.

## 2. What today's data actually shows

### 2.1 The admission gate's real behavior in the field

Across three sessions today (CW, CCW, and a live in-progress run), 26
`AMPLITUDE`-caused rejections were recorded. Every one that was not pure
noise (peak <60, duration <30ms — two instances) had a duration comfortably
clearing its floor by 45–70ms at every PWM tested (42, 61, 84, 90, 118).
Duration did zero discriminating work; amplitude alone decided every real
case. This confirms the code's own finding from the original 7,238-excursion
offline capture, now against live field traffic.

### 2.2 The rejected magnets were real, not noise

Two physical zones produced the great majority of today's rejects: MM140–142
(CW and CCW, both directions) and MM69–71 (CCW). At MM140–142 specifically,
peaks clustered tightly at 108–146 across three separate passes at two
different PWM settings (90, 118) — far too consistent, repeatable, and
location-locked to be electrical noise. Physical inspection confirmed
position nominal; the magnets were genuinely weak, not misplaced.

### 2.3 The per-marker strength map already predicts this

`strengthPct[]` (measured 2026-08-20/21, cross-validated Otto/Toby at
r=0.938) gives MM140 = 69% of railway median, MM141 = 76%, MM142 = 76% —
the three weakest-adjacent markers on the entire 171-marker table, and the
same relative ordering (140 weakest, then 141, then 142) that today's field
peaks independently reproduce. Railway median peak runs ~190; MM141's
expected peak is therefore ~144. Today's field reads at MM141 (125–130)
missed that *specific* expectation by ~10–13%, while missing the *flat*
admission floor by 7–11%. The table already knew this marker would read low
before today's session ran.

### 2.4 Limited simulation: `durationAt()` is stronger than expected

Per operator direction, a deliberately limited simulation was run against
today's field data only — 2,038 confirmed marker crossings (508 CW, 1,530
CCW) with PWM > 0, covering all 171 markers.

Method: normalize each crossing's duration to a PWM-90 reference
(`ms × pwm / 90`), take the per-marker median. This is the direct analogue
of how `strengthPct[]` was constructed.

Results:

- **Per-marker consistency is high.** Median coefficient of variation across
  169 markers with ≥8 samples is **0.047**; 90th percentile is 0.170. The
  measure is stable per marker, which is the prerequisite for using it as an
  expected value at all.
- **Railway-wide spread:** p5 = 129ms, p50 = 149ms, p95 = 193ms
  (PWM-90-normalized).
- **Every marginal reject today corroborates.** All 14 real
  amplitude-rejects in the MM140–142 zone fall within ±17% of their marker's
  expected normalized duration (ratios 0.83–1.03, **14 match / 0 miss** at a
  ±30% tolerance). These are precisely the events the flat amplitude floor
  discarded.
- **Noise does not corroborate.** The two genuine noise events today
  normalize to 21ms and 13ms against a ~145ms expectation — ratios of 0.14
  and 0.09, off by an order of magnitude. Duration corroboration separates
  them cleanly from the weak-but-real population.

This is a limited result on one locomotive over one day and is not a
substitute for the multi-session, cross-locomotive validation
`strengthPct[]` received. It is sufficient evidence to proceed to field
test, per operator decision. It is not sufficient to claim final accuracy.

### 2.5 Timing prediction already exists and is unused at admission

`spacingMm[]` plus commanded PWM already produce `dt_expected` and a live
`timing_gate` state (`ACTIVE`/`RAMP`), published on every marker today. The
"how long until we expect to arrive" prediction Revision 3 asks for is not
new engineering — it is already computed, just not consulted at the point
where a candidate event is accepted or discarded.

### 2.6 IR corroboration is a genuinely independent witness

Amplitude, duration, timing, and PWM-derived speed all depend on the same
Hall sensor and the same motor-PWM chain — a systematic fault (electrical
interference, a mounting issue, a wiring fault) can degrade several of them
together. IR speed sensing on an unpowered wheel shares none of that chain.
The 86-byte IR/Hall fusion-interval packet format works. Two measurements
exist, and they differ by transmitter generation:

- **Earlier CW session (original TX):** 121 of 186 reports decoded, the
  shortfall being RF transport loss at the Pi receiver rather than anything
  the locomotive flagged.
- **Upgraded TX, overcast, 2026-08-27 (CODEX evaluation):** all 170
  interval reports delivered, sequences `1–170` complete with no gaps.

The upgraded result is delivery achieved by **retransmission, not link
quality**: 5,031 duplicate packets, every interval transmitted at least
eight times and roughly thirty times on average. Underlying per-packet loss
is therefore not established by this run — the redundancy hid it. Two
consequences follow, both design-relevant:

1. **The buffer is nearly full.** TX holds 192 interval summaries. With
   RX 1.0 unable to acknowledge, nothing is ever retired: 22 free entries
   remained at the end of the run. RX 1.1 (acknowledgement) must be flashed
   before another run, or TX begins evicting the oldest unacknowledged
   records after approximately 22 more intervals.
2. **Channel airtime is a coexistence concern.** ~30x redundancy per
   interval shares the ESP-NOW channel with Toby's own broadcast traffic.
   This has not been measured against the receiver-coexistence gate.

TX memory holds interval *summaries*, not raw waveform samples, and is
volatile — power cycle or reflash clears it. Today's intervals are safe
only because all 170 reached the Pi.

Optical performance under overcast was strong: 4,989 cumulative pulses,
4,962 assigned to recorded intervals, no ADC saturation, no missed samples,
31 late samples out of ~471,000, and optical span generally 895–1,551
against a gate of 32 — a very wide margin. This is an **overcast**
measurement; the bright-sunlight degradation documented in the IR daylight
test series remains the open case and is unaddressed by this run.

IR remains a corroborating vote that is sometimes unavailable, never a
required input — consistent with the omission-tolerant design governing
everything else here.

## 3. Governing principles (unchanged, restated for this revision)

### 3.1 Error asymmetry still governs

False negatives are recoverable — QUORUM's arbitration, `NO_QUORUM`
self-resolution, and dead reckoning already exist to absorb a missed
marker. False positives corrupt the coordinate system and have no
correction mechanism once admitted. Every change in this revision must make
it *easier* to confirm a real, expected magnet and *no easier* to admit an
unexpected one.

### 3.2 Direction certainty is not position certainty

Direction (`session_dir`) is operator-declared and does not drift. `navMm`
can drift even while direction stays correct — today's CW session
demonstrated this exactly: three consecutive correctly-rejected weak magnets
left the position counter three markers behind physical reality, with
direction never in doubt. A target profile built from a wrong `navMm` is not
"no information," it is *confidently wrong* information, and the design must
not let a match against a wrong hypothesis look identical to a match against
a right one.

### 3.3 Identity is conjunctive

A single marker's polarity, strength, duration, and timing signature is not
guaranteed unique across 171 markers — a strong composite match at one
marker can coincidentally resemble another. Confidence comes from the
conjunction of independent attributes, and — where a *correction* to held
position is at stake — from accumulated agreement across successive
markers rather than a single event. See §6 for the adopted standard.

### 3.4 Dead reckoning and QUORUM's recovery machinery are preserved

This revision changes what happens at the admission boundary. It does not
touch `NO_QUORUM` self-resolution, the arbitration scoring already in place,
or dead reckoning as the fallback when no target match is found. Those
mechanisms are why the system can recover from a missed magnet at all, and
nothing here proposes doing without them. They are also the backstop that
makes the lower confidence bar in §6 acceptable to trial.

### 3.5 No match is still the same conservative default as today

A signal that matches no plausible target profile gets exactly the treatment
an amplitude-floor reject gets today: no navigation afterlife, audit record
retained, position unchanged. The bias shifts from "did this clear a
universal bar" to "did this match something we were looking for," not away
from rejecting the unmatched case.

## 4. The attribute set

Given a target hypothesis (current `navMm`, `navDir`, and the next mapped
marker in that direction), the profile to match against consists of:

1. **Polarity** — `dnaAt(nextMm)`. Already implemented, currently the sole
   input to AGREE/DISAGREE.
2. **Strength** — `strengthAt(nextMm)` scaled by the locomotive's current
   trailing-median gain, refreshed by the pre-run calibration of §7.
   Already implemented, currently used only in §3Q quarantine, after
   admission.
3. **Morphology (amplitude + duration jointly)** — pairs the existing peak
   with a new per-marker `durationAt()` table, PWM-normalized to a 90
   reference, constructed as in §2.4. True waveform shape (rise/fall
   symmetry, plateau width) remains unavailable without sample retention,
   which the current 1.16R base does not have. Amplitude and duration
   jointly are the minimum viable morphology and are what this revision
   uses.
4. **Timing** — `spacingMm[nextMm]` and commanded PWM, already computed as
   `dt_expected`. New: feed it into the admission-time score rather than
   only the post-hoc `timing_gate` display.
5. **Sequence** — accumulated agreement across recent markers, contributing
   to the confidence score rather than acting as a separate gate.
6. **IR corroboration** — independent pulse count and interval from the IR
   test car, delivered over ESP-NOW. Raises or lowers the score; never gates
   alone, never required. See §5.

## 5. IR integration — adopted, via ESP-NOW from the IR test car

IR is integrated now rather than deferred. Operator rationale: it is the
necessary catalyst — the only witness independent of the Hall/PWM chain, and
therefore the only input that can break a tie the other attributes cannot.

Transport for this revision is ESP-NOW from the IR test car, using the
existing packet family. This is explicitly an interim arrangement: the
source is a test car, not production instrumentation, and the
receiver-coexistence work on the car side remains in progress. The design
consequence is that IR availability is variable by construction — bright
sun, RF loss, and car absence all remove it — which is why it contributes
to the score and never gates alone.

Transport loss is not treated as contradiction. A missing IR report reduces
the number of available votes; it never counts against a candidate.

**Blocking prerequisite:** RX 1.1 must be flashed before the next run. Under
RX 1.0 the transmitter cannot retire acknowledged records and reached 22 of
192 free entries in a single overcast run (§2.6). Without acknowledgement,
the next run silently evicts the oldest intervals — which converts a clean
"vote unavailable" into lost evidence, and does so without flagging it.

## 6. Correction authority — 67% confidence, on trial

**Adopted:** the correction threshold will be determined empirically through
live field testing, not by a strict sequence-run match. The initial value is
**67%** — an experimental starting point, not an approved safety boundary.
Results from operation on the single-train loop will be used to adjust it.

Rationale: QUORUM's existing recovery machinery (§3.4) is the backstop. If
a 67% bar admits a wrong correction, `NO_QUORUM` self-resolution and
arbitration remain in place to catch it — the same mechanisms that catch
today's failures. Starting lower and observing is preferred to starting at
the provably-unambiguous run length and never learning where the real
boundary sits.

Behavior:

- **Confirmation** (match against the currently-held target hypothesis):
  behaves as today — confirms and advances position by one, as an ordinary
  AGREE.
- **Correction** (resolving in favor of a different `navMm` than currently
  believed): requires combined confidence ≥ 67% across the available
  attributes.
- **Below the bar:** no navigation afterlife, exactly as an amplitude
  reject behaves today; audit record retained.

**This is an experimental starting point, not an approved safety
boundary.** 67% is not derived from anything; it is set to be observed and
adjusted on single-train loop results. The specific scoring formula —
how the six attributes of §4 combine into one percentage, and how absent
attributes (missing IR, unusable position) are handled without penalizing a
candidate — is the remaining implementation question and is not settled
here.

## 7. Pre-run calibration — adopted

**Adopted per operator:** magnet strengths are dynamic day to day
(temperature, moisture, ballast state, sensor drift), so a calibration run
before automatic operations is warranted.

This is consistent with what the code already documents: `strengthPct[]`
supplies the per-marker *shape* while the trailing median supplies the
locomotive's current *gain*, and the table's own revision note says it "goes
stale like any other" evidence and should be rebuilt when magnets are moved,
replaced, or reseated. Today's MM140–142 repair and the pending disk-magnet
swap are exactly such events.

Scope of a calibration run: one full lap under manual or known-good
conditions, capturing the `mm/marker` stream, from which the current gain
and — where drift warrants — refreshed `strengthAt()` / `durationAt()`
expectations are derived before automatic operation begins. Whether
calibration rewrites the stored tables or only establishes the session gain
is an open implementation choice.

## 8. Honest limits

- **Morphology is amplitude + duration only, by explicit scope decision.**
  True waveform shape is unavailable without retaining raw samples per
  event, which the current 1.16R base does not do. This revision does not
  propose porting the trace overlay.
- **`durationAt()` is validated on one locomotive over one day.** §2.4's
  result is strong (14/14 marginal corroborations, clean noise separation,
  CV 0.047) but it is Toby-only, 2026-08-27-only, and built from the same
  session it was tested against. It has not received the cross-locomotive,
  multi-session validation `strengthPct[]` had. Field test is the next step
  by operator decision; final accuracy is unestablished.
- **The correction threshold is empirical and unset.** 67% is an
  experimental starting point, not an approved safety boundary; the operating
  value comes from single-train loop field results. See §6.
- **The scoring formula does not exist yet.** How six attributes combine
  into one percentage, and how missing attributes are handled, is unsettled.
- **IR integration is via a test car over ESP-NOW.** The upgraded TX
  delivered 170/170 intervals under overcast, but by ~30x retransmission,
  so true link reliability is unmeasured; the earlier TX lost 35% of
  reports. Bright-sunlight degradation is documented and unaddressed by any
  run to date. RX 1.1 is a blocking prerequisite (§5). Interim by design.
- **No claim of improved precision is made.** Everything here addresses
  recall — recovering weak-but-real magnets currently discarded. Nothing has
  been tested against Otto's contaminated captures, where precision, not
  recall, is the open risk.

## 9. Remaining implementation questions

1. The scoring formula: attribute weights, and the handling of absent
   attributes so that a missing vote never counts as a negative one.
2. Whether the pre-run calibration (§7) rewrites stored tables or only
   establishes session gain.
3. Whether `durationAt()` ships as a static table (as `strengthPct[]` does)
   or is rebuilt each session from calibration.
