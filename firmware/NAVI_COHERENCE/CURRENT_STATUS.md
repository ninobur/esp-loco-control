# NAVI_COHERENCE package directory

Current orientation: [2026-09-23 work index](../../docs/NAVI_COHERENCE_CURRENT_WORK_20260923.md).

This directory holds Sam's incoming packages and extracted historical patches.
It is not the complete, field-exercised 0.5 build tree. That is
[`../test-programs/NAVI_COHERENCE_0_5_AUTO_ENABLED/`](../test-programs/NAVI_COHERENCE_0_5_AUTO_ENABLED/).
No files were relocated for this index.

First independent implementation:
[`IR_ARCHITECTURE_0_4/`](IR_ARCHITECTURE_0_4/), continuing the preserved
`NAVI_IR_ARCHITECTURE_0_3.zip`. Architecture version 0.4 is separate from
locomotive sketch version 0.5. The classes now compile, and runnable host tests
plus an ESP32 compile-only fixture pass. No NAVI decisions or live transport
have been changed. This is not firmware to flash to Toby.

Next: resolve the documented TX stationary-readiness limitation and wire the
measurement layer into transport/diagnostics without changing navigation
decisions. See the [step-one report](../../docs/NAVI_IR_HEALTH_STEP1_20260923.md)
for tested behavior, design choices and the remaining boundary work.

`NAVI COHERENCE IR HEALTH/` is an earlier intermediate patch, not the final Epoch
implementation. Preserve it as provenance rather than installing it over 0.5.
