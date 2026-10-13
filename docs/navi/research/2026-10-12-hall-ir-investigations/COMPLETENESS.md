# October 12 research completeness report

## Bounded scope

The archive covers all nine investigations requested in the October 12 preservation assignment. [ARTIFACTS.md](ARTIFACTS.md) lists the stored materials, and [source-manifest.json](provenance/source-manifest.json) maps every preserved artifact to its original path, exact byte count and SHA-256. Exclusions are individually recorded in the same manifest. The index's evidence classifications qualify the original reports; original wording and unsuccessful hypotheses remain intact.

This is completeness of the identified experimental archive, not completeness of all NAVI engineering history. The parent checkpoint and its 1,599 unfinished reviews remain unfinished. This assignment does not create completed versions of `NAVI_ENGINEERING_CONTINUITY.md`, `NAVI_DECISION_HISTORY.md` or `NAVI_EXPERIMENTAL_HISTORY.md`.

## Coverage

| Requested investigation | Preserved evidence | Reproduction or limitation |
| --- | --- | --- |
| Hall field morphology | Original report, JSON, three plots, script, full raw NSR capture and six supporting logs | Replayed from archived sources; same numerical results |
| Opening to peak | Report, readable magnet table, per-magnet/per-passage/interval/direction CSVs, JSON, five plots and script | Replayed; physical peak-center assumption remains unverified |
| Opening geographic map | Report, four CSVs, JSON, five plots and script | Replayed; no independent operational calibration validation |
| IR odometry accuracy | Map/odometry report, timing/quantization results and MM115→116 evidence | Runtime integers and offline interpolation kept separate |
| Candidate selection | Report, five CSVs, JSON, five plots and analysis script | Replayed; actual-data outcomes and synthetic cases remain separately labeled |
| Raw events 254–257 | Report, two plots showing all conversions/medians, CSV, JSON and script | Replayed; no baseline return established, physical field overlap not established |
| Reporting frequency | Report, JSON, plot and arrival-age script; repository source context is pinned by the parent checkpoint | Replayed arrival-age measurement; historical radio-delivery claims retain their original source scope |
| Complete circuits | Report, endpoint and circuit CSVs, original inline command records, new independent raw-count verifier | Counters and aligned brackets reproduced; geographic reconstruction remains an interpretation |
| IR synchronization experiment | Superseded submission and notes, full independent review, failing/passing build logs, defect probes, corrected source and tests, implementation and final outcome records | Original defects and corrected regressions replayed separately; see [final outcome](IR_SYNC_OUTCOME.md) for deployment status and field limits |

## What is missing or deliberately omitted

1. **Original standalone circuit analysis script:** none existed in the source directory. The work used inline commands. Those commands were recovered from the original chat and preserved verbatim as command records, including failed attempts. The archive adds a clearly identified verifier for raw endpoint counts, brackets and table arithmetic. It does not retroactively label that verifier as the original research script.
2. **Independent physical position truth:** no synchronized tape survey, independent wheel encoder, physical magnet-center survey, or individual IR pulse timestamps exist in the September 28 recording. These were not lost during archiving. Absolute physical accuracy, exact optical age and the no-missed-physical-pulses premise cannot be reconstructed from this archive.
3. **Coverage for external validation:** repeated CCW calibration groups, a complete CCW circuit, comparable second-locomotive observations and a later unchanged-layout replication are absent. Physical track and instrumentation may have changed since September 28.
4. **Type-6 geographic field validation:** the corrected experiment compares receiver-arrival observations. It does not implement or validate Type-6 Hall geography. Build, deterministic isolation tests and any recorded boot are not railway timing/reliability acceptance. A paired transmitter and controlled physical run remain necessary.
5. **Local credentials and device binaries:** `credentials.h`, full flash backups, backup chunks and application/build binaries are excluded. Device images may contain private configuration. The correction manifest retains relevant hashes and device/build identity, while exact nonsecret source, dependencies, tests and logs are preserved. A full Arduino rebuild requires separately supplied credentials and the documented external toolchain/libraries.
6. **Regenerable machine artifacts:** Python/Matplotlib caches, host executables, debug-symbol bundles and compiler intermediates are excluded. Source, generated test source where useful, compiler logs and regression outputs are retained. The exclusions list accounts for these; they are not missing experimental measurements.
7. **Full conversation transcripts:** the repository preserves the reports, source artifacts, essential original circuit commands and final correction result. It does not claim to export all private conversation history or complete the parent checkpoint's transcript/authority reconciliation.

## Verification

- All source entries pass original and stored SHA-256 verification; compressed recordings restore exact bytes.
- Six numerical JSON outputs match the original calculations, and [14 generated CSVs match byte for byte](verification/csv-comparison.json).
- Raw endpoint counters reproduce the 5,342 / 5,385 / 5,351 circuit totals and aligned count brackets.
- The four original logic defects reproduce with the submitted source and historical dependencies.
- The corrected archive passes ten inherited C++ suites, thirteen health JSON cases, source-equivalence assertions and the 240-judgment Type-6 isolation regression.
- Relative links and staged archive coverage are checked before commit. Only this research directory is added; operational firmware, governing decisions, magnet maps and calibration constants remain unchanged on this branch.

See [reproduction instructions](REPRODUCIBILITY.md) and [verification records](ARTIFACTS.md#archive-verification) for exact scope. No firmware build or flash is performed by the preservation assignment. The separate correction chat's original build and deployment records are historical evidence, explicitly distinguished from archive replay.
