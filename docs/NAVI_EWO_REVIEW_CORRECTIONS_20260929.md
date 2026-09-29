# Integrated EWO R2 — authorized review corrections (historical)

This review record remains preserved as historical evidence. Its five-position
boot-reference correction and open post–Missed Magnet origin question were
superseded by current decision 0115 during final cleanup on 2026-09-29.

Status: development candidate; not flashed, not field accepted. David and Sam
accepted the do-not-flash review of `cd55929c5f95c2d41bc00771c97641938612bd8a`
and authorized these corrections, commit/push, then fresh review. The isolated
worktree starts at governing-document commit
`612b791ee90e2404ea8febded675edbd622e063e`; its firmware is the reviewed candidate.
The dirty governing checkout at `638c635` is not used as implementation source.

## Corrections and boundaries

| Finding | Correction | Regression |
|---|---|---|
| B1 | Pass explicit judgment time; use latest applicable received IR rather than Hall backlog time for applicability | Healthy 40-mm travel cannot confirm a 330-mm target; coherent later IR can |
| B2 | Serialized monotonic command arrival IDs; assertion latch independent of queue insertion; release must be newer | Full 16-command queue, dropped assertion, stale releases, reverse consumption order, later valid release |
| M1/M2 | Five distinct observed pulse positions; exact NAVI-owned histogram median per position; equal spatial weight; fixed allocation | Initial 0→2 jump at 9.652-mm pitch, repeated counts, 10,000 stationary observations, two contaminated representatives, empty population |
| M3 | Invalidate dependent spatial cycle and incompatible boundary window, retain valid reference, allow expected-target re-anchor | Boot/source changes in clearance, break during collection, PWM-zero displacement, subsequent re-anchor |
| M4 | Output-only console adapter; usable state/MM and NAVI-qualified physical IR speed | Actual C++ JSON passed to actual repository controller state function and JavaScript speed renderer |
| M5 | Actual decision time/input ID; reception-time IR context; separate consumed-input and action records | Actual C++ binary records decoded by Python, NAVI/input/action joins, independent ring overflow |
| MINOR 1 | Compare new cumulative loss counters with previous values | A later Hall-only loss does not reapply historical IR loss |
| MINOR 2 | Track loss by locomotive, boot, session, record type; distinguish uncertain mixed loss | Interleaved streams, late packets, boot/session/loco changes, sequence wrap, STATUS counter semantics |

### Canonical refinement explicitly required and recorded

Decision **0114** records the human revision of 0113's startup clause and David's
follow-up selection of the **first five distinct observed pulse positions**.
0113 is marked partially superseded, not silently rewritten. The 100/200-mm
post-confirmation geometry and open spatial-policy questions are unchanged.

Closing the fifth population needs a subsequent distinct report; no elapsed-time
or sample-count cutoff is invented. Incomplete populations remain visibly
unready; no new automatic retry/stop behavior is claimed or authorized here.

### Files

- Integrated sketch/core/recorder and test runner: B1–M5 wiring.
- `NaviBootReference.h`, `NaviEstop.h`, `NaviCompatibility.h`: bounded NAVI-owned
  startup population, ordered safety latch, output-only compatibility.
- Integrated core/regression/record-emitter tests; Python interoperability tests.
- `tools/navi_sync_format.py`, receiver and decoder: versioned decision payload,
  consumption/action records and independent loss accounting.
- Integrated README, firmware catalog, governing directory, 0113 annotation,
  new 0114 and this correction record. No controller, profile, credential or
  legacy navigation implementation is changed.

## Verification and resource limits

Run the integrated `run_tests.sh` for sanitized map/core/recorder/operational/
audit/IR-architecture/station-position tests plus the original EWO regression.
Run `python3 -m unittest tools.tests.test_navi_sync_format tools.tests.test_ewo_integration`
for decoder and actual producer/consumer interoperability. Neither contacts a
broker, Pi service or locomotive.

The actual ESP32 compile uses `esp32:esp32:esp32`, installed core 3.3.12 and the
existing private credentials header via an include path. Only the committed
Toby profile is compiled; common-polarity macro tests are not an Otto build.
Final verification passed the complete integrated host runner and all nine
Python tests. The ESP32 build uses 1,002,867 / 1,310,720 flash bytes (76%) and
117,980 / 327,680 static RAM bytes (36%), leaving 209,700 bytes before runtime
allocations. The exact resulting commit SHA accompanies the handoff.

An initial compile exposed ESP32's static DRAM segment limit. The solution is
placement, not a different algorithm or smaller evidence window: checked fixed
heap allocations of 16,384 bytes for the histogram and 16,416 bytes for the new
trace storage. The histogram is released after successful initialization.
Allocation failure leaves motor output low before starting control tasks.
Arduino's static-RAM report does **not** include these allocations, queues,
network buffers or task stacks.

## Disposition and remaining evidence

The integrated README repeats all 21 legacy-authority dispositions. NAVI still
owns target-only confirmation, reference/IR applicability, degraded operation
and Missed Magnet. No X22R, upstream median, proximal/alternative-position
search, shadow accepted-MM reference or 10-MM AUTO recovery limit is introduced.
Normal stationary contrast/no-change semantics and common Toby/Otto polarity
remain. Station start-inside-zone behavior remains under the same policy.

Before controlled hardware work, David and Sam must review the corrected commit
and separately authorize deployment, including matching recorder tools. Use a
single intended IR source unless/until a source-selection policy is approved.
Measure heap/stack margin, loop/estop latency, five-position startup behavior,
and sustained recorder throughput; the new consumption trace adds about
180 KB/s at the nominal 5-kHz native Hall rate. Finite rings, UDP failure and
partial final batches prevent a lossless/exact replay claim. No on-track or
hardware result is inferred from these host tests.
