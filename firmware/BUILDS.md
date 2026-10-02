# Firmware build register

Every firmware build that can run on a locomotive gets a number, and every
number gets one row here. Rule: decision
[0119](../docs/decisions/0119-firmware-builds-are-numbered-and-registered.md).

How to use it:

- **Which firmware produced a log?** Read `build` in `state/loopstat` (or
  `sketch` in `state/bootid`) and look the number up below.
- **Changing firmware?** In the same commit: bump the build number in
  `SKETCH_NAME`, add a row to the table below, and say what changed.
- **Flashed a locomotive?** Add a line to the flash log, and mark the build's
  row "flashed". If nobody recorded it at the time, the first agent to analyse
  a log from that build adds it, citing the log.

Statuses follow `firmware/README.md` (Built, Reviewed, Field accepted…).
"Flashed" is a fact about a locomotive, not a status of quality.

## EWO line — `firmware/programs/NAVI_EYES_WIDE_OPEN_INTEGRATED/`

EWO-1 to EWO-13 were numbered after the fact (2026-10-02). Their firmware
does **not** report these numbers: EWO-1 reports
`NAVI_EYES_WIDE_OPEN_INTEGRATED_R1`, EWO-2 to EWO-12 all report
`NAVI_EYES_WIDE_OPEN_INTEGRATED_R2`, and EWO-13 reports `…_R2_FT2`. The
"Tell apart by" column says how to identify them from a log. From EWO-14 on,
the firmware reports its own number (`SKETCH_NAME = "EWO-14"`).

| Build | Commit | Date (PDT) | Branch | What changed | Tell apart by | Flashed / status |
|---|---|---|---|---|---|---|
| EWO-1 | `cd55929` | 09-29 02:27 | claude/navi-ps-r1-20q-standards | First integrated target-only NAVI candidate | name `…_R1` | Not recorded |
| EWO-2 | `caf2280` | 09-29 03:51 | codex/ewo-review-corrections | Review corrections, five-position startup reference (0114) | `…_R2`; no `ir_distance_state` | Not recorded |
| EWO-3 | `bf7b487` | 09-29 13:08 | codex/ewo-review-corrections | Single-sample provisional boot reference, missed-magnet origin (0115) | as above | Not recorded |
| EWO-4 | `43f358f` | 09-29 15:21 | codex/ewo-review-corrections | Explicit Otto credentials include | as above | Not recorded |
| EWO-5 | `7830483` | 09-29 18:23 | codex/ewo-review-corrections | Recorder allocation deferred to app startup | as above | Not recorded |
| EWO-6 | `c199380` | 09-29 18:43 | codex/ewo-review-corrections | Runtime network and IR diagnostics | as above | Not recorded |
| EWO-7 | `40a1925` | 09-29 18:56 | codex/ewo-review-corrections | Wi-Fi started before ESP-NOW | as above | Not recorded |
| EWO-8 | `9924314` | 09-29 19:12 | codex/otto-wifi-auth-diag | DIAGNOSTIC: Wi-Fi auth expiry and AP scan | as above | Probably on Otto during Wi-Fi diagnosis; not recorded |
| EWO-9 | `182a202` | 09-29 19:43 | codex/otto-wifi-auth-diag | DIAGNOSTIC: publish queue 48→16 | as above | Tested on Otto during Wi-Fi diagnosis (flash record, item 4) |
| EWO-10 | `b7bdfb5` | 09-29 19:53 | codex/ewo-review-corrections | Wi-Fi startup memory budget fix | as above | **Flashed to Otto 2026-09-29**, upload hash verified |
| EWO-11 | `d0185be` | 09-30 11:52 | codex/ewo-ir-authoritative | IR distance required for navigation, Hall-only navigation withdrawn (0116); speed display fix | first build with `ir_distance_state` in loopstat | **On Otto by 2026-10-02 12:04**; flash itself not recorded |
| EWO-12 | `c3c925a` | 10-02 12:59 | codex/ewo-first-target-after-declare | First target after declaration: window (0, interval+15%] (0117) | — | Built; not flashed; superseded by EWO-13 |
| EWO-13 | `ab0938b` | 10-02 | codex/ewo-first-target-after-declare | First target found by Hall onset, no distance window, no first-target miss (0118) | reports `NAVI_EYES_WIDE_OPEN_INTEGRATED_R2_FT2` | Built, reviewed; not flashed |

Not builds: `a156b70` (09-29 20:02) changed only a source comment. The
Wi-Fi-only probe `b35103a` (`OTTO_WIFI_MINIMAL_PROBE`) is a separate
diagnostic sketch, not an EWO build; it ran on Otto during the diagnosis.

## Flash log

| Date | Loco | Build | Evidence |
|---|---|---|---|
| 2026-09-29 (before 19:53) | Otto | An EWO build earlier than EWO-10; which one is not recorded | `field-records/20260929_OTTO_EWO_WIFI_DIAGNOSIS_AND_FLASH.md`: "stale in the dashboard after the integrated EWO candidate was flashed" |
| 2026-09-29 | Otto | Wi-Fi probe `b35103a`, then EWO diagnostic builds | same record, items 3–4 |
| 2026-09-29 ~20:00 | Otto | EWO-10 | same record: "uploaded to Otto, and the upload hash was verified" |
| 2026-09-30 | Otto | EWO-10 (consistent) | 09-30 run logs: `…_R2`, no `ir_distance_state` field |
| between 2026-09-30 15:04 and 2026-10-02 12:04 | Otto | EWO-11 | `9950011_20261002_120424.log`: loopstat carries `ir_distance_state`, which only EWO-11 emits. Inferred from telemetry; a local uncommitted build cannot be excluded |
