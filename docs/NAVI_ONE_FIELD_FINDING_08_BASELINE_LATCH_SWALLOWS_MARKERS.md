# NAVI_ONE — Field finding 08

## A stale baseline latches a passage open, and every magnet crossed inside it is invisible

**Date:** 2026-08-31, 18:14 and 18:15
**Locomotive:** Toby (9950012), CW, AUTO
**Status:** Observed. Not a decision. Nothing here has been ratified.
**Significance:** explains the stops that caused the 0.5 rollback, and the
mechanism is **not** in the rolled-back change. It is present in 0.3, 0.4 and
0.5 alike, because the code involved is identical in all three.

---

## The observation

The dump at 18:14:10, in time order, millis since boot:

```
 8491236 ->  8503976   12740 ms  TOO_WEAK   pol=S peak=51
 8504013 ->  8504201     188 ms  MAGNET     pol=N peak=174      37 ms gap
 8504224 ->  8505474    1250 ms  TOO_SOON   pol=S peak=64       23 ms gap
 8505495 ->  8505611     116 ms  MAGNET     pol=N peak=152      21 ms gap
 8505620 ->  8509800    4180 ms  TOO_SOON   pol=S peak=281       9 ms gap
 8509820 ->  8509942     122 ms  MAGNET     pol=N peak=167      20 ms gap
```

The window spans 18,706 ms. **A passage was open for 18,596 ms of it — 99%.**

---

## The mechanism

`HallCapture::updateBaseline()` contains, deliberately and correctly for its
original purpose:

```cpp
if (open_) return;      // never sample the baseline inside a passage
```

The baseline is a rolling median of `MED` = 41 samples taken one per
`baselineMs` = 25 ms, so it needs **1,025 ms of passage-free time** to
re-reference itself. Across the window above it received **110 ms** — roughly
four samples of forty-one. The median could not move. The baseline was frozen.

That closes a loop:

1. A sustained offset above `entryMargin` (38 counts) opens a passage. A
   locomotive standing in a magnet's fringe field will do it; so will slow
   drift while it stands.
2. The passage cannot close. Closing requires the signal below `exitMargin`
   (25) for `exitHoldMs`, and a 45–60 count DC offset never gets there. The
   hysteresis that correctly prevents one magnet becoming several passages
   also prevents a DC offset from ever ending one.
3. While it is open the baseline cannot re-reference, so the offset that is
   holding the passage open is precisely what cannot be measured away.
4. **Every magnet crossed while it is latched is invisible.** `HallCapture`
   cannot open a passage while one is open, so a real crossing merely
   modulates the ongoing passage and never produces a marker event.
5. The navigator does not advance. It surfaces later as a
   `POLARITY_MISMATCH` strike, when the next passage that does escape is
   compared against a target several markers stale.

### It is a stale baseline, not a magnetic field

The 1,250 ms passage held **mean 49 counts, standard deviation 6.6**, while the
locomotive travelled a full 300 mm marker spacing. No magnetic field on this
railway is flat to ±6.6 counts over 300 mm. It is a DC offset.

### The swallowed magnets are visible in the recording

Inside the 4,180 ms latched passage, above its ~60-count plateau:

| into the passage | width | peak |
|---|---|---|
| +1,040 ms | ~96 ms | **285** |
| +2,048 ms | ~112 ms | **260** |
| +3,120 ms | ~112 ms | **264** |

Three magnets, evenly spaced at the prevailing ~1,040 ms cadence, at full and
entirely healthy amplitude. None of them produced a marker event. The navigator
was three markers behind by the time the next passage escaped, which is exactly
the `POLARITY_MISMATCH` that stopped the locomotive.

This also corrects a reading recorded earlier in the day: accepted peaks
appeared to have fallen from a median of 201 to 120–174, which was taken as
evidence that the sensor had moved. It had not. Those low figures are only the
fragments that escaped as their own passages; the real magnets, inside the
latch, were at 260–285.

---

## Confirmed on the bench, on 0.4's own code

Gate 8 (`tests/gate_baseline_latch.cpp`, new) drives a synthetic DC offset
through the real `HallCapture` at repo HEAD — that is, **0.4**. Twenty seconds of
running, one 150 ms 220-count magnet per second.

| offset | magnets crossed | passages closed | magnets seen | baseline |
|---|---|---|---|---|
| none | 20 | 20 | **20** | 1834, undisturbed |
| −50, magnets of **opposite** polarity | 20 | 39 | **20** | **1834 — frozen**, never re-references to 1784 |
| −50, magnets of **same** polarity | 20 | **0** | **0** | **1834 — frozen** |

The mechanism is therefore **not** in the withdrawn 0.5 change. It is in 0.4, and
in 0.3 before it.

### It is polarity-dependent, which is why one evening produced two signatures

An earlier draft of this record said the latch swallows markers. That is only
half right, and the half that is wrong matters:

- **Opposite polarity.** The magnet's field cancels the offset, carries the
  signal through the exit window, and the stalled passage closes. The magnet
  then gets its own passage. Observed as an offset passage plus a magnet passage
  for every marker — the pattern at 18:20 and 18:25, which produced rejections
  but no lag.
- **Same polarity.** The magnet deepens the offset. The passage never closes and
  the marker is invisible, because `HallCapture` cannot open a passage while one
  is open. The pattern at 18:14 and 18:15 — where the offset was South and
  MM121, MM122 and MM123 are three consecutive South markers.

### The threshold is exitMargin, not entryMargin

This record first assumed a latch needs an offset above `entryMargin` (38).
**It does not.** The magnet itself opens the passage; the offset then only has
to hold the signal above `exitMargin` (25) for it never to close. Gate 8 finds
the boundary exactly:

```
offset -24 -> 20/20 magnets seen   clean
offset -26 ->  0/20 magnets seen   LATCHED
```

**A sustained offset of 26 counts is enough** — about 12% of a typical 200-count
magnet, and a third below the figure this record first assumed. That is a far
easier condition to meet than originally stated.

---

## What five minutes of standing did NOT do

On 0.4, at MM033, Toby stood for five and a half minutes and was restarted:
27 advances, no rejections, no strike, peaks 169–264. The first passage
afterwards reported `gap_ms = 333485`, matching the stand exactly — meaning **no
passage opened at all while he stood.**

So standing is not by itself sufficient. The latch needs an offset to exist
first, and five minutes at that spot did not produce one. What produced the
offset on the evening of 2026-08-31 remains **unknown**. The candidates not yet
separated are the length of the stand (two hours, against five minutes), where
he stood, and the build.

---

## Relation to the 0.5 rollback

The three stops that caused the rollback all carry this signature. The code
involved — `updateBaseline`, the entry and exit thresholds, the open/close
logic — is **byte-identical** across 0.3, 0.4 and 0.5. Nothing in decision 0064
or 0065 touches it.

The two sessions differed in operating condition, not only in build:

- **0.5 session:** the locomotive was flashed, then **stood for over two hours**
  before running. The first latched passage was already 12,740 ms old when the
  first magnet appeared.
- **0.4 session:** deliberately placed at a known startup location, MM040–041,
  and started immediately. 70 advances, one `TOO_SOON`, no strike.

**The mechanism is now proven build-independent** — gate 8 reproduces every
symptom on 0.4's own `HallCapture`, including the polarity-dependent split
between swallowing and pairing. 0.5 cannot be the cause of a behaviour that 0.4
exhibits on the bench.

**What is still unexplained is the trigger.** The latch needs a sustained offset
of 26 counts or more to already exist. Five minutes standing on 0.4 did not
produce one. Two hours standing, on the build that failed, apparently did — but
the stand length, the location and the build were all different at once, so the
correlation with 0.5 remains unexplained rather than dismissed.

---

## A reboot clears it, and nothing else does — so four stops were one condition

The operator's reading, 2026-08-31: "the earlier circumstances may have been
alleviated by a simple re-boot. It is a rare event even though persistent
circumstances made it look like four."

Confirmed on the bench. The baseline is learned at boot: `primeMs` is 2,000 ms
with no passage open, so priming adopts whatever DC level is present as the new
zero. Identical 20-second runs, the only difference being whether the boot
happened after the offset appeared:

| | baseline after 20 s | magnets seen |
|---|---|---|
| offset appeared after boot, no reboot since | 1834 | **0 / 20 — latched** |
| rebooted with the offset present | **1784** | **20 / 20 — clean** |

A latch can never clear itself: the passage stays open, so the baseline can never
sample, so the offset is never measured away. But a reboot re-references the
sensor onto the offset and there is nothing left to latch. Gate 8 case E.

**Two consequences.**

The four stops of 18:14, 18:15, 18:20 and 18:25 were **one persistent condition**,
not four events. That is why they looked like a deteriorating fault.

And the flash at 18:36 that appeared to fix them **fixed them by rebooting**. The
rollback to 0.4 and the reboot were the same action, and this record's earlier
drafts never separated them. On the evidence, 0.5 was never implicated at all:
gate 8 returns results identical line for line on both builds, and the apparent
cure was the reboot. A second field trial of 0.5 at 19:03 ran 164 advances with
no strike.

**Operationally, until this is addressed:** if the railway starts swallowing
markers, reboot the locomotive. Nothing else recovers it.

---

## A prior record that may need re-reading

`HallCapture.h:17-19` records that there is deliberately no duration ceiling,
because "a passage spanning a station dwell legitimately lasted 18,707 ms on
2026-08-29 (decision 0057)."

A locomotive standing still, producing a passage that never closes, is exactly
what happened tonight. Whether that 2026-08-29 passage was a legitimate dwell or
an unrecognised instance of this latch is worth establishing, because the
absence of a duration ceiling is what allows the latch to persist indefinitely.
Not asserted here — flagged.

---

## Not proposed

No remedy is proposed and nothing has been changed. Candidate directions exist —
allowing the baseline to re-reference after a passage has been open beyond any
physically plausible duration, or treating a passage that outlives one as
something other than a magnet — and every one of them touches the deliberate
reasoning of decision 0057. That is an operator decision, and it should be made
knowing that a station dwell and this latch may be the same signal.

## References

- `firmware/test-programs/NAVI_ONE/HallCapture.h` — `updateBaseline()`,
  `entryMargin` 38, `exitMargin` 25, `MED` 41, `baselineMs` 25
- `docs/decisions/0057-*` — the removal of the duration ceiling
- `docs/decisions/0065-*` — withdrawn; this finding is what the field showed
- `~/ngr-telemetry/waveforms/waveform_20260831T181410_915_slot*.csv`
