# IR validity and encounter qualification correction — 2026-10-11

Branch: `codex/navi-ewo-0-3-ir-validity`.
Local starting commit/rollback: `352a72e` on `codex/navi-ewo-0-3-complete`.
GitHub starting source: `16c65f61662c358923c780a2edffda9733f0a8da`.
The local starting tree exactly matches that published source. Commit identities
differ because the prior work was published through the GitHub Git-object API.
No history is rewritten.

**Scoped correction, incomplete physical missed-pulse verification. This is not
ready for field testing. Other independent review findings remain unresolved.**

## Original defects

1. `irPulseObservable_` had no age check. Continuing Type-5 snapshots could
   preserve distance authority after Type-6 deliveries stopped indefinitely.
2. `TimingAnomaly` (above 1.5 times the five-interval median) and the subsequent
   missing-compatible bracket withdrew distance authority. Neither distinguishes
   an uncounted optical cycle from ordinary wheel timing variation. Healthy-control
   replay contains both findings. A diagnostic question was given authority the
   evidence did not establish.
3. The full +/-15% mapped window was computed, but `judgeExpected()` only reported
   disagreement. The Hall qualifier enforced a lower-edge displacement guard;
   the upper edge did not qualify the encounter. Expected polarity could therefore
   confirm an outside-window encounter with purportedly credible IR.

## Exact code changes

- `NaviIntegratedCore.h`: the authority query now requires fresh Type-5 and
  Type-6 evidence, both using the existing `kIrFreshUs = 1,000,000` microsecond
  observation bound. It accepts an explicit current timestamp. Hall qualification
  and distance-state reporting use the same query. Expiry clears the existing
  pulse population; resumption must establish a new population rather than inherit
  old authority. Freshness expiry means unavailable supporting observations, not
  proven pulse loss. No speed limit or actuator restriction is added.
- Timing anomalies and missing-compatible brackets remain recorded diagnostics.
  They no longer set `irWindowQuestionable_`, `reanchorNeeded_`, or withdraw
  authority by themselves. The diagnostic pulse algorithm and thresholds are not
  promoted to a physical missed-pulse classifier or replaced with a new classifier.
- `NaviEvidenceBridge.h` and `NaviEvidenceCore.h`: pass the already calculated
  mapped window to encounter qualification, preserving its value at the first
  qualifying Hall sample. With usable distance, an outside-window candidate is
  not a qualified encounter and cannot reach polarity identity judgment. It does
  not move the last qualified encounter's exclusion origin. With unavailable IR,
  the existing detection-to-detection 650-ms guard applies. The existing continuous
  field latch remains unchanged; this work does not solve the stationary finding.
- The established first-target-after-declaration exemption has no mapped marker
  origin and remains unchanged. Magnetic polarity, DNA logic, geography, map,
  calibration, STOP, AUTO and PWM code are unchanged. No missing-marker recovery
  changes are attempted. Qualification changes only when an encounter is admitted
  to the unchanged geographic judgment.

## Required verification and results

`sh firmware/programs/NAVI_EWO_0_3/run_tests.sh`: exit 0.
Full output: [verification/ir-validity-host-results.txt](verification/ir-validity-host-results.txt).

| Requested behavior | Result |
| --- | --- |
| Current continuous Type-6 evidence selects distance qualification | Pass, actual integrated core; distance works before 650 ms |
| Both +/-15% boundaries govern qualification | Pass, calibrated pulse counts inside/outside both edges; opening rather than second-sample coordinate governs |
| Invalid evidence selects 650-ms fallback | Pass, 649,999 us rejects; 650,000 us qualifies; invalid undercount cannot veto |
| Ordinary timing variation does not falsely withdraw authority | Pass, including the old long-interval/returning-normal bracket |
| Stopped Type-6 cannot remain indefinitely authoritative | Pass, explicit queries at 1,000,000 and 1,000,001 us; continued Type-5 alone cannot refresh Type-6 |
| Supporting evidence can resume | Pass, first resumed event does not inherit old authority; fresh population restores it |
| 70 counts, two samples, 650 ms, calibration preserved | Pass, static constants and existing threshold/cadence regressions; installed pitch remains 9,652 um |
| Genuine physical missed pulse invalidates IR | **Not demonstrated; unresolved with present evidence** |
| Existing regressions | 23 C++ executions and 31 Python methods pass, plus native transport and pulse fixtures |
| STOP/actuator preservation | Existing source comparison and actual shell tests pass |
| Full ESP32 compile of this correction | Blocked: ESP32 platform/core absent; downloads.arduino.cc unreachable |

The original distance-duplicate regression was expecting distance authority after
Type-6 had expired. Its setup now keeps Type-6 within the existing freshness bound;
a separate test explicitly proves expiry selects fallback. The old timing test's
expectation of automatic authority withdrawal is superseded by this correction;
it now proves diagnostic timing alone retains authority and tests invalid delivery
observability separately. Initial new test setup also exposed pulse quantization
at the lower edge and an older Type-5 age than Type-6; these setup errors were
corrected without changing constants or measured distances. Final suite has no
failures. Address/undefined-behavior sanitizers remain enabled; leak inspection
remains excluded by the container limitation recorded previously.

The recorded replay is unchanged: 22,940 events, healthy control 136 timing questions
and 17 missing-compatible brackets. Those findings remain diagnostics. Zero proven
physical missed pulses and no magnetic DNA replay are claimed. Delivery gaps and
counter jumps are tested as lost observability, never called physical missed pulses.

## Remaining limitation: no-missed-pulses requirement

Type-6 reports a completed *detected* pulse and its cumulative detected count.
An optical cycle that was never detected increments neither field. Consecutive
counts and continuous delivered records therefore do not prove no physical pulses
were missed. A longer completion interval alone can represent slowdown, binding,
slip or an undetected cycle. The packet does not independently measure the wheel's
physical phase or expected true cycle count. Type-5 coherence does not supply that
missing truth either.

This correction removes the demonstrated stale-authority and false timing-veto
defects and enforces the specified qualification window. **It does not establish
that the surviving continuity-based authority predicate satisfies the physical
"no missed pulses" condition.** That part of IR validity remains unresolved, rather
than silently replacing it with a new timing threshold or declaring consecutive
telemetry to be proof. Resolving it needs an established, evidence-supported
physical missed-pulse criterion or independent reference evidence. No such new
criterion is selected in this assignment, and no fake missed-pulse acceptance test
is offered. The present code can still retain authority for an undetected physical
pulse; this is an explicit remaining defect, not a successful test exclusion.

The reused one-second bound is a bounded observability check, not a claim that
silence of that duration establishes physical pulse loss. It can also make evidence
unavailable during sufficiently sparse genuine pulses, selecting only the already
specified fallback. The choice is explicit for review; no timing or distance
constants were recalibrated.

No merge, flash, Pi access, service modification or deployment was performed.
