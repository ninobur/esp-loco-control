# NAVI_ONE — replay of DETECT → WINDOW → CLASSIFY → 500 ms REFRACTORY → RE-ARM

**Date:** 2026-09-15
**Build:** `NAVI_ONE_1_0X18_LAP_BASELINE_FIELDTEST` (working tree), Otto 9950011,
entry 70 / exit 25 / floor 82, `fixedAfterPrime = true`.
**Data:** complete 2026-09-15 Otto telemetry **including the Pi tail** to the
clean shutdown at 13:50:10 (the archive stops at 13:48:34). 52 passages with
complete raw samples; 4,367 nav events; 24 floor rejections.
**Status:** Replay only. No firmware written, X18 unmodified, no X19, no
decision record proposed.

---

## Verdict in one paragraph

**The architecture is right and one step of it is wrong.** Closure is not
necessary: a fixed window reproduces peak and polarity exactly, the 500 ms
refractory is never violated by a legitimate next magnet, and the adversarial
low-shelf event recovers the real magnet cleanly. But **unconditional re-arm is
contradicted by the data**: at rest in a magnet's fringe field — which happens at
every station stop — it generates a duplicate event every 500 ms, up to **50
from a single dwell**. The fix is one bit, not a closure test: re-arm at
`max(detection + 500 ms, first sample below the ENTRY threshold)`. That keeps
every property you asked for — no exit margin, no local baseline, no
return-to-baseline question — and at cruise it is bit-identical to the
unconditional rule on all 52 passages.

---

## 1. Is Hall closure necessary at all? — **DEMONSTRATED: no**

Navigation reads three scalars from a passage: `openedAtMs`, `peakCounts`,
`polarity` (source-traced; `closedAtMs` is only the *next* passage's guard
anchor plus a display-only speed estimate). None of the three requires a
termination test. There is no sorting or morphology stage to preserve —
`TwoSided.h` is not included by the sketch and references a `Passage::stitchAt`
field that no longer exists.

## 2. Shortest defensible measurement window — **DEMONSTRATED, with a catch**

Peak and polarity from window *W* after detection, against the full passage:

```
   W      clean singles (n=21): peak differs / polarity differs   worst shortfall
  50 ms                8 / 0                                        28 counts
  75 ms                1 / 0                                         1 count
 100 ms                0 / 0                                         -
 150..500 ms           0 / 0                                         -
```

**100 ms is the shortest zero-error window at cruise.** But the window must
reach the peak at the *slowest* speed, and the minimum window that captures the
full peak, per passage:

```
  pwm 80+     n=26   24 of 26 need <= 84 ms   (two outliers: 284 ms, 2512 ms)
  pwm 40-79   n= 6   73, 157, 164, 246, 310 ms   (+ one 19,456 ms dwell)
```

**A 300 ms window would have been 10 ms too short for a PWM-42 passage.** On
this evidence the defensible choice is **400 ms**, not 300 — and note the whole
low-speed sample is n=5 and comes from boot 3's struck period (§10). A
speed-scaled window would be tighter, but a fixed one costs nothing
navigationally: the next magnet is ≥719 ms away, so deciding 300 ms later than
necessary changes nothing.

## 3. Genuine-magnet recognition and polarity — **DEMONSTRATED: preserved**

Zero peak differences and zero polarity differences at every window ≥100 ms
across all 21 clean single passages. Polarity is not marginal: the running
signed sum reaches 20× the largest single sample at 23–33 ms (decision 0064's
own safeguard), against final ratios of 60–144×.

## 4. Transient-flux rejection — **CONTRADICTED: unchanged, and it fails**

The fixed window does **not** improve transient rejection, because rejection is
done by the recognizer's amplitude test and that test cannot see this
population. The 3652 ms low shelf measures ratio **0.43 at every window length
tried** (100/150/200/300 ms) — inside the "real magnet" band of the 2026-08-28
survey (0.403–2.677) and well above the 0.34 floor.

The present system does not reject it either: it accepted the whole 3652 ms as
one MAGNET. So this is **not a regression** — but the hypothesis that a fixed
window helps with problem A is not supported. Transient flux remains unsolved.

Context worth keeping: with position known, the recognizer refused **4** passages
all day against 4,229 accepted. The two sustained low-amplitude shelves (25 s and
111 s, peaks 54 and 56) both occurred in boot 3 *after* it was already struck.
Transient flux was not an active failure mode on September 15.

## 5. Does unconditional re-arm duplicate? — **CONTRADICTED: yes, badly**

Replaying the entry detector at exactly `detection + 500 ms` on real samples:
**9 of 52 passages are still above the entry margin and fire again on the same
magnet.**

Simulating the full architecture over all 52 passages, window 150 ms:

```
  A  unconditional re-arm at +500 ms          183 events
  B  re-arm at max(+500 ms, first below entry) 71 events
```

**A and B are identical on every passage at PWM 80–91.** They differ only at
rest or under a broken reference:

```
  11:50:57.201  22,053 ms dwell   pwm 68   A: 50 events   B: 1
  11:29:43.146  16,004 ms         pwm 31   A: 34 events   B: 1
  11:12:30.608  28,158 ms         pwm  0   A: 13 events   B: 6
  11:18:10.395     672 ms         pwm 42   A:  2 events   B: 1
```

This is not an edge case. **Station dwells are routine AUTO behaviour** — the
logs show full 30 s dwells at PWM 0 at both Patio (11:02:56–11:03:26) and Bamboo
(11:04:36–11:05:06), and finding 10 already established that a locomotive comes
to rest inside a magnet's fringe field. Under unconditional re-arm each such
dwell emits a spurious advance every 500 ms with correct amplitude and correct
polarity, so nothing downstream can refuse them. Position runs away while the
locomotive is stationary.

**The minimal repair is one bit, and it is not a closure test.** Re-arm when the
refractory has expired *and* the signal has been below the **entry** threshold at
least once. There is no exit margin, no hysteresis gap, no local reference, and
no question about baselines — it is simply the definition of an edge-triggered
detector: a new rising edge cannot be seen until the signal has come down.
Because it reuses the entry threshold (70) rather than an exit margin (25), it
tolerates a reference error of 70 counts instead of 25 — which is the entire
benefit you were seeking.

*Alternative repair that never inspects Hall at all:* do not let the refractory
expire while `actualPwm <= NAVI_BASELINE_ADAPT_PWM`. This fixes every dwell case
(all the PWM ≤ 31 rows above) but not the two boot-5 rows at PWM 68–88, where the
prime was 112 counts wrong and ordinary track sat above the entry margin. That
is a broken-reference fault, not a re-arm fault.

## 6. Can a legitimate next magnet arrive inside 500 ms? — **DEMONSTRATED: no**

The refractory is anchored at detection, so the governing quantity is
**detection-to-detection** (= gap + previous duration), not the close-to-open gap
the present guard uses. *This is a real relaxation: the proposal permits a
re-read that the present guard refuses, by one passage duration (~145 ms
typical).*

```
  pwm 80+     n=4203   min  719 ms   p0.5  981 ms   p50 1231 ms
  pwm 60-79   n=  20   min 1695 ms
  pwm 40-59   n=   1   min 3581 ms

  ACROSS ALL 4,224 intervals of the day:  0 below 500 ms
```

Minimum margin 219 ms, and that tightest case (719 ms, 11:05:19) was itself in
the offset regime. **The 500 ms refractory never suppresses a real magnet.**

Distance in 500 ms, from measured speed (surveyed span ÷ measured interval):

```
  pwm 80+     median 245 mm/s  ->  122 mm      (marker spacing 280-355 mm, median 300)
  pwm 60-79   median 104 mm/s  ->   52 mm
  pwm 40-59          81 mm/s   ->   40 mm      n=1
```

The footprint is centimetres, so at cruise the magnet is well behind. At PWM 42,
40 mm is *comparable to* the footprint — which is exactly why the re-arm gate in
§5 matters rather than being belt-and-braces.

## 7. The 3652 ms low-shelf event — **DEMONSTRATED, sample by sample**

Detection opens on a shelf at |v| = 75, barely over the entry margin. Then:

```
  t=+  0..480 ms   |v| oscillates 33..77, never a magnet
  t=+ 500 ms       |v| = 34   -> BELOW entry: the detector re-arms cleanly
  t=+ 992 ms       |v| crosses 70 -- the real magnet
  t=+1104 ms       |v| = 206 peak
  a 300 ms window at +992 ms measures peak 223, ratio 1.25 -> correctly ACCEPTED
```

**The real magnet is recovered, not missed.** That is the direct answer to the
physical model you wanted tested: something triggered detection, it was not a
route magnet, the guard expired, and the actual magnet was then found.

The cost is that the shelf itself is **falsely accepted** (ratio 0.43), and the
full simulation yields **5 events where 2 real magnets exist** (at +992 and
+2152 ms). Adding "at least 82 ms above entry within the window" cuts it to 3
accepted — but see §9, that screen is not free.

## 8. The two September 15 lost magnets — **DEMONSTRATED, mixed**

Under B (window 150 ms, gated re-arm):

```
  12:06 merge, MM013 strike        -> 2 accepted (peaks 159, 222)   CORRECT -- strike prevented
  13:20:55 passage + floor reject  -> 1 accepted (peak 227)         CORRECT
  13:21:03 passage                 -> 1 accepted (peak 220)         CORRECT
  11:05 merge, MM150 strike        -> 3 accepted (204, 205, 83)     OVER-COUNTS BY ONE
```

The 11:05 extra is a tail fragment at peak 83 / ratio 0.47 — above the amplitude
floor, so amplitude cannot refuse it. Three of four known failure events come out
right; one trades an under-count for an over-count.

## 9. The 82 ms floor — **DEMONSTRATED that it does not transplant**

The floor currently measures time from crossing entry (70) to falling below exit
(25). **That quantity only exists if a closure test exists.** Remove closure and
there is nothing to apply 82 ms to.

The obvious substitute — "at least 82 ms above the entry threshold inside the
window" — **rejects 7 of 21 genuine magnets (a third of them)**:

```
  dur=153 above=20   dur=122 above=79   dur=108 above=67   dur=91 above=62
  dur=87  above=67   dur=102 above=70   dur=91  above=65
```

Time above entry for clean singles runs 20–158 ms (median 89). An 82 ms
threshold cuts straight through the population. **Do not transplant the number.**

What the floor actually protects against is **UNTESTED**: all 24 floor rejections
have peaks of 70–129, which mostly *exceed* the amplitude floor (≈61–65), so the
amplitude test would not catch them — but a floor rejection is deliberately not a
`Passage` (`HallCapture.h:73-81`), so **no waveform is ever published for one**
and none can be replayed. The screen it provides is real, its replacement cannot
be derived from this dataset, and decision 0085's value cannot be carried across.

## 10. Low-speed evidence — **largely UNTESTED**

```
  accepted passages by PWM:   >=80: 4204 (99.4%)   60-79: 21   40-59: 4   <40: 0
  passages with raw samples below PWM 80:  5  -- ALL from boot 3's struck period
```

Every low-speed waveform available comes from the regime the architecture is
meant to repair, so it cannot serve as a control. Station stops are routine and
fully represented in the *event* log (approach → zone → zero-ramp → 30 s dwell →
depart) but not in raw samples: the departure diagnostic is 10 Hz, too coarse for
a 150 ms passage, and covers 5 episodes in one boot.

**Do not extrapolate the window length or the re-arm behaviour from PWM 90 to
station speeds.** §2's 310 ms requirement already comes from a single PWM-42
passage; the tractive floor is PWM 24 and nothing that slow was recorded.

## 11. What remains for X18's lap baseline controller? — **SUPPORTED: nothing**

If termination no longer consults the reference, the reference's only remaining
job is *detection*, and detection is far more tolerant than closure was:

```
  closure fails once |E| > 25
  detection fails once |E| > A - 70:   median magnet A=174 -> 104
                                       p1 magnet     A=128 ->  58
                                       weakest credible A~102 -> 32
  observed clean |E| on the day: -53 .. +54
```

The controller was built to hold an error the new architecture mostly does not
care about. It saturated its ±2 cap on **13 of 23 laps (57%)**, requesting
−29…+19. And the drift is not predictable: clean-shadow p99 error is 22 counts at
30 s and 21 counts at 300 s — beyond a few seconds, history carries no
information. **Retire it rather than retune it.** (Reference-age measurements
from the previous report re-verified on the same clean-classified samples: 1 s →
p99 6, max 20; 2 s → p99 9; 120 s → p99 27, max 40.)

## 12. Smallest falsifying field test

A build identical to X18 in every navigation decision, publishing per passage:
where the fixed window would have put peak and polarity; the `|v|` at
detection+500 ms; and the time above entry inside the window. No behaviour
changes, so this is a measurement aperture, not a policy change.

**It must include station stops and a deliberately slow section**, because that
is the entire content of §10 and §5's failure. One session of the size already
achieved (3,436 advances) then answers, over thousands of passages:

- Does any genuine magnet need a window longer than 400 ms? *Falsifies §2.*
- Does `|v|` at +500 ms ever exceed entry while moving normally? *Falsifies §5's
  claim that A and B agree at cruise.*
- What is the distribution of time-above-entry, so the floor's replacement can be
  derived rather than guessed? *Unblocks §9.*

---

## Label summary

| | |
|---|---|
| **DEMONSTRATED** | Closure is not required for peak/polarity (§1, §3). 100 ms is the shortest zero-error window at cruise; 310 ms was needed at PWM 42 (§2). No legitimate magnet arrives inside 500 ms — 0 of 4,224 (§6). Unconditional re-arm duplicates at rest, up to 50 events per dwell (§5). The 3652 ms shelf recovers the real magnet (§7). 12:06 and 13:21 come out correct; 11:05 over-counts by one (§8). An 82 ms above-entry floor rejects a third of genuine magnets (§9). |
| **SUPPORTED, NOT PROVEN** | Entry-gated re-arm is sufficient — shown on 52 passages, not a season. The lap controller has no remaining job (§11). A 400 ms window covers all real speeds. |
| **UNTESTED** | Everything below PWM 40. Station-dwell behaviour in raw samples. What the 82 ms floor screens out — floor rejections publish no waveform (§9). Acquisition coverage: no continuous raw stream exists, so no replay here tests what the detector would have *opened* on. |
| **CONTRADICTED** | "At 500 ms, re-arm unconditionally, do not inspect the Hall value" (§5). "A fixed window improves transient-flux rejection" (§4). "The 82 ms floor has a drop-in equivalent" (§9). |

## Provenance

`david@192.168.68.142:~/NGR/telemetry/runs/9950011_20260915_*.log`, read-only;
the serial port was not touched. Boots segmented by `alert.uptime_ms` regression.
Waveforms decoded from `diag/waveform` base64 per `WaveformDump.h` (40-byte
header, `<BBBBBBBBHHHHHHffIII`). Detection index within a stored passage taken as
`n − dur/dec − 1`, verified against the 12-sample pre-roll at dec=1. The frozen
−51 shadow of 13:21 onward is treated as stale throughout, per the correction in
`docs/NAVI_BASELINE_AND_CLOSURE_NECESSITY_20260915.md` §7.
