# X22 — the locked per-interval baseline. Implementation report

**2026-09-19.** Sketch directory `firmware/test-programs/NAVI_ONE_X22/`.
Build identity on `state/bootid`: `NAVI_ONE_1_0X22_LOCKED_BASELINE_FIELDTEST`.
Base: `NAVI_ONE_X20/` at commit `c255700`, the X21 image.

**NOT FIELD ACCEPTED.** `FIELD_ACCEPTED 0`, `BUILD_CLASS
EXPERIMENTAL_FIELD_TEST`. **X18 remains the build to fall back to.** Nothing
here has been flashed and the railway has not been run.

**Authorisation.** Written on the operator's direct instruction of 2026-09-19,
after the cross-variant analysis and the two findings against
`NAVI_SIMPLIFIED`'s locked baseline. The X18–X21 audit's §19 advised *"do not
write X22 first"*; the operator asked for it, which supersedes that advice. It
is recorded here so the sequence is not misread later.

---

## 1. The one change

The X19 adaptive-rest architecture is deleted and replaced by a reference that
is **locked between marker intervals**.

Deleted outright: `restRef_`, `restValid_`, `noteRest()`, the quiet-level
persistence state (`quietRun_`, `quietSinceMs_`, `quietLevel_`), the 512-bin
window histogram (`histN_`, `histS_`, `histAdd`, `windowLevel`), and the
trailing-minimum `localRef()`. The rolling median survives as
`shadowBaseline_` with **no write path to the operative reference at all** —
X17 demoted it behind a `fixedAfterPrime` flag; X22 removes the branch, so the
demotion cannot be undone by a configuration mistake.

`baseline_` now has exactly two write sites: the prime, and the acceptance at
the end of a collection.

Everything X21 was authorised for is unchanged: polarity fixed at the opening,
navigation queued at detection, the 400 ms window as telemetry only, the
645 ms guard from the detection instant, PWM-0 suppression, and the
dwell/`IN_OLD_FIELD` machinery.

## 2. The cycle, and its gates

```
recognise N  ->  get clear of N  ->  establish a level for N->N+1  ->  LOCK
             ->  detect N+1 against it  ->  repeat
```

A collection runs once per detection and is accepted only under all of:

| gate | value | what it is |
|---|---|---|
| CLEAR | 30 samples within `departCounts` of the lock, after window + guard | electrical quiet, and **not** claimed to be spatial clearance |
| SETTLE | 80 ms | the `NAVI_ONE_SIMPLE` replay finding: starting the timer at the last above-threshold sample put windows in the magnetic tail |
| MOVING | `mayAdapt` throughout | finding 10, now structural rather than a freeze rule that can be outrun |
| CADENCE | prior interval ≤ 3000 ms | running between two mapped markers, not entering or leaving a station |
| QUIET | spread ≤ 32 | the collection window is not noisy |
| LEASH | `|candidate − lock| < departCounts` | see §5 — it is inert, and the gates proved it |

Every outcome, accepted or refused, publishes on `diag/baseline` with its
candidate, the level it replaced, the delta, the spread, the prior gap and the
consecutive-refusal run.

## 3. Finding 2 is addressed: the lock has an age, and the age has no authority

`baselineSetMs_`, `baselineAgeMs()`, `baselineStale()`, `baselineAccepted()`,
`baselineRejected()`, `baselineConsecutiveRejects()` and
`lastBaselineReject()`. `base_age_ms`, `base_stale`, `base_acc`, `base_rej`
and `base_rejr` appear on `state/status` and on every `diag/excursion` record;
the age at detection also rides on the navigation `Detection`.

**The stale flag gates nothing.** Whether a stale lock should change behaviour
is a policy question with no measured answer, and the field test needs the
measurement first. The reporting half of finding 2 is done; the policy half is
deliberately not.

One correction against the first draft of this build: refusals that had no
measured candidate — `NOT_MOVING`, `OFF_CADENCE`, `STOPPED`, `INTERRUPTED` —
were counted but **not published**. Those are the commonest refusals on a
railway with four stations, and counting them without recording them is
exactly how a lock goes stale invisibly. They now publish with the lock in
force and a zero candidate. Gate 25 caught this.

## 4. Two additions the architecture made necessary

Both are declared rather than slipped in, and both were forced by gates that
failed.

**4.1 Electrical rearm.** X19's reference was a trailing minimum, so it walked
onto any level that persisted and a sustained departure could not repeat — the
detector got its rearm free from the reference moving. A locked reference does
not move, so without this a 110-count shelf re-declares every time the 645 ms
guard expires, for as long as it lasts. X22 requires the signal to return
within `departCounts` of the lock before another declaration. The condition is
the project's own accepted one (reconstruction item 4): electrical quiet **may**
rearm the detector and does **not** claim spatial clearance. Everything it
refuses is counted into `suppressed_`.

**4.2 Lost-lock recovery.** Every other rule assumes the lock is approximately
right. Otto's prime was **112 counts wrong on 2026-09-15** — the measurement
X19 was built in response to — and under a locked reference that is
unrecoverable: `over` is true for ever, CLEAR never completes, no collection
ever runs. A locked reference must carry its own falsification test or a bad
prime is a brick.

The test is physical: 2 s of continuous departure **at cruise** (`pwm >= 70`).
Spans are 300 mm and cruise is 240+ mm/s, so that is at least 480 mm of
unbroken field, which no magnet produces. Recovery re-primes from the line as
it is, with CADENCE and the LEASH lifted; MOVING and QUIET still apply, and
every recovery publishes.

**The cruise condition is not decoration, and it is the second defect the
gates caught.** The first version required only `mayAdapt` — above the tractive
floor. A station approach ramp is above the floor for its first seconds *and*
inside a platform field, so gate 12's Grillers case installed the 2150-count
shelf as the lock: the 2026-09-16 failure arriving by the back door, in the
mechanism written to prevent it. A time bound alone repeats `noteRest`'s
mistake one level up. The recovery is also refused during a dwell or an
old-field hold, which are the states where the firmware *knows* it is sitting
in a magnet it has already identified.

`sample()`'s `pwm` parameter defaults to **0** — "propulsion unknown" — so a
caller that says nothing about the throttle cannot trigger a re-prime by
omission.

## 5. Finding 1 is NOT closed, and the leash is inert

The leash was written to close finding 1: reject a candidate a magnet's
distance from the lock, on the argument that such a level is a magnet, not a
resting level. `departCounts` is already in use, so no new constant.

**It is unreachable, and the gates proved it.** CLEAR requires 30 consecutive
samples *within* `departCounts` before a collection may start, and Collect
abandons on any sample that goes over. Every collected sample is therefore
already inside the leash, and so is their median. The Grillers shelf never
reaches a collection at all — CLEAR simply never completes while the
locomotive sits on it.

So the protection against the ~200-count shelf is real, but it is **CLEAR**,
not the leash. The leash is kept as a guarded invariant costing one
comparison, so the property survives if CLEAR is ever relaxed;
`reject:"LEASH"` in a field record means CLEAR has been changed.

**What remains open.** A *sub-threshold* field still becomes the lock. Measured
on this build: two magnets at cadence, then a flat +45-count fringe — finding
13's 41 counts, near enough — and the collection accepts it. 45 is inside
CLEAR, inside the leash, and its spread is nothing. A tighter leash would
refuse it and would also refuse the largest genuine drift measured on this
railway (~33 counts across one direction reversal, session C3B93D0B), which is
why the X18–X21 audit judged a leash weak without measured drift evidence and
why no tighter number is invented here.

The consequence is bounded and reported: the lock moves by at most the
fringe's own displacement, the next interval can move it back, and every
acceptance publishes its delta. This is recorded as a **standing gate
assertion** (gate 25, "KNOWN OPEN") so it cannot be forgotten. The real answer
is the audit's question — *when is the track demonstrably clear* — and it needs
the X18 recorder's data, not another threshold.

## 6. What this build measures but does not act on

The opening-vs-window disagreement, `sign(departAtDetect)` against
`sign(peakSigned)`, is computed on every record and published as `disagree`.
It fired once in 97 records on X21 (MM117) and twice in 266 on X20 (MM136
first) — both times selecting the failure. **It is still telemetry.** Acting on
it needs a navigator that can hold an observation as ambiguous instead of
advancing or striking, which is a larger change and has not been asked for.

## 7. The measured cost of a locked reference

Stated rather than buried, because it is the thing a locked reference buys the
Grillers fix with:

* **Sensitivity is referenced.** An arc pulling *toward* a lock displaced by E
  needs `|A| >= departCounts + |E|` to reach the threshold at all. Gate 11
  measures this: 4 of 70 cells are lost, exactly as predicted, and they are
  lost silently because nothing was ever declared. Above the threshold the
  recovery re-primes and the peak returns to within 12 counts.
* **Peak is measured from the lock**, so on a displaced level it is wrong by
  about the displacement. Tolerable only because the amplitude screen has no
  navigation authority — if it ever regains one, gate 11 is why it must not.
* **A magnet arriving while the sensor is still inside another field is
  missed**, because the rearm has not been satisfied. Counted, never silent.
* **A level step of `departCounts` or more is a detection at any rate.** X19
  could ask "how fast must a level change before it reads as a magnet"; a
  locked reference cannot, and X22 does not pretend to. Gate 12 was rewritten
  to assert this instead of X19's rate table.

**No cell anywhere inverts the pole.** That assertion is permanent, and it
passed at every offset and amplitude.

## 8. Verification

| check | result |
|---|---|
| `arduino-cli compile --fqbn esp32:esp32:esp32` (core 3.3.11, Otto profile) | **builds.** 971,991 B flash (74%), 69,812 B static RAM (21%) |
| sketch-local warnings, `--warnings all` | **none** |
| RAM against X21 | **1,872 bytes lower**, despite the 512-sample collection buffer — the 3 kB window histogram is gone |
| `tests/check_payload_bounds.py` | 34 records checked, **0 over limit** |
| boot record, rendered with real values | **624 bytes against the 704-byte transport, 80 spare** — measured by rendering it, not counted by hand (finding 15) |
| `tests/gate_excursion.cpp` | **ALL GATES PASS**, 29 gates, 0 failures |
| `tests/replay_x22_20260915.cpp` | **PASS.** 33/33 normal passages exactly once; 4/4 first departures; **0 rearm leaks** across the corpus |

Gates 23–29 are new and every one exercises the reference architecture that
changed. This is deliberate: X17's blocker B1 was that exactly one capture in
a twelve-gate suite ran the policy the flashed image had, so "all 12 gates
passed" was evidence about the *previous* build. It should not be possible to
say that about X22.

### What the replay's changed expectations mean

The replay's eleven "next-event" targets were X19's. Seven are second-and-later
departures *inside* long records — a locomotive sitting in a field while the
field wanders — which X19 found because its reference walked onto whatever
level persisted. That is the Arches mechanism exactly.

Rather than hand-marking which X22 should find, the replay now asserts X22's
own contract mechanically: **no declaration anywhere in the corpus without an
intervening return to within `departCounts` of the lock.** Zero leaks. Two of
the seven turn out to be taken anyway, because the field genuinely came home
and went out again — the detector's own answer, not a guess. X19's
"persistent-field extras" and "unmatched" counters are still printed, as
comparison, and are no longer asserted.

## 9. What to watch on the first run

**The predicted failure of this build is the opposite of X21's: not a
reference that migrates, but a reference that goes STALE because collections
keep being refused.** The station gates (MOVING, CADENCE) are correlated with
exactly the states that persist.

* `base_age_ms` on `state/status` and on every `diag/excursion`
* `base_rejr` — the consecutive-refusal run — and `reject` on `diag/baseline`
* `base_stale` going to 1, which gates nothing and means only that 120 s have
  passed without an accepted collection
* `LEASH` appearing at all, which would mean CLEAR has been changed
* `recovery:1` on `diag/baseline` — a re-prime is the most consequential thing
  this class does, and it should be rare

**Ending condition for the experiment:** a strike whose `diag/baseline` history
shows the lock was last accepted more than one station sequence earlier. That
is the stale-lock mechanism catching the railway, and it reopens §5 with a
record to argue from.

## 10. Rollback

X18 as flown (`242109e`). X21 (`c255700`) is the intermediate and is the image
Otto is fielded on.

## 11. Not done, and deliberately

No morphology. No width or duration floor — decision 0085's 82 ms is a property
of a closed passage and this build has no closure. No speed-dependent
threshold. No second threshold. No IR. No change to `Navigator`, `RouteMap`,
`Stations` or `Ops`. No ambiguity state. The lap-baseline controller's
correction still applies to the lock, bounded by the caller at ±2 counts per
lap, refused mid-window and mid-collection; it does **not** reset the lock's
age, because a nudged reference is the same reference and the age is what the
field test is measuring.
