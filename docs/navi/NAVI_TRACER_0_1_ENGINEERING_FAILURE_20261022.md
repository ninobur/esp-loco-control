# NAVI_TRACER_0_1 engineering failure — 22 October 2026

**Status:** Research and documentation only.  No firmware, test, or governing
specification is changed by this report.

**Subject:** Otto (9950011), `NAVI_TRACER_0_1`, branch
`feature/navi-tracer-0-1`, field operation on 22 October 2026.

## Scope and evidence

This report reconstructs the PWM-dependent failures from three independent
evidence sources:

1. The 5 kHz `NativeHallWire` recording retained by the Pi in
   `/home/david/NGR/navi_sync/navi_sync_20261015_100403.nsr`.  The relevant
   22 October interval was extracted as native ADC samples and re-binned using
   the sketch's unchanged 20 ms rule.
2. The Pi MQTT/Serial logs, including `diag/tracer_event` and `state/nav`.
   The contemporaneous field summary is
   `docs/NAVI_TRACER_0_1_FIELD_RUN_20261022.md` (field evidence, not a
   decision record).
3. The recognizer as fielded: `firmware/test-programs/NAVI_TRACER_0_1/`
   from commit `fb8c3840`, plus its direct research predecessor at
   `6efc9522`.

The conclusion below is about the recognizer's response to the recorded Hall
signal.  It does not claim that a particular labelled MM in historical research
data is independently surveyed ground truth.

## Field result

At PWM 90–100, Otto tracked the bracketed run cleanly.  At PWM approximately
60, it repeatedly stopped advancing while Otto continued to move.

| Trial | PWM | Observed result |
|---|---:|---|
| 15:31:48 declaration at MM051 | about 60 | Last clean close MM045; MM044 then opened and never closed before suspension. |
| 15:35:14 declaration at MM014 | about 60 | Last clean close MM021; position subsequently lost. |
| 15:36:31 declaration at MM160 | about 100 | One-marker uncertainty only; otherwise clean. |
| 15:37:59 declaration at MM140 | about 60 | MM152 remained open for about 28 s, then the following target contradicted; position froze. |
| 15:39:29 declaration at MM110 | about 90 | Thirty consecutive closes through MM110; no gap. |
| Final deceleration | 75 to 60 | MM063 opened at the low end and stayed open through E-stop; Otto continued toward MM049. |

The first run supplies the same pattern: a PWM-60 MM31 candidate stayed open
for 182.78 s, followed by contradictions and a permanent open candidate.  The
same physical markers had closed in about 0.20–0.22 s at PWM 89.

These facts establish a speed-dependent recognizer failure.  They do not show
a source-level PWM threshold: `TargetAcquisition::observe()` contains no
PWM-60 gate.  It only treats literal `pwm == 0` as a non-navigation interval.

## Reconstructed mechanism

### What the recognizer actually consumes

`RvmTracing.h` makes each input to the recognizer the unfiltered mean of native
ADC samples in one timestamp-defined 20 ms block.  It emits `NaN` only for a
block with fewer than 50 reads or a serial discontinuity:

```cpp
static constexpr uint32_t BLOCK_US = 20000;
static constexpr uint16_t MIN_READS = 50;
b.hall = (n_ >= MIN_READS && !bad_) ? float(sum_ / n_) : NAN;
```

See `firmware/test-programs/NAVI_TRACER_0_1/RvmTracing.h:22-55`.
The replay used this same rule, then passed the resulting bins through the
unchanged C++ `TargetAcquisition` implementation.

### MM045 to MM044 at PWM about 60

The raw-ADC replay reproduces the field sequence without changing a threshold:

| Event | Replay time | Result |
|---|---:|---|
| MM048 close | 160.49 s | Candidate duration 63.16 s. |
| MM045 opens / closes | 168.11 s / 169.61 s | Candidate duration 1.50 s. |
| MM044 opens | 170.37 s | Opens with the opposite sign to the map target and remains open in the observed trial. |

Only 0.76 s separates the MM045 close and the MM044 opening.  Closure seeds the
next baseline with the final six samples, invalidates the old fit, then starts
collecting a replacement fit:

```cpp
baselineHistCount_ = c.tailCount;
// copy the final six closing samples
baselineFitValid_ = false;
candidateOpen_ = false;
```

`NaviTracerCore.h:338-347`

The new fit needs eight samples spanning 0.65 s, but the collection path admits
every finite, PWM-nonzero sample without testing that it is ordinary track:

```cpp
if (!expected(t, &level, &noise)) {
  baselineHistT_[baselineHistCount_] = t;
  baselineHistHall_[baselineHistCount_] = hall;
  ++baselineHistCount_;
  return 0;
}
```

`NaviTracerCore.h:245-259`

The reconstructed MM044 fit has a slope of **+213.80 ADC/s**, noise 16.20,
and a fitted level of **2115.41 ADC** at candidate opening.  Ordinary nearby
background is approximately 1900 ADC.  The slow approach of the next magnetic
field was therefore learned as a rising background line before it crossed the
70-ADC opening criterion.

When the actual signal then descends, it is interpreted relative to that
poisoned reference as a negative departure.  The candidate consequently opens
with the wrong sign.  While open, the reference is evaluated at the *opening*
time, not at each current sample:

```cpp
if (!expected(c.start_s, &level, &noise)) { level = hall; noise = 1.0; }
const double departure = fabs(hall - level);
```

`NaviTracerCore.h:295-303`

Because the frozen value is already tens to hundreds of ADC counts away from
ordinary background, ordinary samples need not fall inside the closure band.
The candidate cannot accumulate the required six consecutive quiet bins and
therefore cannot close.

### Closure rule and its effect

The closure rule is:

```cpp
const bool stillActive = departure > fmax(HALL_DEPARTURE_ADC / 2.0,
                                          RTB_QUIET_SIGMA * noise);
if (stillActive) { c.last_active_t = t; c.quiet_run = 0; }
else { ++c.quiet_run; }
if (c.quiet_run >= RTB_QUIET_BINS) { /* close */ }
```

`NaviTracerCore.h:303-318`

At the nominal cadence this is six consecutive 20 ms observations, or 120 ms,
inside `max(35 ADC, 3 × fitted noise)`.  The rule is not a five-second
timeout, and there is no timeout in the implementation.  The 28 s, 63 s, and
182.78 s candidates prove that a real candidate can stay open much longer than
any proposed arbitrary lifetime while eventually producing six qualifying
observations.

The failure is therefore not merely “closure takes too long.”  A bad
post-close fit creates a reference from which a physically ordinary return may
never qualify as ordinary.

## Why PWM 75/90 succeeds and PWM 60 exposes the defect

The successful PWM-90 segment does not prove that elapsed time is a reliable
description of a magnet.  It shows that the following accidental assumptions
happened to remain true at that speed:

1. The interval after a close contained enough uncontaminated background for
   a usable fit.
2. The next magnet developed past the 70-ADC opening threshold before its
   shallow leading flank materially distorted that fit.
3. The frozen opening reference remained close enough to ordinary background
   that the later return generated six quiet bins.

At PWM about 60, the leading flank develops slowly enough to overlap the
unqualified 650 ms baseline-collection period.  The raw replay demonstrates
that this is sufficient to reverse the apparent sign at MM044.  This is a
specific mechanism, not an inference from PWM correlation alone.

The successful portion at PWM 75 is limited evidence: it demonstrates that
the failure is not a binary `PWM < 75` rule.  It does not establish a safe
speed boundary or validate the recognizer at all intermediate waveform shapes.

## Findings and limits

### Verified

- The raw recording, binned by the production rule and consumed by unchanged
  recognizer code, reproduces the MM048/MM045/MM044 progression and the
  erroneous MM044 polarity.
- The post-MM045 baseline fit includes the developing MM044 flank and has the
  recorded extreme positive slope.
- Candidate closure compares against the baseline evaluated at candidate
  opening, not against a newly recognized ordinary-track condition.
- The recognizer has no elapsed-time/PWM recovery path for an open candidate.
- No recorder, MQTT, or UDP queue mechanism supplies input to
  `TargetAcquisition`; loss in those paths cannot explain the reproduced
  waveform decision.

### Not established

- A universal PWM threshold, speed threshold, or magnetic-field width at
  which failure begins.
- Whether every slow-PWM stall has the same immediate baseline poisoning
  sequence as MM044.  MM152, MM31, and MM063 require their own waveform traces
  before claiming identical local causes.
- Whether the Hall sensor itself drifts with motor conditions in a way that
  contributes to some stalls.
- A replacement estimator, return-to-baseline test, or recovery policy.

No firmware modification follows from this report.
