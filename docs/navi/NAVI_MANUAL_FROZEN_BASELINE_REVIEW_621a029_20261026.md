# NAVI Manual sketch — independent review of `621a029`

- Branch: `experiment/navi-manual-frozen-baseline-20261026`
- Commit: `621a029` ("NAVI: freeze baseline at timing gate and qualify MM directly at Hall opening"), read together with `cc540c0` and `b75d3ae`
- Sketch: `firmware/test-programs/NAVI_POWER_OF_NORMAL_MANUAL_ONLY/`
- Reviewer: Claude, 2026-10-26. This is a review only. No code was changed and nothing was flashed.
- All line numbers refer to the files as they are at `621a029`.

## Compilation

`arduino-cli compile --fqbn esp32:esp32:esp32 --warnings all` (core 3.3.12, real libraries, `credentials.example.h` copied to `credentials.h`) **builds cleanly** for the default `NAVI_LOCO=9950011` (Otto): 989 411 bytes of flash (75%), 61 580 bytes of RAM. None of the sketch's own files raise a warning. The Toby build (`9950012`) was **not** compiled.

## Compliance with the nine decisions

| # | Decision | Verdict |
|---|---|---|
| 1 | One expected next magnet, from last confirmed position, direction and map | **Complies.** `NaviTarget.h:86-88`. All 342 directed PWM-90 profiles exist (171 × 2, no duplicates), so `lowerGateMs_()` never returns 0 on the route. |
| 2 | 70 counts for two consecutive samples; timestamp at the second | **Timestamp complies** (`NaviCandidate.h:59`). The word "native" needs a ruling: see finding L1. |
| 3 | Previous median kept while waiting for return to baseline | **Complies.** `NaviBaseline.h:31-34`: a Frozen median is not recomputed, and `level_` stays in use for the return test. |
| 4 | Return confirmed for 200 ms before collecting | **Complies, counted in blocks:** 10 × 20 ms blocks (`NaviBaseline.h:12,22`). The confirming blocks are not added to the median. See L3. |
| 5 | Fresh median frozen when the PWM-90 gate opens; keep the previous one if no fresh samples | **Complies for every target after the first** (`.ino:402-405`, `NaviBaseline.h:37-42`). **Does not apply to the first target**, where the boot median is never frozen: finding M2. |
| 6 | Window has no upper time limit | **Complies.** `NaviTarget.h:59-65` sets no upper bound. |
| 7 | Fresh opening with expected polarity inside the window confirms immediately; closure and width not required | **Not met: a magnetic excursion already in progress can be accepted as a fresh opening** (H1). Gate-edge latency also affects this decision (M1). |
| 8 | Nonqualifying observations leave position unchanged; no search or recovery | **Complies** (`NaviTarget.h:86`, `.ino:357-359`). One consequence is a permanent stall: H2. |
| 9 | AUTO/GO disabled | **Complies.** `AUTO_GO_PERMITTED = false` (`.ino:64`). Both `auto` (`.ino:643`) and `go` (`.ino:655`) are refused before `autoRunning` or `autoEnrolled` can become true. `serviceAuto` is unreachable in practice. |

## Findings by severity

"Confirmed defect" means the code demonstrably behaves this way, deterministically. "Risk" means it depends on a physical condition that has not been shown to occur on NGR.

### H1 — An excursion already active when the gate opens is reported as a fresh opening (confirmed defect)

`.ino:307` calls `candidate.reset()` on every sample while the gate is closed. `reset()` clears `excursionActive_` and `pendingCross_` (`NaviCandidate.h:48`). If the Hall signal is already ≥70 counts off the frozen median when the gate opens, the first two evaluated samples both cross. Because `excursionActive_` is false, `NaviCandidate.h:58-63` emits an onset about 1–2 ms after the gate opens. Its timestamp is the gate time, not the time the magnet actually opened.

The reset discards the only state that could tell a fresh departure from one already in progress. The behaviour is the same whether the excursion is a magnet or a sustained offset.

Consequences, if it happens:
- The opening is inside the window, so a magnet whose true opening came **before** the minimum-travel gate is confirmed. Decisions 5 and 7 intend that opening to be nonqualifying.
- The anchor (`confirmedOpenUs_`) becomes the gate time. Every later gate is shifted by the same error.
- A retained stale median (decision 5's fallback), or a Hall step of 70 counts or more, is confirmed as the expected magnet at every gate whose polarity matches. Otto showed +80..+150 background steps on 2026-10-25, later fixed by a wiring tweak.

How plausible: the gate opens at `lowerFactor` × survey base (`NaviTarget.h:103-104`). `lowerFactor` is the **fastest interval observed** in the survey, so any interval faster than the survey's fastest triggers this. Declaring position while parked over a magnet does too: the first target has no anchor, so its window is open immediately (`NaviTarget.h:61`). How often either happens on the track is not known. The code path itself is certain.

Possible correction, for David to decide; no new navigation state is needed. Do not reset the detector on gate transitions. Alternatively, keep `excursionActive_ = true` through the reset whenever the current sample already crosses, so that an onset needs a return below threshold first. Either way, only a departure that starts inside the window can produce an onset.

### H2 — A first confirmation at a PWM other than 90 halts navigation silently and permanently (confirmed defect)

For the first target after a declaration (no anchor), `timingOk = !hasAnchor || …` (`NaviTarget.h:84`) accepts **any** PWM. The confirmation stores `confirmedOpenPwm_ = e.pwmAtOpen` (`:87`). From then on, `windowOpen()` returns false whenever `confirmedOpenPwm_ != 90` (`:62`). The gate never opens again, so the detector never runs (`.ino:308`). No onsets, no NONQUALIFYING events and no warning are produced. State still reports `TRACKING` (`.ino:417`). Only a new declaration recovers it.

How plausible: very. The manual ramp is 150 ms per count (`.ino:79`), so going from 0 to 90 takes 13.5 s. A declaration made at standstill, followed by throttle-up, will usually meet the first magnet during the ramp.

Correction for David to choose. Option (a): apply the same PWM-90 requirement to the first target. This does not change the windowless first gate. Option (b): stop storing an anchor that can never open a gate. Option (a) is the smaller change and matches the "PWM-90 gate" wording.

### M1 — The gate edge lags by up to one rvmTask loop (confirmed; impact is a risk)

`rvmTask` checks `windowOpen(now)` only once per loop (`.ino:402`). That loop runs after a block arrives, which is every ~20 ms, and waits up to 100 ms if blocks stall (`.ino:386`). The detector therefore starts 0–20 ms (worst case 100 ms) after the mapped lower bound.

An opening that falls in that slice either goes undetected, or, combined with H1, is confirmed at the late gate time. Either way the opening itself is inside the window according to `onOnset` (`NaviTarget.h:84`), but the detector was not running.

At PWM 90 a 20 ms lag is small compared with ~1 s segments. It matters only when openings land close to the lower bound, which is exactly where H1 applies. Fixing H1 limits the damage from M1 to "late detection", never "wrong timestamp".

### M2 — The first target's boot median is never frozen and keeps changing while identification is live (confirmed discrepancy with decision 5)

`.ino:404` skips `freezeAtGate()` during Boot, and `freezeAtGate()` refuses Boot as well (`NaviBaseline.h:40`). In Boot, `onBlock` adds every block taken at PWM 0 (`NaviBaseline.h:19`) and recomputes the median every time. So for the first target the reference moves while the detector is armed, whenever the train stands still.

Whether the first target should be an exception to decision 5 has not been ruled on.

Related low-level point: `append_` writes `buf_[count_ % CAPACITY]`, but `nth_element` reorders `buf_` in place (`NaviBaseline.h:49,56`). Once 2048 boot blocks have been collected (≈41 s stationary), each new sample overwrites an arbitrary element, not the oldest. The boot median is then a median of a randomly thinned history, not of the last 2048 blocks. During Collect this cannot happen: it adds only at PWM > 0 and lasts about one segment.

### M3 — Published evidence misrepresents what was measured (confirmed)

- `judgeOnset` hard-codes `ev.e.meetsMinWidth = true` (`.ino:355`). `diag/navi_encounter` and `mm/marker` then publish `meets_min_width:1, width_blocks:0, peak_adc:0.0` (`.ino:459-466`) for an event where width and peak were never measured.
- `encounter_active` is always 0 (`.ino:329`).
- `timing_calibration_ready`, `timing_predicted_ms` and `timing_upper_ms` are always 0, because `onOnset` never sets them.

Under AGENTS.md §5, field records should show "not measured", not a value. Log readers and replay tools may take these for real measurements.

### L1 — "Two consecutive native samples" versus 1 kHz evaluation (needs a ruling), and stride phase now drifts (confirmed)

The detector evaluates every fifth 5 kHz read (`NaviCandidate.h:32,51`). "Consecutive" therefore means 1 ms apart, at the rule's own 1 kHz cadence, not 200 µs apart in native reads. This matches the established 70×2 rule (docs/NAVI_RVM_70_COUNT_GATE_20261020.md). If decision 2's "native" means 5 kHz, the code does not comply. David should say which is meant.

New in this revision: `onSample` is called only while the gate is open (`.ino:308`), and `strideCounter_` advances only inside `onSample`. Which serial phase gets evaluated therefore changes with each gate period, instead of staying fixed at `serial % 5 == 1`. Host replays aligned to the old fixed phase will be offset by up to 4 ms.

### L2 — Comments that no longer match the code (confirmed)

- `NaviCandidate.h:10` says "the first of the two samples is the true opening instant". The struct (`:35-36`) and decision 2 both use the second sample.
- `NaviCandidate.h:12-21` says "no lock" and "nothing … can be suspended". The detector is now reset and suspended by the gate.
- `.ino:27-33` and `.ino:305-306` describe encounter packaging and width qualification, and say "gated only on whether a baseline level exists". Neither is true any more.
- `NaviTarget.h:17-28` lists width as a qualifier.
- `encounter` (`.ino:158`) is still constructed but unused.

### L3 — Parameters not covered by the nine decisions (unverified assumptions)

- The return band is ±20 ADC (`BASELINE_ENTRY_MARGIN_ADC`, `NaviBaseline.h:11`). No decision fixes it.
- "200 ms" is counted as 10 *received* blocks. A dropped block does not count, and a NaN block restarts the count (`NaviBaseline.h:18`). Both make the wait longer, never shorter.
- If the Hall never returns within ±20 of the retained median (zero drift, a background step), the tracker stays in AwaitReturn. Every gate then freezes the stale median, which is the input that triggers H1.

## Concurrency: items checked and found sound

- **Order of publication.** `rvmTask` stores `sharedBaselineLevel` before `sharedIdentificationGate` (`.ino:407-409`, seq_cst atomics). `hallTask`, which runs at higher priority on the same core, can never see the gate open together with the previous level.
- **Stale and echo onsets.** After a confirmation, `judgeOnset` closes the gate at once (`.ino:363`). Any onsets still in `onsetQ` are rejected by the `sharedIdentificationGate.load()` check (`.ino:392`). Onsets from before a new anchor also fail `windowOpen(onset.openUs)`. Onsets queued before a declaration or suspension are rejected because `applyControl` closes the gate first (`.ino:371,376`).
- **Two reads of the gate per sample** (`.ino:307-308`). Interleaving between them is harmless: at worst one sample is reset or skipped.
- **After confirmation**, the gate stays closed until the next lower bound, because `windowOpen` is re-evaluated against the new anchor (`.ino:402`).

## Consequence of the decisions (not a defect)

Under AGENTS.md and the operator's rule that decisions must state their implications: because there is no upper limit (6), no recovery (8), and arrivals must be at PWM 90, a missed or nonqualifying expected magnet stays the target. Examples are a magnet reached during a ramp after a stop, or one opening before the gate. The **next** magnet of the same polarity that opens at PWM 90 inside the window is then confirmed *as* the missed one. Geography is then off by one or more magnets and nothing reports it. This follows directly from decisions 6 and 8 as written; it is recorded here as an implication, not proposed as a change.

## Summary

| Sev | ID | Kind | Location |
|---|---|---|---|
| High | H1 | Confirmed defect | `.ino:307`, `NaviCandidate.h:48,58-63` |
| High | H2 | Confirmed defect | `NaviTarget.h:62,84,87` |
| Medium | M1 | Confirmed latency; impact is a risk | `.ino:386,402` |
| Medium | M2 | Discrepancy with decision 5 / needs ruling | `.ino:404`, `NaviBaseline.h:19,40,49,56` |
| Medium | M3 | Confirmed (telemetry) | `.ino:329,355,459-466` |
| Low | L1 | Ruling needed + confirmed replay drift | `NaviCandidate.h:32,51`, `.ino:308` |
| Low | L2 | Stale comments | as listed |
| Low | L3 | Unratified parameters | `NaviBaseline.h:11-12,18` |

Before track testing, H1 and H2 should be resolved, because each one deterministically produces a wrong or absent navigation result. The others can be recorded and testing can go ahead.
