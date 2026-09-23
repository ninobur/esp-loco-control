# NAVI_COHERENCE package directory

Current orientation: [2026-09-23 work index](../../docs/NAVI_COHERENCE_CURRENT_WORK_20260923.md).

This directory holds Sam's incoming packages and extracted historical patches.
It is not the complete, field-exercised 0.5 build tree. That is
[`../programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_5_AUTO_ENABLED/`](../programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_5_AUTO_ENABLED/).
No files were relocated for this index.

First independent implementation:
[`IR_ARCHITECTURE_0_4/`](IR_ARCHITECTURE_0_4/), continuing the preserved
`NAVI_IR_ARCHITECTURE_0_3.zip`. Architecture version 0.4 is separate from
locomotive sketch version 0.5. The classes now compile, and runnable host tests
plus an ESP32 compile-only fixture pass. No NAVI decisions or live transport
have been changed. This is not firmware to flash to Toby.

Latest, 2026-09-23: the measurement layer is now wired into parallel transport
and diagnostics in [`../programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/`](../programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_HEALTH/).
This is the flashable Toby observation-only field candidate; AUTO/navigation
remain 0.5. Host and ESP32 builds pass, not yet flashed or field accepted.
See the [0.6 report](../../docs/NAVI_COHERENCE_0_6_IR_HEALTH_20260923.md).
The TX stationary-readiness limitation remains open. The independent
[step-one report](../../docs/NAVI_IR_HEALTH_STEP1_20260923.md) records the
preceding implementation stage, not the current integration status.

`NAVI COHERENCE IR HEALTH/` is an earlier intermediate patch, not the final Epoch
implementation. Preserve it as provenance rather than installing it over 0.5.
