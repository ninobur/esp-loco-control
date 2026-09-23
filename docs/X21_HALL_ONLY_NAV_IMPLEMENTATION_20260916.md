# X21 — Hall-only navigation. Implementation report

**2026-09-16.** Subtractive redesign of the X20 Hall navigation path.
Sketch directory `firmware/programs/NAVI_ONE/variants/NAVI_ONE_X20/` (name retained; the
build identity on `state/bootid` is now `NAVI_ONE_1_0X21_HALL_ONLY_FIELDTEST`).
Base: commit `75cb888`, the only commit that directory has ever had.

**NOT FIELD ACCEPTED.** `FIELD_ACCEPTED 0`, `BUILD_CLASS
EXPERIMENTAL_FIELD_TEST`. X18 remains the build to fall back to.

---

## 1. The audit, before anything was changed

### The path from a >=70/2 opening to an advance, as X20 shipped it

| stage | where |
| --- | --- |
| `\|raw - L\| >= 70` for 2 consecutive samples | `ExcursionDetector::sample` |
| detection latched: `acquiring_`, `windowEndMs_ = now + 400`. **No polarity, no output.** | same |
| 400 ms later `finish()`: argmax over the whole window -> excursion at 34% of peak -> signed sum -> **polarity** | `ExcursionDetector::finish` |
| guard written: `refractoryUntil_ = detectedAtMs + 500` | `finish()`, i.e. only once the window completed |
| `Excursion` -> `Passage` -> `recognizer.examine()` -> `isMagnet` | `hallTask` |
| `Judged` queued -> `navigator.judge()` -> polarity vs `polarityAt(target)` -> advance or strike | `loop()` |

Navigation therefore advanced **400 ms after detection**, on a polarity taken
from the whole window.

### What had authority over navigation

* **the window-wide peak search** — chose where the excursion was, therefore
  the pole. *This is the MM136 mechanism.*
* **the signed integration over the excursion** — the pole itself.
* **`peakCounts` -> the recognizer's amplitude ratio -> `isMagnet`** —
  window-derived power to refuse an event outright (`Ruling::NotAMagnet`).
* **the 400 ms window** — delayed every advance.
* widths and `widthFloorMs` were telemetry only (`widthFloorMs = 0`).
  Morphology is not compiled into the production path.

### Where the guard began, and `EVENT_CLOSED`

**X20 has no `EVENT_CLOSED` at all** — closure was deleted in X19. The guard
was `detectedAtMs + 500`, but it was *written inside `finish()`*, so it did
not exist until the window completed. The recognizer's own 500 ms guard is
detect-to-detect under X19/X20 and structurally cannot refuse anything the
detector admitted.

### PWM 0

`stopped = stationHolding && actualPwm == 0` — **station stops only.** A
hand-parked or post-strike locomotive kept the full X19 path. `stopped` froze
rest, refused declaration, truncated an in-flight window, and cleared
`persistRun_`.

### REST / IN_OLD_FIELD

`beginDwell` classified against the rest measured *while moving*:
`\|raw - rest\| >= 70` -> `IN_OLD_FIELD`, else `REST`. `endDwell` armed
immediately on `REST`; on `IN_OLD_FIELD` it held declaration off until the
signal was back within 70 counts of the frozen rest for 2 samples. Never
executed on a locomotive.

**No material contradiction with the requested design was found.**

---

## 2. What changed

### `ExcursionDetector.h`

* new `struct Detection` — `detectedAtMs`, `rawAtDetect`, `localRef`,
  `departAtDetect`, `restRef`, `reference`, `shadowRef`, `polarity`,
  `guardUntilMs`. Drained by `takeDetection()` on the sample it happens.
* at the detection sample: `detPolarity_ = departure >= 0`, the guard is armed
  (`refractoryUntil_ = now + refractoryMs`), and the Detection is published.
* `finish()`: `pol` is now `detPolarity_`. The argmax, the 34% excursion, the
  signed sum and both widths are still computed and still published — the sum
  no longer decides anything.
* `peakSigned` now carries the sign the window **actually measured** at its
  argmax, so a record where the window disagrees with the opening says so.
* `finish()` no longer writes the guard.
* `refractoryMs` default 500 -> **645**.
* `reset()` drops a pending Detection with the frame.

### `NAVI_ONE_X20.ino`

* `detCfg.refractoryMs` 500 -> 645, with the derivation in the comment.
* `stopped = actualPwm == 0`. `stationHolding` removed entirely.
* `hallTask` queues `Judged` **at the detection sample**, with
  `polarity = d.polarity`, `isMagnet = 1`, `outcome = Magnet`, and the
  window-derived fields (`peak`, `peakSigned`, `ratio`, `gain`, `win_ms`,
  `w_caliper_ms`, `exc_n`) zero — they have not been measured yet, and a
  record reporting them would be reporting the previous candidate's.
  `gap_ms` is measured here, detect-to-detect.
* the `candidateReady` block is now telemetry only and queues nothing.
* `dumpWindowRequest` is held until `!detector.acquiring()`. `withdraw()` now
  fires ~400 ms before the offending record reaches `waveformWindow`; without
  this the strike dump would publish the five records *before* the one the
  operator was sent to look at.
* build identity, `BUILD_SUBTITLE`, and the file header rewritten.

### Untouched

`Navigator.h`, `MagnetRecognizer.h`, `RouteMap.h`, `Stations.h`, `Ops.h`,
`WaveformWindow.h`, `WaveformDump.h`, `LapBaselineController.h`,
`NAVIFieldConfig.h`, all `LocoConfig` files. Disagreement and strike behaviour
is exactly as it was.

---

## 3. The resulting Hall NAV state flow

```
  sample (1 kHz, GPIO 33)
    |
    +-- ramped PWM == 0 ?  --------------> DWELL. rest frozen, persistence run
    |                                      discarded, nothing declared.
    |                                      REST -> armed on departure
    |                                      IN_OLD_FIELD -> armed when the
    |                                        signal returns within 70 counts
    |                                        of the frozen rest for 2 samples
    |
    +-- inside the 645 ms guard ? -------> counted on suppressed_total, no event
    |
    +-- |raw - L| >= 70 for 2 samples ?
          |
          NO  -> nothing
          YES -> DETECTION.  t = this sample.
                 polarity = sign(raw - L).            <-- FINAL
                 guard armed to t + 645.              <-- FINAL
                 navigation event queued.             <-- COMPLETE
                   |
                   +-> Navigator: polarity vs polarityAt(target)
                         agree    -> advance one marker
                         disagree -> one strike, position withdrawn
                   |
                 ... 400 ms later, downstream and powerless ...
                   +-> window closes: peak, excursion, signed sum, widths,
                       recognizer verdict -> diag/excursion, waveform dumps
```

---

## 4. Verification

**Compile.** `arduino-cli compile --fqbn esp32:esp32:esp32`, esp32 core
3.3.11, real libraries. Clean, no warnings. 970007 bytes flash (74%), 71684
bytes RAM (21%). Boot record renders at 508 bytes against the 704-byte
transport (the oversize guard halts setup, so this was computed, not eyeballed).

**`tests/check_payload_bounds.py`** — 33 records checked, 0 over limit.

**`tests/gate_excursion.cpp`** — ALL GATES PASS, 0 failures. Gates 1–18 are
unchanged and still pass. Changed and added:

* **5** rewritten for 645 ms. A second arc at +340 ms and at +500 ms — the
  latter being exactly what the old 500 ms guard admitted — are both refused;
  +840 ms is a second candidate. It also states plainly that a magnet *still*
  departed when the guard expires is taken at expiry: 645 ms is a floor on the
  interval, not a window the magnet must fit inside.
* **5b** (new) the guard begins at detection and expires 645 ms later,
  to the millisecond: `guardUntilMs == detectedAtMs + 645`, and the detector is
  already guarding 400 ms before the window closes. The window closing does
  not move the expiry.
* **5c** (new) a window cut short at PWM 0 leaves the expiry at
  detection + 645. *(X20/X21 have no `EVENT_CLOSED`; this is the closest
  analogue the architecture admits.)*
* **6** rewritten: polarity is the opening sign; the excursion is still
  measured.
* **6b** (new) **MM136 reconstructed** — a South magnet reaching -106,
  recovery through zero, then a +117 shelf holding to the end of the window.
  The pole published is **South**, the declaring departure is negative, and
  `peakSigned` is positive, i.e. the record shows the window disagreeing and
  losing.
* **6c** (new) the same shelf with no magnet before it is North, correctly —
  so 6b measures the ordering and not a blanket sign flip.
* **19** (new) the navigation event exists within 240 ms of the arc starting,
  while the window is still open; the window closes afterwards and agrees with
  the event it can no longer change.
* **20** (new) a 260-count arc at PWM 0 creates no navigation event and no
  candidate, and is counted; the identical arc while moving is one event.
* **21** (new) a persistence run straddling the transition to PWM 0 is
  discarded, not stitched.
* **22** (new) a magnet identified before stopping is not rediscovered:
  one event while moving, nothing across a 30 s stop, nothing on the way out
  of the field it stopped in, and the next magnet counted normally.

**`tests/replay_x20_20260915.cpp`** — PASS. 11/11 next-event targets found,
0 unmatched at dec<=4, 0 persistent-field extras, 33/33 normal passages
producing exactly one candidate.

Differences against the X20 baseline on the same corpus:

* four candidates detected 8–67 ms later, because the longer guard pushes a
  detection that was previously taken at the old expiry to the new one. All
  still inside the ±350 ms matching tolerance.
* **one record's polarity flipped, N -> S**, and the replay now reports this
  category explicitly: `window argmax disagrees with the opening sign: 1`.
  The record is `13:21:03.076`, `dec=4`: a single stored sample of -71 followed
  by a long positive shelf peaking at +229. **This is not evidence that X21 is
  right here.** The corpus is decimated, so the zero-order hold stretches that
  one conversion to 4 ms and manufactures the two-sample persistence the real
  1 kHz stream may never have contained; and the corpus is oriented, so its
  polarity column is relative to the record rather than an absolute N/S. It
  demonstrates the mechanism and arbitrates nothing. The arbitration is MM136's
  raw record and gate 6b.

### What was NOT verified

* **The slow-speed spatial ambiguity is not solved and replay does not claim
  it is.** 645 ms remains a time proxy.
* No field run. The `IN_OLD_FIELD` branch has still never executed on a
  locomotive.
* Station stopping positions have not been re-measured — see limitations.

---

## 5. Retained solely for telemetry

Everything below is measured, published, and powerless:

* the 400 ms acquisition window and the whole 912-sample record
  (`diag/waveform`);
* the window-wide argmax (`peak`, `peak_signed`), the 34%-of-peak excursion
  (`exc_sum`, `exc_n`, `exc_first`, `exc_last`), and both widths
  (`w_caliper_ms`, `w_frac_ms`) — all on `diag/excursion`;
* **`MagnetRecognizer` in full.** It still runs on every completed window and
  still publishes `outcome`, `ratio`, `gain`, `gap_ms`. `is_magnet 0` now
  means *"the amplitude screen would have refused this one"*, and it still
  triggers the raw waveform dump when it says so. It has no path to `navMm`.
* `sign(exc_sum)` and `peak_signed` against `sign(depart)` on
  `diag/excursion` is the field measurement of how often the window disagrees
  with the opening. On 2026-09-15 that was 2 records in 266.

---

## 6. Known limitations

1. **The amplitude screen no longer refuses anything.** It reads the
   window-wide peak, which is the authority X21 removes. A weak artifact that
   survives >=70 counts for two consecutive samples *and* matches the expected
   next polarity will now advance the map, where X19/X20 might have refused it
   as `TOO_WEAK` with no position consequence. Two-sample persistence and the
   645 ms guard are the only screens left. **This is the largest cost of the
   change.**
2. **645 ms is a time proxy for spatial separation**, imperfect at low enough
   speed. Not addressed here, on purpose.
3. **Every advance lands 400 ms earlier**, so a station ZERO_RAMP triggers
   400 ms sooner and the locomotive stops correspondingly short — roughly
   60 mm at a 150 mm/s approach. Station offsets were calibrated against the
   delayed trigger and need re-checking in the field. This is the one
   AUTO-visible behavioural change and it is unavoidable given requirement 3.
4. **`stopped` widened to PWM 0** puts REST / IN_OLD_FIELD on every stop,
   including MANUAL and post-strike ones. This covers the post-strike
   condition observed at MM136 (four candidates while parked, `suppressed`
   25 -> 558), but it exercises a branch that has never run on a locomotive.
5. **Rest is not established while parked at boot.** `restFrozen_` holds
   through any PWM-0 period, so `restRef_` only becomes valid after the
   locomotive first moves. Until then `localRef()` falls back to the window's
   own most-occupied level, which is reference-free by design. No new
   behaviour is introduced, but it is a state X20 did not reach at boot.
6. **The sketch directory is still named `NAVI_ONE_X20`.** Arduino requires
   the `.ino` basename to match, and renaming would have touched the test
   harness paths for no behavioural gain. The build names itself correctly on
   `state/bootid`.
