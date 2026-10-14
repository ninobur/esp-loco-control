# NAVI EWO 0 5 IR credibility integration

October 14, 2026. The original operator criteria remain verbatim in
[APPROVED_RULES.txt](APPROVED_RULES.txt). Its 0_4 title predates the numbering
clarification; the integrated continuation is now `NAVI_EWO_0_5`.

The evaluator is now called by the actual navigator after a unique geographic
selection. Credibility limits never reject Hall candidates. Signed and absolute
residuals, pulse equivalents, both endpoint IDs and measurements, source/boot/
epoch/receiver-time provenance, expected distance, comparison method and
transition are serialized through the production diagnostic formatter.

- ≤10,000 µm absolute discrepancy: consistent, retain authority.
- >10,000 and <48,260 µm: inconsistent but possible, retain authority.
- ≥48,260 µm: immediate withdrawal, preserving the geographic identification.
- Continue acquisition while withdrawn. The withdrawal opening is not its own
  restoration reference. The next independently identified real opening starts
  the new interval; its independently identified successor can restore authority
  if both endpoint measurements are applicable and discrepancy is <48,260 µm.
  An interval >10 mm may restore authority while remaining inconsistent.
- Fresh packets alone do not restore geographic authority. Missing measurement,
  an inferred midpoint, a changed frame or reversed traversal cannot certify a
  measured restoration interval. No pulses or mapped coordinates are fabricated.
- Subsequent selections use the previous complete lap's timing during withdrawal.
  The method changes without replaying one opening as two geographic advances.

The isolated evaluator passes 223 assertions. Production integration passes
410 assertions, including both error signs, exact limits, new-reference
restoration, repeated subthreshold residuals, missing coordinates and timing,
and preservation of chosen anchors. The extracted telemetry fault test passes
712 assertions. Two labelled withdrawal/restoration records were read back
from the Pi's actual MQTT file; see [recorder proof](../mqtt-recorder-result.json).

The historical timing source, no-Hall deadline and mid-tile throttle transition
are now settled in [the operator decisions](../OPERATING_DECISIONS_20261014.md).
Manual STOP/GO is also settled: pause/resume both timing clocks while retaining
identity, subject to the existing moved-while-stopped declaration safeguard.
The [review report](../REVIEW.md) records compilation and all test limitations.
Neither the numerical criteria nor previous-lap timing reliability are field
validated. See [the deployment follow-up](../STOP_GO_AND_OTTO_DEPLOYMENT_20261014.md) for
the initial failed upload and rollback. The subsequent
[startup resource repair](../startup-repair/REPORT.md) is now installed on Otto,
including these authority rules. Railway validation is still outstanding.
