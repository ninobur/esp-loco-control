# 2026-09-10 — Otto stops at Grillers: a solved problem that cannot reach the code path it solves

Otto, 9950011, on `NAVI_ONE_1_0X13_FIELDTEST`, entry 70, guard 200 (as flashed),
`quorum 0`, `offsets 0`, `ir_fitted 0`. Stage 1: Otto alone, no CTO, no source
change. This record covers the Grillers shutdown and everything established
after it.

---

## 1. What happened, from the telemetry

The stop was ordinary and entirely commanded. Every second of it:

```
[STATION] ZONE       Grillers off=-5  mm=58  pwm=60
[STATION] ZERO_RAMP  Grillers off=-1  mm=62  pwm 56 -> 1 over 12 s
[STATION] DWELL      Grillers off=-1  mm=62  pwm=0, powered=0, 30 s
[STATION] DEPART     Grillers off=-1  mm=62  pwm=110
          pwm 15 -> 31 -> 47 -> 64 -> 80 -> 96 -> 110
```

Then, crossing the next magnet on the way out:

```
mm=63  pk=139  gap=45022  paused=0     ratio=0.799  -> ADVANCED (MAGNET)
mm=63  pk= 80  gap=  158  paused=2750  ratio=0.460  -> NOT_A_MAGNET (TOO_SOON)  stitched=1
          STITCHED WAVEFORM REFUSED at MM063 ... A marker may have gone
          uncounted. Position is not known. Declare it.
mm=63  pk= 80                                        -> UNRESOLVED_INTERRUPTION
mm=63  pk= 77  obs=N exp=S                           -> NO_POSITION
mm=63  pk= 72                                        -> NO_POSITION
mm=63  pk=108  obs=N exp=S                           -> NO_POSITION
mm=63  pk= 76                                        -> NO_POSITION
```

Position withdrawn. Locomotive stopped. Operator declaration required to move.

**The operator's independent observation:** Otto comes to rest at the same
physical point every time — the Hall just past MM066.

## 2. What it was not

- **Not a stall.** The deceleration, the 30 s dwell and the departure to PWM 110
  are all in the record, commanded by the station machine, with `powered=1`
  through the departure ramp.
- **Not the boot baseline.** That fault belongs to the earlier session and was
  cured by the power cycle. Post-cycle baseline sits 1969–2000 against the
  poisoned session's 1882–1968.
- **Not MM067.** On the two clean laps before this one MM067 read **146** and
  **143**, against its surveyed CW median of **137**. The magnet is healthy.
- **Not a weak magnet at MM063.** On its five earlier passes tonight MM063 read
  **194, 196, 201, 202, 209**. On this pass the first fragment read **139** —
  the split took a third of the amplitude with it.

## 3. The mechanism

The locomotive comes to rest with its Hall sensor inside a magnet's field. On
departure the field moves again, and the passage over that magnet arrives as
**two fragments of one arc**: one at 139, then a second at 80 arriving **158 ms**
later, same polarity, `paused_ms 2750`, `stitched 1`.

The second fragment is refused as `TOO_SOON` — it is inside the rebound guard.
That refusal, on a stitched passage, is routed to a navigation shutdown.

It repeats at the same place because the stop point is deterministic: Otto rests
in the same relation to that magnet every lap.

**This is not rare and it cannot be tuned away by moving the stop.** Gate 12 H,
recorded in decision 0070, swept the coast across a whole marker spacing:
**two landings in nine leave the sensor in a field strong enough to hold a
passage open.** Both September incidents that motivated 0070 happened with the
stop offsets doing exactly what they were set to do.

## 4. Why the sketch stitches at stations at all

Decision 0070, *A passage may not span a stop*, ratified for field test
2026-09-02, in the operator's words:

> A controlled stop should behave like a timeout in a game. The measurement is
> paused while Toby is stationary, then resumed when moving-magnet morphology
> returns.

It was needed because on 2026-09-01, twice, twenty-one minutes apart, at
Grillers CCW and Arches CCW, a stop mid-field merged everything before and after
into one object that was fitted to a Gaussian and thrown away: MM59 lost and a
strike, MM106 lost and a strike.

**Why morphology and not the station machine.** 0070's own risk section:

> The risk is that "a stop was requested" becomes evidence. It is not. PWM arms
> observation and decides nothing: it cannot pause a measurement, resume one,
> decide a polarity, contribute a sample, or advance anything.

With the throttle barred from deciding anything, the only thing left to decide
pause and resume was the shape of the field.

## 5. Why 0070's framing does not transfer to a production sketch

0070 states, under "the unintended consequence, stated plainly":

> **Toby will stop more often than he used to.** … **A stop is the build
> working, not the build failing.** Anyone reading the dashboard should expect
> it.

That was written about a build that names itself `FIELDTEST` with
`field_accepted: 0`, whose purpose was to convert a quiet, delayed fault into a
loud, immediate one so it could be measured. **It is a development stance and it
does not answer for NAVI_CTO**, whose requirement is two trains running the
Lowline with *no unnecessary shutdowns and no operator micromanagement*.

Against that requirement, a locomotive that halts at a station and needs a
declaration is a failure. It was quoted at the operator during this session as
though it settled the matter; it does not, and the archive's job is to be
checked against the code, not used as an argument.

## 6. What morphology actually contributes

**It identifies nothing, by construction.** `MagnetRecognizer::examine()` sets

```c
v.outcome = Outcome::Magnet;
v.isMagnet = true;
accept(p);
```

*before* the shape block is entered. Shape has no admitting power; it can only
take away. This is architecture, not an empirical result.

**It excludes nothing the physical tests do not already exclude.** Measured
three separate times:

| measurement | result |
|---|---|
| diagnostic run, 1,177 passages, 344 at station phases | *"The shape test refused nothing at any station phase… The test does no work at stations and only threatens the train there."* (0074) |
| recorder 0.3 field, 2026-09-04, 3,284 magnets, 76 stops | *"zero passages the old rule would have refused"* |
| rebound population, 2026-08-28, n=149 | jitter 149/149 pass, Gaussian 122/149, all three 45/149 — **30% of the false population passes morphology**. Amplitude refuses **149/149**. |

`MagnetRecognizer.h` states it outright: *"AMPLITUDE alone refuses all 153
surveyed rebounds, so this test is not carrying them."*

On the one population where morphology could have earned its place it disagrees
with the right answer three times in ten, while amplitude is never wrong.

## 7. The solution exists, and cannot be reached

X9, commit `904c32d`, 2026-09-02, on the operator's instruction:

> The sketch should take an archaeological approach. It is restoring a total
> picture from parts.

`TwoSided.h` / `examineInterrupted()` judges a split passage as two fragments of
one arc: the fragment that decelerated into a top sets the scale, the other is
fitted as a tail with that amplitude fixed. **It is in the flashed firmware** —
`MagnetRecognizer.h:233` includes it, line 312 calls it.

**It did not run tonight.** Every record reads `two_sided:0`, `trunk:0`. From
`examine()`:

```c
// TIME.
if (haveAccepted_) {
  v.gapMs = p.openedAtMs - lastAcceptedCloseMs_;
  if (v.gapMs < cfg_.guardMs) { v.outcome = Outcome::TooSoon; return v; }   // line 249
}
// AMPLITUDE.
if (v.amplitudeRatio < cfg_.amplitudeFloor) { v.outcome = Outcome::TooWeak; return v; }

v.outcome = Outcome::Magnet;
v.isMagnet = true;
accept(p);

// SHAPE ...
if (!p.truncated && !p.clipped) {
  if (p.stitchAt) {
    const ArchVerdict a = examineInterrupted(p, cfg_.residualCeiling);      // line 312
```

**The rebound guard returns sixty lines above the archaeology.** Tonight's
fragment arrived at 158 ms against a guard of 200: refused and returned at line
249. The code written for exactly this case was never called.

**And even when reached it is powerless.** `examineInterrupted()` sits after
`v.isMagnet = true`, inside the shape block, and writes only `shapeOutcome`.
Decision 0074 made shape diagnostic, and the archaeology lives inside what 0074
demoted. X9 can describe a split passage; it can no longer rescue one.

**A split fragment is not a rebound, and the test that can tell them apart sits
downstream of the test that assumes they are the same. No value of the guard
fixes that ordering.**

## 8. The ruling that was made, and the one line that did not follow it

Operator, 2026-09-03, recorded as decision 0074:

> Gaussian shape and two-sided archaeology are diagnostic measurements, not
> navigation tests. They may be calculated, published and archived, but may not
> reject a passage or stop the locomotive.

0074 lists four retirements. Commit `7d163fa` implemented three:

| 0074 said retire | in the flashed build |
|---|---|
| 1. The WRONG_SHAPE refusal | **done** — shape runs after acceptance, writes only `shapeOutcome` |
| 2. The X5 stop-episode rule (`kind==1 \|\| stopEpisode`) | **done** — widening removed |
| 4. The rescue half of the archaeology | **done** — cannot change a verdict |
| 3. **STITCHED_REFUSED as a shutdown** | **NOT done** — `refusedStitched()` still calls `withdraw()`, reached at `NAVI_ONE.ino:1379` |

The comment immediately above the surviving line:

```c
// Decision 0074: the stop-episode widening is withdrawn. Only a
// passage whose measurement was actually paused and stitched (kind 1)
// is a stitched refusal. An ordinary refusal around a stop -- now only
// ever TOO_SOON or TOO_WEAK -- is an ordinary refusal.
if (j.kind == 1) refusedStitched(j);
```

It names **TOO_SOON** as the surviving refusal type, correctly, and then routes a
TOO_SOON refusal on a stitched passage into a shutdown regardless. The shutdown
was **narrowed rather than retired**, on 0074's stated grounds that it had
*"nothing left to act on."* Something did. That is the line that stopped Otto.

**This is an implementation gap, not a missing ruling.**

## 9. What tonight's guard change does to this

The guard was restored to 500 ms on the operator's ruling earlier this session,
after the 436 ms datum that justified 200 was shown to require PWM 144 against a
dataset topping out at 120.

On gaps alone, 500 looked free: of **17 stitched passages tonight**, fourteen
have gaps of 3,832–4,820 ms and no guard touches them. 500 refuses three — the
two 158 ms fragments (already refused at 200; this failure, unchanged) and one at
exactly 200 ms, `mm=161 pk=78 ratio=0.488`, which was the bad advance that began
the first strike chain of the night.

**That accounting was made before the ordering at §7 was read, and it is
incomplete.** Because the guard returns above the archaeology, raising it
200 → 500 widens by 300 ms the window in which a split fragment is refused
*without the archaeology ever being consulted*. On tonight's data exactly one
fragment moves into that window.

This does not make 200 right — 200 is what admitted the mm-161 re-read. It means
the guard is being asked to do two incompatible jobs, and the fix is the
ordering, not the value.

## 10. Decision 0070's abort condition, and tonight's evidence

0070 closes with:

> Gate 12 reports 196 checks, 0 failures, **and one known risk it explicitly does
> not close.**
>
> **Abort and go back to `1b8b828` if a stitched waveform is ever accepted at the
> wrong marker — that is the one failure mode gate 12 cannot rule out from
> records alone.**

Earlier this session Otto accepted **14 stitched passages** and finished
believing MM161 while standing physically between MM066 and MM067 — confirmed by
the operator walking to the locomotive, and corroborated when a declaration of 66
produced a **South** reading at MM067, exactly as MM067's surveyed polarity
predicts and against the **North** that MM162 would have given.

A stitched waveform accepted at the wrong marker is precisely the mechanism that
produces that, and 0070 states it cannot be ruled out from records — which is why
the operator's walk supplied evidence the gate could not.

**The abort condition is written and the field has now spoken to it. Whether it
has fired is the operator's ruling; it means reverting to `1b8b828`, NAVI_ONE
0.9, the last accepted firmware.** It is not declared here.

## 11. The principle that separates the two candidate fixes

0070 bars station state from being evidence, and that is right. But it conflates
two different acts:

- Letting station state **create** evidence — *"I commanded a stop, therefore
  that excursion was a magnet."* That is the sketch telling itself what it
  expected to see. Barring it is correct.
- Letting station state **remove the need for** evidence — *"I am at Grillers,
  so the magnet under the sensor needs no identifying."* That is not
  manufacturing evidence. It is declining to re-derive a fact already held.

At a Grillers dwell the locomotive is at MM062 because that is why it stopped
there. The station machine ran ZONE, ZERO_RAMP and DWELL against a known marker,
in a chain that never consulted the Hall sensor. When it pulls away, the
excursion under the sensor is the magnet it is standing on, already counted.

**Morphology is being asked to recover a fact that was never lost** — and the arc
it must reconstruct is the one arc on the railway guaranteed to be broken,
because the locomotive stopped in the middle of it.

The narrower rule that keeps the principle intact:

> **Station state may never establish that something is a magnet. It may
> establish that a magnet does not need identifying.**

## 12. The two candidate fixes

**A — the narrow fix: implement 0074 item 3 as ruled.** `refusedStitched()`
publishes `STITCHED_REFUSED` and keeps the record, and does **not** withdraw
position. A refused stitched passage becomes an ordinary lost marker that the
polarity chain catches, exactly as a refusal anywhere else on the railway already
behaves. One line. It removes tonight's shutdown and nothing else.

**B — the broad fix: do not stitch at a station at all.** A dwell ends the
passage: the pre-stop fragment is discarded, not retained. On departure the
recogniser starts clean and refuses everything until the locomotive could
physically have reached the *next* marker — the minimum-time floor already
specified by the operator, shortest interval divided by the fastest it can be
going. At PWM 110 departing Grillers that is ~649 ms.

B consults no shape, holds no pause state, keeps no stitched buffer, performs no
reconstruction, and cannot accept a reconstruction at the wrong marker. It
deletes the path rather than repairing its exit, and it retires 0070's open risk
with it.

**What B requires, and it is the real design decision:** when a stop lands
mid-field the arrival passage never completed, so that marker is genuinely
uncounted. The departure must then advance by one *on the strength of the known
station position*, not on a waveform. That is §11's rule applied concretely, and
it needs the operator's ruling.

**What B costs:** a diagnostic, not a capability. Decision 0072 says keep it —
publish the departure waveform as a record, with no authority over navigation.
That is the same demotion 0074 applied to shape everywhere else; the stitching
path is the one place it was not carried through.

## 13. What is NOT established

- **Where the 95-marker position error accumulated.** The earlier session ran 993
  contiguous +1 advances with zero polarity mismatches and an amplitude
  fingerprint correlating with the 2026-09-09 survey at **r = +0.988, shift 0**,
  while finishing 95 markers from the truth. No mechanism in the telemetry
  accounts for it. The 14 accepted stitched passages are the leading candidate
  and 0070 names that risk, but it is not demonstrated here.
- **That fix A alone is sufficient.** It removes the shutdown; it does not stop a
  marker being lost at every station landing that falls in Gate 12 H's two-in-
  nine.
- **The entry threshold.** Still the operator's to rule. Otto runs 70. Tonight's
  fragments read 72–108, inside the band where genuine weak reads also live
  (MM106 read 90 earlier this session), so amplitude cannot separate them.

## 14. Method

Read-only telemetry throughout: MQTT subscription to `ngr/loco/9950011/#`, the
locomotive's own `mm/marker`, `state/station`, `alert` and `state/warning`
records, and read-only inspection of the Pi's `/home/david/NGR/telemetry/`. Code
citations are from the flashed tree. Nothing was flashed, no MQTT command was
published, no locomotive was operated, and nothing on the Pi was altered.
