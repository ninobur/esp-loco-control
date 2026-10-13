# IR synchronization experiment review and correction

The final experimental source is the corrected **NAVI_COHERENCE_0_6_IR_SYNC_TEST_R1** snapshot. The initial submitted sketch is preserved as superseded evidence. Neither version is NAVI_EWO_0_4 or a validated Type-6 geographic synchronization implementation.

## Original submission and independent review

- [Submitted sketch](dependencies/review-baseline/firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_SYNC_TEST.ino) and [submitted notes](dependencies/review-baseline/firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_SYNC_TEST_README.md)
- [Full independent review](originals/navi-ir-sync-review-20261012/REVIEW.txt), [source manifest](originals/navi-ir-sync-review-20261012/manifest.json), [submitted diff](originals/navi-ir-sync-review-20261012/submitted-changes.diff)
- [Failed submitted build](originals/navi-ir-sync-review-20261012/compile.log), [passing baseline build](originals/navi-ir-sync-review-20261012/baseline-compile.log), [defect reproduction output](originals/navi-ir-sync-review-20261012/probe-results.txt), [original probe generator](originals/navi-ir-sync-review-20261012/reproduce_review.py)

The submitted source SHA-256 is `e0facae1241425f3a97e984a608011dd3b14b5c11ee7b6f8c9a69375dae14fa6`. The review compared it with the matching historical `NAVI_COHERENCE_0_6_POSITION_STATIONS_R1_20Q3_SYNC_R1` baseline at local checkout HEAD `638c6354df39ab73dd758d58126d9eaaaf65c562`. The review manifest gives exact hashes of the baseline and 13 local headers, including the pre-existing Otto profile selector. A commit name alone would not capture that local selector; this archive preserves its bytes.

Five defects/findings were demonstrated:

| Finding | Original evidence | Corrected handling |
| --- | --- | --- |
| Declaration ordering prevents compilation | `SyncPoint` used before definition; Arduino-generated declarations precede two needed types | Diagnostic helper moved into an ordered header; full 3.3.12 build succeeds |
| Type-6 loss alters Type-5/navigation state | Shared 24-entry queue overflow calls movement/health reset paths | Separate static 32-entry diagnostic queue and loss accounting |
| Queued pre-Hall arrivals omitted | Hall snapshot freezes last processed packet instead of latest received packet | Retained arrival histories and deferred diagnostic comparison after navigation services |
| Foreign transmitter overwrites Type-6 evidence | No paired-MAC check | Same paired source required; re-pairing clears diagnostic state |
| Post-gap counts disappear | Valid count replaced by zero/unavailable output | Raw count/time/boot/sequence retained separately from continuity and eligibility |

Four logic defects reproduce using extracted submitted functions, real movement/health headers and mocked clocks/queues. The compile failure is independently documented by the original ESP32 build log. These are demonstrated software failures, not proof of how often a physical queue or radio fails.

## Corrected implementation

- [Corrected sketch](originals/navi-ir-sync-fix-20261012/firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_SYNC_TEST/NAVI_COHERENCE_0_6_IR_SYNC_TEST.ino)
- [Diagnostic helper](originals/navi-ir-sync-fix-20261012/firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_SYNC_TEST/IrSyncDiagnostics.h)
- [Implementation README](originals/navi-ir-sync-fix-20261012/firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_SYNC_TEST/README.md) and [validation record](originals/navi-ir-sync-fix-20261012/firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_SYNC_TEST/VALIDATION.md)
- [Implementation manifest](originals/navi-ir-sync-fix-20261012/manifest.json), [baseline diff](originals/navi-ir-sync-fix-20261012/diff-from-baseline.patch), [build log](originals/navi-ir-sync-fix-20261012/compile.log), [all-tests output](originals/navi-ir-sync-fix-20261012/all-tests.log)
- [Unchanged regression runner](originals/navi-ir-sync-fix-20261012/firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_SYNC_TEST/tests/run_all.py), [isolation test](originals/navi-ir-sync-fix-20261012/firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_SYNC_TEST/tests/test_ir_sync.py)

The corrected sketch SHA-256 is `e9c5f6fccd253188bd4da7548e3f18018d123a91c3f36cb4c248b66c1f5d9cb9`. The new helper SHA-256 is `59962fe73dcd3dbd0b358091327a49f8a30a33910fad295201bea5f7ca51577d`.

The corrected build uses ESP32 core 3.3.12 and the Otto 9950011 profile: 1,064,543 bytes flash and 113,996 bytes static RAM. The build reports no sketch warnings; three inherited Adafruit INA219 deprecation warnings also occurred in the baseline build. Compilation does not establish field readiness.

Ten inherited C++ suites, thirteen health-JSON cases and the new isolation/timing test passed. Across 240 actual Navigator judgments under ten synthetic Type-6 delivery/failure patterns, Type-5 state, health/reference, recorder output and navigation status match the historical baseline. Source checks retain all 13 inherited headers and original control flow apart from diagnostic taps. This proves deterministic isolation relative to the supplied historical baseline, **not equivalence to the newer EWO software installed on Otto before this experiment**.

The archive independently reran the corrected tests from preserved files: [result](verification/corrected-result.json), [full output](verification/corrected-regression.log) and [tested source hashes](verification/corrected-tested-source-hashes.json). See [reproduction instructions](REPRODUCIBILITY.md).

## Final correction and deployment outcome

The separate chat “Review NAVI IR sync test” completed the correction, passed its required build/regression/source-equivalence gates, and uploaded the already-tested binary to Otto 9950011. The saved manifest reports `UPLOAD_AND_BOOT_VERIFIED`, upload exit code 0 and verified R1 boot identity. These actions occurred under that chat's separate user authorization; the preservation assignment did not operate hardware.

The [upload log](originals/navi-ir-sync-fix-20261012/upload.log), [post-flash boot](originals/navi-ir-sync-fix-20261012/postflash-boot.log), [pre-flash boot](originals/navi-ir-sync-fix-20261012/preflash-boot.log), [final validation record](originals/navi-ir-sync-fix-20261012/firmware/programs/NAVI_COHERENCE/variants/NAVI_COHERENCE_0_6_IR_SYNC_TEST/VALIDATION.md) and [completed chat result](provenance/final-sync-chat-result.json) preserve the result. Application SHA-256 is `1486b5041d18ac85c0e4677311907ab5f732136978e7ccada3fe70cbd543d89b`. Boot reported no available IR source and the pre-existing INA219-not-found condition, with battery protection unavailable for that session. No physical movement or paired Type-6 timing-comparison field test was performed, and `FIELD_ACCEPTED` remains zero.

Full-chip backup reads failed with serial errors. Five successful chunks preserved addresses `0x000000` through `0x27ffff`, covering every sector changed by this upload. The high flash region was neither backed up nor written. This was **not a complete flash archive**. [Rollback manifest](originals/navi-ir-sync-fix-20261012/rollback/manifest.json), [failed/retried read log](originals/navi-ir-sync-fix-20261012/backup.log) and [chunk summary](originals/navi-ir-sync-fix-20261012/backup-retry.log) preserve that qualification. The private rollback bytes are excluded from Git; their SHA-256 is `f8f7974e2a758d75382bbdd35abdafc0d6edf73dba8762efefd3be738ada6d29`.

The corrected implementation was saved to the local historical checkout; this archive does not claim a separate firmware implementation commit. The exact source snapshot and its test/build records are now preserved as documentary evidence on the research branch, without changing its operational firmware tree.

## Remaining experimental limits

This is AUTO-enabled development firmware with `FIELD_ACCEPTED=0`, not a passive receiver-only sketch. Type-6 diagnostic data never become navigation inputs. The two board clocks remain unsynchronized; reported age is receiver-arrival age. Type-6 records remain Serial-only because the NSR1 wire format is unchanged. Added CPU/Serial/radio activity still needs physical timing and reliability measurement even though host-state isolation passes.

The September 28 transmitter did not provide Type 6. The correction assignment does not change the transmitter; a compatible paired transmitter is required for a meaningful comparison. No successful build, backup, upload or boot establishes wheel-scale accuracy, complete optical counting, railway behavior, or an operational geographic benefit. Those remain unresolved experiments for later NAVI work.
