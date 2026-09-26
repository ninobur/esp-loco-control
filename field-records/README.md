# field-records

Committed evidence from the railway: session logs as captured, cal
recordings as captured, and the analysis verdicts drawn from them. This is
the record the specs and implementation reports cite.

```
logs/       session logs, YYYYMMDD_loco_purpose.log
cal/        cal recordings, as captured (cal_<locoid>_<date>_<time>.txt)
verdicts/   per-session analysis, named to match its log
```

`.gitignore` ignores `*.log` and `logs/` repo-wide — deliberately, since run
captures are large and constantly changing. `field-records/logs/` is the
curated exception and is un-ignored there; only logs worth keeping belong
in it.

## Logs

Named from content, not from the filename they arrived with: the date is the
log's own first timestamp, and the locomotive is the one whose topics the
log carries.

| file | source on Pi | span | loco | notes |
|---|---|---|---|---|
| `20260730_otto_v221-v222-outage-tests.log` | `~/run_1905.log` | 2026-07-30 19:05 → 08-01 15:47 | Otto 9950011 | spans SOLONAV_2_17 → 2_22; the broker-outage and marker-drain work |
| `20260731_otto_v222-outage-test-capture2.log` | `~/run_1947.log` | 2026-07-31 19:47 → 08-01 15:47 | Otto 9950011 | second concurrent capture of the same period, retained for cross-checking |
| `20260801_otto_aborted-session.log` | `~/run_0801_1555.log` | 2026-08-01 15:55 (38 s) | Otto 9950011 | 2 KB; logger restarted immediately — kept so the sequence has no gap |
| `20260801_otto_chain-v3-cert.log` | `~/run_0801_1600.log` | 2026-08-01 16:00 → 08-03 15:31 | Otto 9950011 | contains the 16:03 dashboard-failure session that produced v1.10.1 |
| `20260802_toby_lapA-sensor-flip-cert.log` | `~/toby_cert_0802_1831.log` | 2026-08-02 18:31 → 08-03 20:54 | Toby 9950012 (+ Otto traffic) | Lap A / sensor-flip certification; analysed in `verdicts/` |
| `20260810_IR_SPEED_LOCAL_1_2_otto.log` | `~/NGR/telemetry/runs/20260810_IR_SPEED_LOCAL_1_2_otto.log` | 2026-08-10 19:19 → 21:09 PDT | Otto 9950011 + IR_SPEED_SENSOR | Merged beta-test capture: Otto Hall/navigation and IR test-car telemetry. Contains the three `HARD_BOUND` NO_QUORUM incidents, including the final Patio-area incident at log lines 25020–25120. |
| `20260926_toby_20q3_run1.log` | operator capture `9950012_20260926_085225.log` | 2026-09-26 08:52:25 → 08:54:53 | Toby 9950012 | first run of NAVI_COHERENCE 0.6 POSITION_STATIONS_R1_20Q3 (`c1651ef`); manual, MM049→045 CCW; verdict in `verdicts/` |
| `20260926_toby_20q3_run2.log` | operator capture `9950012_20260926_085454.log` | 2026-09-26 08:54:54 → 09:37:00 | Toby 9950012 | 20Q3 (`c1651ef`), same boot as run 1; manual MM045→MM021 CCW; three Epoch-loss/resync cycles; ends with IR car hand-tilted over MM21 (contaminated after ~09:35:37); verdict in `verdicts/` |
| `20260926_toby_20q3_run3.log` | operator capture `9950012_20260926_093737.log` | 2026-09-26 09:37:37 → 09:45:04 | Toby 9950012 | 20Q3 (`c1651ef`), same boot; no redeclaration; entire run in reverse (CW) from physical MM21 to MM053; IR car tilted only after the stop; verdict in `verdicts/` |

## Cal recordings

Sixteen non-empty recordings, filenames as captured (they already encode
locomotive and timestamp). Twenty-eight zero-byte files were left on the Pi:
they record that a recording was started and nothing arrived, which the
absence of a file states equally well.

## Verdicts

| file | covers |
|---|---|
| `20260802_toby_lapA-sensor-flip-cert.md` | Toby's five certification metrics before/after the shielded cable |
| `20260926_toby_20q3_run1.md` | 20Q3 run 1: Hall-only → Epoch MM-reference handoff, ±15% windows at 1.03/0.97/1.03, X22R cadence-gate observation, next test |
| `20260926_toby_20q3_run2.md` | 20Q3 run 2: recovery chain ×3 incl. mid-interval stop, ~402 ms extra Hall rejected by IR, #30 false advance to MM020 during manual handling (contamination noted) |
| `20260926_toby_20q3_run3.md` | 20Q3 run 3: two-MM identity error across a reversal, uncorrected by recovery (TRAVEL_UNAVAILABLE_HOLD); IR rejected three false Hall events incl. a stop-on-magnet re-detection at 720 ms |
