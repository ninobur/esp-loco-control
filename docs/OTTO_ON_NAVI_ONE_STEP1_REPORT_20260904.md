# Otto on NAVI_ONE — step 1 blocked at the compiler, and what unblocks it

**Date:** 2026-09-04
**Branch:** agent/toby-1-13-flash
**Scope:** CTO restoration step 1 — Otto (9950011) on recorder 0.3, alone.
**Outcome: no image handed over.** Otto's profile has no measured NAVI_ONE
recognizer block, and three compile-time guards stop the build. Nothing was
changed in the repository. All builds below ran in a scratchpad copy with
`--build-path`; the operator's build directories were not touched.

---

## 1. What the operator asked for, and what actually happens

> *"Build recorder 0.3 for Otto's profile, confirm the boot line names 9950011,
> hand the operator the image."*

Flipping the selector in `LocoConfig.h` from Toby to Otto and compiling
`NAVI_ONE_STATION_CURVES` (0.3) produces four errors:

```
LocoConfig.h:66:  #error "This profile has no measured NAVI_ONE recognizer block. Run the
                          survey on THIS locomotive and record the values in its profile;
                          do not copy another's."
LocoConfig.h:69:  #error "The NAVI_ONE recognizer thresholds in this profile were measured
                          on a DIFFERENT locomotive."
Stations.h:180:   #error "NAVI_APPROACH_MARKER_MS must be defined in this locomotive's
                          config header"
NAVI_ONE_STATION_CURVES.ino:68:
                  #error "This locomotive's profile has no NAVI_BASELINE_ADAPT_PWM.
                          Measure the tractive floor from its own PWM/speed fit; do not
                          copy another locomotive's."
```

The control build, same tree, Toby selected, compiles clean —
984,911 bytes flash (75%), 64,588 bytes RAM (19%). The toolchain is fine
(arduino-cli 1.5.1, esp32 core 3.3.11). This is not an environment problem.

**These guards are not an obstacle to work around. They are the recorded
conclusion of the work that got Toby to 3,284 magnets with zero false
positions,** and they exist to stop exactly the shortcut that would otherwise be
taken here. The rest of this report is about satisfying them, not evading them.

---

## 2. What Otto's profile differs in from Toby's

Comparing the two headers as the preprocessor sees them, in the
`NAVI_ONE_STATION_CURVES` tree.

### Identical — 33 defines
Motor pins (PWM 4, DIR 2), PWM channel/frequency/resolution, all six voltage
thresholds, both ramp delays, direction constants, `SAFE_DIRECTION_CHANGE_PWM`,
`NORMAL_PWM 110`, `CONSIST_EXTENT_FRONT/REAR_MARKERS` (2/4), Blynk template and
virtual pins, `HALL_DEADBAND_COUNTS 25`, `HALL_MIN_PEAK_DELTA 35`.

### Defined in both, different value — 5

| symbol | Otto | Toby | read by NAVI_ONE? |
|---|---|---|---|
| `LOCO_NAME` | `"9950011"` | `"9950012"` | yes — identity |
| `LOCO_ID` | `9950011UL` | `9950012UL` | yes — identity, MQTT topics |
| `BLYNK_AUTH_TOKEN` | `..._9950011` | `..._9950012` | yes |
| **`HALL_ENTRY_MARGIN_COUNTS`** | **45** | **13** | **yes — see §3** |
| `HALL_POLARITY_INVERTED` | `true` | `false` | **no** — dead symbol |

`HALL_ENTRY_MARGIN_COUNTS` is the only substantive difference between the two
profiles that NAVI_ONE actually consumes. Entry threshold is
`DEADBAND + ENTRY_MARGIN`: **Otto enters at 70 counts, Toby at 38.**

`HALL_POLARITY_INVERTED` is read by nothing, as `docs/CLAUDE.md` records.
Confirmed again here: it appears in no NAVI_ONE source file.

### Defined for Otto only — 3, all dead in NAVI_ONE

`IR_TEST_A_ENABLED`, `IR_SENSOR_MAC_BYTES`, `Q_FLOOR_MS_OVERRIDE`. All three are
QUORUM symbols. None appears in any NAVI_ONE source file.

**This is a useful fact for step 2:** Otto's ESP-NOW IR pairing does *not* come
along for the ride when he moves to NAVI_ONE. The radio arrives deliberately at
step 2 or not at all. Likewise `Q_FLOOR_MS_OVERRIDE 500` — his QUORUM quarantine
floor — has no effect here; NAVI_ONE's time test is `NAVI_GUARD_MS`.

### Defined for Toby only — 10

`HALL_DOMINANCE_PERCENT` (dead everywhere; boot telemetry only), plus the nine
NAVI_ONE symbols Otto lacks. Those nine are §4.

---

## 3. The finding that matters most: on Otto the amplitude floor is probably inert

In recorder 0.3, after decision 0074, **exactly two recognizer constants can
refuse a passage** (`MagnetRecognizer.h:249,253`):

```
if (haveAccepted_ && gapMs < cfg_.guardMs)          -> TOO_SOON
if (amplitudeRatio < cfg_.amplitudeFloor)           -> TOO_WEAK
// THE PHYSICAL TESTS HAVE PASSED. From here the passage is a magnet.
```

Shape is computed and published but cannot refuse. `bootstrapGain` matters only
as the denominator of `amplitudeRatio` until eight peaks are in hand.

The amplitude floor is `peak / gain < 0.34`, where gain is the trailing median
accepted peak. A passage must first *open*, which requires peak ≥ the entry
threshold. So the floor can only bite when

```
entry_threshold  >  0.34 x gain     is FALSE,  i.e.  gain > entry / 0.34
```

- **Toby:** entry 38, so the floor bites above gain 112. His observed gain range
  is 169–278. The floor is live on Toby, always.
- **Otto:** entry 70, so the floor bites only above **gain 206**. His mean peak
  after the 2026-08-20 realignment was **176.4**.

**On Otto, at his measured signal strength, the amplitude floor and the entry
threshold are the same test done twice, and the entry threshold is the stronger
one.** Nothing that clears entry can fail the floor until his gain exceeds 206.

Two consequences, and they point in opposite directions:

1. **Reassuring.** Otto's admission under recorder 0.3 reduces to his own
   measured 2026-08-20 entry threshold, `HallCapture`'s duration floor,
   `NAVI_GUARD_MS`, and the Navigator's polarity and sequence. The un-measured
   amplitude constant has less influence on Otto than the compile guard implies.
2. **Not reassuring.** Otto's 70-count gate was chosen under QUORUM to reject a
   phantom population at peaks 40–91, because QUORUM's quorum had no polarity
   and sequence test to fall back on. NAVI_ONE does. That gate now costs
   sensitivity for a defence NAVI_ONE may no longer need — and his own profile
   warns, in capitals, that a genuine marker below the gate is silently missed.
   **A missed marker under NAVI_ONE is a lost position, which is precisely the
   failure step 1's acceptance test exists to detect.** Otto's profile records
   that 65 was already tried and reverted the same evening for exactly this.

I am not proposing to change the gate. I am flagging that if Otto's first
NAVI_ONE session shows lost position rather than false advance, the entry
threshold is the first suspect, and it is a QUORUM-era number, not a NAVI_ONE
one.

---

## 4. The nine missing values, and which of them can change Otto's behaviour

| # | symbol | Toby | effect under recorder 0.3 | can it come from Otto's archive? |
|---|---|---|---|---|
| 1 | `NAVI_RECOGNIZER_MEASURED_ON` | `9950012UL` | compile guard only | it is the attestation, not a measurement |
| 2 | `NAVI_GUARD_MS` | `200U` | **refuses TOO_SOON** | no — see §5 |
| 3 | `NAVI_AMPLITUDE_FLOOR` | `0.34f` | **refuses TOO_WEAK** (§3: likely inert on Otto) | no — see §5 |
| 4 | `NAVI_BOOTSTRAP_GAIN` | `190U` | **denominator of #3 for 8 passages** | no — see §5 |
| 5 | `NAVI_RESIDUAL_CEILING` | `0.13f` | **none — decision 0074, cannot refuse** | n/a, see below |
| 6 | `NAVI_AUTO_CRUISE_PWM` | `90` | **AUTO cruise speed** | policy, not measurement |
| 7 | `NAVI_APPROACH_MARKER_MS` | 5 values | **paces the station approach ramp** | no — see §5 |
| 8 | `NAVI_BASELINE_ADAPT_PWM` | `25` | **gates Hall baseline adaptation** | no — see §6 |
| 9 | `IR_FITTED` | `0` | pin 34 sampling; `0` is fully inert | operator declaration |

**#5 is genuinely free.** Decision 0074 made the residual diagnostic. It sets
`shape_outcome` and `would_shape_refuse` in `wave_meta` and nothing else. Giving
Otto `0.13f` transplants no behaviour whatsoever — it only makes his shape
diagnostics directly comparable with Toby's, which is what we want them for.
This is the one value where "copy Toby's" is not a shortcut but the correct
answer, and it should be labelled as a *diagnostic reference*, not a threshold.

A scratchpad compile probe with all nine supplied builds clean, names **9950011**
in the ELF, and reports the same 984,911 / 64,588 as Toby's image. **Nothing else
in Otto's profile is incompatible with NAVI_ONE.** The measured block is the only
blocker. (That probe was never written to the repository and is not flashable.)

---

## 5. Otto's archived calibration cannot supply the block

Otto has thirteen calibration files in `field-records/cal/`, 8,928 rows,
2026-06-29/30 — six times Toby's. I tried to fit the block from them. It does
not work, for three separate reasons.

**The peaks are stale.** June median peak is 253. `field-records/
20260820_OTTO_HALL_SENSOR_WEAK.md` records his mean at **146.7** in August,
rising to **176.4** after the operator realigned the sensor. Otto's sensor
changed twice between this data and today. Anything amplitude-derived — the
bootstrap gain, the floor — would be fitted to a sensor that no longer exists.

**The intervals contain his phantoms.** Shortest marker interval in the June
data is **83 ms**, with 1% below 346 ms, against Toby's shortest real interval of
876 ms. These are the spurious reads the 2026-08-20 record identifies at peaks
40–91, admitted as markers at the time. A guard fitted to this data would be
fitted to the fault.

**The approach throttles are not covered.** Of the five throttles
`NAVI_APPROACH_MARKER_MS` needs, PWM 87 has **one** sample, and PWM 69 and 63
return effectively the same marker time (2121 vs 2118 ms) — which cannot be
true of a monotonic speed/PWM relation and means those bins are contaminated by
grade and station approaches rather than measuring throttle.

Conclusion: **Otto has to be measured, on the railway, as he is now.**

---

## 6. An open issue found on the way, not blocking, not proposed for change

`NAVI_BASELINE_ADAPT_PWM 25` is documented in Toby's profile as *"TOBY'S
MEASURED FLOOR. From his own PWM/speed calibration: speed_mm_s = 3.990 x
(PWM - 25.1)"*, and decision 0066 cites that fit as *"fitted to 4,617 samples"*.

I could not reproduce it from either locomotive's calibration files:

| method | Toby's files | Otto's files |
|---|---|---|
| OLS on all MOVING rows | `2.471 x (PWM - 1.5)`, n=1,469 | `3.975 x (PWM - 25.8)`, n=8,808 |
| OLS on per-PWM medians, 26–105 | `2.590 x (PWM - 8.1)` | `3.111 x (PWM - 15.3)` |
| lowest observed moving PWM | 39 | 35 |

The documented coefficients `3.990 / 25.1` land almost exactly on **Otto's**
data (`3.975 / 25.8`) and nowhere near Toby's. Toby's own files hold 1,575 rows,
not 4,617. No script in the repository reproduces the fit.

I am **not** proposing any change to Toby's frozen, field-accepted build on the
strength of this, and I have not touched it. There are innocent explanations —
a combined pool, a filter I have not guessed, or simply that these segment
measurements (300 mm between markers) structurally cannot observe a locomotive
that is barely moving, so the intercept was always an extrapolation and is
method-sensitive by nature. Recording it because it is the provenance of a
number that step 1 now has to reproduce for Otto, and because the method is not
written down anywhere.

**The practical consequence for step 1:** Otto's tractive floor cannot be
derived "the same way", because the way is not recoverable. It needs either a
creep test (raise PWM one count at a time from 15 until he just moves) or an
operator ruling to err high, since freezing the baseline too often is a much
milder fault than letting it walk onto a magnet.

---

## 7. Proposal: step 1 splits into a survey and a navigation build

### The observation that makes this cheap

`NAVI_ONE_STATION_CURVES.ino:876-884` calls `publishCurveMeta()` and
`publishWaveformSlot()` **unconditionally, for every completed passage, before
any navigation ruling**, carrying `peakCounts`, `amplitudeRatio`, `residual`,
`gain`, `gap_ms` and the outcome.

**Under recorder 0.3 the recognizer constants label the published data; they do
not gate it.** A passage refused as TOO_WEAK is published in full, with the peak
and the gain that refused it. Every one of Otto's real numbers can therefore be
fitted *afterwards*, from the published record, whatever provisional values were
compiled in.

And a locomotive that has never been given `start_mm` sits at `NO_POSITION`:
the recognizer runs and publishes, navigation advances nothing, AUTO is a
separate command the operator simply does not send.

So Otto can be surveyed with the frozen build, unmodified, driven manually.

### 1a — Otto survey build (proposed)

- **Single behavioural difference from Toby's field-verdicted image:** the
  compiled profile is Otto's. Nothing else.
- **Unchanged:** recognizer, capture, navigator, stations, wire formats, the
  whole sketch. No ESP-NOW (Otto's IR symbols are dead here, §2).
- **Observed failure addressed:** none. This is a measurement, not a fix.
- **Acceptance:** enough published passages, both directions, across the
  approach throttles, to fit items 2, 3, 4, 7 and 8 of §4 on Otto's present
  sensor. `pub_drop` zero and no gaps in `wave_meta.seq`.
- **Rollback image:** Otto's current QUORUM 1.12C.
- **Ends when:** the block is fitted, or the survey shows Otto's signal will not
  support NAVI_ONE at the current entry threshold — which is itself the answer
  to §3.
- **Why it cannot cause a false advance or a shutdown:** position is never
  declared, so nothing advances; AUTO is never commanded, so nothing drives.

### 1b — Otto navigation build

The fitted block replaces the provisional one, `NAVI_RECOGNIZER_MEASURED_ON`
becomes `9950011UL` as a true attestation, and Otto runs the acceptance session
the operator specified: laps and station stops, no false advance, no shutdown,
measured from the telemetry mirror.

### The thing I will not do without a ruling

1a requires putting provisional numbers in Otto's profile so it compiles, which
is what the compile guard exists to prevent. The guard's purpose — *do not fly
another locomotive's thresholds on this sensor* — is not violated by a build
that never navigates, but that is a judgement about the guard's intent, and the
guard is the operator's. **I have not written those values into the repository.**

---

## 8. Questions for the operator

1. **Is the survey build (1a) acceptable** — Otto's profile carrying explicitly
   provisional, survey-only recognizer values so the image compiles, on a build
   that never declares position and never enters AUTO? If yes, it wants a
   decision record, because it is a deliberate qualification of the §1 guard.
2. **`IR_FITTED` for Otto: 0 or 1?** This is the pin-34 IR sensor on the
   locomotive, not the ESP-NOW test car. Declaration, not probe — the firmware
   is explicit that it will not guess. `0` means pin 34 is never touched, which
   also keeps floating-input crosstalk off the Hall line.
3. **Otto's tractive floor** — a creep test, or set it high and conservative
   pending one? (§6)
4. **Otto's entry threshold** — leave the QUORUM-era 70 alone for the survey, so
   the survey measures the loco as he actually runs today? My recommendation is
   yes: change one thing at a time, and the survey will tell us whether 70 is
   costing markers. (§3)

## 9. Housekeeping

`docs/decisions/` contains two files numbered 0075
(`...a-rising-reading-is-not-a-false-start.md` and
`...publishing-every-passage-is-gated-on-measurement.md`). Numbers are never
reused by the rules in `docs/CLAUDE.md`. Flagged, not touched.

---

## Evidence

- Compile failure and clean control build: arduino-cli 1.5.1, esp32 3.3.11,
  scratchpad copies, `--build-path`, operator's build directories untouched.
- Profile comparison: preprocessor-level diff of the two headers.
- Refusal path: `MagnetRecognizer.h:249,253`; publish path:
  `NAVI_ONE_STATION_CURVES.ino:876-884`.
- Otto's signal history: `field-records/20260820_OTTO_HALL_SENSOR_WEAK.md`,
  and the profile's own 2026-08-20 notes.
- Calibration fits: `field-records/cal/cal_9950011_*.txt` (8,928 rows),
  `cal_9950012_*.txt` (1,575 rows).
- Amplitude floor behaviour: `docs/NAVI_ONE_AMPLITUDE_FLOOR_SWEEP_20260904.md`.
