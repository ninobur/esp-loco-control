# IR missed-spoke analysis (2026-10-11)

These scripts support two reports:

- `docs/NAVI_IR_185PCT_MISSED_PULSE_EVALUATION_20261011.md`
- the segment-validity detector section in the same report

The analysis is read-only. It extracts its inputs from repository history:

| Input | Commit |
|---|---|
| 0_3 fixtures | `253faa4` |
| October 9 raw log | `bf95cc4` |
| Route map | `85a2cf2` |

Run it with:

```sh
sh tools/analysis/ir_missed_spoke_20261011/run.sh [workdir]
```

It needs `python3`, `c++` and `gzip`.

These scripts are analysis only. Their thresholds are not approved navigation rules, and no firmware uses them.
