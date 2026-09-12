# NAVI_ONE X16 — what to do about the post-stop successor exception

**Date:** 2026-09-12
**Status:** PROPOSAL. Nothing here is implemented, and nothing here is a decision.
**Occasion:** Otto's unplanned stop leaving Arches, 2026-09-11 23:44, after 618
clean advances on X15.
**Prior art consulted:** decision 0052 (the measured taxonomy of false signals),
decision 0081 (the 500 ms guard), decision 0083 (the X15 exception),
`field-records/20260910_GRILLERS_STITCH_SHUTDOWN.md`,
`field-records/20260828_MAGNET_TYPE_MAP_AND_SHAPE_VALIDATION.md`.

---

## 1. The event, restated against the map

| time | event | map |
|---|---|---|
| 23:44:37.800 | Arches `DEPART`, CCW, NAVI at MM107 | Arches centre MM108, `stopOffsetCCW` 0 |
| 23:44:40.339 | MM106 accepted, **South**, peak 84 | `ROUTE_POLARITY[106] = 0` = S ✔ |
| 23:44:42.619 | a **North** passage, peak 74, opened 318 ms after MM106 closed | expected next was MM105, `ROUTE_POLARITY[105] = 0` = S |
| | X15 marked it `post_stop_successor:1` and bypassed the guard | opposite polarity to the anchor, so the 0083 rule fired as written |
| | Navigator refused it and struck | correct behaviour under rule 3 |

X15 did exactly what decision 0083 specifies. **The implementation is not the
defect. The specification is.**

---

## 2. Three independent measurements, all already in this repository, say the
##    318 ms passage could not have been MM105

### 2.1 Polarity — the exception selects the false population

Decision 0052 measured every excursion on a full circuit:

| population | arrives after previous close | polarity | amplitude |
|---|---|---|---|
| rebound (~150/lap) | 14–64 ms | **opposite, 149 of 150** | 16–52 counts |
| re-read (~2/lap) | **139–347 ms** | **opposite, 13 of 13** | 38–44 counts |
| real magnet | 1116–2410 ms | mixed | ≥140 counts |

The Arches passage arrived at **318 ms**, **opposite pole** — inside the re-read
band on both measured axes.

On this railway, *opposite polarity is the signature of a magnet's own departing
tail*, not evidence of a different magnet. Decision 0083 reads that signature
backwards. Quantitatively:

* an artifact is opposite to its parent **~100 %** of the time (13/13, 149/150);
* the genuine next marker is opposite to the previous one **52.6 %** of the time
  (90 polarity changes in 171 markers).

So the X15 rule admits the false population about twice as readily as it admits
the population it was written to rescue. It is biased toward the artifact.

**At Arches CCW it could never have done anything else.** MM105 and MM106 are
both South. Had a genuine MM105 arrived inside 500 ms, X15's same-polarity
clause would have refused it `TOO_SOON`. The only thing the exception could ever
admit at that departure was a polarity-reversed non-magnet — which is what it
admitted.

### 2.2 Physics — the speed required does not exist on this railway

`ROUTE_SPACING_MM[105] = 300` mm between MM106 and MM105.

| reading of the 318 ms figure | implied speed | pKph | PWM by the QUORUM fit `3.90·pwm − 99.2` |
|---|---|---|---|
| strict close-to-open, 318 ms | 943 mm/s | 175 | 267 (off the 8-bit scale) |
| generous, allowing 150 ms of passage halves | 638 mm/s | 119 | 189 |
| **fleet maximum ever recorded** | **400 mm/s** | **74** | 128 |

And the locomotive was nowhere near even its own cruise. The departure ramps up
at `AUTO_STEP_UP_MS` = 62 ms per count from zero, so at 23:44:42.6 — 4.8 s after
`DEPART` — actual PWM was about **47**, against a commanded 90. By the same fit
that is roughly **85 mm/s**. The passage 300 mm earlier corroborates it: MM107 to
MM106 took **2.54 s**, an average of at most 118 mm/s.

The candidate would have required the locomotive to be travelling **seven times
faster** than the throttle could drive it, 300 mm after a standing start.

This is the check the recognizer's own header demands before any timing value is
moved: *convert the gap to mm/s, to pKph, and to the PWM the locomotive would
need.* Applied to the X14 Bamboo datum that justified 0083, it fails the same way
— 300 mm (`ROUTE_SPACING_MM[160]`) in 136 ms is 2200 mm/s, from a standing start
at a platform.

### 2.3 Geometry — MM103–MM109 are bar magnets

The 2026-08-28 walking survey records MM103–MM109 as **bar** magnets, level with
the sleeper. A bar's field reverses at its ends; that is the physical generator
of 0052's rebound row. Arches is the stretch where a strong reversed tail is most
expected, and a reversed tail is precisely what X15 is written to admit.

### 2.4 The conclusion these three share

**A post-stop window is the worst place on the railway to relax a timing guard,
not the best.** Immediately after a controlled stop the locomotive is at its
slowest, so the genuine marker-to-marker gap is at its *longest* — seconds, not
milliseconds. The population 0083 was built to rescue is empty at exactly the
moment 0083 arms.

---

## 3. The X14 premise needs re-reading before anything is built on it

Decision 0083's field evidence says genuine MM161 opened 136 ms after MM160's
long departure passage closed and was wrongly refused.

`field-records/20260910_GRILLERS_STITCH_SHUTDOWN.md:243`, written the same day,
says of a close-gap MM161 event: *"200 is what admitted the mm-161 re-read"*,
and at line 233 records `mm=161 pk=78 ratio=0.488` at a 200 ms gap as **"the bad
advance that began the first strike chain of the night."**

One record treats a close-gap MM161 passage as a genuine magnet wrongly refused;
the other treats it as a re-read wrongly admitted. They may or may not be the
same passage, but they cannot both be the right reading of that population, and
0083 rests entirely on the first one.

I have not been able to settle this from the repository alone. **It should be
settled from the X14 log before any exception is rebuilt.**

---

## 4. Assessment of the recommended X16 direction

CODEX proposes: report the under-500 ms post-stop passage as an ambiguous
candidate, do not move the anchor or consume the exception, let Navigator accept
it only if it agrees with the expected marker, and ignore-and-record a
disagreement instead of striking.

**What it gets right.** The strike was the wrong response to an event the
program itself had labelled exceptional, and "ignored and recorded" is the right
disposal for an ambiguous passage. Leaving identity wholly with Navigator is
also right, and it removes the polarity comparison from the recognizer, which
restores the position-free contract.

**What it does not close.** It keeps a sub-500 ms bypass alive, and §2 says that
window has no genuine population. So the only traffic the bypass can carry is
artifacts — and an artifact is now admitted whenever its polarity happens to
match the expectation. Because the artifact is opposite to the anchor ~100 % of
the time and the next marker is opposite to the anchor 52.6 % of the time,
**roughly half of all post-stop artifacts would produce a silent false advance**
rather than a stop.

That is a worse *kind* of failure than the one it replaces. Last night's failure
was loud, immediate, correctly reasoned and left a precise record. A false
advance is quiet: `navMm` is one marker wrong, the locomotive keeps running, the
station machine brakes for the wrong platform, and the ten-magnet witness — which
`Navigator.h` documents as never having fired — is the only thing that could
catch it.

Had it been in force last night the outcome would have been correct, because
MM105 is same-pole as MM106 and the artifact would have been ignored. That is
luck of the map, not of the design: at 90 of the railway's 171 markers the same
artifact advances position instead.

---

## 5. Recommendation — X16-A: withdraw the bypass, keep the reporting

**The guard returns to being unconditional. The post-stop machinery stays, and
reports instead of deciding.**

1. **`MagnetRecognizer.h`** — delete the exception. `postStopWaitingAnchor_`,
   `postStopAnchorActive_` and `lastAcceptedPolarity_` go; the guard branch
   returns to the plain `gapMs >= guardMs` test. The recognizer becomes
   polarity-blind again and the 0083 amendments to its header comment come out.
2. **The stop-reason machinery is kept and re-aimed.** `Ops.h::StopArmingPolicy`,
   the `postStopRequest` mailbox and the `POST_STOP_ARM`/`CANCEL` plumbing are
   correct and gated; they cost nothing and they are the only way the firmware
   knows a departure is in progress. They now set a **reporting window only**.
3. **`Verdict::postStopSuccessor` becomes `Verdict::postStopClose`** — "this
   passage was refused inside the guard, during a post-stop window". It changes
   no outcome. Telemetry renames `post_stop_successor` to `post_stop_close` so
   no future field record can read the flag as an admission.
4. **Publish the duration.** Add `dur_ms` to the marker JSON. 0052 measured
   re-reads at 40–103 ms against 131 ms for the shortest real magnet — a clean
   measured separation this program currently throws away, because
   `HallCapture::floorMs` is 40, exactly the bottom of the re-read band. Publishing
   it costs ~12 bytes in a 640-byte buffer and makes the next such event
   self-diagnosing. **This is instrumentation, not a third test**; I am not
   proposing to threshold on it.
5. **Nothing else changes.** The 500 ms value is untouched. No morphology, no
   stitching, no PWM timing model, no station identity in the recognizer, no
   automatic position correction.

Outcome under X16-A, replaying last night: the 318 ms North passage is refused
`TOO_SOON`, published with `post_stop_close:1`, waveform preserved, no strike, no
stop, Otto continues to MM105 and advances normally.

### What X16-A deliberately does not do, and what that costs

* **If a genuine marker ever does open inside 500 ms, it stays refused**, and the
  following marker will strike — the X14 pattern. §2 argues that population does
  not exist at any speed this railway reaches. If the operator does not accept
  that argument, X16-A is the wrong option and §6 is the fallback.
* **It does not address the real X14 failure mode**, whatever that turns out to
  be. If MM161 was genuinely missed — Otto's Hall sensor is documented weak
  (`20260820_OTTO_HALL_SENSOR_WEAK.md`), and at Arches he reads peak 84 where
  Toby's fixtures read 210 — then the cause is acquisition, not timing, and the
  fix belongs in the sensor or the amplitude reference, not in the guard.
* **It leaves the one-strike behaviour exactly as it is.** A missed marker still
  stops the locomotive one marker later. That is decision 0056 working, and it is
  not in scope here.

---

## 6. Fallback — X16-B, if the operator wants the two-stage anyway

This is the CODEX direction, written so that it is at least safe to build. I do
not recommend it, for the reasons in §4, but if the under-500 ms population is
judged real then this is the shape it should take.

1. **The recognizer does not accept it.** A post-stop passage inside the guard
   returns `outcome = TooSoon`, `isMagnet = false`, plus a new
   `Verdict::ambiguousSuccessor`. The anchor does not move, the gain history does
   not take the peak, and nothing is consumed. Every existing consumer that tests
   `isMagnet` keeps its current behaviour.
2. **Amplitude runs first.** Today the guard returns *before* the amplitude test,
   so an ambiguous candidate would never have been amplitude-tested at all. The
   flag may only be set on a passage that has already cleared
   `amplitudeFloor`.
3. **Navigator gains one branch and one ruling.** Before the `!isMagnet`
   disposal: if `ambiguousSuccessor` and `positionKnown()` and the polarity equals
   `polarityAt(target)`, advance and rule `AdvancedProvisional`; otherwise
   `NotAMagnet` — recorded, published, no strike, no stop. Navigator would then
   be admitting a passage the recognizer refused, which is a real change to the
   contract at `Navigator.h:7` and must be written into the header, not left
   implicit.
4. **The candidate's own re-read must be suppressed.** Because the anchor never
   moved, a re-read of an accepted candidate is measured from the *old* anchor,
   can clear 500 ms, and is then judged as a full magnet against the new
   expectation — a 50/50 false advance or strike. The recognizer must therefore
   hold the candidate's close time and polarity and refuse a same-polarity
   passage inside 500 ms of *it* as well.
5. **Expiry stays time-based** — the window ends at the ordinary 500 ms from the
   anchor close — so no cross-core confirmation feedback is needed and no new
   race is introduced.
6. **Accept the stated residual**: about half of all post-stop artifacts advance
   position silently. That must be written into the decision record as the
   consequence, per the 2026-08-30 ruling on implications.

---

## 7. Verification required before either is flashed

* All 12 existing gates green, unchanged.
* **A replay gate built from last night's Arches record** — MM106 at peak 84
  South, then North peak 74 at +318 ms — asserting no strike and no advance on
  the second passage. This is the regression that X15 lacked: `gate_flank_discard`
  replays Arches *departures*, but from Toby's 2026-09-03 run at peak ~210, so it
  could not have shown this.
* A replay asserting a genuine post-departure sequence (MM107→106→105→104 at
  real departure speeds) still advances once per marker.
* For X16-B only: gates for the candidate's own re-read, for a disagreeing
  candidate producing `NOT_A_MAGNET` and no stop, and for the provisional-advance
  ruling appearing distinctly in telemetry.
* ESP32 target compiles; Otto/9950011 still selected; no profile constants moved.
* The build identifies itself as a field-test build, per standing practice.

---

## 8. Open questions for the operator

1. **May decision 0052's measured taxonomy be used as the basis for this
   decision?** It is the only measurement of the under-500 ms population this
   railway has.
2. **Does the X14 Bamboo record survive the physical check in §2.2?** If not,
   0083's premise goes with it, and 0081's plain guard was never wrong.
3. **X16-A or X16-B?**
4. Separately, and not part of this proposal: Otto reads peak 84 at MM106 where
   the Arches fixtures read 210 for Toby. That is worth a look on its own.
