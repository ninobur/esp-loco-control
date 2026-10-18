# NAVI_RVM — Otto review candidate

This is the compilable candidate requested by the [implementation assignment](../../../docs/NAVI_RVM_CODEX_IMPLEMENTATION_ASSIGNMENT_20261017.md). It has not been flashed or field accepted. The focused recorded stall check fails; recognition timing and station compatibility also require review before a powered test.

Exact sketch: `firmware/test-programs/NAVI_RVM/NAVI_RVM.ino`.

The recognizer carries a bounded persistent alignment against one immutable CW/CCW reference bundle. It compares continuous Hall shape at the expected next location, emits one tile token, and later assesses that new MM's quality once. It does not run the retired Hall-opening detector. The board is never learned or replaced during a run.

Otto's inherited ESP32 profile uses locomotive ID 9950011, Hall GPIO 33, motor PWM GPIO 4 and direction GPIO 16. This model is not for Toby. The route, four station centers, operator commands, native recorder, motor ramps, independent E-STOP and electrical protections retain the local EWO controller implementation. The changed navigation-to-controller timing is described in the reconciliation.

From the repository root:

```sh
# Requires a C++17 compiler and Python 3; no recording archive or numpy required.
python3 tools/navi_rvm/check.py

# ESP32 core 3.3.12, Adafruit INA219 1.2.3, BusIO 1.17.4, PubSubClient 2.8.
# Compiles with inert review credentials unless a private include is supplied.
python3 tools/navi_rvm/build.py

# Optional local build with existing private credentials; this still only compiles.
python3 tools/navi_rvm/build.py --credentials-dir /path/to/private/config
```

`check.py` intentionally exits **1** while the recorded stall failure remains. Deterministic software checks pass, and the script still writes every decision, comparison and finding under `tests/navi_rvm/results/`. Do not reinterpret its expected failure as a passing suite. Neither script uploads firmware or contacts Otto.

The compiled reference and causal excerpt CSVs are committed, so review and replay need no private files. `prepare_reference.py` documents extraction from the original local archives and needs those archives plus numpy; it is not a prerequisite for building or replaying this candidate. `calibrate.py` recreates the separate calibration experiment and rewrites the calibration header; do not run it as an evaluation step.

- [Requirement reconciliation](../../../docs/NAVI_RVM_IMPLEMENTATION_RECONCILIATION.md)
- [Build, measurements, failures and provenance](../../../docs/NAVI_RVM_BUILD_AND_TEST_REPORT.md)
- [Proposed field procedure and rollback](../../../docs/NAVI_RVM_FIELD_TEST_PROCEDURE.md)
- [Committed test evidence](../../../tests/navi_rvm/results/summary.json)
