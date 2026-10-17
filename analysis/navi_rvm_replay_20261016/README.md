# NAVI_RVM replay validation (October 15, 2026 recordings)

Research code and result summaries for
[`docs/NAVI_RVM_REPLAY_VALIDATION_20261016.md`](../../docs/NAVI_RVM_REPLAY_VALIDATION_20261016.md).
Copied from the working package `~/NGR/NGR-Files/navi-rvm-replay-20261016/`, which also holds the
large derived arrays (blocks, steps, templates, alignments) that are not committed here.

Source recordings (read only): `~/NGR/NGR-Files/hall-ground-20261015/derived/native_*.bin`,
`ir_coordinate_source.csv`, and the IR_WAVE_1 field record (`selected_records.json`,
`session_commands.csv`, `interval_discrepancies.csv`).

Run order: blocks.py → context.py → template.py → speed_model.py → experiments.py → experiments2.py →
experiments3.py. `results/FROZEN.json` records the parameters and file hashes frozen before the boot 2
holdout. Not firmware; not for deployment.
