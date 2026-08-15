# QUORUM 1.16 implementation report — quarantine, suffix rescue, self-resolution

**Status: built as `QUORUM_1_16`, revised to `QUORUM_1_16R` after CODEX's
seven-finding review of 2026-08-14 (all seven accepted; response:
`docs/QUORUM_1_16R_REVIEW_RESPONSE.md`), harness-proven on every capture and
an adversarial set, both locomotive profiles compiling clean (999,019 bytes,
76%). CODEX re-review closed 2026-08-15: approved for SUPERVISED field
testing in the CTO Bubble (attended only); flashing to Otto and Toby began
the same day. Field verdict pending.**
Controlling record: decision 0035 (Proposed). Operator authorization:
2026-08-14, "Do the work."

## What was built

Three mechanisms, one per layer of CODEX's diagnosis:

| mechanism | where | what |
|---|---|---|
| §3Q Quarantine | `navOnMarker()` wrapper above the gate ladder | 350 ms physical floor (decisive) + steady-band conjunction (corroborating); one-deep pending slot; successor arbitrates by map polarity under both hypotheses; discard folds the interval into the successor's dt; commit replays the pending event through the unchanged ladder |
| §2.4Q Suffix rescue | `decideEvaluation()` at the hard bound | newest-7 ring suffix tested per candidate; exactly one full fit → adopt (`QUORUM_SUFFIX_RESCUE` + ordinary adoption); else terminal exactly as before |
| §2.5Q Self-resolution | `acceptEvent()` NAV_NO_QUORUM branch | after 12 fresh events, route-wide unique window match (`dnaMatchWide`, not the ±5 fenced advisory) held across three consistent matches in all → relabel, ring rebased, `SELF_RESOLVED`, state NORMAL — and since 1.16R, **AUTO is dropped by the resolution itself** (the resume interlock, finding 1) |

Derived constants, every one from measurement (see the proposal for the
distributions): `Q_FLOOR_MS 350` (280 mm / 350 ms = 800 mm/s, **1.81×** the
441 mm/s demonstrated maximum), `Q_INT_RATIO 0.40`, `Q_FLUX_RATIO 0.55`,
`Q_DUR_RATIO 3.5`, steady band 600–4000 ms (the PWM ≥ 40 condition was
removed in 1.16R on finding 5 — PWM is a request, not a measurement),
`SUFFIX_RESCUE_N 7` (since 1.16R proven the minimum unambiguous suffix
length on NGR_DNA1, not a probability estimate), `NQ_CONFIRM_N 3`
(three consistent matches in all), medians over 11 accepted events.

Lifecycle: declaration clears pending, medians and the learning pass;
entering NO_QUORUM restarts the fresh-event clock; a direction change
discards a pending event (`DIRECTION_CHANGED`) — the arbitration frame died
with the reversal.

Telemetry: `QUARANTINED` / `QUARANTINE_DISCARDED` / `QUARANTINE_COMMITTED`
(with polarity, peak, duration, dt, verdict reason and running counters),
`QUORUM_SUFFIX_RESCUE`, `SELF_RESOLVED` (old/new mm, fresh count, confirms).
A quarantined event's marker line publishes gate `QUARANTINED`. Nothing is
erased: the operator's dig signal is richer than before.

## Verification

1. **Both profiles compile clean** under `--warnings all` (998,859 bytes, 76%).
2. **Fidelity first**: 12 boot-session segments extracted from the complete
   2026-08-13/14 captures (`extract_session.py`, per-locomotive, split on
   uptime regression); 10 verified byte-perfect against the OLD binary —
   zero odometer divergence, decision streams identical (up to 1,393 events).
   The 2 failures are runlog reception gaps, excluded with the reason logged.
3. **Old-vs-new stateful diff** over the 10 verified segments
   (`statediff.py`; 1.16 enumeration in
   `docs/QUORUM_1_16_STATEFUL_DIFF_ENUMERATION.txt`, re-run after the
   review fixes as `docs/QUORUM_1_16R_STATEFUL_DIFF_ENUMERATION.txt`):
   5,544 markers, 17 quarantined, 17 discarded, 0 committed, zero adverse
   changes; the 17:21 terminal eliminated; three segments byte-identical.
   The only capture-level change the review fixes introduced outside the
   2026-08-10 goldens, on the ten fidelity-verified segments, is
   toby_0813_s02 (excluded toby_0813_s03 improves the same way): two events at PWM 24 and 19 with
   20-second durations, exempt from the conjunction under the old
   `pwm >= 40` condition, are now quarantined and discarded — finding 5
   paying out on real data (disagree 24 → 4, same final position). The
   first draft's totals (5,644 / 14 / 14) did not match the enumeration
   file itself (5,544 / 15 / 15 pre-review); corrected against the file.
4. **The 2026-08-10 capture goldens** (pinned in the suite, re-measured
   for 1.16R): A and B prevented; the mm 66–82 stretch still commits 2
   wrongly-held genuine events back (reversibility on real data) — and
   finding 2 was LIVE here: pre-fix, the mm 85 commit was committed and
   then killed by the legacy conservation gate one line later. With the
   vouched bypass it lands; its genuine successor is then eaten by that
   same legacy gate (246+1222 ms sums into the reject band — 0024's
   documented defect, biting the other event of the pair; count identical,
   ring polarity different), and the second pass through the stretch now
   ends in an honest HARD_BOUND terminal at mm 87, cleared four events
   later by the operator's declare already present in the record. Both terminals
   are pinned board-by-board. C stays byte-identical at its terminal,
   advisory 18 intact, healed by SELF_RESOLVED 13 → 8 on the session's
   final event (fresh = 17); final NORMAL at mm 8 exactly as before; and
   **input-invariance** holds — with or without the Bamboo phantoms, same
   final position, and the mm 87 terminal appears in both branches
   (phantom-independent, as it must be).
5. **30 synthetic fixtures green** (the first draft of this report said 24
   while the generator held 23 — finding 7): the original adversarials —
   max acceleration (quarantine silent; the legacy 0024 gate's cost pinned
   honestly), missed-then-accel (+1 adopted), dt-fold, double phantom (now
   also pinning the `SUCCESSOR_SUSPECT` verdict order), reinstatement
   proof, reversal mid-quarantine, forced-terminal self-resolution — plus
   the 1.16R review set: suffix rescue fired for real (CW straddle, CCW,
   CW across the 170/000 wrap; every start swept, none hand-picked), the
   rescue refusing an excluded candidate on a perfect 12-entry suffix, the
   vouched commit surviving the legacy gate (fails on the pre-fix build at
   exactly 4 of 26 polarity-eligible starts — spacing-dependent, which is
   the arbitrariness that condemns the model), the slow phantom family,
   and the resume interlock (`auto` goes in 1, must come out 0). The
   strict CCW sweep found no wrap-crossing rescue on this map, so CCW and
   the wrap are covered by separate fixtures — stated, not implied away.
6. **Era-aware suite**: `run_suite.py` branches on constants the harness now
   reports (`q_floor_ms` etc.); legacy mode re-verified green against the
   frozen pre-1.16 binary — the historical assertions are preserved, not
   overwritten.

## Honest limits

- **Identity errors are not fixed** (weak-first, on-time, wrong-polarity:
  Toby Event B). `syn_pair_weak_then_strong` asserts the limitation. The
  recovery for that class is self-resolution, which incident C demonstrates.
- The **legacy conservation gate is untouched** and still rejects one genuine
  marker under maximum acceleration (0024's defect, pinned by
  `syn_adv_accel_max`). Removing it in favour of quarantine alone is separate
  work with its own record.
- The conjunction is calibrated on **steady running** and is gated to it; in
  crawl regimes only the physical floor applies. A crawl double slower than
  the floor AND outside the steady band would still be admitted — the 17:21
  doubles were inside the band, but the gate is stated, not assumed.
- Harness caveats as ever: single instance, deterministic timing, LAYER 2
  (the analog detector) not exercised, ESP-NOW inert.
- Version numbering: 1.15 remains the CTO mode expansion
  (`agent/cto-mode-1-15`); the 1.14-era note reserving 1.15 for transport
  work is stale — transport moves to 1.17.

## Deployment gate

Operator + CODEX review of decision 0035 and this report; then supervised
track time with the packet log watched for `QUARANTINED` verdicts — each is
also a dig pointer per the phantom-source maintenance record (0025 on
`agent/phantom-verdict-20260812`; this branch's 0025 is the console-roster
record — renumbering queued). Self-resolution's first field exercise
should be deliberate: force a terminal on the bench (fixture command), let it
recover, confirm the console shows the relabel.
