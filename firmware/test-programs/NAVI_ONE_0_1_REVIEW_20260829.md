# Review — NAVI_ONE 0.1: every fault and weakness found

**Reviewer:** Claude (session `agent/toby-1-13-flash`, 2026-08-29 night)
**Subject:** `firmware/test-programs/NAVI_ONE/` — `NAVI_ONE_0_1`, all headers, tests, and configs
**Assignment:** operator — *"Critique it. Go step by step. Find every fault, every weakness."* Purpose of the sketch: enable 0-error magnet identification for navigation on the Lowline loop.
**Constraint:** review only. No code was changed.

## Verification performed before critiquing

Claims were checked, not assumed:

- **All three host gates pass on this machine tonight**: survey replay 195/195 primaries accepted, 156/156 non-primaries refused, zero misclassified; contract 87 checks 0 failures; real-lap replay 172 advances, 0 refusals, circuit closed at MM040, PROVEN.
- **RouteMap tables are bit-identical to QUORUM's** `NGR_DNA1` and `spacingMm` (verified element-by-element, 171/171 each).
- **The ten-window uniqueness claim is true**, and in *both* directions: every cyclic window of length 10 is unique CW and CCW. Windows of length 9 are not (4 duplicates) — SEQ_N = 10 is exactly minimal.
- **Spacing sums to 52,150 mm, min 280, max 355** — as documented.
- **The longest run of identical polarity is 7 markers** (MM107–MM113). This number is load-bearing for Finding 2.
- `credentials.h` is correctly git-ignored (`.gitignore:2`) and follows the template convention.

The design's strengths are real and are acknowledged here once, so the rest of the document can be what was asked for: the capture/recognizer/navigator layering is the right shape; thresholds are measured, not chosen; abstention semantics are mostly principled; the oversize-JSON guard, the no-inert-retained-zero stance, and the queue-only transport are all lessons actually carried. The tests drive the real headers, not copies. Now the faults.

---

## Findings, most severe first

### 1. The one strike is not latched — the navigator keeps saying where it is, and AUTO can be restarted on the discredited position

`oneStrike()` (NAVI_ONE.ino:311) publishes *"Position is not known. Declare it."* — and then nothing in the program makes that true:

- `Navigator::judge()` on `WrongMagnet` increments a counter and returns. `NavState` stays `Declared`, `positionKnown()` stays true, `navMm` and `target` are unchanged.
- `serviceStatus()` keeps publishing `level: CLEAR`, `state: NORMAL`, `nav_ready: 1`, and the same `mm` — one second after declaring position unknown.
- `oneStrike()` clears `autoRunning` but not `autoEnrolled`. A subsequent `cmd/go` — from the operator **or from the dispatcher** — passes every check (`enrolled` ✓, `positionKnown()` ✓, interlocks ✓) and restarts AUTO **on the exact position the strike just discredited, with no new declaration**. If the dispatcher ever re-issues `go` automatically, the locomotive restarts itself after a strike.
- Worse: judging continues after the strike. While the loco coasts down (~2.8 s from PWM 90, roughly a marker of travel), the next magnet is still judged, and if its polarity happens to match the stale target — 50% — **`navMm` advances after the strike**, on a position the sketch itself declared unknown.

The contract (Navigator.h:23, decision 0056) says the navigator's whole job at that moment is *to stop saying where it is*. It says it once, in a warning string, and then resumes saying where it is. This is the largest gap between the stated design and the code. It needs a latched state (whatever it is called) that only `declare()` clears, and it needs the strike to withdraw enrolment the way `dispatcher_release` does.

### 2. `Trust::Contradicted` never stops AUTO — the sketch can drive up to 7 markers on a position it has already computed to be wrong

The sequence witness is the best instrument in the sketch and it is deliberately unarmed (Navigator.h:163: *"It REPORTS and never acts"*). Walk the consequence through:

- A real magnet is missed for any reason (capture miss, a weak magnet under the floor, an ADC glitch during the passage). `navMm` is now one behind.
- The next magnet's polarity matches the stale target whenever adjacent polarities repeat — i.e. inside any same-polarity run. It **advances**. Position is now wrong and moving.
- The witness catches this almost immediately: because window-10 uniqueness holds, the observed ten-word fits exactly one place, `trust` flips to `Contradicted`, and `seqAt` **names the true position** — verified in the route data. The navigator now *knows*, to route-level uniqueness, that it is lying about `mm`, and knows the correction.
- And AUTO keeps driving. The hard stop comes only when polarity finally mismatches — at the end of the same-polarity run. The measured worst case on this route is the run MM107–MM113: **7 markers, roughly 2.1 m, through the Arches**, at cruise, on a known-wrong position, with `state: NORMAL` on the console the whole way (only the `trust` chip disagreeing).

The operator's constraint was *"An error stops the locomotive."* A `Contradicted` verdict is a detected error — it is precisely "the map and the world disagree" from decision 0056, established with more evidence than a single polarity mismatch, and it does not stop the locomotive. The report-only stance was a deliberate scoping choice, so this is presented for a ruling rather than as a coding defect — but the current behaviour honours the letter of one-strike while defeating its purpose for up to 7 markers. (Edge case that weakens it further: the witness is silent for the first 10 advances after any declaration, while `seqLen_` fills.)

### 3. Concurrency: the recognizer is reset from the wrong thread, defeating the sketch's own safety mechanism

The .ino builds a careful cross-thread contract — the recognizer lives on the Hall task, `recognizerResetRequest` exists so resets happen *on that task* (NAVI_ONE.ino:410). Then `Navigator::declare()` (Navigator.h:111) and `setDirection()` (Navigator.h:126) call `rec_.reset()` **directly**, and both are invoked from the loop thread via `handleCommand()` → `declarePosition()` / `applyTravelDirection()`. So every declaration and direction change writes `gainLen_`, `gainHead_`, `haveAccepted_` on core 1 while `examine()` may be mid-read on core 0. The flag mechanism is then a redundant second reset one tick later.

Consequences are bounded (fixed arrays, no pointers — no memory unsafety) but real: `gain()` can compute a median over a half-reset ring, and the guard anchor can vanish mid-check. It is formally a data race and exactly the class of bug the flag was built to prevent. Related, smaller:

- A `Judged` event already sitting in `judgedQ` when a declaration lands is judged **against the new declaration** — a passage captured under the old frame can advance the fresh one. Window is ~2 ms, but it exists.
- Loop-thread reads of `capture.baseline()` / `capture.floorRejects()` while the Hall task writes them, and the `serviceIr()` reset of `irRawMin/Max` against Hall-task writes — benign on this architecture, but uncontracted.

### 4. "0-error identification" rests on one bit per magnet, and the failure arithmetic should be stated honestly

The recognizer's record is genuinely clean: zero misclassifications across 351 survey passages and a 172-detection out-of-sample lap. But *identification* — WHICH magnet — is a single polarity bit checked against a single expected value. That means:

- A **false accept** (non-magnet passed by the recognizer) advances position with probability 50%.
- A **missed magnet** leads to a wrong advance with probability ~50% (whenever the next polarity matches), and then Finding 2's window applies.
- Detection of either error is *guaranteed* only by the polarity chain (geometric, mean ~2 markers, measured worst case 7) and by the ten-word witness (certain within 1 advance after the window fills — but unarmed).

So the honest statement of what NAVI_ONE achieves is not "0-error magnet identification per event." It is: **zero recorded recognizer errors to date, plus a bounded-detection guarantee — no identification error can persist beyond 7 markers undetected, and none survives a full ten-window unflagged.** That is a strong and defensible property — but only if Finding 1 is fixed (a strike must actually end the run) and Finding 2 is ruled on. As the code stands, the bound on *detection* is 7 markers but the bound on *acting wrongly* is unbounded, because nothing latches.

### 5. The shape test quietly disappears exactly where it is most and least needed

Three separate mechanisms all funnel passages past the shape test into the weaker amplitude+guard-only path:

- **`clipped_` is contaminated from outside the passage.** `updateBaseline()` (HallCapture.h:113) sets `clipped_` on any railed sample *at any time*, and it is only cleared when a passage closes. A supply transient between passages — minutes earlier — marks the *next* passage clipped, and its shape test silently abstains.
- **Any passage longer than 512 samples is truncated** (RING = 512 at 1 kHz = 512 ms). At crawl speeds a normal passage exceeds this, so at station-approach speed the shape test is systematically absent. The legitimate 18.7 s dwell passage of decision 0057 was accepted on amplitude and guard alone.
- **The abstention is asymmetric in the permissive direction.** A clean-but-unfittable waveform is *refused* (`NoCurve`), but a truncated or clipped one is *excused*. An electrical event that rails the ADC gets a weaker examination than one that stays in range. Composite worst case: a railed transient longer than 40 ms with high peak passes amplitude (no upper bound on ratio), abstains shape (clipped), passes guard, and is accepted — leaving 1-bit polarity as the entire defence (Finding 4).

Each abstention is individually principled ("a fit to a cropped arc is not evidence"). Their combination means the sketch's second-strongest test can be absent without any operational signal beyond a JSON field.

### 6. Transport: messages are silently destroyed in exactly the scenarios where they matter

The queue architecture protects the *Hall task*, as designed. It does not protect the *evidence*:

- When MQTT is disconnected, `networkTask` **dequeues and discards** every message (`xQueueReceive` then `if (mqtt.connected())` — NAVI_ONE.ino:456-457). A broker outage throws away marker events, DISAGREE verdicts, and the WRONG-MAGNET warning itself, unrecorded. There is no drop counter, so the loss is invisible afterward — in a project whose founding transport story is "67 marker events destroyed," the current design destroys them deliberately and silently when the broker blinks.
- `mqtt.connect()` runs with no `setConnectionTimeout()` on the underlying `WiFiClient`. A wrong broker IP or a dead Pi means each connect attempt can block the network task for the full TCP timeout; at ~10 messages/s of steady publishing, the 48-slot `pubQ` fills in ~5 s and then `pub()` starts dropping at the *enqueue* end too — also uncounted. (This is the known core gotcha: `setTimeout` is not the connect timeout; `setConnectionTimeout(ms)` is.)
- `onMqtt` enqueues commands with zero timeout into a 16-deep `cmdQ`; when full, **an ESTOP command is dropped silently**. The window is small; the command is the one that must never be dropped.
- Nothing re-establishes WiFi if the association drops — the sketch relies on the core's default auto-reconnect, unstated and unverified for core 3.3.11.

### 7. Command handling accepts dangerous input at dangerous times

`handleCommand()` (NAVI_ONE.ino:463) has no notion of "not while running":

- `start_mm`, `start_interval`, and `session_direction` are all accepted **while `autoRunning`**. Re-declaring position mid-run resets the recognizer (Finding 3's race), empties the witness, and re-aims the target under a moving locomotive. A `session_direction` flip while driving steps `navMm` and reverses the expected sequence while the wheels keep going the same way — a guaranteed strike or, worse, a wrong advance.
- `cmd/direction` parses with `atoi`: any non-numeric payload becomes 0 → **REVERSE**. A malformed message from a console bug silently reverses a locomotive (at ≤15 PWM, but still: garbage should be refused, not interpreted as a direction). The gate also checks `actualPwm` only — a direction change is accepted mid-ramp-up while `commandedPwm` is 90.
- `cmd/estop` parses with `atoi`: a non-numeric or empty payload on the **broadcast** dispatcher estop topic *clears* an active e-stop (`estopped = 0 != 0`). On an emergency topic, an unparseable message should never mean "stand down."
- After a one-strike stop, `autoEnrolled` stays true, so `cmd/throttle` is **silently ignored** (no warn) — the operator can restart AUTO with `go` (Finding 1) but cannot drive manually to the fault without first sending `cmd/auto 0`, and nothing tells them so.
- Unverified against the console: if any `cmd/*` topic is ever published retained, the broker replays the last command on every reconnect — the sketch has no staleness guard. Needs one confirmation on the Pi side.

### 8. The ADC channel-switch fix is half a fix

The 2026-08-29 bench discovery (floating pin 34 dragging the Hall reading, ~1.1 floor-rejects/s) was correctly diagnosed and the not-fitted case correctly nailed shut. But when IR **is** fitted:

- `irService()` discards the first IR read after switching *to* pin 34 — and nothing discards the first **Hall** read after switching *back*. Every 10th tick, the Hall sample the locomotive navigates by is taken immediately after the mux left the IR channel, with exactly the sample-and-hold behaviour the file's own comment describes. Margin erosion at 10% duty on the primary sensor.
- During the 4-second boot probe, pin 34 is sampled at 1 kHz alongside every Hall read — while the Hall baseline primes at 2 s. The median is robust and the prime window mostly outlasts the probe, but the baseline the session starts with is taken under the exact interference condition the sketch documents as harmful.
- The probe logic itself is backwards — already recorded as `NAVI_ONE_NEXT.md` item 4 (a fitted-but-stationary sensor reads NOT FITTED, permanently, for the whole session). Noted here only for completeness; presence should be declared, not inferred.

### 9. Config hygiene: the selector file has re-grown every trap its own comments document

`LocoConfig.h` is a museum of its own warnings:

- The header says **"TARGET: Otto (9950011)"** while the active include is **Toby's** — the *identical* trap the 2026-08-12 note in the same file records ("this file's comment said TARGET Toby while the active include was Otto's").
- Toby's include appears **twice** — once commented, once active — the identical trap the 2026-08-18 note in the same file records for Otto ("appeared TWICE… the same trap wearing a different hat").
- The boot-verify instruction says the banner must read `QUORUM_1_16R_IR_TEST_A — 9950011`; NAVI_ONE's banner is `NAVI_ONE_0_1 — <loco>`. Following the file's own verification procedure verbatim would fail every correct build.
- Hans's line references `LocoConfig_2095111.h`, which does not exist in this sketch directory; there is **no include line for Otto at all**, though `LL_LocoConfig_9950011.h` sits in the directory — selecting Otto requires typing a new line, which is how typos happen.

And beyond the selector, per-locomotive and per-railway values are hard-coded in the sketch against the repo's stated rule ("Per-locomotive values belong in the config headers, not in the sketch"):

- `LOW_VOLTAGE_V = 14.4` in the .ino, while Toby's profile carries `SHUTDOWN_VOLTAGE 13.25` / `RECOVERY_VOLTAGE 14.0` / `THROTTLE_LIMIT_VOLTAGE 13.5` — two voltage policies now exist and the profile's is dead code here.
- `AUTO_CRUISE_PWM 90`, brake step constants, and `bootstrapGain 190` are all in-sketch. The recognizer constants (0.34 / 0.13 / 200 ms, bootstrap 190) were measured **on Toby only**. Otto's entry threshold is 70 counts vs Toby's 38 — a materially different sensor environment — and nothing marks the recognizer config as Toby-derived or blocks an Otto build from silently inheriting it.

### 10. Voltage protection fails silent and has no hysteresis

- If `ina219.begin()` fails, `inaReady` stays false and `serviceIna()` returns forever — **low-voltage protection is absent for the whole session and nothing says so**: no warn, no status field, not even a serial line. A loose I²C wire converts "protected" into "unprotected" invisibly.
- Trip and recovery share one threshold (`busV < 14.4` trips, `>= 14.4` recovers). A pack sagging around 14.4 under load chatters in and out of `lowVoltage` every 5 s poll. Each trip drops AUTO; recovery does not restore it (correct), but the retained warning churns and the stop reason blurs. QUORUM's profile has a proper band (13.25 trip / 14.0 recover); it was not carried and not argued against.
- `lowVoltage` shares the e-stop branch in `serviceRamp()` — an **instant PWM 0, no ramp, including in MANUAL**. Only the e-stop's instant stop is documented as bypassing the ramp; low-voltage inherits it silently. Combined with the 5 s polling and no hysteresis, one glitched INA reading above 0.1 V hard-stops a manually driven locomotive.

### 11. The layer where both of 0.1's field bugs lived is the layer with no tests

The header gates are good. But `NAVI_ONE.ino` itself — `handleCommand()`, `travelDir()`, `oneStrike()`, the ramp, the JSON — has no host coverage at all, and the record shows that is precisely where this sketch fails in practice: the sessionDir-only `travelDir()` bug ("He complied and died"), the `b[400]` truncation that silently destroyed telemetry fields, and the missing `dispatcher_release` subscription were all .ino-layer defects found in the field, on the railway, by the operator. The contract gate cannot see Finding 1 (unlatched strike), Finding 7 (commands during AUTO), or the estop parsing — those are all .ino behaviours. A fourth gate that drives `handleCommand()`-level logic through a harness would have caught most of this review's severe findings before flashing.

### 12. Evidence and test-harness nits

- `replay_lap.cpp` caps replay at `i < 172`, but the evidence file holds **177** records — the trailing five are from a later, separately-declared segment (18:09) appended to the same TSV. The cap is correct for the lap; the file's provenance is impure, and the gate silently ignoring records in its own evidence file is the kind of thing this project's records exist to prevent. Split the file or assert on the extras.
- Gate 1's survey waveforms were captured by the 1.13X survey sketch, not by this `HallCapture`. If the survey capture's entry/exit/pre-roll differ from 38/25/12-at-8 ms, the recognizer has been validated on passages built slightly differently from the ones it will receive in the field. Probably immaterial; currently unstated.
- The contract has no test asserting post-strike behaviour (what Finding 1 would have caught): nothing pins down whether judging continues, whether an advance is legal after `WrongMagnet`, or what the .ino must do with enrolment.

### 13. Smaller faults, all real, quickly

1. **Duplicated entry sample**: in `HallCapture::sample()`, the entry-crossing sample is pushed into the pre-roll ring *and* replayed *and* then pushed again by the main `push()` — it appears twice in the buffer at the pre/passage boundary. Negligible effect on the fit; still wrong.
2. **`Outcome::TooSoon` comment says "inside the 500 ms guard"** (MagnetRecognizer.h:64); the guard is 200 ms. The constant was re-measured and the comment not.
3. **`"moving": (actualPwm>0)`** in the status JSON is PWM-as-motion — the exact inference decision 0057 spends a page banning. Display-only, but publishing a field named `moving` derived from throttle invites the next consumer to trust it.
4. **`setDirection()` publishes an unvisited marker as position.** Reversing at 45 sets `navMm = 46` — bookkeeping so the next advance lands on 45, verified correct — but `serviceStatus` then publishes `mm: 46` / `dead_reckoned_mm: 46` for a marker never confirmed, directly contradicting the comment above it ("every advance is magnet-confirmed and the field carries the confirmed position").
5. **`lastAdvanceMs` is not reset by `declare()`/`setDirection()`** — the first speed estimate after a re-declaration is computed across the declaration boundary (guarded only by the 30 s cap), and `telem/speed` stays retained at the last value forever after a stop; a stopped locomotive advertises cruise speed.
6. **Clearing e-stop wipes the strike record**: `warn("")` on estop-clear (and on GO) overwrites the retained WRONG-MAGNET warning — the one field the operator was told to go read.
7. **Refusal/advance counters reset on `declare()` but not on `setDirection()`** — inconsistent bookkeeping across the two "frame ended" operations.
8. **Task/queue creation results are unchecked.** QUORUM checks `xTaskCreatePinnedToCore` and halts loudly; NAVI_ONE ignores the return. A failed Hall-task creation would boot a locomotive that navigates by nothing and says nothing about it.
9. **The Hall task shares core 0 with the WiFi stack at priority 3** — the radio core, where WiFi system tasks (priority ~18-23) preempt it. Lineage-standard (QUORUM did the same, at priority 2) and field-proven at 1 kHz, but NAVI_ONE also runs the Gaussian fit (~0.5–1 ms of `expf` over up to 512 samples) *on the sampling task* at passage close, which QUORUM's task never did. `vTaskDelayUntil` catches up, so samples bunch rather than vanish; jitter at exactly the moment a passage closes is still the price. Worth one bench measurement, not a redesign.
10. **Declaring while the sensor sits inside a magnet's field**: `capture` is never reset by a declaration, so the already-open passage closes on drive-off and is judged against the *next* target — a 50% immediate strike on an otherwise correct declaration. `start_interval` semantics (stand between magnets) make this rare; nothing makes it impossible.
11. **`NAVI_ONE_NEXT.md` says AUTO stops on "fault, release, or completion"** — there is no completion condition anywhere in the code; AUTO cruises until something goes wrong. The doc overstates the code.
12. **`publishNav` for a `WrongMagnet` event fills `"why"` with `outcomeName(Magnet)`** — the JSON says `"ruling":"WRONG_MAGNET","why":"MAGNET"`, which is technically true (it *was* a magnet) and reads like a contradiction on the console.

---

## The purpose question, answered plainly

Does NAVI_ONE enable 0-error magnet identification on the Lowline?

**As a recognizer: yes, on all evidence to date.** 351 survey passages plus a 172-detection out-of-sample lap, zero errors, thresholds sitting in measured gaps with real margin, re-verified tonight.

**As an identifier: not per-event, and it should not claim to.** Identity per event is one polarity bit. What the design actually provides — and it is the right property for this railway — is *bounded detectability*: no identification error can survive more than 7 markers (measured worst case; ~2 average) past the polarity chain, and none survives the ten-word witness by more than one advance. That guarantee is currently undermined by the two governing faults: a strike does not latch (Finding 1), so the stop is advisory; and a Contradicted witness does not act (Finding 2), so the one instrument that *knows* the truth watches the error drive by.

Fix Finding 1, rule on Finding 2, and the honest claim becomes: *no wrong position is ever acted on for more than one advance after detection, and detection is bounded at 7 markers.* That is as close to "0 error" as a one-bit-per-magnet railway can get without more bits — and the route data shows where more bits already exist if ever wanted: spacing (`ROUTE_SPACING_MM` is surveyed and already on board, unused in the accept path by design, per decisions 0053/0056).

## What needs the operator, and what needs no one

**Operator rulings needed (2):**
1. Should a one-strike latch until re-declaration — withdrawing enrolment, refusing GO, and suspending judging? (Finding 1. The review's answer: yes, all three; anything less makes the strike a suggestion.)
2. Should `Trust::Contradicted` stop AUTO? (Finding 2. This is a genuine design question — the witness acting would be the first "recovery-adjacent" mechanism, and 0056 forbids mechanisms that *survive* disagreement. Note the distinction: stopping on Contradicted is not surviving a disagreement, it is refusing to drive through one. It adds a stop, never an advance.)

**Straightforward fixes, no ruling needed:** the reset-thread race (3), the drained-queue discard and missing connect timeout and drop counters (6), command guards while running and payload validation (7), the Hall-side ADC settle discard (8), the selector file (9), the INA-absent warning and voltage hysteresis (10), a `handleCommand` host gate (11), and the small items (13).

## References

- `firmware/test-programs/NAVI_ONE/` — subject, at this session's state (uncommitted-at-review content identical to tracked)
- decisions 0052–0057; `docs/NAVI_ONE_NEXT.md`; `docs/reviews/NAVI_FRESH_0_2_REVIEW_20260829.md`
- `docs/CLAUDE.md` — per-loco config rule, selector-file history
- gates run this session: `tests/run_tests.sh` (all three PASS)
- route-table verification: element-wise comparison against `firmware/QUORUM/QUORUM.ino` `NGR_DNA1`/`spacingMm`; window-uniqueness and run-length computation over `ROUTE_POLARITY`
