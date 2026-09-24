# IR TX 1.6 R2: Independent Review of the Physical Bench Failure

Date: 2026-09-24. Reviewer: Claude (independent review requested by the operator).
Reviewed: `agent/toby-1-13-flash` through `19ecb08`, plus Codex's parallel
investigation `78c84d8` (`IR_TX_R3_DESIGN_REVIEW_PROPOSAL_20260923.md`), which
was pushed during this review and is assessed in §6.
Audience: SAM and Codex.

No hardware was flashed or touched. No live services, motors, NAV or AUTO
were involved. `firmware/` is unchanged. The R3 design below is a **proposal**.
It lives only in `tools/ir_r3_proposal/IrMovementDetector.h`, so the host
suites can run against it. It needs review before any production edit.

## Verdict

1. **The candidate explanation is right for two of the four recorded stops,
   and it does not explain the other two.** R2's `live span < 300` branch
   erased the optical reference during the deceleration. That happened
   before span fell below 120, so quiet retention could not start. The
   resting level would have passed R2's own plateau test in two cases:
   count 99 (first short roll) and count 462 (roll 1). Counts 532 and 627
   came to rest **between** the plateaus. R2's plateau rule refuses those
   even with the reference kept, and correctly so.
2. **A new R2 finding: the last completion of each final roll (462, 532, 627)
   is a threshold-drift completion over a still signal.** The collapsing
   live envelope raised the low threshold past a resting level. There was
   no wheel motion. The same sample records an abort, so the epoch consumer
   never uses it. It does mean the recorded roll deltas of 44, 70 and 95 each
   end with a completion that has no physical fall.
3. **Yes, separate the two lifetimes** (question 2). The price is
   quantified: R2's erasure was also, by accident, R2's only barrier against
   one false zero. That case is contrast fading gradually while the wheel
   keeps turning. A narrowly scoped R3 is justified. It needs a stillness
   bound, a refresh gate that stops the reference from ratcheting, thresholds
   taken from the reference while holding, and phase re-establishment. It
   leaves one explicit residual exposure that SAM/Codex must accept or reject
   (§5.4).
4. Nothing here authorizes a flash, a field trial, NAV or AUTO.

## 1. Evidence base: observed vs replayed

| Kind | What | Trust |
|---|---|---|
| **Observed** | Recorded ADC values. Per sample: the transmitted `CONTRAST/RISE/FALL/INPULSE` bits. Per 96-sample batch: `runMin/runMax` at the first sample and cumulative `pulses` (rises) and `latch` (openAborts) at the last. 10 Hz type-5 snapshots (reason, span, counters). All checked by CRC. | Primary |
| **Assumed** | Sample timestamps. The TX reported `late=0` and `missed=0`, so each sample lies within 250 µs of its 1 ms slot. Envelope updates therefore land 50 or 51 samples apart, depending on jitter that was not transmitted. | Structural |
| **Fitted** | The 50/51 update schedule only. It is chosen to agree with the transmitted CONTRAST bits, batch-start envelopes and snapshot spans. The real detector is then checked to update exactly on that schedule (0 violations). | Reconstructed |
| **Checked, not fitted** | Rise/fall/in-pulse bits, per-batch openAborts and rise increments, and snapshot reasons. | Verification |
| **Inferred** | Private detector state (`proven_`, reference levels, branch taken). It is read from the actual R2 header, compiled with `private` mapped to `public` in a host-only translation unit. | Replay inference |

Replay rules:
- Only contiguous received ranges are replayed. Missing radio batches are
  never turned into sample gaps. (The earlier replays did that; it resets
  the detector window.)
- Hidden state is trusted only after an event that pins all of it:
  - a replay reset confirmed by a transmitted `INADEQUATE_CONTRAST` snapshot
    at span below 120, which in R2 implies the ESP32 cleared the same state;
    or
  - two consecutive compatible completions at span 300 or more, which fully
    determine `candidate_` and `proven_`.

Agreement after that point, on the actual R2 detector:

| Capture | Flag words | Batch openAborts increments | Batch rise increments | Snapshot reasons |
|---|---|---|---|---|
| `20260923_ir_r2_bench_radio` | 423,558 / 423,612 | 4,362 / 4,362 | 4,361 / 4,362 | all agree (4,186) |
| `20260923_ir_r2_three_stops_radio` | 151,502 / 151,530 | 1,558 / 1,558 | 1,558 / 1,558 | all agree (1,499) |

The remaining flag differences are edges displaced by a few samples. The
fit cannot always resolve the update phase to the sample, so a threshold
sits one bucket apart. The exception is one near-threshold dip at bench
sample 69735, which the ESP32 counted and the replay did not. It is outside
every stop window, and the test asserts that.

Roll 2 (count 532) is **not replay-certified**. A radio batch is missing in
the middle of the roll (2240736–2240831), and no state-fixing event happens
between that gap and the stop. Its rest level is still an observed fact.

## 2. Question 1: which invalidation paths explain the stops

Times are relative to the last transmitted completion. `q300` is the R2
branch at `IrMovementDetector.h:73-77`. `inv120` is `:49` (span below 120
with no reference, `invalidate()`).

| Stop | Path (replay, actual R2) | openAborts, observed | Reference in force | Rest level (observed median) | Near a plateau by R2's rule? |
|---|---|---|---|---|---|
| count 99, first roll | +24 ms q300 **erases** 832/1231; +474 ms inv120 | +2 (1→2→3) | 832/1231 (**ratcheted**, §4.2) | 888 | yes (dark) |
| count 462, roll 1 | −50 ms q300 **erases** 800/2047; +0 drift completion then q300; +100 ms inv120 | +3 (37→40) | 800/2047 | 1937 | yes (bright) |
| count 532, roll 2 | +0 drift completion, q300 **erases** 880/2063; +50 ms inv120 (uncertified) | +2 (40→42) | 880/2063 | 1587 | **no**: mid-band |
| count 627, roll 3 | +100 ms q300 **erases** 816/2047; +150 ms inv120 | +2 (42→44) | 816/2047 | 1611 | **no**: mid-band |

The per-stop increments in the observed counter match the replay paths
exactly. No other branch was involved:
- no open-pulse timeouts;
- no saturation (the counter stayed 0);
- no sample gaps after startup (the counter stayed 1);
- no holding-mode revocations. R2 never reached holding at these stops.

Each q300 erase lands in the last few hundred milliseconds before quiet.
On a smooth optical waveform the percentile envelope does not jump from
full contrast to quiet. Old samples age out of the 512-sample window one at
a time, so span always passes through 120–300. **R2's retention was
therefore almost unreachable after real motion.** It works only when the
last edges are square enough that fewer than about 25 intermediate samples
remain in the window. The one retained stop recorded on the bench (count 53)
shows that this can happen. Its approach was not captured contiguously, so
it could not be replayed.

## 3. Question 2: separate lifetimes

Yes. They are different facts with different evidence.

- **Measurement continuity** (can counts on either side of an interval be
  subtracted?) must end whenever edge decisions become unreliable. The q300
  band is unreliable: the drift completions in §5.1 are completions made
  from a still signal inside that band. R3 keeps the abort there, so the
  irreversible epoch boundary and the MM-reference invalidation stand
  exactly as in R2.
- **The optical reference** (what the dark and bright surfaces look like to
  this sensor) was learned from steady, full cycles. A fading envelope does
  not contradict it. Only saturation, a sample gap, a sustained departure
  from both plateaus while quiet, or a reboot does.

Continuity can end without forgetting trustworthy levels. The reference
must then only **validate a later still plateau**. It must never bridge the
blind interval, preserve the old epoch, or revive the old MM association.
In the pipeline tests, after an R3 recovery the epoch id is new and the
pre-stop `MmDistanceReference` is invalid (`stop_dark/bright_plateau`,
Codex's `smooth→1200` case).

## 4. Question 4: what real waveforms do to the design

1. **Deceleration always crosses the 120–300 band.** This is the §2
   mechanism. Any design that erases or distrusts the reference in that
   band loses every smooth stop.
2. **Reference ratchet.** This confirms Codex's medium finding, and it is
   more severe once the reference is kept.
   - Each completion may refresh against the previous *candidate*. A
     decelerating wheel produces partial cycles, whose percentiles shrink
     gradually, one compatible step at a time.
   - At the count-99 stop the reference had narrowed to 832/1231, while the
     same roll's full modulation was about 897/2058.
   - "Bright" at 1231 is really 29% of the way up the true range. Its margin
     is only 99 counts.
   - With a retained reference, a mid-edge rest near 1231 would be admitted
     as a plateau stop. That is exactly what R2's acceptance check 4
     forbids.
3. **Threshold drift over a still signal.** The final completion of rolls
   1–3 happened over a still signal: the 5th–95th percentile band of the
   ±100 samples around each edge is 18 counts or less. For roll 1 the signal
   had been settled for about 340 ms. It was caused by live thresholds
   sweeping past the rest level.
4. **ADC outliers.** In quiet stretches, samples more than 100 counts from
   the local level occur about 3,155 times per million (bench) and 655 per
   million (three-stops). They are single samples (1,559 runs of 1, 6 runs
   of 2, never 3). Excursions reach about 450 counts: bench rest 888, with a
   minimum of 655 and a maximum of 1100. With the ratcheted margin, R2-style
   holding would revoke itself about every 0.3 s. Even with a full-range
   margin (about 290), rare excursions above 300 counts still occur (4 per
   million).
5. **Stationary spread.** Quiet snapshots show spans of 15–47 in all but 10
   of about 6,000. That is far below the 120 gate, so the gate leaves room
   for modulation that is not stillness.
6. **Restart from a hold** on a smooth waveform spends many samples between
   plateaus. Revocation, and so one continuity break per restart, is the
   honest outcome. R2 recorded the same thing at restart (openAborts 0→1,
   REACQUIRING, 117.7 s).

## 5. Question 3: evidence for a new valid stationary measurement

All of the following, and nothing less:

1. **Continuity already ended visibly.** The durable `openAborts` increment
   happened before or at the loss, so the blind interval can never be
   bridged. The new stationary report starts a new epoch.
2. **A reference learned from steady cycles.** Two agreeing completions are
   required, as in R2. In addition, each one's cycle must fit twice in the
   window: high phase ≤ 128 ms, and the cycle ≤ 256 ms while continuity is
   intact. Refresh is gated the same way, so deceleration cannot ratchet it.
3. **A still window.** Live span ≤ 63 (four ADC buckets), not merely below
   120.
4. **A level near a learned plateau** under the existing quarter-span rule,
   with thresholds taken from the **reference** while holding.
   - 1–2-sample excursions are counted in a diagnostic and make no edge
     decision.
   - 3 consecutive off-plateau samples revoke, as R2 does.
5. **Re-established phase.** At the dark plateau, armed. At the bright
   plateau, wait for a low first, so a restart never credits a partial tail.
6. **Consumer:** zero comes only from ready endpoints within the new epoch
   (unchanged NAVI code). The old MM reference stays invalid. NAV must
   establish a new one independently.

A mid-band rest stays unavailable. Neither PWM 0 nor Hall may be used to
admit it.

### 5.1 R3 on the captured waveforms (prediction, not observation)

| Stop | R3 outcome (+3 s) | First stationary-ready |
|---|---|---|
| count 99 | `SIGNAL_STALE`, after openAborts +1 | +600 ms |
| count 462 | `SIGNAL_STALE`, after openAborts +2 | +100 ms after the drift completion (the signal had been still for about 340 ms) |
| count 627 | `INADEQUATE_CONTRAST` throughout | never |
| count 532 | not certified; mid-band rest → unavailable under the rule | — |

Across both captures, R3:
- is never stationary-ready within 500 ms of a *motion* edge the ESP32
  transmitted. Drift edges over a still signal are listed separately: 4 in
  the three-stops capture, 0 in the bench capture;
- never counts a completion the ESP32 did not;
- has 1 fewer completion in the bench capture. That is the first cycle
  after a revoked restart, which is visible as an abort.

### 5.2 R3 synthetic cases (`tools/test_ir_r3_proposal.cpp`, real pipeline)

The model uses smooth cosine motion, a deceleration from 150 ms to about
1.5 s periods, ±8-count noise and 260-count single outliers at 3 per 1000.

| Case | Result |
|---|---|
| Stops at the dark and bright plateaus | Valid zero, new epoch, old MM invalid. Reference span 991→1007: no ratchet. |
| Stops mid-band and at quarter-band | 0/30 ready snapshots, unavailable |
| 60 s hold with outliers | Held throughout, 0 aborts, 128 outliers counted |
| 3-sample departure | Revokes, and the abort is counted |
| Restart, 10 cycles from dark / from bright | 9 of 10 / 9 of 9 counted, with an abort. Never a credited tail. |

### 5.3 Protections preserved (`tools/run_ir_r2_bench_review.sh`, both variants)

Every existing suite passes on R3 under ASan/UBSan:
- movement, phase retention, contract, wire, stationary, pipeline, revision
  and adversarial;
- NAVI 0.6 health monitor, speed, coherence and proximal recovery.

Specific protections:
- **Adaptive thresholds in motion:** unchanged.
- **Two-cycle acquisition:** unchanged, plus the steady gate.
- **Durable counters:** `openAborts` semantics unchanged. The new
  `holdOutliers` is a diagnostic only, and not on the wire.
- **Epoch boundaries and frozen-capture rejection:** revision test.
- **Default mode:** 4,000,000-sample differential tests are bit-identical to
  R2 and to 1.5 (`1de06ae^`).
- **Inherited 23/25 lighting-transition undercount:** still 23/25, with one
  concealed ready window, identical to the default detector. It remains
  **open** and is not fixed by this revision.

One existing test changed. In `test_ir_stationary_revision.cpp`, fault 0
was a single 2600-count sample during a hold; it is now a 3-sample
excursion. R3 deliberately treats 1–2 samples as ADC outliers (§4.4). R2
passes both versions.

Codex's `test_ir_stop_decay.cpp` passes R3 in normal mode. It still
reproduces the R2 failure in `--expect-r2-failure` mode.

### 5.4 Residual exposure that R3 introduces (decision required)

`tools/test_ir_fade_turning.cpp` models contrast fading gradually toward
the bright plateau at 5 Hz while the wheel keeps turning:

| Final modulation | R2: stationary-ready snapshots while turning | R3 |
|---|---|---|
| 400, 100, 70 counts | 0 | 0 |
| 40, 20 counts | 0 | **16** |

R2 refuses here only because it erases the reference in the 120–300 band.
That is the same act that broke all four real stops. Once the reference is
kept, a turning wheel whose modulation has faded to about 63 counts or
less, at a learned plateau, looks exactly like a still one on one optical
channel. That is the same class as the known obstruction limit (R2 review
limit 3).

The captured stops carry no usable deceleration signature to separate the
two cases: fall intervals stretched only from about 55 to 70–95 ms before
these hand stops. I have therefore not added a heuristic. The exposure is
bounded:
- the epoch has already ended, so no distance is affected;
- `IrSpeedQualification` still flags zero under PWM > 0 for 3 s.

It still remains for unpowered coasting. **SAM/Codex should accept this
bound explicitly, or reject R3 in favour of keeping R2's unavailability at
real stops.** Physical evidence on how far sunlight compresses contrast
should inform that choice.

## 6. On Codex's `78c84d8`

- **Agree with the High finding and with the qualification that not all
  three pauses are valid stops.** The certified replay confirms it: roll 3's
  rest is mid-band, and roll 2's is mid-band but uncertified.
- **Agree with the Medium ratchet finding, and raise it to High for any
  design that keeps the reference.** See §4.2.
- **Replay integrity.** Codex's replay turned radio gaps into sample gaps
  and used idealized timing: 272 of 44,713 flag words differed, and it did
  not check counters. The replay here uses contiguous ranges and a fitted,
  verified update schedule. It matches every batch abort increment and every
  snapshot reason. Codex's sample indices differ by tens of milliseconds
  from these; the conclusions agree.
- **The proposed separation (reference / phase / continuity) is the design
  implemented in the proposal header.** It adds four elements Codex did not
  specify:
  - the stillness bound;
  - reference-derived hold thresholds (without them, a replay of count 99
    counted **2 false completions** from noise after the stop);
  - the steady refresh gate;
  - the outlier rule.

  It also states the fade-while-turning cost that Codex's proposal does not
  address.
- **Not verified here:** Codex's "first new speed endpoint is WARMUP, not
  zero". These tests assert only that zero appears within the new epoch and
  that the old MM reference is invalid.

## 7. R3 recommendation (narrow)

Opt-in `retainStationary` mode only; the wire format is unchanged. Proposed
diff against R2 is `tools/ir_r3_proposal/IrMovementDetector.h`, with every
change marked `R3:`:
1. The q300 branch and the "quiet but not still" hold count continuity loss
   **without** erasing the reference.
2. The reference is learned and refreshed only from steady cycles.
3. Holding requires span ≤ 63 and uses reference-derived thresholds. After
   continuity loss, phase is re-established from the plateau.
4. Off-plateau runs of 1–2 samples are ignored, and counted, while holding.

The constants (63, 3 samples, 128/256 ms) come from this one bench session.
They are not measured error bounds.

Before any flash:
- SAM/Codex accept or reject §5.4;
- Codex implements from the reviewed proposal;
- this runner passes on the production file;
- a new independent review.

Bench checks to add:
- controlled dark and bright stops after smooth decelerations;
- a mid-edge stop that must stay unavailable;
- restart counts;
- a stationary-noise and outlier log per minute;
- a partial-shade compression while hand-rolling, measuring how small the
  modulation actually gets.

## Reproduction

```sh
tools/run_ir_r2_bench_review.sh   # from the repo root; exit 0 = all gates pass
```

It extracts CRC-checked fixtures from the two committed captures
(`tools/ir_r2_capture_extract.py`). It then builds every suite against a
staged R2 tree and a staged R3-proposal tree. Two new files carry the
replay: `tools/ir_bench_replay.h` (schedule reconstruction) and
`tools/test_ir_r2_bench_replay.cpp` (branch attribution). About 1 minute.
