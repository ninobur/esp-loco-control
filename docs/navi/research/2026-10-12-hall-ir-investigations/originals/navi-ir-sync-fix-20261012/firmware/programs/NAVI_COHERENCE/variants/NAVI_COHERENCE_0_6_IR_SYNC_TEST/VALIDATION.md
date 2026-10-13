# IR_SYNC_TEST_R1 validation — 2026-10-12

## Required pre-flash gates

All gates requested by David/Sam passed before flashing:

1. **ESP32 3.3.12 compilation: PASS.** `arduino-cli 1.5.1`,
   `esp32:esp32:esp32`, Otto 9950011 profile, `--warnings all`.
   Flash 1,064,543 / 1,310,720 bytes (81%); static RAM 113,996 / 327,680
   bytes (34%). No sketch warnings. Three existing Adafruit INA219 enum
   deprecation warnings also occur when building the unchanged baseline.
2. **Regression tests: PASS.** All ten inherited C++ suites and the thirteen
   actual health JSON payload checks pass. The new regression exercises the
   original and corrected receive/service functions with actual movement,
   health and Navigator classes under identical Type-5/Hall inputs and times.
   Across 240 navigation judgments it injects Type-6 delivery/overflow, bad
   CRC, duplicates, reordered sequences, boot changes, wrong-source packets,
   malformed length, unavailable diagnostic queue, no Type-6, and unsupported
   version. Type-5 counters/history, health/MM references, recorder packets,
   navigator rulings and full public navigation status agree with the original.
   Genuine Type-5 overflow still follows the original loss path.
   AddressSanitizer and UndefinedBehaviorSanitizer report no errors.
3. **Original navigation behavior: source verification PASS.** All thirteen
   inherited headers are byte-identical to the matching historical baseline.
   Hall acquisition is unchanged. Type-5 service differs only by copying its
   accepted record into diagnostic history. The original control-loop call
   order is unchanged; new calls record a Hall timestamp and service diagnostics
   after the original control services. Type-6 has no reference to NAVI,
   movement, health, motor or station objects. Only its separate diagnostics
   state and queue can be changed by its validation/loss paths.

The comparison baseline is the supplied September 28 lineage,
`NAVI_COHERENCE_0_6_POSITION_STATIONS_R1_20Q3_SYNC_R1`, at the local historical
checkout HEAD `638c635`. It is not a claim of equivalence to the newer EWO
firmware that was installed on the connected board.

Other passing checks: causal receive-time selection, queued pre-Hall packets,
exclusion of post-Hall arrivals, post-gap raw evidence, MAC filtering, source
reset, sequence wrap/duplicates/order, history bounds, diagnostic Hall overflow,
and formatting bounds. Inherited tests were not changed. A fixture maps the
old filename expected by the 20Q3 source check to the corrected sketch.

These checks prove deterministic software isolation, not universal radio or
scheduling isolation. Added work and Serial output still use the same ESP32;
execution timing and delivery reliability require field measurement.

## Device and deployment

Connected serial port: `/dev/cu.wchusbserial110`.
ESP32-D0WD-V3 rev 3.1, MAC `68:09:47:ac:4e:b4`.
Pre-flash boot identifies profile `9950011` (Otto) and installed sketch
`NAVI_EWO_0_1_MM045_STOP`. The pre-flash boot already reports INA219 absent;
that condition precedes this replacement.

**Deployment: flashed and boot verified on Otto, October 12, 2026.**
The upload used the tested binary at 115200 baud; esptool verified every written
image hash and the uploader returned exit code zero. Post-flash Serial confirms
`NAVI_COHERENCE_0_6_IR_SYNC_TEST_R1`, profile `9950011`, AUTO-enabled field-test
classification, and MQTT connected. No panic or fatal boot error appeared in
the 15-second capture. The boot reports `NO_SOURCE / UNAVAILABLE` for IR and
the pre-existing INA219-not-found condition. No live Hall/Type-6 comparison or
physical movement test was performed.

Application SHA-256:
`1486b5041d18ac85c0e4677311907ab5f732136978e7ccada3fe70cbd543d89b`.

The full-chip read encountered serial corruption. Five independently successful
chunk reads preserve addresses `0x000000` through `0x27ffff` (2,621,440 bytes).
The actual upload writes only the bootloader at `0x1000`, partition table at
`0x8000`, boot metadata at `0xe000`, and application at `0x10000`; its highest
erased sector ends at `0x114000` exclusive. Thus every region changed by this
upload is covered by the rollback image. High flash above `0x27ffff` was not
backed up and was not written. This is a rollback image for the modified
regions, not a complete flash archive.

Rollback SHA-256:
`f8f7974e2a758d75382bbdd35abdafc0d6edf73dba8762efefd3be738ada6d29`.
The precise restore command and address coverage are in `rollback/manifest.json`.
Original application execution was held in the bootloader between successful
chunk reads, preserving the consistency of the backed-up regions.

Local evidence directory:
`/Users/davidbrown/NGR/NGR-Files/navi-ir-sync-fix-20261012/`.
Build log: `compile.log`; test summary: `all-tests.log`; detailed test logs:
`test-output/`; original boot: `preflash-boot.log`; backup progress: `backup.log`.
Upload: `upload.log`; verified boot: `postflash-boot.log`; source/build/device
hashes and gate results: `manifest.json`; chunked read results: `backup-retry.log`.
Rollback image: `rollback/otto-9950011-pre-ir-sync-0x000000-0x27ffff.bin`
(private device data, complete coverage of the sectors changed by this upload).
Original uploaded source and README are also preserved under `rollback/`.

No field acceptance, locomotive movement test, or IR transmitter deployment
is implied by the build, regressions, or upload. `FIELD_ACCEPTED` remains zero.
