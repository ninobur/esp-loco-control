# NAVI_EWO_0_3 — initial implementation status

Baseline target: EWO 0.1 commit 85a2cf2. This directory is an isolated, **partial** implementation, not a complete sketch or deployable candidate.

Implemented: host-testable Hall 70x2 qualifier with detection-started 650 ms guard and separate opening/detection timestamps; bounded 12-entry Hall/DNA evidence ledger retained across confirmations. No map position is inferred by the qualifier or ledger. No motor, STOP, calibration, governance, or firmware files modified.

Tested: `g++ -std=c++17 -Wall -Wextra -Werror tests/test_evidence.cpp -o /tmp/test_evidence && /tmp/test_evidence` passes.

Not implemented yet: full NAVI 0.1 integration, IR quality evaluation, DNA matching/reconciliation, declaration/reversal behavior, physical displacement duplicate protection at station speeds, replay tests, full ESP32 compilation. The 650 ms guard alone is not sufficient for stationary/repeated encounters. No code has been flashed.

Integration requires exact 0.1 dependencies and the full test suite. This initial module is not evidence of complete NAVI correctness.
