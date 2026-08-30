# Closeout — every finding of the NAVI_ONE 0.1 review

**Date:** 2026-08-30
**Result:** `NAVI_ONE_0_3`, **not flashed**. All thirteen findings disposed of.
**Gates:** four, all green — survey 195/195 + 156/156, contract 176/0, ops 72/0,
real-lap 172 advances closing at MM040.
**Build:** 959,291 bytes (73%), 51,748 bytes globals. Banner `NAVI_ONE_0_3 — 9950012`,
verified in the compiled image.

Three findings needed a ruling or a measurement and got a decision record;
eight were straightforward; two items are deliberately not fixed and are named
below with the reason.

## The findings

### 1 — the strike did not latch — **FIXED**, decision 0058

`NavState::Struck`. `positionKnown()` false, every later passage rules
`NoPosition`, `nav_ready` 0, enrolment withdrawn and `state/auto 0` published so
GO is refused, only `declare()` clears it. Gate 2 T4 pins all of it, including
the worst case: a *correct* magnet arriving after the strike must rule
`NO_POSITION` and must not advance.

### 2 — a Contradicted witness never stopped AUTO — **ARMED, AND IT WILL NEVER FIRE**, decision 0059

The operator ruled it should stop and name the true marker. It does. Then the
gate written to watch it work showed it cannot: `verifySequence()` compares each
stored reading against the polarity of the marker it was stored at, and a
reading is only stored *after* it matched that polarity, so the word always
fits. Zero firings across all 171 start positions. Kept as an assertion, with
no claim made for it. The honest bound is measured instead: **six markers,
~1.8 m**, and 81 of 171 drifts advance silently at the moment of the miss.

### 3 — the recognizer was reset from the wrong thread — **FIXED**

The Navigator no longer holds a recognizer at all. `declare()` and
`setDirection()` raise a request; `takeResetRequest()` is drained on the loop
thread into the existing volatile flag; the Hall task remains the only thread
that touches the recognizer or the capture buffer. The host gates do the same
thing, so they now exercise the real structure.

Two related items in the same finding:

- **A queued passage judged against a fresh declaration** — fixed with an
  epoch. Every `Judged` carries the declaration it was captured under; the loop
  discards any that do not match and counts them as `stale` in the status line.
- **Loop-thread reads of `capture.baseline()` / `floorRejects()` and the
  `irRawMin/Max` reset** — **not changed**. These are aligned 32-bit reads of
  display-only counters on a single-writer path; making them atomic would add
  synchronisation to values nothing decides on. Named here so it is a choice
  rather than an oversight.

### 4 — what "0-error identification" actually means — **ANSWERED**, decision 0059

Not "zero errors per event". It is: zero recorded recognizer errors across 351
survey passages and a 172-detection out-of-sample lap, **plus** a detection
bound of six markers, now measured rather than estimated.

### 5 — the shape test disappeared through three doors — **TWO FIXED, ONE MEASURED AND KEPT**, decision 0062

- `clipped_` was contaminated from outside the passage — **fixed**; it is
  cleared when a passage opens, so it now means *this passage railed*.
- Passages over 512 samples were truncated, removing the shape test at crawl
  speed — **fixed by decimation**: the buffer halves the arc in place and
  halves its sample rate rather than dropping the tail. Gate 4 C2 replays a
  four-second passage and confirms the Gaussian fit still reads inside 0.13.
- The permissive abstention on a railed passage — **kept, on measurement**. The
  survey's 195 real primaries run to amplitude ratio **2.621**, against a
  non-primary maximum of 0.274: a wide gap below, and none above. An upper
  bound would refuse a real magnet, and the highest record in the survey is
  both real *and* one of the two that railed. Refusing clipped passages would
  have refused 2 of 195 real magnets. Under decision 0059 that is the more
  expensive error.

### 6 — evidence destroyed when the broker blinked — **FIXED**

- The publish queue **holds** while MQTT is down instead of being drained into
  nothing; a blink of a few seconds now costs nothing at all.
- Every genuine loss is counted at both ends and published: `pub_drop`,
  `cmd_drop`, `stale`.
- `wifiClient.setConnectionTimeout(3000)` before every connect — the core
  gotcha, where `setTimeout()` is the read timeout and not this — plus a 2 s
  backoff so a dead broker is not hammered.
- WiFi is explicitly re-associated after 15 s down rather than trusting the
  core's undocumented auto-reconnect.
- **The e-stop can no longer be dropped.** `onMqtt` parses and raises it the
  instant the packet arrives, before the queue, and `serviceRamp()` reads that
  flag directly. The motor stops whether or not the command ever reaches
  `loop()`.

### 7 — commands accepted at dangerous times, and parsed dangerously — **FIXED**, and one item verified rather than fixed

A new pure-C++ layer, `Ops.h`, holds all parsing and admissibility, so gate 4
can drive it:

- **Nothing calls `atoi()` any more.** `parseInt` refuses empty strings,
  trailing rubbish and non-numerics. `parseBool` takes 1/0/true/false/on/off.
- **On the emergency topic, ambiguity STOPS.** `parseEstop` asserts the stop for
  anything it cannot read. 0.1 computed `atoi("") != 0` — false — and stood
  down.
- **`cmd/direction` accepts only 0 (REV) and 2 (FWD).** 0.1 turned any
  unparseable payload into 0, and 0 is REVERSE.
- **Position is declared standing still.** `start_mm`, `start_interval` and
  `session_direction` are refused while AUTO runs and while any PWM is
  commanded or applied — `commandedPwm` as well as `actualPwm`, so a
  mid-ramp-up command is caught too. The same `commandedPwm` fix applies to
  `cmd/direction`.
- **Every refusal says so.** 0.1 dropped throttle commands silently while
  enlisted, so after a strike the operator could neither restart AUTO nor drive
  by hand, and nothing said which.
- **Retained-command replay: verified absent, not fixed.** Every command the
  console publishes goes through one function, `pub()` in `ngr_app_v1_11_2.py`,
  with `retain=False`. There is nothing to replay. The single choke point is
  what makes that checkable; the finding stands as a thing to re-check if the
  console ever grows a second publish path.

### 8 — the ADC channel-switch fix was half a fix — **FIXED**, decision 0061

- A settle read on the **Hall** pin after switching back from IR, which 0.1 did
  only in the other direction. Every tenth Hall sample was being taken straight
  after the mux left the IR channel.
- The boot probe no longer runs at 1 kHz for four seconds while the Hall
  baseline primes; it runs at 100 Hz, and not at all when IR is declared absent.
- **Presence is declared in the profile, not inferred.** The probe's logic was
  backwards — a floating pin swings and reads present, a fitted-but-stationary
  sensor is quiet and reads absent. It survives as a report only.

### 9 — the selector file had re-grown every trap its own comments document — **FIXED**, decision 0061

Header, duplicate include, boot-verify text and the missing Otto line all
corrected; one line per locomotive, every line pointing at a file that exists.
Per-locomotive values moved out of the sketch: cruise PWM, the whole battery
policy, and the recognizer's measured thresholds. `LocoConfig.h` now **refuses
to build** a profile whose recognizer block is missing or was measured on a
different locomotive — verified by selecting Otto and watching the build stop.

### 10 — voltage protection failed silent and had no hysteresis — **FIXED**, decision 0060

Profile band (13.25 trip after five consecutive readings / 14.0 recover /
below 12.5 there is no pack), an absent INA219 announced in capitals at boot and
carried as `"ina":0` in every status line, and low voltage demoted from the
e-stop's instant cut to the steep informative ramp the operator ruled for.

### 11 — the layer where both field bugs lived had no tests — **FIXED**

**Gate 4**, 72 checks: payload parsing, every interlock, and — for the first
time — `HallCapture` itself, which no gate had ever driven. Gate 1 replays
waveforms straight into the recognizer, so passage assembly had never been
tested at all.

### 12 — evidence and harness nits — **FIXED**

- `replay_lap` no longer caps at `i < 172`. It selects the circuit's records by
  their own content — the lap was driven with position UNSET, so every one of
  its records carries `mm 0` and `timing_gate NO_POSITION` — and asserts the
  full accounting: 172 circuit records **and** 5 from the later 18:09 declared
  segment. Nothing in the evidence file is silently skipped.
- Gate 1's waveforms were captured by the 1.13X survey sketch, so capture
  geometry was unproven. Gate 4 now drives entry margin, exit hysteresis,
  pre-roll, duration floor and decimation directly.
- Post-strike behaviour is pinned in gate 2 T4.

### 13 — the small items

| | |
|---|---|
| 1 duplicated entry sample | **fixed** — the entry crossing no longer enters the pre-roll; gate 4 C1 asserts it appears once |
| 2 `TooSoon` said 500 ms | **fixed** — the guard is 200 ms, measured |
| 3 `"moving"` derived from PWM | **fixed** — the field is now `"powered"`, which is what it measures. *Console-side check needed if anything read `moving`.* |
| 4 an unvisited marker published as position | **comment corrected, value kept** — the value is needed for the next advance to land right, and it is published alongside `trust: DECLARED` and `nav_state: DECLARED`, which is exactly its provenance |
| 5 `lastAdvanceMs` and a retained cruise speed | **fixed** — reset on declaration and on withdrawal, and `telem/speed` publishes 0 when the run ends |
| 6 clearing e-stop wiped the strike record | **fixed** — a strike warning is sticky and only a declaration clears it |
| 7 counters reset inconsistently | **fixed** — `setDirection()` ends a frame like `declare()` does |
| 8 unchecked task and queue creation | **fixed** — a failed Hall task or queue halts loudly instead of booting a locomotive that navigates by nothing |
| 9 the Gaussian fit runs on the sampling task | **not changed** — worth one bench measurement of tick jitter at passage close, not a redesign. Carried in NEXT. |
| 10 declaring while inside a magnet's field | **fixed** — a declaration resets the capture buffer; gate 4 C4 |
| 11 the NEXT doc claimed a completion condition | **fixed** — there isn't one; the doc says so |
| 12 `"ruling":"WRONG_MAGNET","why":"MAGNET"` | **fixed** — `why` now reads `POLARITY_MISMATCH` / `SEQUENCE_MISMATCH` |

## One new finding, for the operator

**In AUTO, the dashboard is the only remote stop, and nothing watches whether
it is still there.** If MQTT drops while AUTO is running at cruise, the
locomotive keeps going and the operator's stop button reaches nothing. QUORUM
had no such watchdog either, so this is inherited rather than introduced, and a
comms watchdog is a new stop condition — which is the operator's call, not
mine. Flagged, not built.

## Not done, and why

- The Gaussian fit's placement on the sampling task (13.9) — wants a bench
  measurement first.
- The two display-only cross-thread reads in Finding 3 — deliberate.
- A comms watchdog — needs a ruling.

## References

- `docs/reviews/NAVI_ONE_0_1_REVIEW_20260829.md` — the review
- `docs/reviews/NAVI_ONE_0_1_REVIEW_RESPONSE_20260830.md` — findings 1 and 2
- decisions 0058, 0059, 0060, 0061, 0062
- `firmware/test-programs/NAVI_ONE/` at `NAVI_ONE_0_3`
