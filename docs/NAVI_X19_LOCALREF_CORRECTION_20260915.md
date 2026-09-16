# X19 — the reference had not actually left detection

**2026-09-15.** Correction to `NAVI_ONE_X19` following the operator's
challenge. Compiled, gated, replayed. **Still not flashed.**

> raw − L cancels the reference algebraically, but the current implementation
> uses `baseline_` to select L, so the reference has not actually disappeared
> from detection authority.

Correct, and the consequence is worse than a missed magnet.

---

## 1. What the shipped detector did

`localRef()` chose the trailing sample of smallest `|raw − baseline_|`. Probed
with the reference primed 112 counts away from the resting level and a single
140 ms magnet arriving on top of it — half the arc pulling the signal **toward**
the erroneous reference:

```
E = +112   rest = 2012          (baseline_ primed at 1900 and frozen)
amplitude        82    100    120    137    173    220    270
away        [+53 pk81 N] ...  correct throughout
toward      [+134 pk82 N] [+127 pk100 N] [+125 pk112 N] [+127 pk112 N]
            [+130 pk113 N] [+45 pk112 N] [+34 pk158 S]
                  ^^^^          the arc is over at +140
```

Five of the seven amplitudes — **82 through 173, which is the genuine range up
to its own median** — were detected **after the magnet had finished**, with a
peak clamped at `|E|` rather than the amplitude, and with **the polarity
inverted**. Every one of those is physically South and every one was reported
North.

At `E = ±45` the hole is different and no better: amplitudes 82 and 100 were
**missed outright**, and the rest detected late with peaks short by ~45.

**The mechanism.** As the arc moves toward the reference, every new sample is
nearer to it, so the selection walks down with the signal and the departure
stays at zero — the same behaviour that correctly makes a decaying tail
invisible, applied to an *arriving* magnet because the reference was wrong
about which way rest lay. Detection then happens on the trailing edge, against
a zero taken at the apex, so the recovery is measured as if it were the magnet
and the pole comes out backwards.

A wrong count is recoverable. An inverted pole strikes.

---

## 2. Two attempts, and why the first was not enough

**Attempt 1 — drop the anchor entirely.** Make `L` the most-occupied level of
the trailing window: reference-free, and the counterexample matrix went
completely clean. It also **broke four of the slow magnets of 2026-09-15**:

```
11:18:10.395  672 ms  [+1ms pk138 N]  [+665ms pk79 S]   <- spurious
11:18:27.238  564 ms  [+1ms pk141 N]  [+509ms pk98 S]   <- spurious
11:29:24.399  656 ms  [+1ms pk187 N]  [+521ms pk156 S]  <- spurious
11:30:58.071  431 ms  [+1ms pk130 N]  [+502ms pk83 S]   <- spurious
```

All four at PWM 42. Their decaying tails became second candidates **of the
opposite pole**, emitted the instant the refractory expired. Removing the
anchor removed the tail-following property along with it.

**Attempt 2 — keep the form, replace the anchor.** The original *shape* was
right: picking the trailing sample nearest a known resting level is exactly
what makes a tail invisible. Only the anchor was wrong. So `baseline_` is
replaced by `restRef_`, a resting level the detector measures for itself.

That still failed on the same four records, for a reason worth recording: at
PWM 42 an arc's shoulders sit inside a 70-count band for about **400 ms**, so
the magnet fills the 300 ms window, looks like a level, and **installs itself
as rest** — after which its own tail departs from it. A level therefore has to
outlast a magnet before it is believed:

> A level must hold the trailing window, continuously, for longer than
> `localWindowMs + refractoryMs` = **800 ms** — the detector's own blind period.
> Nothing shorter can be trusted, because anything shorter might be one magnet
> the detector is still in the middle of.

Derived from parameters already in use; no new number. At cruise the line
between two magnets is quiet for about 950 ms, so rest is still re-measured on
most intervals — and it need not be re-measured often, since the level it
tracks moves at 3.6–7.2 counts **per minute**.

**This is not a closure test.** It ends nothing, re-arms nothing, gates no
candidate, and no signal has to return anywhere for one to be admitted. The
event boundary is still `detect → 400 ms → 500 ms`, unconditional. It decides
one thing: what the next departure is measured from.

---

## 3. The minimal diff

```
ExcursionDetector.h   +169 / -6      ~45 lines of executable change
NAVI_ONE_X19.ino        +4 / -3      rest_ref added to diag/excursion,
                                     win_ms dropped for payload headroom
tests/gate_excursion.cpp +119 / -6   gates 11 and 12; two assertions corrected
```

The executable change, in full:

1. **`localRef()`** — one line differs in substance. `anchor` is
   `restRef_` (measured) instead of `baseline_` (global); the trailing scan
   that follows is unchanged.
2. **`windowLevel()`** — new. The most-occupied level of the window and how
   much of the window it holds, from an incremental quantised histogram.
   Band width is `departCounts`: two levels less than one detection threshold
   apart are not distinguishable events, so this is not a new constant.
3. **`noteRest()`** — new. Applies the 800 ms persistence rule above.
4. **`histAdd()`** and two tables — new. `histN_[512]`, `histS_[512]`,
   maintained O(1) per sample in `sample()` beside the ring push.
5. **`Excursion::restRef`** — new field, published as `rest_ref`, so the field
   run can watch the measured rest and the global reference diverge.

**Cost:** flash 967,531 (+596). RAM 70,788 (+3,096, the two tables). The
`localRef()` scan is unchanged; `windowLevel()` adds a 512-bin pass, a few
microseconds of a 1,000 µs tick.

**`baseline_` now appears in no reachable detection path.** The two remaining
mentions inside `localRef()`/`windowLevel()` are `filled_ == 0` fallbacks, and
`sample()` pushes to the ring before it asks — both are marked unreachable.
Everything else that reads it is telemetry.

---

## 4. Results

**The counterexample, now a permanent gate (11).** 70 cells: offsets
{+112, −112, +45, −45, 0} × amplitudes {82, 100, 120, 137, 173, 220, 270} ×
{toward, away}. Every cell must give exactly one candidate, detected before the
apex, with the polarity of its own excursion and a peak within 12 counts of the
true amplitude. **All 70 pass**, and the table is symmetric in the sign of the
offset and in the direction of the arc — which is the real evidence that the
reference has gone.

**September 15 replay, unchanged verdict:**

```
next-event targets found      : 11 / 11
unmatched, records dec<=4     :  0
persistent-field extra events :  0
normal passages, exactly one  : 33 / 33
PASS
```

**All twelve gates pass.** Two assertions in the existing set were corrected,
and both were over-specified rather than wrong about behaviour:

* gate 7 asserted the quiet pre-roll reads exactly `0`. `L` is now the trailing
  sample nearest a *measured* level, so it reads zero to within the noise of
  that level; the assertion is now one bin.
* gate 1 asserted a 110-count step yields one candidate at 300 ms. A candidate
  is emitted when its 400 ms window closes, not when it is detected, so 300 ms
  measured nothing. It is now 600 ms.

---

## 5. What the correction costs, measured

Gate 1's third assertion is new and it is a genuine behaviour change: after a
110-count step, **60 seconds at the new level add nothing** — persistence still
never repeats, which is the property that was asked for — but an
**instantaneous return to the old level is now a second candidate.** It has to
be. After 800 ms at 2010 the detector has *measured* 2010 to be rest, and it
has nothing left that could tell it 1900 is more entitled to be home. That is
the price of taking the reference out of the event path.

Gate 12 measures the price rather than asserting it away. Candidates produced
by a displaced level returning to the original:

```
        ramp:     1    50   100   150   200   300   400   600  1000  2000 ms
  step   30:      0     0     0     0     0     0     0     0     0     0
  step   51:      0     0     0     0     0     0     0     0     0     0
  step   70:      1     0     0     0     0     0     0     0     0     0
  step   90:      1     1     1     1     0     0     0     0     0     0
  step  110:      1     1     1     1     1     1     0     0     0     0
  step  150:      1     1     1     1     1     1     1     0     0     0
```

**The largest resting-level excursion ever recorded on this railway is 51
counts** — 2026-09-15, high state 1964–1987, low state 1936–1959. Nothing of
that size fires at any rate whatsoever. The zero shift cannot reach this, and
the field record's "transitions inside a single 1 Hz sample" are 20–30 counts.

What *could* reach it is the fringe-field departure: a ~90-count field decaying
as the locomotive accelerates away. Absorbed if the decay takes longer than
about 200 ms, a candidate if faster. **That case is untested — no dataset
contains it** — and it is already field-test target 4. It is now the sharpest
reason to run that target deliberately, and `rest_ref` on `diag/excursion` is
what will show it.

---

## 6. Standing

Unchanged: `widthFloorMs = 0`, no closure, no PWM in any Hall decision, 512 ms
pre-roll, parameters 70 / 300 / 400 / 500 untouched. X18 untouched. Not flashed.
