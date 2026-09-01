# NAVI_ONE — Field finding 09

## The baseline latch, caught whole: a 9-minute passage, and why a declaration cannot clear it

**Date:** 2026-09-01
**Locomotive:** Toby (9950012)
**Firmware actually running:** `NAVI_ONE_0_5` — see "The version" below
**Boot:** ~15:14:01, never restarted; the strike is at uptime 4,367,636 ms
**Status:** Observed and decoded. Not a decision. Nothing here has been ratified.

---

## The version, first, because it changes what this record is about

The `state/bootid` message reads:

```
{"sketch":"NAVI_ONE_0_5","loco":"9950012","entry":38,"exit":25,"floor_ms":40,
 "amp_floor":0.34,"resid_ceil":0.13,"guard_ms":200,"seq_n":10, ...}
```

**Toby is running 0.5.** The boot that produced this capture began at ~15:14:01
and has not restarted since; every `bootid` line after it is an MQTT reconnect
re-publishing the retained message, not a new boot. No `state/station` topic
appears anywhere in today's log, and 0.6 publishes one.

So this is **not** a test of the station machine, and the 30 s dwell has not yet
been exercised. It is something better: the latch of finding 08 arriving on its
own, with the six-passage window pointed straight at it.

---

## What the operator saw

Toby sat through a connectivity delay with no input. Once that was sorted he was
declared at 040-041 CW and started. One marker in, the dashboard showed MM41
twice and the train stopped.

---

## What the recorder holds

Six slots, decoded with `tools/waveform_b64_to_csv.py`. Timestamps below are the
header's own `openedAtMs`/`closedAtMs`, converted against the uptime anchor in
the 16:26:32.972 alert (uptime 4,351,969 ms). Every one lands within ~5 ms of
its published `mm/marker` line, so the conversion is sound.

| slot | opened | open for | pol | outcome | peak | ratio | resid | mean | s.d. |
|-----:|--------|---------:|:---:|---------|-----:|------:|------:|-----:|-----:|
| 5 | 15:14:03.3 | 1.040 s | N | WRONG_SHAPE | 84 | 0.442 | 0.2108 | 35.9 | 81.3 |
| 4 | 15:14:04.4 | **554.998 s** | N | WRONG_SHAPE | 82 | 0.432 | 0.2765 | 75.6 | 7.9 |
| 3 | 16:26:33.0 | 12.717 s | N | WRONG_SHAPE | 72 | 0.379 | 0.2726 | 68.4 | 3.6 |
| 2 | 16:26:45.74 | 0.178 s | S | **MAGNET** | 139 | 0.732 | 0.1039 | 92.2 | 41.3 |
| 1 | 16:26:45.94 | 2.575 s | N | **TOO_SOON** | 247 | 1.300 | — | 73.1 | 32.5 |
| 0 | 16:26:48.53 | 0.109 s | S | MAGNET | 144 | 0.758 | 0.1043 | 85.3 | 50.5 |

### Slot 4 is the headline: one passage held open for 9 minutes 15 seconds

554,998 ms. The capture decimated it 2048:1 to fit the ring. Its middle and last
thirds have a standard deviation of **3.8 and 3.7 counts** about a mean of +75.

That is not a magnet and it is not noise. It is a flat DC shift of about
+75 counts that the acquisition layer held open as a single passage for a
quarter of an hour's worth of railway.

Slot 3 is the same object, later and shorter: mean +68.4, s.d. **3.6**, held for
12.7 seconds. Its head is `64,67,69,69,69,63,69,70…` and its tail decays out
`72,70,60,56,31` — it opens flat, stays flat, and ends by fading. There is no
arc in it anywhere.

### The offset is large enough to *open* a passage, not merely to sustain one

Finding 08 established that a sustained offset ≥ `exitMargin` (25) holds an
already-open passage open. This is a stronger condition: **+68 to +75 counts
exceeds `entryMargin` (38) as well.** The offset does not need a magnet to get a
passage started. It starts one by itself, from a clean idle line.

---

## The chain that produced the strike

All six events, in order, with the gaps between them:

```
16:26:33.0   slot 3 opens  ....... flat +68 offset, no magnet in it
             (declaration cmd/start_interval 040-041 lands at 16:26:32.966)
16:26:45.7   slot 3 closes ....... 12.717 s -> WRONG_SHAPE (resid 0.2726)
        +27 ms
16:26:45.74  slot 2 opens  ....... the real MM041 South magnet
16:26:45.92  slot 2 closes ....... 178 ms, peak 139 -> MAGNET, ADVANCE 40 -> 41
        +27 ms
16:26:45.94  slot 1 opens  ....... flat +67 offset again
16:26:48.52  slot 1 closes ....... 2.575 s -> TOO_SOON (gap 27 ms vs guard 200)
        +9 ms
16:26:48.53  slot 0 opens  ....... the next real South magnet
16:26:48.64  slot 0 closes ....... 109 ms, peak 144, pol S
             navigator still expects N for MM042 -> POLARITY_MISMATCH -> STRIKE
```

That is "MM41 twice" exactly: it advanced to 41 once, then stayed at 41 for the
rejected passage and again for the mismatch that struck.

### MM042's magnet was captured, intact, and thrown away on a timing rule

Slot 1's peak sits at sample index 181 — with decimation 8, that is 1,448 ms
into the passage, i.e. 16:26:47.39. The samples around it:

```
98 116 132 154 180 205 225 240 247 245 258 245 238 228 206 183 160 136 112 90 80
```

A clean, symmetric arc, ~168 ms wide, riding on the +67 offset. **That is
MM042's North magnet.** It is a perfectly good crossing. The recognizer never
judged it, because the passage it happened to be inside had opened 27 ms after
the previous one closed, and the 200 ms guard rejects on the passage's opening
time. One flat offset therefore consumed a real marker whole.

---

## Why the declaration could not save it — the part that is new

`HallCapture::reset()` is called on every declaration and direction change. It
deliberately preserves the baseline:

```cpp
  void reset() {
    open_ = false; n_ = 0; ...
    sum_ = 0;
  }
```

with the comment that the baseline "is a property of the sensor, not of the
frame." That is the right instinct and it is why the latch survives a
declaration anyway. Look at what `reset()` actually achieves against an offset:

1. It sets `open_ = false`, silently dropping whatever passage was open. Nothing
   is published; that passage leaves no record at all.
2. `updateBaseline()` can now sample again — but it samples once per
   `baselineMs` = **25 ms**, into a **41-deep median**. Moving that median from
   the old level to the new one takes 21 fresh samples: **~525 ms of closed
   line.**
3. On the very next sample, 25 ms later, `delta = raw − baseline_` is still ~68,
   which exceeds `entryMargin` 38. A new passage opens.
4. `updateBaseline()` returns early while `open_`. The median never advances.

**The recovery needs 525 ms of quiet and is given 25.** It cannot win. This is
visible in the timing: the declaration landed at 16:26:32.966 and slot 3 opened
at 16:26:33.0 — one baseline interval later.

The practical consequence for the operator: **re-declaring and starting again
will not clear this.** The normal recovery gesture is a no-op against it.

---

## Corrections this capture forces

**To finding 08 — mine.** Finding 08 says "Only a reboot clears it." That is
wrong. Slot 4 closed on its own at 15:23:19 after 9m15s, and the line was then
quiet for the next 63 minutes with no passage closing at all. The offset went
away by itself and the baseline re-converged without a restart. The mechanism in
finding 08 stands; the "only a reboot" claim was too strong and is withdrawn.
What is true is narrower and worse: *nothing NAVI_ONE does* clears it — not a
declaration, not a direction change — so recovery waits on the offset itself.

One caveat on that 63-minute quiet: because `reset()` drops an open passage
without publishing, a passage open across that hour would leave no trace. What
the record supports is that **no passage closed** between 15:23:19 and 16:26:33.

**To CODEX's account.** The chain he gives for 16:26 is correct in every step,
including the reading that MM042's field was swallowed inside the TOO_SOON
passage. He stopped one slot short: slot 4's 9m15s passage is in the same dump
and is the most extreme instance in it. He also reads the offset as "beyond the
25-count latch boundary," which understates it — at 68–75 counts it is past the
38-count *entry* margin too, which is a different and stronger regime than the
one gate 8 reproduces.

---

## Ruled out

**Supply.** Across the whole window `telem/voltage` sits at 15.46–15.60 V,
`telem/current` at 0.16–0.22 A, `state/lowvolt` 0 throughout, including the
seconds either side of both offset passages. No sag, no transient.

**A parked magnet.** Toby was declared at MM040, whose expected polarity in this
frame is S. Both offsets are N. And a magnet under the sensor would give far
more than 68 counts and would end sharply on drive-off; slot 3 fades out instead.

**The station machine.** Not running. This is 0.5.

---

## Still unknown

The physical origin of the offset. It is intermittent — twice within 4 seconds of
boot, absent for 63 minutes, then twice more inside 20 seconds — flat to within
~4 counts while present, and always positive (N) in this capture. The 5-minute
0.4 stand on 2026-08-31 did not produce it, so sitting still is a permissive
condition and not a sufficient one.

## Carried forward

- The 0.6 flash has not reached Toby. The station machine is untested.
- A remedy for the latch still collides with decision 0057's removal of the
  duration ceiling. This record adds a second, independent handle on the problem
  that 0057 does not touch: `reset()` re-opening within one baseline interval.
- Findings 02/03 (MM110 resid 0.1422, MM146 0.1811) remain open.

## Artefacts

- `field-records/logs/20260901_navi_one_mm41_latch/` — the raw dump line and all
  six decoded CSVs, headers included.

---

# Addendum, same day — the baseline telemetry names the cause

The 1 Hz `alert` payload publishes `capture.baseline()`. Reading it across the
day settles what the waveform window could only imply.

## Every baseline change today

```
15:11:34 .. 15:13:31   1917 / 1918 / 1919   <- previous boot, healthy wander
                       ---- reboot at ~15:14:01 ----
15:14:02.493           1842
15:14:02.957           1751
15:14:03.957           1787   <- frozen here
16:26:45.977           1789   <- ONE count, in the 27 ms gap between slots 3 and 2
                                 still 1789 at 16:40 and counting
```

A healthy rolling median on this ADC wanders a count or two every few seconds;
the previous boot shows exactly that. **Since 15:14:03.957 it has moved by one
count in 86 minutes.** `updateBaseline()` has been returning early at
`if (open_) return;` essentially without interruption.

## Three things follow

**1. The latch is live right now, and has been for 86 minutes.** The frozen
baseline is direct evidence of a passage open under the sensor at this moment,
with `pwm` 0 and `nav_state` STRUCK.

**2. It resolves the caveat this record left open.** Finding 09 above noted that
a passage open across the 15:23→16:26 hour would leave no trace, because
`reset()` drops an open passage without publishing. The frozen baseline is that
trace. The line was *not* quiet for 63 minutes; it was latched the whole time,
and the passage was silently discarded by the declaration at 16:26:33.

**3. The trigger was a poisoned boot prime, not a drifting sensor.**
`primeMs` is 2000 ms and priming requires only that 2 s have passed and the
median holds ≥ 20 samples. It does **not** require the line to have been quiet.
The prime window here — 15:14:01 to 15:14:03 — overlaps slot 5, which opened at
15:14:03.3 and swings to −225 counts. The median settled at **1787**.

The resting level it should have found is recoverable from the recording:
slot 3's samples average +68.4 and slot 4's +75.6 against a baseline of 1787,
so the true idle level was about **1857–1863**. The previous boot sat at 1918.

**The baseline was primed ~70 counts low, which is above `entryMargin` (38).**
A passage therefore opened on the first sample after priming and could never
close, and because it never closed the baseline could never be corrected. The
"mysterious DC offset" is the priming error, seen from the wrong side.

## What this predicts for the operator's test

- **Does the latch persist?** It is persisting now. Nothing needs to be done to
  reproduce it.
- **Does a power cycle clear it?** It should — but only if the prime window is
  clean, and the evidence is that the last one was not. The test is therefore
  sharper than it looks: **read `baseline` in the first status message after the
  reboot.** Near ~1860 means a good prime. Another value ~70 low means the prime
  itself is the recurring fault and the reboot has merely re-armed it.
- **Order matters:** a flash reboots the ESP32. Power-cycling must be tested
  *before* flashing 0.6, or the test is spent.
