# NAVI_SIMPLIFIED — two findings against the locked per-interval baseline

**Date:** 2026-09-19
**Subject:** `firmware/programs/NAVI_SIMPLIFIED/SimpleHall.h` (untracked at
the time of writing) and the identical mechanism in
`firmware/programs/NAVI_ONE/variants/NAVI_ONE_SIMPLE/SimpleHall.h`
**Status:** Findings. Not a ruling, not a decision, not a proposal to change
the file. No source was modified by this analysis.

---

## Preamble: what the file already gets right

`SimpleHall` is the first implementation in the NAVI_ONE line of the
locked-per-interval reference model. It deserves to be recognised as such,
because the property is structural rather than tuned:

**`baseline_` has exactly two write sites in the class.**

```c
// SimpleHall.h:50  — prime, once, the median of 2000 boot samples
baseline_ = median(values_, count_);

// SimpleHall.h:147 — the only write after prime
baseline_ = out.candidate;
```

Between those writes the reference is a constant, and line 58 is its only
arithmetic reader. There is no rate-limited adaptation path, no rolling
median that runs regardless, and no `noteRest()`. Compare the X18/X19 path at
`NAVI_ONE/HallCapture.h:346`, which reaches an unscreened `med_[medHead_] =
raw;` past three *time* gates and no quality gate at all — the write that
produced the Northpoint latch and the Grillers shelf.

The state machine implements the interval cycle directly:

| step | code | line |
|---|---|---|
| recognise N | two consecutive same-signed samples ≥ 70 counts → `State::Open` | `:67` |
| get clear of N | `++underCount_ >= 30` below threshold | `:98` |
| only at cadence | `state_ = priorGapMs_ <= 3000 ? Guard : Idle` | `:102` |
| settle | `now - guardStartMs_ >= 80` → `Collect` | `:113` |
| establish for N→N+1 | 200 samples, median, gated on `!lowPwm_ && spread <= 32` | `:128–150` |
| lock | accept writes `baseline_`; **reject retains the previous value** | `:142–150` |

Line 102 is what makes this per *interval* rather than per closure: a new
reference is established only after a magnet that arrived within 3 s of the
previous one — that is, only while running normally between two mapped
markers. A station approach fails that gate and the existing lock is kept.

Both findings below are about the acceptance gate at `:142`. Neither is about
the locking architecture, which I think is correct.

---

## Finding 1 — `spread <= 32` tests flatness, not correctness

### The code

```c
// SimpleHall.h:131-150
if (count_ == 200) {
  int16_t lo = values_[0], hi = values_[0];
  for (uint16_t i = 1; i < count_; ++i) { ... }
  out.spread    = (uint16_t)((int32_t)hi - lo);
  out.candidate = median(values_, count_);
  if (lowPwm_ || out.spread > 32) {
    ++rejected_;
    out.event  = HallEvent::BaselineRejected;
    out.reject = lowPwm_ ? BaselineReject::LowPwm : BaselineReject::Spread;
  } else {
    baseline_ = out.candidate;   // installed wholesale
    ++accepted_;
    out.event = HallEvent::BaselineAccepted;
  }
```

`spread` is `max - min` over 200 consecutive samples. It answers one question:
**was the line quiet during those 200 ms?** It cannot answer the question the
acceptance actually depends on: **was the line at the true track level?**

### Why that distinction is the whole history of this program

A displaced magnetic shelf is *flat*. The measured example is in the X21 field
record of 2026-09-16: every clean `DWELL_BEGIN` that day sat near the quiet
line — Patio 1885/1893 and 1947/1945, Bamboo 1904/1903 and 1949/1945, Arches
1951/1951 — and **Grillers read 2163/2176 and 2141/2138 against a quiet line
of ~1950**. Two hundred milliseconds anywhere on that shelf produces a spread
of a few counts and a median about 200 counts wrong. `spread <= 32` passes it
without hesitation.

The same shape appears in field finding 13, where Toby held a **41-count**
fringe field flat for 36.7 s — fifths means 41/41/40/41 at standard deviations
of 2 to 6 — and in field finding 10, where the reference was captured by the
magnet the locomotive parked on and moved 1853 → 1899.

**A flatness test cannot distinguish a quiet track from a quiet magnet.** It
is the same category error that defeated five previous reference
architectures, surviving into the sixth in a narrower form.

### What is actually holding the line here, and how far it reaches

Three things keep this from firing in normal running, and it is worth being
exact about each, because two of them are weaker than they look:

1. **`priorGapMs_ <= 3000` (`:102`)** — collection only after a magnet at
   cadence. This is genuinely strong and excludes the station cases above:
   a Grillers dwell produces an enormous prior gap.
2. **`pwm <= 30 → lowPwm_` (`:114`, `:129`)** — a motion proxy. It excludes
   the zero-ramp and the dwell. It does **not** exclude a slow, powered crawl
   through a broad field at PWM 35–60, which is ordinary station-approach
   running on this railway.
3. **The 30-sample close plus 80 ms settle (`:98`, `:113`)** — proves the
   *electrical* excursion has ended. The project's own accepted design state
   says this in as many words (reconstruction item 4): *electrical quiet or
   return to baseline may rearm the detector but cannot prove spatial
   clearance*. Item 15 repeats it: *a quiet 200 ms Hall window does not prove
   spatial clearance*.

So the acceptance gate establishes a new locked reference on evidence the
design record explicitly says is not proof of the thing being claimed.

### The unguarded step

There is no comparison between `out.candidate` and the `baseline_` it
replaces. A candidate 200 counts from the current lock is installed by the
same line as a candidate 2 counts away. Once installed, the lock's own
virtue works against recovery: a wrong locked reference is **stable**, and
line 58 measures every subsequent magnet against it until some later
collection happens to succeed.

The X21 Grillers failure is exactly this arithmetic running the other way:
departure from a displaced reference back to the true line read as a
−190-count South excursion and was counted as a magnet.

### What this finding does not say

It does not say a leash is the right answer. A leash was evaluated in the
X18–X21 audit as candidate C and judged weak without measured long-session
drift evidence, because it assumes genuine drift stays inside the leash and it
adds another threshold. The measured drift in this repository —
`NAVI_BASELINE_DRIFT_PER_HOUR_20260914`, and the session-C3B93D0B analysis
showing a genuine ~33-count shift across one direction reversal — is the
evidence that would size one, and I have not done that work here.

The finding is narrower and I think unarguable: **the gate as written tests a
property that does not imply the property it is used to establish, and the
step it guards is unbounded.**

---

## Finding 2 — a retained baseline has no age, and nothing in the class knows how old it is

### The code

Every rejection path retains the previous reference:

```c
// :109  cadence failed  -> State::Idle, SlowCadence, no collection at all
// :118  pwm <= 30       -> BaselineRejected, LowPwm
// :145  spread > 32     -> BaselineRejected, Spread
// :69   a magnet interrupts the collection -> counted rejected, Open wins
```

In each case `baseline_` keeps its existing value and the class returns to
`Idle`. There is no timer, no counter of consecutive rejections, and no field
on `HallDecision` carrying the age of the reference in use. `accepted_` and
`rejected_` are lifetime totals; neither says when the last acceptance was.

### Why retention is the right behaviour and still not sufficient

Retention is the correct failure direction and should stay. The alternative —
installing a doubtful candidate — is the failure this architecture exists to
prevent, and the project's accepted design state requires a permissive
fallback rather than a manufactured claim (reconstruction item 8).

The gap is that retention is **unbounded and invisible**:

* Nothing bounds how long a lock may persist unrefreshed. The gates above are
  correlated with exactly the conditions that persist — a long station
  sequence is low-PWM *and* off-cadence for its whole duration, so a
  locomotive can complete an approach, a dwell and a departure without a
  single successful collection.
* Nothing reports it. A stale reference and a fresh one are indistinguishable
  on `diag/hall_decision`, which carries `baseline` but not its age. The
  operator, the dashboard and any later analysis see a number with no
  provenance.
* `resetFrame()` (`:36`) deliberately keeps the baseline across a declaration
  — correctly, since a declaration changes the navigation frame and not the
  physical line — so a re-declaration does not refresh it either. Field
  finding 09 is the precedent: a declaration could not clear a latched
  reference, and only a reboot did.

### The measured reason this matters

`hall-zero-age-not-value`, from the X18 Hall work: the Hall zero moves at most
about 6 counts per second and does not trend, so a ±25-count margin on the
*value* is fine — but a 40-minute-old reference is not, and no predictor
helps. The relevant quantity is the age, and the age is precisely what this
class does not track.

The drift figures support the same reading: the Otto session of 2026-09-13
drifted 1936 → 2021, about 85 counts over three hours, spread across thousands
of markers and never appearing as a step. That is harmless *if* the reference
is refreshed at cadence and cumulative if it is not.

### The narrow statement

**A locked reference is only as good as the freshness guarantee attached to
it, and this class provides none — neither a bound nor a report.** The first
of those is a design question. The second is not: the age of the operative
reference belongs on the diagnostic record whatever the eventual policy, and
it costs one `uint32_t`.

---

## Disposition

Both findings are against the acceptance gate, not the lock. The locking
architecture is, on the evidence of the whole NAVI_ONE line, the right move
and should not be reopened by either of these.

Neither finding is accompanied by a proposed fix, and neither authorises one.
Finding 1's remedy is entangled with the question the audit named as the real
one — *when is the track demonstrably clear enough to establish a level* —
which needs measured evidence from the X18 recorder rather than another
threshold. Finding 2's reporting half is cheap and separable from its policy
half.
