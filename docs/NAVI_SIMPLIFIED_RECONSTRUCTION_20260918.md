# NAVI_SIMPLIFIED — reconstructed project record

**Date reconstructed:** 2026-09-18

**Status:** Controlling continuation record; firmware work paused
**Authority:** Operator rulings override chat summaries. The prospectus remains the governing architecture unless a later operator ruling expressly adds specificity.

## Why this record exists

Important work was split among Codex tasks, a ChatGPT/Sam conversation, Claude analysis, and untracked source. This record reconstructs the accepted state without pretending that every historical statement remained valid. It separates verified facts, accepted decisions, superseded proposals, and open engineering work.

## Evidence recovered

- Governing prospectus: `docs/NGR NAVI_SIMPLIFIED.md`, current name established by commit `c16553e`.
- Original Codex implementation reconciliation, recovered from the saved response in Codex task `01a0b3ff-f9f6-7951-b8a1-cb2b3c2edf9a` and summarized in the companion recovered-reports document.
- Codex post-reconciliation report and pre-implementation contract, recovered from the same task.
- Sam's 22-point decision summary, recovered from ChatGPT conversation `6aad0f72-ba6c-83ea-80f1-1ecdc8f9ed2a`.
- Experimental lineage and `NAVI_ONE_SIMPLE` provenance, recovered from Codex task `01a0abd7-8c1f-7ca3-83dd-c982a5824e95`.
- Claude's committed review: `docs/NAVI_SIMPLIFIED_REVIEW_CLAUDE_20260918.md` at commits `e5b586c` and `e658045`.
- Committed Hall, baseline, ramp, and PWM analyses listed in the references below.
- Dashboard screenshots supplied by the operator showing the failed first run.
- The locally retained, untracked `firmware/test-programs/NAVI_SIMPLIFIED/` source and build artifacts.

The duplicate Codex task `01a0b767-ef0e-71b1-87b8-06e240b34f8d` is a forked snapshot ending at the operator's “50 first dates” message. It contributes no unique later work.

## Current accepted design state

1. The architecture is `DETECTION → HARD PROTECTION → TRANSPARENT JUDGMENT`. Operational test termination is a separate layer.
2. Detection is two consecutive Hall samples at least 70 counts from baseline, sampled at 1 kHz. Declare on sample two and capture opening polarity.
3. Closure, waveform completion, morphology, peak shape, stitching, and diagnostic classification have no navigation authority.
4. Electrical rearm is separate from physical same-magnet protection. Electrical quiet or return to baseline may rearm the detector but cannot prove spatial clearance.
5. Physical protection begins at detection and survives stop, dwell, restart, and unresolved navigation state.
6. Hard Protection suppresses only an observation that cannot physically be the next mapped magnet.
7. Hard Protection must use conservative evidence specific to locomotive, direction, and mapped interval. Its timer starts at the prior detection. A global 400 mm/s rule, a global 1000 mm/s rule, and a railway-wide PWM-120 envelope are rejected.
8. Missing or unreliable interval evidence must produce a permissive fallback: do not claim physical impossibility. PWM and measured speed may support reasoning but do not independently manufacture a hard impossibility claim.
9. A physically possible contradiction proceeds to Transparent Judgment before position changes. Judgment must expose observation, evidence, alternatives, evaluation, decision, and action.
10. Temporary ambiguity is allowed. The system must not invent a position and must not stop merely because the first contradiction is unresolved.
11. The ambiguity-opening observation is count 0. Up to ten subsequent qualifying NAV observations may be judged. Resolution on observation 10 succeeds; failure to resolve after judging observation 10 produces a controlled stop because further autonomous testing has no useful value.
12. Same-magnet suppressions, physically impossible events, stationary/zero-transition events, diagnostics, duplicate publications, and telemetry copies do not count toward the ten observations.
13. `(boot_id,event_serial)` follows each opening through detection, suppression, judgment, action, and diagnostics. Loss and suppression accounting must be reconstructable.
14. Direction disagreement is evidence, not an automatic position correction. Session/declaration direction, motor direction, and NAV direction remain visible.
15. Station arrival is a programmed deceleration ramp, not “creep.” A locomotive may stop on a magnet. A quiet 200 ms Hall window does not prove spatial clearance.
16. The operator's later direct station ruling controls: **when position is unsure, skip the station stop** so continued motion can help reestablish location. Do not arm a new position-dependent stop while unresolved. The exact safe cancellation behavior for a ramp already in progress must be specified and tested before implementation; do not silently reinterpret “skip” in code.
17. Station dwell is five seconds unless a later operational reason changes it. Older 30-second comments are stale.
18. Station departure must ramp upward at 200 ms per PWM count. The failed build put the 200 ms value in the down-rate slot and actually departed at 62 ms/count.
19. IR may later measure movement and distance. It does not navigate.
20. Historical mixed-firmware logs generate hypotheses and engineering bounds. Runs of the new architecture are the primary evidence for revising its behavior.
21. The dashboard must continue to show the exact running sketch. Every firmware must publish an unambiguous sketch/build identity on retained `state/bootid` after boot and every MQTT reconnect; the regular heartbeat should carry it as a fallback.

## Superseded or rejected proposals

- Five-sample detection for NAVI_SIMPLIFIED.
- Requiring Hall closure before physical protection releases a magnet.
- Treating a quiet sample window as proof of movement or spatial clearance.
- The legacy closure-anchored 500 ms rebound rule as Hard Protection.
- The X20/X21 645 ms guard as the final physical rule.
- A universal `Vmax = 400 mm/s`.
- A universal `Vmax = 1000 mm/s` derived from a broad drivetrain estimate.
- Applying the PWM-120 envelope to the whole railway; PWM 120 is used on one uphill section and does not describe every interval.
- Immediate strike or immediate stop on unexpected polarity.
- Automatic restoration of QUORUM or an opaque classifier.
- Cancelling the recovery count with suppressed rereads included.
- The older rule that an already committed station operation necessarily continues through ambiguity; the later operator ruling is to skip the stop, subject to defining a safe cancellation mechanism.

## The failed SIMPLIFIED railway run

### Observed facts

- The operator intended to flash `NAVI_SIMPLIFIED` to Otto and verified that an earlier attempt had actually left `NAVI_ONE_RECORDER` running.
- On the subsequent run, Otto travelled one physical magnet and stopped.
- The dashboard displayed: `NAVI unresolved after 10 further magnets. Declare position.`
- Otto had not physically travelled ten additional mapped magnets.
- The source directory was untracked. The build announced `NAVI_SIMPLIFIED_DEV_20260918` but its ordinary alert was nearly indistinguishable from X18; `state/bootid` was the only authoritative build identity and the dashboard showed `SW ?`.

### Root-cause account

The most strongly supported explanation is the interaction of Claude findings 3 and 11:

1. The detector had no claimed 500 ms rebound guard. After 30 below-threshold samples, a second opening could be emitted about 32 ms after close.
2. The first repeated opening from the same physical magnet could enter Judgment after the short, incorrectly permissive reachability interval.
3. Once that contradiction made position unresolved, the implementation disabled physical reachability because it required `navigator.positionKnown()`.
4. Further rereads of the same physical magnet entered the ten-observation recovery word.
5. Ten such rereads triggered `UNRESOLVED_LIMIT_STOP` even though ten new mapped magnets had not been reached.

This explanation is consistent with the code and the observed dashboard state. It remains an inference because the complete event stream from that railway run has not yet been recovered and correlated by event ID.

### Verdict

The tested build is rejected. **Do not flash or promote it.** Its controlled stop was fail-safe in form but triggered by invalid evidence. No incremental railway patch is authorized.

## Open engineering work before implementation

1. Recover the per-locomotive, per-direction, per-mapped-interval timing dataset and define a conservative ceiling and margin for every usable interval.
2. Specify the permissive fallback for missing, stale, reversal-affected, or otherwise unreliable interval evidence.
3. Specify electrical rearm without giving closure navigation authority.
4. Specify how the physical anchor survives stop, restart, declaration, direction changes, and unresolved state.
5. Specify exactly how “skip the station stop” safely cancels or prevents each station phase, especially a zero ramp already underway.
6. Reconcile every Claude finding before modifying firmware.
7. Put the candidate source, tests, review response, and build identity under version control before another flash.

## Mandatory pre-railway tests

- Two-sample detection, sample-two timestamp, and opening polarity.
- Sustained excursion and electrical rearm.
- PWM-zero transition and a candidate spanning the transition to zero.
- Stop and restart while physically over a magnet.
- Physical-protection strict boundaries, equality, wraparound, every mapped interval, and permissive fallback.
- Physical protection remaining active throughout ambiguity.
- Ambiguity count 0 through 10, including resolution on observation 10.
- Exclusion of suppressed rereads, stationary events, diagnostics, duplicate publications, and lost events from the recovery count.
- Direction discrepancies and explicit redeclaration.
- Station behavior through every phase under the operator's “skip when unsure” ruling.
- Five-second dwell and 200 ms/count departure ramp.
- Queue overflow, telemetry loss, multipart loss, counter behavior, and event-ID integrity.
- Sketch identity at boot, reconnect, heartbeat, and dashboard display.

## Closest recoverable source and binary evidence

The retained directory is the closest recoverable candidate, not yet proven to be the exact flashed source. Source mtimes are approximately 11:07–11:17 PDT; retained build artifacts are dated 17:06 PDT. A rebuild or second flash may have occurred between those times. Exact equivalence requires the flash log/cache or a readback from Otto before another flash.

Key SHA-256 values:

| Artifact | SHA-256 |
|---|---|
| `NAVI_SIMPLIFIED.ino` | `dbb4dd4cd2cca39bca87d7c38598e2b61527ea4b8a25e650d7af3e7aeca59259` |
| `SimpleHall.h` | `23bd4521d0b0dd4c0a989e040d6560dd37810ac90ad4e72b76decb52dd013639` |
| `SimpleNavigator.h` | `7ddd19f4542fc5b2abc4d85ff8793bd1fffd612fe88330708c55341e29337efa` |
| `Reachability.h` | `1f25b3e2ec5f57ba61e2618612557e3831293c1b102dd223b0832032741876db` |
| `Stations.h` | `7f9741e66b28ab6387a34bba45e00f6a9c8f9145f7926afa9ea6f4be970fc593` |
| `test_navigation.cpp` | `b467ea286e495ad595ed885ae925abc04c220e8dc987e783871748159d8d1f6e` |
| retained application `.bin` | `9b9cbdf494b99fb6ae0647ce17ba8f2128ac52b0305fa02f8866d1a3e3426b88` |
| retained merged `.bin` | `2a2b426edd9957c4aeaed13286f396924e65484919f798377635f5e05c37c1d9` |
| retained `.elf` | `245451d63664bbb6db2da7f1f1117100a95fec34ab835c074d5d3c108d064e5c` |

## References

- `docs/NGR NAVI_SIMPLIFIED.md`
- `docs/NAVI_SIMPLIFIED_REVIEW_CLAUDE_20260918.md`
- `docs/NAVI_VMAX_PWM90_20260918.md`
- `docs/HALL_SELECTIVITY_AND_GATE_TIMING_FROM_CAL_20260630.md`
- `docs/NAVI_BASELINE_TIMING_20260916_C3B93D0B.md`
- `docs/NAVI_BASELINE_DRIFT_20260916_C3B93D0B.md`
- `docs/NAVI_BASELINE_ELIGIBILITY_20260916_C3B93D0B.md`
- `docs/NAVI_BASELINE_PWM_SAMPLING_GATE_20260916_C3B93D0B.md`
- `docs/NAVI_BASELINE_STOP_RECOVERY_20260916_C3B93D0B.md`
- `docs/NAVI_RAMP_TRAVEL_FROM_RUNLOGS.md`
- `docs/NAVI_RAMP_TRAVEL_TEMPLATES_20260916_C3B93D0B.md`

## Pause condition

Do not change or flash NAVI_SIMPLIFIED until the interval-specific Hard Protection design is recovered and reviewed, the station cancellation semantics are explicit, Claude's findings have dispositions and tests, and the candidate is version controlled.
