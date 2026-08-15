# QUORUM 1.16 implementation report — quarantine, suffix rescue, self-resolution

**Status: built, harness-proven on every capture and an adversarial set, both
locomotive profiles compiling clean. NOT flashed, NOT field-tested.**
Controlling record: decision 0035 (Proposed). Operator authorization:
2026-08-14, "Do the work."

## What was built

Three mechanisms, one per layer of CODEX's diagnosis:

| mechanism | where | what |
|---|---|---|
| §3Q Quarantine | `navOnMarker()` wrapper above the gate ladder | 350 ms physical floor (decisive) + steady-band conjunction (corroborating); one-deep pending slot; successor arbitrates by map polarity under both hypotheses; discard folds the interval into the successor's dt; commit replays the pending event through the unchanged ladder |
| §2.4Q Suffix rescue | `decideEvaluation()` at the hard bound | newest-7 ring suffix tested per candidate; exactly one full fit → adopt (`QUORUM_SUFFIX_RESCUE` + ordinary adoption); else terminal exactly as before |
| §2.5Q Self-resolution | `acceptEvent()` NAV_NO_QUORUM branch | after 12 fresh events, route-wide unique window match (`dnaMatchWide`, not the ±5 fenced advisory) + 3 consistent confirmations → relabel, ring rebased, `SELF_RESOLVED`, state NORMAL. AUTO stays dropped |

Derived constants, every one from measurement (see the proposal for the
distributions): `Q_FLOOR_MS 350`, `Q_INT_RATIO 0.40`, `Q_FLUX_RATIO 0.55`,
`Q_DUR_RATIO 3.5`, steady band 600–4000 ms / PWM ≥ 40, `SUFFIX_RESCUE_N 7`,
`NQ_CONFIRM_N 3`, medians over 11 accepted events.

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
   (`statediff.py`; full enumeration committed as
   `docs/QUORUM_1_16_STATEFUL_DIFF_ENUMERATION.txt`): 5,644 markers,
   14 quarantined, 14 discarded, 0 committed, zero adverse changes; the
   17:21 terminal eliminated; four segments byte-identical.
4. **The 2026-08-10 capture goldens** (pinned in the suite): A and B
   prevented; the mm 66–82 stretch commits 2 wrongly-held genuine events back
   (reversibility on real data); C byte-identical at the terminal, advisory
   18 intact, healed by SELF_RESOLVED −5; final NORMAL at mm 8; and
   **input-invariance** — with or without the Bamboo phantoms, same final
   position.
5. **24 synthetic fixtures green**, including the 7 adversarials: max
   acceleration (quarantine silent; the legacy 0024 gate's cost pinned
   honestly), missed-then-accel (+1 adopted), dt-fold, double phantom,
   reinstatement proof, reversal mid-quarantine, forced-terminal
   self-resolution end-to-end.
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
also a dig pointer per decision 0025. Self-resolution's first field exercise
should be deliberate: force a terminal on the bench (fixture command), let it
recover, confirm the console shows the relabel.
