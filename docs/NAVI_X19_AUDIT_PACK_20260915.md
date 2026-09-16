# X19 audit pack — what to check, and where I would look for my own mistakes

**For:** the operator's audit of X19 against X18, before any flash.
**X18:** `firmware/test-programs/NAVI_ONE` at **242109e** — the build as flown
2026-09-15, now committed unchanged, defects and all.
**X19:** `firmware/test-programs/NAVI_ONE_X19` at **227c330**.
**Complete diff:** `docs/NAVI_X19_VS_X18_20260915.diff` (1,021 lines, +480/−207).

This is not a summary of `docs/NAVI_X19_IMPLEMENTATION_20260915.md`. It is the
audit itself: every claim with the command that checks it, then the five places
I think are most likely to be wrong.

---

## 1. Reproduce everything

```bash
cd firmware/test-programs
arduino-cli compile --fqbn esp32:esp32:esp32 NAVI_ONE        # X18: 970571 / 60044
arduino-cli compile --fqbn esp32:esp32:esp32 NAVI_ONE_X19    # X19: 966935 / 67692
./NAVI_ONE/tests/run_tests.sh          # 13 gates, 1,118 checks, 0 failures
./NAVI_ONE_X19/tests/run_tests.sh      # 10 gates + the Sept 15 replay
```

---

## 2. The claims, each with its check

| claim | how to check it yourself | result |
|---|---|---|
| navigation is untouched | `diff NAVI_ONE/Navigator.h NAVI_ONE_X19/Navigator.h` — and the same for `RouteMap.h`, `Stations.h`, `Ops.h`, `LocoConfig.h`, `LapBaselineController.h`, both `LL_LocoConfig_*.h` | **8 files, no output** |
| the recognizer is untouched | `diff <(grep -v '^\s*//' NAVI_ONE/MagnetRecognizer.h) <(grep -v '^\s*//' NAVI_ONE_X19/MagnetRecognizer.h)` | one line, a comment that wrapped |
| no closure survives | `grep -rniE "exitMargin\|exitHold\|quietSince\|floorMs\|closure" NAVI_ONE_X19/*.h NAVI_ONE_X19/*.ino` | only `widthFloorMs` (disabled), `"closure":"none"` in the boot record, and prose |
| PWM reaches no Hall decision | `grep -n "mayAdapt\|actualPwm" NAVI_ONE_X19/ExcursionDetector.h` | 3 lines: the `sample()` signature, the forward to `updateBaseline()`, and that function |
| the 82 ms floor is gone, not renamed | `grep -rn "NAVI_PASSAGE_FLOOR_MS" NAVI_ONE_X19/` | nothing. The constant is `NAVI_X18_PASSAGE_FLOOR_MS`, referenced by no code |
| 512 ms pre-roll is real | gate 7 asserts `preSamples == 512`, `sampleCount == 913` on a live record | passes |
| polarity uses the excursion | gate 6; and read `finish()` in `ExcursionDetector.h` — the sum runs `first..last`, not `0..n_` | passes |
| a held level cannot re-fire | gate 1 holds a 110-count step for **60 s** | one candidate |
| the wire format matches the decoder | firmware `sizeof(WavHeader)` vs the Python struct string | both 57 bytes |

---

## 3. The five places I would look for my own mistakes

**a. `localRef()` is the whole architecture, and it is nine lines.**
**This is where the defect was.** The operator put a counterexample to exactly
this paragraph and it failed — see
`docs/NAVI_X19_LOCALREF_CORRECTION_20260915.md`. The text below describes the
superseded implementation and is left standing, because the audit found what it
said to look for: the selection, not the arithmetic.

`ExcursionDetector.h`. It walks back over the trailing window and keeps the raw
sample with the smallest `|raw − baseline_|`. Two things to satisfy yourself
about: that it is a minimum of **distance from the reference** and not of signed
value — written the other way it is blind to one pole, which is the failure the
brief specifically warned against — and that the window walk is
`(i + RAW_RING − 1) % RAW_RING` from `head_`, i.e. genuinely the last N samples
and not off by one into the future. Gate 3 exercises the first. Nothing
exercises the second directly; I checked it by hand.

**b. The ring-buffer arithmetic in `finish()`.**
`detectHead_` is captured one past the detection sample; `start` is
`detectHead_ − 1 − pre`; `total` is `pre + 1 + post`. If any of that is off by
one, the pre-roll is misaligned and every field record is subtly wrong while
everything still *looks* fine. Gate 7's `sampleCount == 913` catches a length
error but not a shift. The honest check is to read it. The ring is 1,152 and
the furthest look-back is 912, so there is 240 samples of margin — deliberately,
but it means an overrun would corrupt silently rather than fail loudly.

**c. The recognizer mapping is a real semantic change dressed as plumbing.**
`p.openedAtMs = p.closedAtMs = detection instant`. That is the only honest
mapping when there is no close, but its consequence is that the 500 ms guard
can no longer refuse anything. If you think the guard was still doing work that
the refractory does not do, this is the line to argue with — `NAVI_ONE_X19.ino`,
in `hallTask()`, with the reasoning in the comment above it.

**d. I added a frame-reset refractory that X18 has no equivalent of.**
`reset()` now arms a full 500 ms refractory from the next sample. My reasoning:
X18 dropped its open passage here because a locomotive declared while parked in
a field emitted that field on drive-off; X19 has no open passage, but a
declaration made *halfway across a magnet* would otherwise let the second half
re-trigger under the new target. Gate 10 covers it. This is the one behavioural
addition that came from my judgement rather than from the replay, and it is the
one I would most want a second opinion on.

**e. The telemetry record was over budget and I only caught it by measuring.**
The first draft was 843 bytes worst case against a 704-byte transport — the
2026-08-29 failure exactly, where one field too many makes every field vanish.
It is now 665 with 39 to spare **and** a runtime guard that counts
`exc_oversize` rather than truncating. The bound is arithmetic over physically
limited field widths, not a measurement of real traffic. If a field can be wider
than I assumed, the guard catches it — but you will lose that record.

---

## 4. What the audit cannot settle, and neither can any replay

* **First detection is untested.** Every X18 waveform dump begins at the entry
  crossing with 12 ms of pre-roll, so the regression hands the detector a
  synthetic quiet prefix. Only the candidates *after* the first one mean
  anything. This is the gap the 512 ms pre-roll exists to close, and it closes
  on the first run, not before it.
* **Nothing exists between PWM 24 and 40**, all day, in any dataset.
* **No magnet was ever recorded on an opposite-sign field.** Gate 3 shows the
  arithmetic handles it. That is not the same as evidence.
* **The four unmatched candidates** in the regression are all in the 32×
  decimated 16-second hand-push, and are an artifact of replaying a decimated
  record with a zero-order hold. The 1 kHz data underneath was discarded on the
  locomotive and cannot be recovered.

---

## 5. The decision the audit is really about

X19 ships with **`widthFloorMs = 0`** — no duration floor at all, as instructed.

What September 15 says that will cost: 24 floor rejections in ~2.5 hours, peaks
70–129 against gain 175, **ratios 0.40–0.74, every one of them above the 0.34
amplitude floor**. Amplitude will not refuse them. Six happened while moving.
The regression flags 5 of 65 candidates below 82 ms; the sharpest,
`12:49:17.921`, has peak 91, ratio 0.52 and an excursion of **two samples** —
X19 accepts that and advances on it. A spurious advance means a polarity
mismatch at the next pole change and a one-strike withdrawal.

Against that: `13:20:55.277` was a **genuine** magnet starved to 76 ms by a
reference sitting 18 counts high, and it is what caused the 13:21 strike.
Measured from a local reference it recovers. That is the same change paying for
itself — SUPPORTED, not proven, because there is no waveform dump for it.

So the run may well end early on a short transient. The brief said that is
acceptable — the objective is to discover demonstrated failures — and the
instrumentation is built so that when it happens, `w_caliper_ms` and
`w_frac_ms` on the offending candidate will say exactly what the replacement
floor should be. **That is a better derivation of 0085 than any amount of
further replay.**

If you would rather not spend a run on it, the single change is
`detCfg.widthFloorMs` in `NAVI_ONE_X19.ino`; the replay indicates ~75 keeps
Otto's known 83 ms borderline magnet while refusing the 16 ms and 43 ms
transients. I have left it at 0.

---

## 6. Flashing

Not done, and not possible from here right now: no `/dev/cu.usbserial-*` is
present, so Otto is not connected to this machine. When he is:

```bash
arduino-cli compile --fqbn esp32:esp32:esp32 firmware/test-programs/NAVI_ONE_X19
arduino-cli upload -p /dev/cu.usbserial-0001 --fqbn esp32:esp32:esp32 \
  firmware/test-programs/NAVI_ONE_X19
```

Confirm on the serial banner before the run starts:

```
[BOOT] NAVI_ONE_1_0X19_NO_CLOSURE_FIELDTEST ...
[BOOT] X19 NO-CLOSURE FIELD TEST — not field-accepted NAVI_ONE 1.0.
[BOOT] RISK: decision 0085's 82 ms duration floor is NOT implemented.
[CAL]  primed at <value>, spread <n> counts over the 2 s window
```

A large prime spread means Otto was primed on or near a magnet. X19 will run
anyway — by design, since detection no longer depends on the reference being
right — but it is the first thing to check if the run misbehaves, and X18 never
reported it at all.
