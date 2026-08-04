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

## Cal recordings

Sixteen non-empty recordings, filenames as captured (they already encode
locomotive and timestamp). Twenty-eight zero-byte files were left on the Pi:
they record that a recording was started and nothing arrived, which the
absence of a file states equally well.

## Verdicts

| file | covers |
|---|---|
| `20260802_toby_lapA-sensor-flip-cert.md` | Toby's five certification metrics before/after the shielded cable |
